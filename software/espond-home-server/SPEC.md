# ESPond32 Home Server — Network Interaction Spec

Version 0.1 — derived directly from the current ESPond32 firmware (`../espond32`), not from
`ai_design_doc.pdf`. The firmware's actual on-wire behavior is now the source of truth for this
contract; the design doc's original §8/§9 examples (weekday-string `days`, `"id"`-keyed outputs,
a `"float"` config key) are superseded and should be ignored where they conflict with this
document.

This spec covers only the **network interaction contract** between the home server and one
ESPond32 unit. It does not cover the server's own UI, storage schema, or scheduling algorithms —
those are implementation details of the home server itself.

## 1. Scope & deployment target

- The home server is a service intended to run on a **Raspberry Pi 3**, reached over the same
  **WiFi LAN** as the ESPond32 device. No wired/Ethernet assumption.
- All device interaction happens over **MQTT**. There is no HTTP/REST/webpage on the device
  itself (the design doc explicitly rejected an on-device web UI in favor of MQTT + a home
  server).
- The firmware connects to a single hardcoded broker URI (`network.c`: `mqtt_cfg.broker.address.uri`,
  currently the placeholder `"mqtt://IP_ADDR"`). **Something has to run a broker** (e.g.
  Mosquitto) reachable at that address. The simplest deployment is to run the broker on the same
  Raspberry Pi as the home server and point the firmware's placeholder URI at the Pi's LAN IP —
  but this is a deployment decision, not something the firmware enforces.
- MQTT is currently **plaintext, unauthenticated at the broker level** beyond a username/password
  pair (`network.c`: client id `"espondcu"`, username `"espond"`, password `"password"` — all
  placeholders). No TLS. The home server should use the same broker credentials scheme; treat
  this as "expected to be hardened later," not something to over-engineer around today.
- **Single-device assumption.** Every topic below is a fixed global string (`pond/...`), not
  namespaced by device ID. The firmware does not support multiple ESPond32 units sharing one
  broker without a firmware change (topic namespacing). Build the server for exactly one device
  for now.

## 2. Topics

| Topic | Direction | QoS | Retained | Payload |
|---|---|---|---|---|
| `pond/config` | server → device | 1 | no | JSON, §3.1 |
| `pond/cmd` | server → device | 1 | no | raw string, §3.2 |
| `pond/override` | server → device | 1 | no | JSON, §3.3 |
| `pond/status` | device → server | 1 | **yes** | JSON, §3.4 |
| `pond/availability` | device → server | 1 | **yes** | `"online"` / `"offline"` (LWT), §3.5 |

The device subscribes to the first three on every MQTT connect and does not persist an MQTT
session across reconnects — the server should assume a fresh subscribe happens each time the
device reconnects, and should be prepared to re-deliver `pond/config` if it suspects the device
missed it (see §6).

## 3. Message schemas

### 3.1 `pond/config` (server → device)

```json
{
  "version": 4,
  "outputs": [
    { "name": "pump1",  "on": "14:00", "off": "14:05", "days": [0,1,2,3,4,5,6] },
    { "name": "pump2",  "on": "14:00", "off": "14:05", "days": [0,1,2,3,4,5,6] },
    { "name": "valve1", "on": null,    "off": null,    "days": [] },
    { "name": "light1", "on": "18:00", "off": "23:00", "days": [5,6] }
  ],
  "float_sens": { "threshold_min": 10, "overflow_min": 30, "max_fills_per_day": 3 }
}
```

Field rules (enforced by `parse_config_json` — any violation causes the **entire message to be
rejected**, with no partial apply and no error reported back to the server over MQTT):

- `version`: integer. The device only accepts a config if `version` is **strictly greater** than
  its currently stored version (persisted in NVS, survives reboot). The server must track the
  last version it successfully pushed and always increment before pushing a change. There is no
  way to query the device's currently-applied version except via `pond/status` (see §3.4) —
  the server is the source of truth for "what config is on the device," not the other way around
  (see §6, known limitation).
