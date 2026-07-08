import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

DB_PATH = Path(os.environ.get("ESPOND_DB_PATH", BASE_DIR / "espond.db"))

SESSION_COOKIE_NAME = "espond_session"
SESSION_MAX_AGE_SECONDS = int(os.environ.get("ESPOND_SESSION_MAX_AGE", 12 * 3600))
# Caddy is the only thing that should ever terminate plaintext HTTP in production, so the
# cookie is Secure by default. Set ESPOND_COOKIE_SECURE=false only for local dev over http://.
SESSION_COOKIE_SECURE = os.environ.get("ESPOND_COOKIE_SECURE", "true").lower() != "false"

# Default broker connection used only to seed app_config on first run; the
# effective values always come from the database (editable in /settings).
DEFAULT_MQTT_HOST = os.environ.get("ESPOND_MQTT_HOST", "localhost")
DEFAULT_MQTT_PORT = int(os.environ.get("ESPOND_MQTT_PORT", 1883))
DEFAULT_MQTT_USERNAME = os.environ.get("ESPOND_MQTT_USERNAME", "espond")
DEFAULT_MQTT_PASSWORD = os.environ.get("ESPOND_MQTT_PASSWORD", "password")

MQTT_CLIENT_ID = "espond-home-server"

TOPIC_CONFIG = "pond/config"
TOPIC_CMD = "pond/cmd"
TOPIC_OVERRIDE = "pond/override"
TOPIC_STATUS = "pond/status"
TOPIC_AVAILABILITY = "pond/availability"

OUTPUT_NAMES = ["pump1", "pump2", "valve1", "light1"]

LOGIN_MAX_ATTEMPTS = 5
LOGIN_LOCKOUT_SECONDS = 60
