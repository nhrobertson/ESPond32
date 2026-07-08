# ESPond32 — Implementation Status Evaluation

_Evaluation of firmware vs. the design doc (`../../ai_design_doc.pdf`, "Pond Control Unit — Design & Build Plan"). Date: 2026-07-07. No code was modified to produce this — full read-through of every `.c`/`.h` file in `main/` and `components/`._

## Overall status

The architecture and most of the parsing/scheduling logic are well-reasoned and track the design doc closely: a device abstraction unifying pumps/valve/light/float behind one polymorphic `device_ops_t` interface, FreeRTOS tasks that split cleanly along the doc's lines (input check / config apply / schedule+precedence resolve / operate / leak / net), NVS-backed JSON config with version gating, and an MQTT sync layer with retained topics and Last-Will.

That said, **this is not "almost done."** A full read of every source file turned up several bugs that would prevent correct operation if flashed today — including bugs very likely to crash on first boot, and one that defeats the design doc's single most important correctness rule (leak lockout must force outputs off). This reads as **mid-Phase-3/4 firmware in progress**, not a working autonomous controller. The hardware/PCB (design doc phases 6–10), home server (phase 11), and enclosure (phase 12) are entirely out of scope for this repo and not evaluated here.

## Firmware, phase by phase (design doc §14)

