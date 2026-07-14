# ESPond32 Home Server

Web control panel for the ESPond32 pond controller. Runs on a Raspberry Pi 3, talks to the
device exclusively over MQTT per `SPEC.md`, and is the durable source of truth for the
device's configuration. There is no on-device UI - this is the only dashboard.

## Provenance and disclosure

The ESPond32 **firmware** (`../espond32`) and **hardware** (`../../hardware`) were written by
hand by the project owner with minimal AI assistance. `SPEC.md` is an AI-derived contract: it
was produced by auditing the firmware's actual on-wire MQTT behavior, so it reflects what the
device really does rather than the earlier `ai_design_doc.pdf` design notes.

Everything else in this directory - the entire home server application, its tests, and its
deployment configuration - was produced **almost entirely by an AI coding agent (Claude
Code)** working from `SPEC.md`, with a human reviewing and approving the plan before
implementation but not hand-writing the code. It has been deployed and works well. Treat it
accordingly: review it (especially the auth and MQTT-handling paths) before relying on it for
anything beyond a home LAN.

## Features

- **Dashboard** - live per-output state (pump1, pump2, valve1, light1) plus the float-sensor
  reading and leak-lockout status, streamed to the browser over server-sent events.
- **Overrides** - force any output ON / AUTO / OFF (honored by the device only while its
  physical switch is in AUTO).
- **Schedule editor** - edit the daily on/off times, weekday mask, and float-sensor
  parameters, validated against `SPEC.md` before publishing.
- **Commands** - clear leak lockout, request an immediate status, reboot the device, and
  resend the current config.
- **Settings** - change the admin password and the MQTT broker connection.
- **Auth** - single admin password (bcrypt-hashed), signed session cookie, and login
  attempt lockout.

The server keeps the full config document and the last version number it pushed, and
version-increments on every change, since the device only accepts strictly-newer configs and
cannot be asked to read its config back (see `SPEC.md` §5).

## Local development

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/pytest tests/           # unit tests, no broker needed
```

Run the app (needs a reachable MQTT broker - see below for a local Mosquitto, or just let it
start without one; it retries the connection in the background):

```bash
# ESPOND_COOKIE_SECURE=false is only for http:// local dev; omit it in production (Caddy/HTTPS)
ESPOND_COOKIE_SECURE=false .venv/bin/uvicorn app.main:app --reload --host 127.0.0.1 --port 8000
```

Open http://127.0.0.1:8000 - first visit prompts you to set the admin password.

### Exercising the full flow without real hardware

Run a local Mosquitto broker, then in another terminal:

```bash
.venv/bin/python tools/mock_device.py --host localhost --username espond --password password
```

`mock_device.py` plays the firmware's role (accepts config, responds to `pond/cmd`, resolves
overrides, publishes status) so you can drive the dashboard/schedule editor end-to-end. Type
`leak on`/`float on`/`reboot` at its prompt to test those states.

## Deployment (Raspberry Pi)

See `deploy/setup.sh` for a scripted bring-up (Mosquitto + this app under systemd + Caddy for
automatic HTTPS). Summary:

1. Copy this project to `/opt/espond-home-server` on the Pi.
2. `sudo bash deploy/setup.sh` - installs Mosquitto/Caddy, creates the venv, installs the
   systemd unit.
3. Point a domain/DDNS name at your home IP, edit `/etc/caddy/Caddyfile`, forward
   ports 443/80 on your router.
4. Point the ESP32 firmware's broker URI (`components/network/network.c` in the firmware
   project) at this Pi's LAN IP, with matching Mosquitto credentials.

The admin panel is reachable from the internet once port forwarding is live - that is why
setup routes everything through Caddy's automatic HTTPS rather than serving plain HTTP.

## Configuration

Broker host/port/credentials are stored in the database and editable at `/settings`; the
`ESPOND_MQTT_*` environment variables only seed the defaults on first run. Other environment
variables: `ESPOND_DB_PATH`, `ESPOND_SESSION_MAX_AGE`, `ESPOND_COOKIE_SECURE`.

## Project layout

- `app/` - FastAPI application
  - `mqtt_client.py` - MQTT wire-contract implementation (publish config/override/cmd, parse status)
  - `routes/` - dashboard, schedule, settings, auth, and the SSE `events` stream
  - `state.py` - in-memory device state fed from `pond/status` / `pond/availability`
  - `validators.py` - `SPEC.md` §3 payload validation
  - `db.py`, `models.py`, `auth.py`, `settings.py` - storage, schemas, auth, config
  - `templates/`, `static/` - server-rendered UI
- `deploy/` - systemd unit, Mosquitto config, Caddyfile, bring-up script
- `tools/mock_device.py` - firmware simulator for local testing
- `tests/` - unit tests (validators, MQTT payload shapes, auth)
- `SPEC.md` - authoritative MQTT wire contract (source of truth over `ai_design_doc.pdf`)
