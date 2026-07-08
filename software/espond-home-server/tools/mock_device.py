#!/usr/bin/env python3
"""Simulates the ESPond32 firmware's MQTT behavior for local dev/testing of the
home server without real hardware. Implements just enough of SPEC.md to
exercise the server: config version acceptance, cmd handling, override
resolution (AUTO/ON/OFF, no physical-switch simulation), leak lockout, and
the retained availability LWT-style online/offline signal.

Usage:
    python tools/mock_device.py [--host localhost] [--port 1883]
                                 [--username espond] [--password password]

Then type commands at the prompt:
    status              print current resolved state
    float on|off        simulate the float sensor reading
    leak on|off         force leak_lockout (test the dashboard banner)
    reboot              simulate a disconnect + reconnect
    quit
"""
import argparse
import json
import sys
import threading
import time

import paho.mqtt.client as mqtt

OUTPUT_NAMES = ["pump1", "pump2", "valve1", "light1"]
TOPIC_CONFIG = "pond/config"
TOPIC_CMD = "pond/cmd"
TOPIC_OVERRIDE = "pond/override"
TOPIC_STATUS = "pond/status"
TOPIC_AVAILABILITY = "pond/availability"


class MockDevice:
    def __init__(self, host, port, username, password):
        self.host, self.port = host, port
        self.version = 0
        self.overrides = {name: 1 for name in OUTPUT_NAMES}  # 1 = AUTO
        self.leak_lockout = False
        self.float_state = False
        self.lock = threading.Lock()

        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="espondcu-mock",
        )
        self.client.username_pw_set(username, password)
        self.client.will_set(TOPIC_AVAILABILITY, payload="offline", qos=1, retain=True)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    def connect(self):
        self.client.connect(self.host, self.port, keepalive=30)
        self.client.loop_start()

    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        print(f"[mock] connected ({reason_code}); subscribing + announcing online")
        client.subscribe(TOPIC_CONFIG, qos=1)
        client.subscribe(TOPIC_CMD, qos=1)
        client.subscribe(TOPIC_OVERRIDE, qos=1)
        client.publish(TOPIC_AVAILABILITY, "online", qos=1, retain=True)
        self._publish_status()

    def _on_message(self, client, userdata, msg):
        if msg.topic == TOPIC_CONFIG:
            self._handle_config(msg.payload)
        elif msg.topic == TOPIC_CMD:
            self._handle_cmd(msg.payload)
        elif msg.topic == TOPIC_OVERRIDE:
            self._handle_override(msg.payload)

    def _handle_config(self, payload: bytes):
        try:
            doc = json.loads(payload.decode())
        except ValueError:
            print("[mock] rejected config: not valid JSON")
            return
        new_version = doc.get("version")
        if not isinstance(new_version, int) or new_version <= self.version:
            print(f"[mock] rejected config: version {new_version} not > current {self.version}")
            return
        with self.lock:
            self.version = new_version
        print(f"[mock] accepted config version {new_version}")

    def _handle_cmd(self, payload: bytes):
        cmd = payload.decode(errors="replace")
        if cmd == "request_status":
            self._publish_status()
        elif cmd == "clear_leak_lockout":
            with self.lock:
                self.leak_lockout = False
            print("[mock] leak lockout cleared")
            self._publish_status()
        elif cmd == "reboot":
            print("[mock] rebooting (simulated)...")
            threading.Thread(target=self._simulate_reboot, daemon=True).start()
        else:
            print(f"[mock] ignoring unknown cmd {cmd!r}")

    def _handle_override(self, payload: bytes):
        try:
            doc = json.loads(payload.decode())
        except ValueError:
            print("[mock] ignored override: not valid JSON")
            return
        name, override = doc.get("name"), doc.get("override")
        if name not in OUTPUT_NAMES:
            print(f"[mock] ignored override for unknown/non-overridable name {name!r}")
            return
        if override not in (0, 1, 2):
            override = 1  # out-of-range silently treated as AUTO, per spec
        with self.lock:
            self.overrides[name] = override
        print(f"[mock] override {name} -> {override}")
        self._publish_status()

    def _simulate_reboot(self):
        # A real reboot is esp_restart() — an abrupt power/reset cycle, not a clean MQTT
        # disconnect. Close the raw socket instead of calling disconnect() so the broker
        # sees an ungraceful drop and fires the Last-Will "offline", matching real hardware.
        self.client.loop_stop()
        sock = self.client.socket()
        if sock is not None:
            sock.close()
        time.sleep(1.5)
        self.connect()

    def _resolved_states(self) -> dict:
        with self.lock:
            leak = self.leak_lockout
            overrides = dict(self.overrides)
        states = {}
        for name in OUTPUT_NAMES:
            if leak and name != "light1":
                states[name] = False
            elif overrides[name] == 0:
                states[name] = True
            elif overrides[name] == 2:
                states[name] = False
            else:  # AUTO, no schedule simulation - baseline off
                states[name] = False
        return states

    def _publish_status(self):
        states = self._resolved_states()
        outputs = [{"name": n, "state": states[n]} for n in OUTPUT_NAMES]
        outputs.append({"name": "float1", "state": self.float_state})
        with self.lock:
            payload = json.dumps(
                {"version": self.version, "leak_lockout": self.leak_lockout, "outputs": outputs}
            )
        self.client.publish(TOPIC_STATUS, payload, qos=1, retain=True)
        print(f"[mock] published status: {payload}")

    def set_float(self, on: bool):
        with self.lock:
            self.float_state = on
        self._publish_status()

    def set_leak(self, on: bool):
        with self.lock:
            self.leak_lockout = on
        self._publish_status()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--username", default="espond")
    parser.add_argument("--password", default="password")
    args = parser.parse_args()

    device = MockDevice(args.host, args.port, args.username, args.password)
    device.connect()

    print("mock device running. commands: status | float on|off | leak on|off | reboot | quit")
    try:
        for line in sys.stdin:
            parts = line.strip().split()
            if not parts:
                continue
            cmd = parts[0]
            if cmd == "quit":
                break
            elif cmd == "status":
                device._publish_status()
            elif cmd == "float" and len(parts) == 2:
                device.set_float(parts[1] == "on")
            elif cmd == "leak" and len(parts) == 2:
                device.set_leak(parts[1] == "on")
            elif cmd == "reboot":
                device._simulate_reboot()
            else:
                print("unknown command")
    except KeyboardInterrupt:
        pass
    finally:
        device.disconnect()


if __name__ == "__main__":
    main()
