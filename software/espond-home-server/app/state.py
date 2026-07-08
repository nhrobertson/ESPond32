"""In-memory cache of the device's last-known state, fed by MQTT callbacks and
broadcast to browser SSE subscribers. Not persisted — pond/status and
pond/availability are already retained on the broker, so a server restart
gets the latest by resubscribing.
"""
import asyncio
import json
from typing import Optional


class DeviceState:
    def __init__(self):
        self.availability: str = "unknown"  # "online" | "offline" | "unknown"
        self.status: Optional[dict] = None
        self._subscribers: set[asyncio.Queue] = set()
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def bind_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop

    def subscribe(self) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue()
        self._subscribers.add(q)
        return q

    def unsubscribe(self, q: asyncio.Queue) -> None:
        self._subscribers.discard(q)

    def snapshot(self) -> dict:
        return {"availability": self.availability, "status": self.status}

    def set_availability(self, value: str) -> bool:
        """Returns True if this is a transition into 'online'."""
        was_online = self.availability == "online"
        self.availability = value
        self._schedule_broadcast()
        return value == "online" and not was_online

    def set_status(self, status: dict) -> None:
        self.status = status
        self._schedule_broadcast()

    def _schedule_broadcast(self) -> None:
        if self._loop is not None:
            self._loop.call_soon_threadsafe(self._broadcast)
        else:
            self._broadcast()

    def _broadcast(self) -> None:
        payload = json.dumps(self.snapshot())
        for q in list(self._subscribers):
            q.put_nowait(payload)


device_state = DeviceState()
