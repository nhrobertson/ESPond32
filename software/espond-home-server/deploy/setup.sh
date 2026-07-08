#!/usr/bin/env bash
# One-time bring-up for the ESPond32 home server on a Raspberry Pi (Raspberry Pi OS /
# Debian-based). Run as root, from a checkout of this project already placed at
# /opt/espond-home-server:
#
#   sudo cp -r espond-home-server /opt/
#   cd /opt/espond-home-server
#   sudo bash deploy/setup.sh
#
# It is safe to re-run; each step is idempotent-ish (apt/systemctl no-ops if already done).
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Run as root (sudo bash deploy/setup.sh)" >&2
  exit 1
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ "$PROJECT_DIR" != "/opt/espond-home-server" ]; then
  echo "Warning: expected this checkout at /opt/espond-home-server, found $PROJECT_DIR." >&2
  echo "The systemd unit hardcodes /opt/espond-home-server — move it there or edit the unit." >&2
fi

echo "==> Installing packages (mosquitto, caddy, python3-venv)"
apt-get update
apt-get install -y mosquitto mosquitto-clients python3-venv python3-pip caddy

echo "==> Creating system user 'espond' (if missing)"
id -u espond >/dev/null 2>&1 || useradd --system --home "$PROJECT_DIR" --shell /usr/sbin/nologin espond

echo "==> Python venv + dependencies"
python3 -m venv "$PROJECT_DIR/.venv"
"$PROJECT_DIR/.venv/bin/pip" install --upgrade pip -q
"$PROJECT_DIR/.venv/bin/pip" install -q -r "$PROJECT_DIR/requirements.txt"

echo "==> Mosquitto config"
cp "$PROJECT_DIR/deploy/mosquitto.conf" /etc/mosquitto/conf.d/espond.conf
if [ ! -f /etc/mosquitto/espond_passwd ]; then
  echo "Creating Mosquitto credentials for user 'espond' (placeholder password 'password' —"
  echo "matches the firmware's current placeholder; change both together later):"
  mosquitto_passwd -b -c /etc/mosquitto/espond_passwd espond password
fi
systemctl restart mosquitto
systemctl enable mosquitto

echo "==> systemd unit for the home server"
cp "$PROJECT_DIR/deploy/espond-home-server.service" /etc/systemd/system/espond-home-server.service
chown -R espond:espond "$PROJECT_DIR"
systemctl daemon-reload
systemctl enable --now espond-home-server

echo "==> Caddy reverse proxy"
echo "Edit deploy/Caddyfile with your real domain before this step matters (it currently"
echo "has a placeholder). Copying it to /etc/caddy/Caddyfile now regardless:"
cp "$PROJECT_DIR/deploy/Caddyfile" /etc/caddy/Caddyfile
systemctl restart caddy
systemctl enable caddy

cat <<'EOF'

==> Done.

Next steps:
  1. Edit /etc/caddy/Caddyfile with your real DDNS/domain, then `systemctl restart caddy`.
  2. Forward ports 443 (and 80) on your router to this Pi.
  3. Visit https://<your-domain>/ once DNS + port forwarding are live — first visit prompts
     you to set the admin password.
  4. Point the ESP32 firmware's broker URI at this Pi's LAN IP (network.c) if not already
     done, and make sure its MQTT username/password match /etc/mosquitto/espond_passwd.
  5. Change the Mosquitto and admin passwords from their placeholders (Settings page +
     `mosquitto_passwd /etc/mosquitto/espond_passwd espond`, then `systemctl restart mosquitto`).
EOF
