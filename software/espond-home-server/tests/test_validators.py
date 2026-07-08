import pytest

from app.validators import (
    ValidationError,
    validate_command,
    validate_config,
    validate_override,
)

VALID_OUTPUTS = [
    {"name": "pump1", "on": "14:00", "off": "14:05", "days": [0, 1, 2, 3, 4, 5, 6]},
    {"name": "pump2", "on": "14:00", "off": "14:05", "days": [0, 1, 2, 3, 4, 5, 6]},
    {"name": "valve1", "on": None, "off": None, "days": []},
    {"name": "light1", "on": "18:00", "off": "23:00", "days": [5, 6]},
]
VALID_FLOAT_SENS = {"threshold_min": 10, "overflow_min": 30, "max_fills_per_day": 3}


def test_valid_config_passes():
    validate_config(VALID_OUTPUTS, VALID_FLOAT_SENS)  # must not raise


def test_missing_output_rejected():
    outputs = VALID_OUTPUTS[:3]
    with pytest.raises(ValidationError):
        validate_config(outputs, VALID_FLOAT_SENS)


def test_unknown_name_rejected():
    outputs = [dict(o) for o in VALID_OUTPUTS]
    outputs[0]["name"] = "float1"
    with pytest.raises(ValidationError):
        validate_config(outputs, VALID_FLOAT_SENS)


def test_mismatched_null_on_off_rejected():
    outputs = [dict(o) for o in VALID_OUTPUTS]
    outputs[2] = {"name": "valve1", "on": "10:00", "off": None, "days": []}
    with pytest.raises(ValidationError):
        validate_config(outputs, VALID_FLOAT_SENS)


def test_bad_time_format_rejected():
    outputs = [dict(o) for o in VALID_OUTPUTS]
    outputs[0] = {"name": "pump1", "on": "2:00pm", "off": "14:05", "days": [0]}
    with pytest.raises(ValidationError):
        validate_config(outputs, VALID_FLOAT_SENS)


def test_out_of_range_day_rejected():
    outputs = [dict(o) for o in VALID_OUTPUTS]
    outputs[0] = {"name": "pump1", "on": "14:00", "off": "14:05", "days": [7]}
    with pytest.raises(ValidationError):
        validate_config(outputs, VALID_FLOAT_SENS)


def test_negative_float_sens_rejected():
    with pytest.raises(ValidationError):
        validate_config(VALID_OUTPUTS, {"threshold_min": -1, "overflow_min": 30, "max_fills_per_day": 3})


def test_valid_override_passes():
    validate_override({"name": "pump1", "override": 0})


def test_float1_override_rejected():
    with pytest.raises(ValidationError):
        validate_override({"name": "float1", "override": 1})


def test_out_of_range_override_rejected():
    with pytest.raises(ValidationError):
        validate_override({"name": "pump1", "override": 99})


def test_known_commands_pass():
    for cmd in ("clear_leak_lockout", "reboot", "request_status"):
        validate_command(cmd)


def test_unknown_command_rejected():
    with pytest.raises(ValidationError):
        validate_command("do_a_barrel_roll")