- `outputs`: array, up to 4 entries. Each entry is matched **by `"name"`, not position or an
  `"id"` field** — must be exactly one of `"pump1"`, `"pump2"`, `"valve1"`, `"light1"`. Unknown
  names are rejected (whole message fails). If an expected device name is *missing* from the
  array, that device silently keeps a fully-off, never-scheduled state — the firmware treats an
  absent entry as "no schedule" rather than erroring, so a partial `outputs` array won't crash
  anything, but it also won't preserve whatever schedule was previously set for the omitted
  device. Always send all 4 entries.
- `on` / `off`: `"HH:MM"` 24-hour strings, or both `null` together to mean "no schedule" (device
  only reachable via `pond/override` or the physical switch, never turns on by schedule).
  Sending only one of the two as `null` is invalid and rejects the message.
- `days`: array of integers **0–6** (Sunday=0 through Saturday=6, matching C `tm_wday` — **not**
  weekday abbreviation strings). Empty array is valid (means "no day is scheduled," equivalent in
  effect to a null on/off). Any value outside 0–6 rejects the whole message.
- `float_sens.threshold_min`: minutes the float sensor must read "wet" continuously before the
  device treats a fill as intentional and opens `valve1` (debounces brief splashes/waves).
- `float_sens.overflow_min`: minutes after the sensor reads "dry" again before the fill is
  considered fully finished (closes out the fill cycle / daily fill count).
- `float_sens.max_fills_per_day`: if the number of completed fill cycles in the current calendar
  day exceeds this, the device engages **leak lockout** (see §4) — forces `pump1`, `pump2`, and
  `valve1` off regardless of schedule or override, until explicitly cleared.

There is no partial-update semantics — every push must include the full desired state for all
4 outputs and all 3 float-sensor parameters. The server should keep this full config resident and
always send a complete document.

### 3.2 `pond/cmd` (server → device)

Plain string payload, **not JSON**. Exactly one of:

| Payload | Effect |
|---|---|
| `clear_leak_lockout` | Clears leak lockout: resets `lockout` state (RAM + NVS) and the daily fill counter. Devices return to normal schedule/override/switch resolution on the next evaluate cycle (≤250ms). |
| `reboot` | Immediate `esp_restart()`. No graceful shutdown, no ack. |
| `request_status` | Forces an immediate `pond/status` publish (see §3.4), independent of whether anything changed. |

Any other string is silently ignored — no error is published back.

### 3.3 `pond/override` (server → device)

```json
{ "name": "pump1", "override": 0 }
```

- `name`: one of `"pump1"`, `"pump2"`, `"valve1"`, `"light1"`. `"float1"` is rejected — the float
  sensor is input-only and cannot be overridden.
- `override`: integer, one of:

  | Value | Meaning |
  |---|---|
  | `0` | Force **ON** |
  | `1` | Release to **AUTO** (schedule decides) |
  | `2` | Force **OFF** |

  Any other integer is accepted by the parser but silently treated as `1` (AUTO) — there is no
  rejection for an out-of-range override value, unlike the other message types.

An override only takes effect while that device's **physical 3-position switch is in the AUTO
position** — see precedence in §4. There is no way to know from the network side alone whether an
override is currently "winning" versus being shadowed by the physical switch; `pond/status`
reports the resulting `state` (on/off) but not *why* it's in that state.

### 3.4 `pond/status` (device → server, retained)

```json
{
  "version": 4,
  "leak_lockout": false,
  "outputs": [
    { "name": "pump1",  "state": true },
    { "name": "pump2",  "state": false },
    { "name": "valve1", "state": false },
    { "name": "light1", "state": true },
    { "name": "float1", "state": false }
  ]
}
```

- Despite the key being `"outputs"`, this array always has **5** entries, not 4 — `"float1"` is
  included and its `"state"` means "sensor currently reads wet" (an input reading), not an output
  being driven. The server must not treat all 5 entries uniformly if it plans to expose
  actuator-only controls.
