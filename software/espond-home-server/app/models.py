from typing import Optional

from pydantic import BaseModel


class OutputSchedule(BaseModel):
    name: str
    on: Optional[str] = None
    off: Optional[str] = None
    days: list[int] = []


class FloatSensorConfig(BaseModel):
    threshold_min: int
    overflow_min: int
    max_fills_per_day: int


class PondConfig(BaseModel):
    version: int
    outputs: list[OutputSchedule]
    float_sens: FloatSensorConfig


class OverrideCommand(BaseModel):
    name: str
    override: int


class OutputStatus(BaseModel):
    name: str
    state: bool


class DeviceStatus(BaseModel):
    version: int
    leak_lockout: bool
    outputs: list[OutputStatus]
