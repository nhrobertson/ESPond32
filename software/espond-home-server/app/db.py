import json
import sqlite3
import threading
from contextlib import contextmanager

from app import settings

_local = threading.local()

_DEFAULT_OUTPUTS = [
    {"name": name, "on": None, "off": None, "days": []}
    for name in settings.OUTPUT_NAMES
]
_DEFAULT_FLOAT_SENS = {
    "threshold_min": 10,
    "overflow_min": 30,
    "max_fills_per_day": 3,
}


def _connect() -> sqlite3.Connection:
    conn = sqlite3.connect(settings.DB_PATH, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn


def get_connection() -> sqlite3.Connection:
    """Thread-local SQLite connection (sqlite3 connections aren't thread-safe to share)."""
    if not hasattr(_local, "conn"):
        _local.conn = _connect()
    return _local.conn


@contextmanager
def cursor():
    conn = get_connection()
    cur = conn.cursor()
    try:
        yield cur
        conn.commit()
    finally:
        cur.close()


def init_db() -> None:
    with cursor() as cur:
        cur.execute(
            """
            CREATE TABLE IF NOT EXISTS app_config (
                key TEXT PRIMARY KEY,
                value TEXT
            )
            """
        )
        cur.execute(
            """
            CREATE TABLE IF NOT EXISTS pond_config (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                version INTEGER NOT NULL,
                outputs TEXT NOT NULL,
                float_sens TEXT NOT NULL
            )
            """
        )
        cur.execute("SELECT 1 FROM pond_config WHERE id = 1")
        if cur.fetchone() is None:
            cur.execute(
                "INSERT INTO pond_config (id, version, outputs, float_sens) VALUES (1, ?, ?, ?)",
                (0, json.dumps(_DEFAULT_OUTPUTS), json.dumps(_DEFAULT_FLOAT_SENS)),
            )
    _seed_default(settings.DEFAULT_MQTT_HOST, "mqtt_host")
    _seed_default(str(settings.DEFAULT_MQTT_PORT), "mqtt_port")
    _seed_default(settings.DEFAULT_MQTT_USERNAME, "mqtt_username")
    _seed_default(settings.DEFAULT_MQTT_PASSWORD, "mqtt_password")


def _seed_default(value: str, key: str) -> None:
    if get_app_config(key) is None:
        set_app_config(key, value)


def get_app_config(key: str) -> str | None:
    with cursor() as cur:
        cur.execute("SELECT value FROM app_config WHERE key = ?", (key,))
        row = cur.fetchone()
        return row["value"] if row else None


def set_app_config(key: str, value: str) -> None:
    with cursor() as cur:
        cur.execute(
            "INSERT INTO app_config (key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value",
            (key, value),
        )


def get_pond_config() -> dict:
    with cursor() as cur:
        cur.execute("SELECT version, outputs, float_sens FROM pond_config WHERE id = 1")
        row = cur.fetchone()
        return {
            "version": row["version"],
            "outputs": json.loads(row["outputs"]),
            "float_sens": json.loads(row["float_sens"]),
        }


def save_pond_config(version: int, outputs: list, float_sens: dict) -> None:
    with cursor() as cur:
        cur.execute(
            "UPDATE pond_config SET version = ?, outputs = ?, float_sens = ? WHERE id = 1",
            (version, json.dumps(outputs), json.dumps(float_sens)),
        )