- `"state"` is the *actual* on/off/active state after all precedence resolution (leak lockout,
  switch, override, schedule) — it is the only place the server can observe the real-world
  outcome of a config or override push.
- Published only when something changes (a device's on/off state flips, leak lockout engages) or
  when explicitly requested via `pond/cmd` → `request_status`. **It is not published on a
  periodic heartbeat**, and it is not automatically (re-)published just because the server
  reconnects to the broker — only the *retained* copy from the last publish is delivered on
  subscribe. If the server needs current truth right after startup/reconnect, send
  `request_status` rather than trusting the retained value blindly (the device could have
  rebooted, or its state could have changed while the broker was unreachable to the retained
  message... though MQTT retained messages do survive broker restarts if persistence is enabled -
  the risk is specifically a *device*-side change with no observer connected to receive it live).

### 3.5 `pond/availability` (device → server, retained, Last Will)

Payload is the literal string `"online"` (published once, on every successful MQTT connect) or
`"offline"` (published automatically **by the broker**, not the device, as the MQTT Last-Will
message if the device disconnects without a clean shutdown — e.g. power loss, WiFi drop, crash).
The server should treat `"offline"` (or the topic simply never having been set) as "device
presence unknown," and should not assume the device's last known `pond/status` is still accurate
once availability drops to offline.

## 4. Precedence model the server must respect

For `pump1`, `pump2`, `valve1` (light1 is the same minus the lockout tier):

1. **Leak lockout** (if engaged) — forces OFF. Beats everything, including a manual override.
   Only `pond/cmd` → `clear_leak_lockout`, or a 5-second physical reset-button hold on the
   device, can lift it.
2. **Physical 3-position switch** — if held to the ON or OFF side (not centered/AUTO), that wins
   over any `pond/override` sent by the server. The server has no way to detect switch position
   directly; it can only infer "my override doesn't seem to be taking effect" from `pond/status`
   not matching what was requested.
3. **`pond/override`** (server command) — only consulted when the switch is centered/AUTO.
4. **Schedule** (`pond/config` on/off/days) — only consulted when both switch and override are
   AUTO.

Implication for the server: don't build a UI that assumes a command always "wins" — always treat
`pond/status` as the arbiter of truth, and design any "why didn't my change apply" UX around the
possibility of the physical switch overriding it silently.

## 5. Known limitations / open items for the server design

- **No config read-back.** The device cannot be asked "what config do you currently have
  applied?" beyond the bare `version` number in `pond/status`. The server must be the durable
  store of the full config document and must not assume it can reconstruct it from the device.
- **No per-command acknowledgment.** Every `pond/cmd`/`pond/override`/`pond/config` message is
  fire-and-forget; success or failure is only observable indirectly through a later
  `pond/status` change (or its absence).
- **No malformed-message feedback.** A rejected `pond/config` (bad version, bad name, out-of-range
  day, etc.) fails silently on the device side — no error topic, no NACK. The server should
  validate against §3.1's rules itself before publishing, since the device won't tell it what
  went wrong.
- **Single device, unauthenticated broker, plaintext transport** — acceptable for a bench/home
  LAN today; flag before this is ever exposed beyond a trusted local network.
- **Broker location is undecided** — this spec assumes *a* broker exists at whatever URI the
  firmware is flashed with; deciding whether that's Mosquitto on the same Pi as the server or a
  separate box is a deployment decision for whoever sets up the Pi, not a firmware or server
  constraint.

## 6. Suggested server responsibilities (non-normative)

Not part of the wire contract, but worth the server implementation keeping in mind:

- Persist the full last-pushed `espond_cfg_t`-equivalent document and the last version number
  used, so a server restart doesn't lose track of what's supposedly on the device.
- On startup / broker reconnect, subscribe to `pond/status` and `pond/availability`, then send
  `request_status` once availability shows `"online"`, rather than trusting a stale retained
  status.
- Treat `pond/status`'s 5-entry array as the single source of "what is actually happening right
  now" for any dashboard, rather than mirroring back whatever config/override was last sent.
