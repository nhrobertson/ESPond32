# ESPond32

A custom, self-hosted pond control system. An ESP32-S3 unit drives two pumps, a
water-fill valve, and a light, monitors pond water level with a float sensor, and is
controlled from a small web dashboard running on a Raspberry Pi over the local network.
It is the spiritual successor to the earlier, much simpler PondPi.

The device has no on-board web UI by design. All remote control happens over MQTT
between the firmware and the home server; the physical unit is fully functional on its
own (schedules, switches, leak lockout) even with the network down.

## System overview

```
  3-position switches          Raspberry Pi 3
  float sensor          WiFi   +-----------------------+
        |                LAN   | Mosquitto (MQTT broker)|
   +----v-----------+   <----> | espond-home-server     |  <-- browser
   | ESP32-S3 unit  |   MQTT   | (FastAPI dashboard)    |      (LAN / HTTPS)
   | (firmware)     |          +-----------------------+
   +----+-----------+
        | 3V3 GPIO
   +----v-----------+
   | SSR / relay    | ---> mains ---> pumps, valve, light
   +----------------+
```

- The firmware resolves output state from a strict precedence chain: leak lockout >
  physical switch > network override > schedule.
- The home server is the durable source of truth for configuration and schedules; the
  device only stores the last-applied config version.
- The MQTT wire contract between the two is documented in
  `software/espond-home-server/SPEC.md`.

## Repository layout

| Path | What it is |
|---|---|
| `software/espond32/` | Device firmware (ESP-IDF, C). See its README. |
| `software/espond-home-server/` | Raspberry Pi web dashboard + MQTT bridge (Python/FastAPI). See its README. |
| `hardware/` | KiCad projects (on-board and off-board SSR variants), BOMs, datasheets. See its README. |
| `ai_design_doc.pdf` | Original "Pond Control Unit — Design & Build Plan." Guidelines only; see disclosure below. |
| `LICENSE.md` | GNU GPL v3. |

## How this project was built (disclosure)

This repository mixes hand-written and AI-generated work, and it matters which is which:

- The **firmware** (`software/espond32/`) and the **hardware** (`hardware/`) were designed
  and written by hand by the project owner, with only minimal AI assistance.
- The **home server** (`software/espond-home-server/`) was generated almost entirely by an
  AI coding agent (Claude Code) working from a hand-authored spec. It has been deployed and
  works well, but should be reviewed like any AI-written code — particularly the auth and
  MQTT-handling paths — before being exposed beyond a trusted home LAN.
- `ai_design_doc.pdf` was an early planning document. It is **not** an accurate description
  of how the device actually works today; it is kept for historical context only. Where it
  conflicts with the firmware or `SPEC.md`, the firmware and `SPEC.md` win.

## Status

The off-board SSR hardware variant is finalized and in manufacturing. The firmware runs on
ESP32-S3 (developed/validated on a DevKitC). The home server is deployed on a Raspberry Pi 3.

## Safety

This system switches mains-voltage loads. Mains wiring, fusing, and enclosure work are the
integrator's responsibility. The float-fill and leak-lockout logic are failsafes, not
substitutes for correct plumbing, a hardware overflow path, and proper electrical practice.
Do not connect this to mains and water without understanding the whole chain.

## License

GNU General Public License v3. See `LICENSE.md`.