| Phase | Status | Notes |
|---|---|---|
| 0 — Toolchain | Done | ESP-IDF project targets `esp32s3`; managed components (cJSON, esp-mqtt, led_strip) resolve; project builds as a normal IDF tree. |
| 1 — Bare I/O | Mostly done | GPIO configure/read/set, majority-count software debounce, and the WS2812 LED driver init all exist and are structurally sound — but see LED bug below: nothing is ever actually pushed to the physical strip. |
| 2 — Timekeeping | Mostly done | SNTP is started and a timezone (`EST5EDT,M3.2.0,M11.1.0`) is set in `network.c`, addressing the exact drift bug the redesign was meant to fix. `get_local_time()` exists and is used by both the scheduler and the leak/float logic. No DS3231/RTC (called out as optional in the doc). |
| 3 — Scheduler + Output manager | Implemented, but the safety-critical path is broken | `evaluate_device_schedule()` correctly handles overnight wraparound and per-weekday masking. `resolve_device_state()` implements the switch/override precedence. **But** the leak-lockout tier silently disables the entire resolver instead of forcing outputs off — see Critical Bug #2. |
| 4 — Float-fill + leak | Implemented, with two logic bugs | State machine (idle → arming → filling → overflow) exists and daily fill counting/latching exists, but the configurable `threshold_min` debounce is never applied (Critical Bug #6) and the MQTT "clear leak lockout" command doesn't actually clear the lockout (Critical Bug #7). |
| 5 — Connectivity | Implemented, least-hardened area | WiFi STA, MQTT with Last-Will, version-gated config → NVS persistence, exponential backoff reconnect, and status publishing all exist. Credentials are still hardcoded placeholders (`"ssid"`, `mqtt://IP_ADDR`) and MQTT is plaintext — expected at this stage. `publish_status` has a memory-corruption bug (Critical Bug #5). |

## Critical bugs (would misbehave or crash on real hardware)

1. **`ovr_change_mutex` is never created — likely crash on boot.** `main/espond32.c:43-44` creates `cfg_buff_mutex` and `cfg_change_mutex` via `xSemaphoreCreateMutex()`, but never does this for `ovr_change_mutex` (declared `components/models/models.c:14`). `devices_init()` (`components/device/device.c:89`) calls `xSemaphoreTake(ovr_change_mutex, portMAX_DELAY)` during the very first boot pass — a `NULL` semaphore handle, which FreeRTOS/ESP-IDF will typically assert or fault on.

2. **Leak lockout doesn't actually turn anything off — and the physical OFF switch stops working once it's active.** `task_evaluate_cfg` (`components/tasks/tasks.c:117-119`):
   ```c
   for (int i = 0; i < NUM_DEVICES; ++i) {
     ...
     if (lockout) { break; }   // breaks the whole for-loop, not just this device
   ```
   `break` exits the entire device loop on the *first* iteration once `lockout` is set, so `resolve_device_state()` is never called again for *any* device — each output's `state` field freezes at whatever it last resolved to. If a pump/valve was ON the instant the leak fired, it stays ON, driven by `task_operate` reading the stale state, indefinitely. Flipping the physical switch to OFF also has no effect during lockout, since switch position is only ever applied through this same frozen resolver. This directly contradicts design-doc §7's priority order (leak lockout > switch OFF > switch ON > AUTO).

3. **All LED indicators are dead — `led_strip_refresh()` is never called.** `components/led/led.c` only calls `led_strip_set_pixel()` and, once at init, `led_strip_clear()`. For a WS2812 strip you must call `led_strip_refresh()` to clock buffered data out over RMT. No call exists anywhere in the tree (confirmed by grep). Every LED write throughout `device.c`/`tasks.c` silently updates a buffer that's never transmitted — the panel will show nothing after boot-clear.

4. **`parse_override_json` corrupts memory and copies the wrong field.** `components/models/models.c:128`:
   ```c
   override_json_t over = { .name = "", .override = OVR_AUTO };
   ...
   strcpy(over.name, name->string);
   ```
   `override_json_t.name` is a `char*` (`models.h:142`) initialized to point at a string literal — `strcpy` into that writes to read-only memory. It also copies `name->string` (the JSON *key*, literally the text `"name"`) instead of `name->valuestring` (the actual device name sent by the server). Any incoming `pond/override` MQTT message will corrupt memory / likely crash, and even if it didn't, would never match a real device name.

5. **`publish_status` has the same class of bug.** `components/network/network.c:165`: `strcpy(devices[i].name, g_devices[i].name)` writes into `devices[i].name`, an uninitialized stack `char*` (`status_json_builder_t.name` is a pointer, not a fixed buffer — `models.h:138`). This runs on every status publish and is effectively guaranteed stack corruption. `g_devices[i].name` should be used directly in the `sniprintf` call below it; the intermediate copy is both unnecessary and broken.

6. **`threshold_min` is parsed but never used — the float-fill "wait before triggering" delay is a no-op.** `eval_float_state` (`components/float/float.c:39`):
   ```c
   case (FILL_IDLE):
     if (active) {
       float_deadline_us = time_us;      // should be time_us + minutes_us(threshold_min)
       dev->u.in.state = FILL_ARMING;
     }
   ```
   The deadline is set to *now*, so on the very next evaluation `time_us >= float_deadline_us` is already true and the valve fires immediately. The configurable debounce period from design-doc §2/§8 (and preserved intentionally from the original PondPi) currently does nothing.

7. **The MQTT "clear leak lockout" command doesn't clear the lockout.** `handle_command_msg` → `task_listen_for_task_event` → `clear_leak()` only resets the daily fill counter (`float.c:11`); it never resets the `lockout` bool or the forced `OVR_OFF` overrides that `task_check_leak` applied. Only the physical 5-second reset-button hold (`tasks.c:39-41`, which also `esp_restart()`s the device) actually clears it. The server-facing command described in design-doc §9 is effectively dead.

8. **Leak lockout state is written to NVS but never read back.** `nvs_set_lockout`/`nvs_clear_lockout` (`components/filesystem/filesystem.c`) persist a `"lockout"` key, but nothing in the tree ever calls `nvs_get_u8` for it — `lockout` always starts `false` on boot regardless of what was persisted before a reset, defeating the point of persisting it. (Partially masked by Bug #2's frozen-state behavior, but the intent — a lockout that survives a power cycle — is broken either way.)

## Secondary issues

- **GPIO interrupts configured but never wired up.** Switches and the float sensor are configured with `GPIO_INTR_HIGH_LEVEL`, and the reset button with `GPIO_INTR_ANYEDGE` (`components/io/io.c:30,104,107,114`), and interrupts are enabled via `gpio_intr_enable` — but `gpio_install_isr_service`/`gpio_isr_handler_add` are never called anywhere. Everything actually works today via 10 ms polling in `task_check_io`, so this is at minimum dead configuration; a level-triggered interrupt enabled with no ISR to service/clear it is also the kind of thing that can trip the interrupt watchdog on real silicon. Either remove it or wire it up properly.
- **`PIX_BLUE` is defined as green.** `components/led/led.c:8`: `const pix_t PIX_BLUE = {.r=0, .g=255, .b=0};` — identical to `PIX_GREEN`. The float-active indicator (`tasks.c:216` uses `PIX_BLUE`) would show green, not blue, once Bug #3 is fixed.
- **`nvs_flash_init()`'s return value is discarded** (`main/espond32.c:41`) — no erase+retry on `ESP_ERR_NVS_NO_FREE_PAGES`/`ESP_ERR_NVS_NEW_VERSION_FOUND`, a common real-world case after reflashing with a different partition layout. A corrupted NVS partition would currently, silently and permanently, prevent config persistence with no self-healing path.
- Dead/unfinished abstractions: `led_state_t` / `get_color()` (declared `static` in `led.h`, never defined or used — LED updates go through raw `pix_t` colors instead of the state machine the types imply); `io_operate()` declared in `io.h` but never defined; `nvs_load_espond_cfg` is file-internal but not marked `static`.
- `#define NUM_LEDS NUM_DEVICES + 2` (`components/config/include/config.h:14`) is unparenthesized — harmless today (only used standalone) but a latent bug if it's ever combined with an operator at a use site.
- Hardcoded placeholder WiFi/MQTT credentials and plaintext (non-TLS) MQTT in `network.c` — expected at this stage, flagged only so it isn't forgotten before anything leaves the bench.

## What's solid

- The `device_t`/`device_ops_t` polymorphic abstraction cleanly unifies pumps/valve/light/float behind one interface and matches the design doc's intent well.
- `evaluate_device_schedule()` (`date_and_time.c`) correctly handles overnight (`on_min > off_min`) wraparound schedules and per-weekday masking — genuinely correct logic.
- `parse_config_json()` is careful and defensive: `goto cleanup` on every malformed field, `strlcpy` for names, explicit range checks in `parse_hhmm`.
- The debounce implementation in `io.c` is a reasonable simple majority-count debounce.
- The MQTT topic layout (`pond/config`, `pond/status`, `pond/availability`, `pond/cmd`, plus an added `pond/override` not in the doc) matches design-doc §9 closely, with retained messages and Last-Will wired correctly.
- Config double-buffering (`g_buff_cfg` written by the MQTT task, atomically swapped into `g_espond_cfg` under `cfg_change_mutex` by `task_check_cfg`) is a sound pattern for cross-core, cross-task config updates.

## Bottom line

The plumbing and the "shape" of the system are close to the design doc and mostly well-structured. But between a near-certain boot-time crash (Bug #1), two memory-corruption bugs on incoming/outgoing MQTT traffic (Bugs #4, #5), a dead LED subsystem (Bug #3), and — most importantly — a leak lockout that doesn't actually cut power (Bug #2), this needs a focused debugging pass before it's safe to move on to phase 6 (prototype circuit) or trust it near mains and water at all.
