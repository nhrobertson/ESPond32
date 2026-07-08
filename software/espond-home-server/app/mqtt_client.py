"""paho-mqtt wrapper implementing the SPEC.md wire contract.

Load-bearing behavior (SPEC.md §2/§5/§6): pond/config is NOT retained, and the
device does not persist its MQTT session across reconnects. So whenever
pond/availability transitions to "online", we must proactively re-publish the
current desired config and request a fresh status — the broker's retained
mechanism alone does not cover a missed config the way it does for status.
"""
import json
import logging

import paho.mqtt.client as mqtt

from app import db, settings, validators
from app.state import device_state

logger = logging.getLogger("espond.mqtt")


class MqttClient:
    def __init__(self):
        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=settings.MQTT_CLIENT_ID,
        )
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._client.on_disconnect = self._on_disconnect

    # -- lifecycle -----------------------------------------------------
    def connect(self) -> None:
        host = db.get_app_config("mqtt_host")
        port = int(db.get_app_config("mqtt_port"))
        username = db.get_app_config("mqtt_username")
        password = db.get_app_config("mqtt_password")
        self._client.username_pw_set(username, password)
        # connect_async + loop_start so a broker that's briefly unreachable (e.g. still
        # starting up, or a temporary LAN blip) doesn't crash app startup; paho retries
        # in the background instead of raising here.
        self._client.connect_async(host, port, keepalive=30)
        self._client.loop_start()

    def disconnect(self) -> None:
        self._client.loop_stop()
        self._client.disconnect()

    # -- callbacks -------------------------------------------------------
    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        logger.info("mqtt connected: %s", reason_code)
        client.subscribe(settings.TOPIC_STATUS, qos=1)
        client.subscribe(settings.TOPIC_AVAILABILITY, qos=1)

    def _on_disconnect(self, client, userdata, flags, reason_code=None, properties=None):
        logger.warning("mqtt disconnected: %s", reason_code)
        device_state.set_availability("unknown")

    def _on_message(self, client, userdata, msg):
        if msg.topic == settings.TOPIC_STATUS:
            self._handle_status(msg.payload)
        elif msg.topic == settings.TOPIC_AVAILABILITY:
            self._handle_availability(msg.payload)

    def _handle_status(self, payload: bytes) -> None:
        try:
            status = json.loads(payload.decode())
        except (ValueError, UnicodeDecodeError):
            logger.warning("dropped malformed pond/status payload")
            return
        device_state.set_status(status)

    def _handle_availability(self, payload: bytes) -> None:
        value = payload.decode(errors="replace")
        became_online = device_state.set_availability(value)
        if became_online:
            self.resend_config_and_request_status()

    def resend_config_and_request_status(self) -> None:
        cfg = db.get_pond_config()
        logger.info("resending config v%s and requesting status", cfg["version"])
        self.publish_config(cfg["version"], cfg["outputs"], cfg["float_sens"])
        self.publish_command("request_status")

    # -- publishing ------------------------------------------------------
    def publish_config(self, version: int, outputs: list, float_sens: dict) -> None:
        payload = json.dumps({"version": version, "outputs": outputs, "float_sens": float_sens})
        self._client.publish(settings.TOPIC_CONFIG, payload, qos=1, retain=False)

    def publish_command(self, cmd: str) -> None:
        validators.validate_command(cmd)
        self._client.publish(settings.TOPIC_CMD, cmd, qos=1, retain=False)

    def publish_override(self, name: str, override: int) -> None:
        validators.validate_override({"name": name, "override": override})
        payload = json.dumps({"name": name, "override": override})
        self._client.publish(settings.TOPIC_OVERRIDE, payload, qos=1, retain=False)

    def push_new_config(self, outputs: list, float_sens: dict) -> int:
        """Validate, bump the stored version, persist, and publish. Returns the new version."""
        validators.validate_config(outputs, float_sens)
        current = db.get_pond_config()
        new_version = current["version"] + 1
        db.save_pond_config(new_version, outputs, float_sens)
        self.publish_config(new_version, outputs, float_sens)
        return new_version


mqtt_client = MqttClient()
