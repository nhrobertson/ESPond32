# ESPond32 — Implementation Status Evaluation

_Evaluation of firmware vs. the design doc (`ai_design_doc.pdf`, "Pond Control Unit — Design & Build Plan"). Date: 2026-07-05. No code was modified to produce this._

## Overall status

The project is a **partially-built firmware skeleton** — roughly **Phase 3 of a 12-phase plan**. The architecture (component split, device abstraction, task model, config/NVS/MQTT plumbing) is scaffolded and mostly matches the design doc. But **the actual control logic — the reason the device exists — is not implemented.** The three things that make it a pond controller (scheduling, float-fill, leak guard) are empty stubs.

It would compile and boot, drive LEDs on init, react to physical switches, and receive config over MQTT — but it would **not** run a schedule, fill on the float, or protect against leaks.

**Bottom line: firmware ~35% complete, whole system ~15%.**

## Firmware, phase by phase (design doc §14)

| Phase | Status | Notes |
|---|---|---|
| 0 — Toolchain | ✅ Done | ESP-IDF project, S3 target, managed deps (cjson/mqtt/led_strip) resolved. |
| 1 — Bare I/O | 🟡 ~70% | GPIO configure/read/set works; switch (ON/OFF/AUTO via 2 pins) and float read work; WS2812 LED driver works. **Missing:** software debounce (design requires it), internal pull config, and the noisy per-pin `ESP_LOGI` on every 50 ms poll. |
| 2 — Timekeeping | 🔴 ~15% | SNTP is *started* in `network.c` but **no timezone is set** (breaks DST/local schedule comparisons — the exact bug this redesign was meant to fix). `date_and_time` component is a literal empty stub (`func(){}`) and isn't even wired into the build. No DS3231/RTC. No "get current local time" helper the scheduler needs. |
| 3 — Scheduler + Output manager | 🔴 ~20% | `task_operate` has the switch-precedence skeleton, but **`SW_AUTO` is three empty comments** ("get time / compare to cfg / act"). No schedule evaluation, no unified precedence resolver, no leak-lockout tier (design §7 defines 4 priority levels — only ON/OFF are wired). Config load-from-NVS and JSON parse exist. |
| 4 — Float-fill + leak | 🔴 ~5% | `DEV_FLOAT` case in `task_operate` is comments only. No threshold/overflow timers, no daily fill counter, no latched leak guard. This is core PondPi behavior — entirely absent. |
| 5 — Connectivity | 🟡 ~60% | Best-developed area: WiFi STA, MQTT with Last-Will, version-gated config → NVS persistence, backoff/reconnect. **Missing:** `pond/status` is never published (device is write-blind to the server); `pond/cmd` handler is a `//TODO` (so "clear leak lockout" can't work); credentials are hardcoded placeholders (`"ssid"`, `"IP_ADDR"`); no provisioning. |

## Correctness bugs that block function (found, not fixed)

These aren't polish — they'd cause wrong behavior once logic is added:

- **`config.c:143`** — `parse_hhmm(off, &dst->off_hour, &dst->off_hour)` passes `off_hour` twice; **`off_min` is never parsed.** Every schedule's off-minute is garbage.
- **`config.c:169`** — `max_fills_per_day = ovf->valueint` assigns the *overflow* value into the leak limit. Leak guard would be misconfigured.
- **`device.c:982`** — `get_device` uses `name == name` (pointer identity), not `strcmp`. Lookup by string fails.
- **`config.h`** — LED pixel indices are inconsistent/out of range: `NUM_LEDS=4` (valid 0–3), but `LIGHT1_LED_PIX=4` and `SYS_LED_PIX=5` address past the strip; float and light pixels collide with the design's "4 LEDs" budget.
- **`led.c:1084`** — `PIX_BLUE` is defined with green values (`g=255`).
- **LED never reflects resolved state** — LEDs are set only during `devices_init`, then cleared. The design's "panel never lies" requirement (LED = resolved output state) is unimplemented.
- Duplicate `config.h` files sharing the `CONFIG_H` guard (the `components/config/config.h` stray with `PUMP_AMOUNT` is dead but confusing).

## Non-firmware (the rest of "the system")

The design doc scopes a full product; firmware is only Phases 0–5. Beyond it, **nothing exists in this repo**:

- **Phase 11 — Home server** (Mosquitto + admin panel that edits/publishes retained config): not started. Without it there's no way to actually set schedules except the hardcoded default.
- **Phases 6–10 — Hardware**: the `hardware/` dir exists but the PCB (isolated AC-DC, ×3 triac SSR stages, isolation barrier, KiCad schematic/layout, fab, bring-up) is a large, safety-critical effort switching mains near water.
- **Phase 12 — Enclosure**: IP-rated box, GFCI, conformal coat, soak test.

## Rough remaining effort

**To reach a bench-working autonomous controller (Phases 2–5 firmware):**

- Fix the parse/lookup/LED bugs above — ~half a day.
- Real timekeeping: timezone, local-time helper, (optional RTC) — ~1 day.
- **Scheduler + precedence resolver** (the `SW_AUTO` path, 4-tier resolution, LED = resolved state) — ~2–3 days.
- **Float-fill + leak guard** (timers, fill counting, latch, `pond/cmd` reset) — ~2 days.
- Finish connectivity: publish `pond/status`, real config/provisioning, debounce — ~1–2 days.

→ **~1.5–2 focused weeks** to a firmware that actually controls pumps on the bench.

**To a deployed product:** add the home server (~3–5 days) and the entire mains-switching PCB + enclosure track (weeks, plus a mandatory qualified safety review — the doc flags this explicitly).

## Summary

The plumbing is ~40% there and reasonably structured; the *decision-making core* (scheduling, float logic, leak protection, status reporting) is essentially unwritten, and the hardware/server tiers are greenfield.
