# ESPond32 Firmware

Firmware for the ESPond32 pond control unit. Targets the **ESP32-S3-WROOM-1 (N8)** and
builds as a standard ESP-IDF project. It drives two pumps, a water-fill valve, and a light
through external solid-state relays, monitors pond level with a float sensor, and is
controlled remotely over MQTT by the home server (`../espond-home-server`).

Written by hand in C with only minimal AI assistance used for minor debugging and heavy review. Committed under the GNU GPL v3.

## What it does

- Runs each output (pump1, pump2, valve1, light1) on a weekday-aware daily schedule.
- Resolves the real output state through a fixed precedence chain (highest wins):
  1. **Leak lockout** - forces pumps and valve off; survives reboot (persisted in NVS).
  2. **Physical 3-position switch** - ON / AUTO / OFF per channel; a thrown switch beats
     any network override.
  3. **Network override** - `pond/override` from the server, honored only while the switch
     is in AUTO.
  4. **Schedule** - the configured on/off/days, honored only when switch and override are
     both AUTO.
- Automatically fills the pond via `valve1` when the float sensor reads dry, with a
  configurable arming debounce and overflow timeout, and a daily fill-count cap.
- Engages leak lockout when the pond fills too many times in a day or a single fill runs
  too long (stuck-float failsafe), and requires an explicit clear (server command or a 5s
  physical reset-button hold) to recover.
- Reports state to the server over MQTT and drives a WS2812 status LED chain.

Precedence and float behavior are the safety-critical parts of the design and are the
reason there is no on-device web UI: the unit keeps running correctly with the network down.

## Architecture

Devices are unified behind a small polymorphic interface (`device_t` / `device_ops_t`) so
pumps, valve, light, and the float sensor share one code path. Work is split across
FreeRTOS tasks pinned to the two cores:

| Task | Core | Cadence | Role |
|---|---|---|---|
| `task_check_io` | 1 | 10 ms | Debounce switches + float sensor |
| `task_evaluate_cfg` | 1 | 250 ms | Resolve schedule/override/lockout precedence |
| `task_operate` | 1 | 50 ms | Apply resolved state to SSR GPIOs |
| `task_check_leak` | 1 | 10 ms | Float-fill state machine + leak detection |
| `task_check_for_reset` | 1 | 50 ms | Reset-button hold detection |
| `task_listen_for_task_event` | 1 | event | Handle clear-leak / command events |
| `task_set_sys_light` | 1 | 1 s | System status LED |
| `task_check_cfg` | 0 | event | Apply new config (double-buffered swap) |
| `task_net_manager` | 0 | event | WiFi + MQTT lifecycle |

Config is double-buffered: the MQTT task writes into a staging buffer, and `task_check_cfg`
atomically swaps it into the live config under a mutex. Config is version-gated and
persisted to NVS, so it survives reboots and only newer versions are applied.

### Components

| Component | Responsibility |
|---|---|
| `config/` | Pin map, device counts, timing constants, debug GPIOs |
| `models/` | Shared types, enums, JSON builders, RTOS handles |
| `device/` | Device abstraction + state resolution |
| `io/` | GPIO configure/set, software debounce, reset button |
| `float/` | Float-fill state machine and leak logic |
| `led/` | WS2812 status LED driver |
| `date_and_time/` | SNTP time + schedule evaluation (handles overnight wrap) |
| `filesystem/` | NVS persistence (config version, leak lockout) |
| `network/` | WiFi STA, MQTT client, config parse, status publish |
| `tasks/` | The FreeRTOS task bodies listed above |

## Pin map

Defined in `components/config/include/config.h`. Each output uses two switch inputs plus one
SSR output; the float sensor is a single input; one GPIO drives the WS2812 chain.

| Function | GPIO |
|---|---|
| Pump1 SSR / SW-A / SW-B | 4 / 5 / 6 |
| Pump2 SSR / SW-A / SW-B | 7 / 15 / 16 |
| Valve1 SSR / SW-A / SW-B | 17 / 18 / 8 |
| Light1 SSR / SW-A / SW-B | 11 / 12 / 13 |
| Float sensor | 9 |
| WS2812 data | 10 |
| Reset / leak-clear button | 14 |

GPIO35-38 are used as task-timing debug test points (`DEBUG_GPIO_*`), which is why the
module **must** be the N8 (no-PSRAM) variant - the PSRAM variants use those pins internally.
The switch inputs are configured pull-down and the reset button pull-up, so no external pull
resistors are needed.

## MQTT interface

The device connects to one broker and speaks the contract in
`../espond-home-server/SPEC.md`. Summary of topics:

| Topic | Direction | Purpose |
|---|---|---|
| `pond/config` | server -> device | Full config (version-gated, all 4 outputs + float params) |
| `pond/override` | server -> device | Force a single output ON/AUTO/OFF |
| `pond/cmd` | server -> device | `clear_leak_lockout` / `reboot` / `request_status` |
| `pond/status` | device -> server | Retained state of all 4 outputs + float reading |
| `pond/availability` | device -> server | Retained `online`/`offline` (MQTT Last-Will) |

Single-device assumption: topics are fixed global strings, not namespaced per unit.

## Build and flash

Requires ESP-IDF (v4.1+; developed on a recent v5.x). Managed components (`led_strip`,
`cjson`, `mqtt`) resolve automatically via the component manager.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Configuration you must change before flashing (disclosure)

`components/network/network.c` currently contains **hardcoded placeholder credentials** that
need to be replaced for your environment before deployment:

- WiFi SSID and password.
- MQTT broker URI (point it at your Raspberry Pi / Mosquitto LAN IP).
- MQTT client id / username / password (must match the broker's credentials).

MQTT is plaintext (no TLS) and the broker auth is a single username/password. This is
acceptable for a trusted home LAN but should be hardened before the device is reachable from
anywhere less trusted. Do not commit real WiFi/broker secrets.

## Provenance and disclosure

- This firmware was written by hand; AI assistance was minimal.
- `IMPLEMENTATION_STATUS.md` in this directory is a **historical AI-generated audit** from an
  earlier revision. It documented bugs that were subsequently addressed and does not describe
  the current state of the code - read it as a snapshot, not as current documentation.
- `SPEC.md` (in the home-server directory) was derived by auditing this firmware's actual
  on-wire behavior, so it reflects what the device really does. The older `ai_design_doc.pdf`
  is guidelines only and is superseded wherever it conflicts.

## License

GNU General Public License v3. See `../../LICENSE.md`.
