"""Server-side enforcement of SPEC.md's §3.1/§3.2/§3.3 wire-format rules.

The device silently rejects (whole-message, no partial apply) anything that
violates these rules and reports nothing back over MQTT (SPEC.md §5), so the
server must catch violations itself before publishing.
"""
import re

from app import settings

_TIME_RE = re.compile(r"^([01]\d|2[0-3]):[0-5]\d$")

KNOWN_COMMANDS = {"clear_leak_lockout", "reboot", "request_status"}


class ValidationError(Exception):
    def __init__(self, errors: list[str]):
        super().__init__("; ".join(errors))
        self.errors = errors


def _validate_output_entry(entry: dict) -> list[str]:
    errors = []
    name = entry.get("name", "<missing>")
    on, off = entry.get("on"), entry.get("off")

    if (on is None) != (off is None):
        errors.append(f"{name}: 'on' and 'off' must both be null or both set")
    elif on is not None and off is not None:
        if not _TIME_RE.match(on):
            errors.append(f"{name}: 'on' must be an HH:MM 24-hour string, got {on!r}")
        if not _TIME_RE.match(off):
            errors.append(f"{name}: 'off' must be an HH:MM 24-hour string, got {off!r}")

    days = entry.get("days", [])
    if not isinstance(days, list):
        errors.append(f"{name}: 'days' must be a list")
    else:
        for d in days:
            if not isinstance(d, int) or isinstance(d, bool) or not (0 <= d <= 6):
                errors.append(f"{name}: day value {d!r} out of range (must be 0-6, Sun-Sat)")

    return errors


def validate_outputs(outputs: list[dict]) -> list[str]:
    errors = []
    names = [o.get("name") for o in outputs]
    expected = set(settings.OUTPUT_NAMES)

    if len(outputs) != len(settings.OUTPUT_NAMES) or set(names) != expected:
        errors.append(
            f"outputs must contain exactly these {len(settings.OUTPUT_NAMES)} names, "
            f"each once: {sorted(expected)} (got {names})"
        )

    for entry in outputs:
        errors.extend(_validate_output_entry(entry))

    return errors


def validate_float_sens(float_sens: dict) -> list[str]:
    errors = []
    for key in ("threshold_min", "overflow_min", "max_fills_per_day"):
        value = float_sens.get(key)
        if not isinstance(value, int) or isinstance(value, bool) or value < 0:
            errors.append(f"float_sens.{key} must be a non-negative integer, got {value!r}")
    return errors


def validate_config(outputs: list[dict], float_sens: dict) -> None:
    errors = validate_outputs(outputs) + validate_float_sens(float_sens)
    if errors:
        raise ValidationError(errors)


def validate_override(payload: dict) -> None:
    errors = []
    if payload.get("name") not in settings.OUTPUT_NAMES:
        errors.append(
            f"name must be one of {settings.OUTPUT_NAMES} (float1 is input-only, not overridable)"
        )
    if payload.get("override") not in (0, 1, 2):
        errors.append("override must be 0 (ON), 1 (AUTO), or 2 (OFF)")
    if errors:
        raise ValidationError(errors)


def validate_command(cmd: str) -> None:
    if cmd not in KNOWN_COMMANDS:
        raise ValidationError([f"unknown command {cmd!r}, must be one of {sorted(KNOWN_COMMANDS)}"])
