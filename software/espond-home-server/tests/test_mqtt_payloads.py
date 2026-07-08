import json
from unittest.mock import MagicMock

import pytest

from app.mqtt_client import MqttClient
from app.validators import ValidationError

VALID_OUTPUTS = [
    {"name": "pump1", "on": "14:00", "off": "14:05", "days": [0, 1, 2, 3, 4, 5, 6]},
    {"name": "pump2", "on": "14:00", "off": "14:05", "days": [0, 1, 2, 3, 4, 5, 6]},
    {"name": "valve1", "on": None, "off": None, "days": []},
    {"name": "light1", "on": "18:00", "off": "23:00", "days": [5, 6]},
]
VALID_FLOAT_SENS = {"threshold_min": 10, "overflow_min": 30, "max_fills_per_day": 3}


@pytest.fixture()
def client_with_mock_transport(temp_db):
    client = MqttClient()
    client._client = MagicMock()
    return client


def test_push_new_config_increments_version_and_publishes(client_with_mock_transport, temp_db):
    client = client_with_mock_transport
    assert temp_db.get_pond_config()["version"] == 0

    new_version = client.push_new_config(VALID_OUTPUTS, VALID_FLOAT_SENS)

    assert new_version == 1
    assert temp_db.get_pond_config()["version"] == 1
    client._client.publish.assert_called_once()
    topic, payload = client._client.publish.call_args[0][:2]
    assert topic == "pond/config"
    body = json.loads(payload)
    assert body["version"] == 1
    assert body["outputs"] == VALID_OUTPUTS
    assert body["float_sens"] == VALID_FLOAT_SENS
    # SPEC.md §2: pond/config must not be published retained
    assert client._client.publish.call_args.kwargs.get("retain") is False


def test_push_new_config_rejects_invalid_document(client_with_mock_transport, temp_db):
    client = client_with_mock_transport
    bad_outputs = VALID_OUTPUTS[:3]  # missing one required output

    with pytest.raises(ValidationError):
        client.push_new_config(bad_outputs, VALID_FLOAT_SENS)

    client._client.publish.assert_not_called()
    assert temp_db.get_pond_config()["version"] == 0


def test_override_publish_matches_spec_schema(client_with_mock_transport):
    client = client_with_mock_transport
    client.publish_override("pump1", 0)

    client._client.publish.assert_called_once()
    topic, payload = client._client.publish.call_args[0][:2]
    assert topic == "pond/override"
    assert json.loads(payload) == {"name": "pump1", "override": 0}
    assert client._client.publish.call_args.kwargs.get("retain") is False


def test_override_rejects_float1():
    client = MqttClient()
    client._client = MagicMock()
    with pytest.raises(ValidationError):
        client.publish_override("float1", 1)
    client._client.publish.assert_not_called()


def test_availability_online_transition_triggers_resend(client_with_mock_transport, temp_db):
    client = client_with_mock_transport
    temp_db.save_pond_config(3, VALID_OUTPUTS, VALID_FLOAT_SENS)

    client._handle_availability(b"online")

    calls = client._client.publish.call_args_list
    topics = [c.args[0] for c in calls]
    assert "pond/config" in topics
    assert "pond/cmd" in topics
    cmd_call = next(c for c in calls if c.args[0] == "pond/cmd")
    assert cmd_call.args[1] == "request_status"


def test_availability_repeated_online_does_not_resend(client_with_mock_transport, temp_db):
    client = client_with_mock_transport
    client._handle_availability(b"online")
    client._client.publish.reset_mock()

    client._handle_availability(b"online")

    client._client.publish.assert_not_called()
