#!/usr/bin/env bash
# One-time bring-up for the ESPond32 home server on a Raspberry Pi 3 running Raspbian
# (Raspberry Pi OS, Bullseye or Bookworm — 32-bit armhf or 64-bit arm64). Run as root,
# from a checkout of this project already placed at /opt/espond-home-server:
#
#   sudo cp -r espond-home-server /opt/
#   sudo rm -rf /opt/espond-home-server/.venv   # if the checkout came from another machine
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

if ! grep -qi "raspbian\|raspberry pi" /etc/os-release /proc/cpuinfo 2>/dev/null; then
  echo "Warning: this doesn't look like Raspbian/Raspberry Pi OS — continuing anyway," >&2
  echo "but the Caddy repo setup and ARM wheel fallback below are tuned for it." >&2
fi

MEM_KB="$(awk '/MemTotal/ {print $2}' /proc/meminfo 2>/dev/null || echo 0)"
if [ "$MEM_KB" -gt 0 ] && [ "$MEM_KB" -lt 1500000 ]; then
  echo "Note: ${MEM_KB}kB RAM detected (Pi 3 class). If the pip install step below is slow"
  echo "or gets OOM-killed compiling a dependency from source, temporarily bump swap:"
  echo "  sudo dphys-swapfile swapoff; sudo sed -i 's/^CONF_SWAPSIZE=.*/CONF_SWAPSIZE=512/' /etc/dphys-swapfile; sudo dphys-swapfile setup; sudo dphys-swapfile swapon"
fi

echo "==> Installing packages (mosquitto, python3-venv)"
apt-get update
apt-get install -y mosquitto mosquitto-clients python3-venv python3-pip \
  debian-keyring debian-archive-keyring apt-transport-https curl gnupg

echo "==> Adding the official Caddy APT repo (not in the default Raspbian repos)"
if ! command -v caddy >/dev/null 2>&1; then
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' \
    | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' \
    -o /etc/apt/sources.list.d/caddy-stable.list
  apt-get update
fi
apt-get install -y caddy

echo "==> Creating system user 'espond' (if missing)"
id -u espond >/dev/null 2>&1 || useradd --system --home "$PROJECT_DIR" --shell /usr/sbin/nologin espond

echo "==> Python venv + dependencies"
# All pip use must happen inside a venv: on Bookworm the system Python is
# "externally managed" (PEP 668) and system-wide pip refuses to install anything.
# A venv copied over from a dev machine (`cp -r espond-home-server /opt/` brings the
# dev checkout's .venv along) has the wrong interpreter paths/architecture, and pip
# run through it falls back to the system Python and hits that PEP 668 error —
# so validate the venv actually works and belongs to this machine, else rebuild it.
VENV="$PROJECT_DIR/.venv"
VENV_PY="$VENV/bin/python"
if [ -e "$VENV" ]; then
  if ! "$VENV_PY" -c 'import sys; sys.exit(0 if sys.prefix != sys.base_prefix else 1)' >/dev/null 2>&1 \
     || ! "$VENV_PY" -m pip --version >/dev/null 2>&1; then
    echo "Existing $VENV is broken or was copied from another machine — recreating it."
    rm -rf "$VENV"
  fi
fi
python3 -m venv "$VENV"
"$VENV_PY" -m pip install --upgrade pip -q
# Raspbian's armhf wheels for some packages (pydantic-core, uvloop, etc.) aren't always on
# PyPI proper — piwheels.org is the Raspberry Pi Foundation's ARM wheel index and is usually
# already configured system-wide, but fall back to it explicitly here in case this venv
# doesn't inherit that config, rather than silently compiling from source for 20+ minutes.
if ! "$VENV_PY" -m pip install -q -r "$PROJECT_DIR/requirements.txt"; then
  echo "Default install failed, retrying with piwheels as an extra index..."
  "$VENV_PY" -m pip install -q -r "$PROJECT_DIR/requirements.txt" \
    --extra-index-url https://www.piwheels.org/simple
fi

echo "==> Mosquitto config"
cp "$PROJECT_DIR/deploy/mosquitto.conf" /etc/mosquitto/conf.d/espond.conf
if [ ! -f /etc/mosquitto/espond_passwd ]; then
  echo "Creating Mosquitto credentials for user 'espond' (placeholder password 'password' —"
  echo "matches the firmware's current placeholder; change both together later):"
  mosquitto_passwd -b -c /etc/mosquitto/espond_passwd espond password
fi
# mosquitto_passwd leaves the file root:root 0600; the broker reads it as root at startup
# but newer mosquitto builds warn (and reloads fail) unless the mosquitto user can read it.
chown root:mosquitto /etc/mosquitto/espond_passwd
chmod 640 /etc/mosquitto/espond_passwd
if ! systemctl restart mosquitto; then
  echo "mosquitto failed to start. Its own error (from the journal) is:" >&2
  journalctl -u mosquitto -n 20 --no-pager >&2 || true
  echo "Common cause: an option in /etc/mosquitto/conf.d/espond.conf duplicating one already" >&2
  echo "set in /etc/mosquitto/mosquitto.conf (mosquitto 2.x treats that as fatal)." >&2
  exit 1
fi
systemctl enable mosquitto

echo "==> systemd unit for the home server"
cp "$PROJECT_DIR/deploy/espond-home-server.service" /etc/systemd/system/espond-home-server.service
chown -R espond:espond "$PROJECT_DIR"
systemctl daemon-reload
systemctl enable --now espond-home-server

echo "==> Caddy reverse proxy"
echo "Installing deploy/Caddyfile (LAN-only HTTP by default; see its comments to switch"
echo "to HTTPS with a real domain for remote access):"
cp "$PROJECT_DIR/deploy/Caddyfile" /etc/caddy/Caddyfile
systemctl restart caddy
systemctl enable caddy

echo "==> Verifying services actually came up"
FAILED=0
for svc in mosquitto espond-home-server caddy; do
  if systemctl is-active --quiet "$svc"; then
    echo "  $svc: active"
  else
    echo "  $svc: NOT active — check 'journalctl -u $svc -e'" >&2
    FAILED=1
  fi
done
if [ "$FAILED" -ne 0 ]; then
  echo "One or more services failed to start — see above before assuming this worked." >&2
fi

cat <<'EOF'

==> Done.

Next steps:
  1. Visit http://<this-pi's-LAN-IP>/ from inside your network — first visit prompts you
     to set the admin password. (Want remote access instead? See the comments in
     /etc/caddy/Caddyfile for the HTTPS/domain mode, then `systemctl restart caddy`.)
  2. Point the ESP32 firmware's broker URI at this Pi's LAN IP (network.c) if not already
     done, and make sure its MQTT username/password match /etc/mosquitto/espond_passwd.
  3. Change the Mosquitto and admin passwords from their placeholders (Settings page +
     `mosquitto_passwd /etc/mosquitto/espond_passwd espond`, then `systemctl restart mosquitto`).
EOF
