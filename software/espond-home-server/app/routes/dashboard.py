from fastapi import APIRouter, Depends, HTTPException, Request
from fastapi.responses import JSONResponse
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel

from app import settings
from app.auth import csrf_token_from_request, require_login, verify_csrf_from_request
from app.mqtt_client import mqtt_client
from app.state import device_state
from app.validators import ValidationError

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")

OUTPUT_LABELS = {
    "pump1": "Pump 1",
    "pump2": "Pump 2",
    "valve1": "Valve 1 (fill)",
    "light1": "Light 1",
}


def _find_state(status: dict | None, name: str) -> bool | None:
    if not status:
        return None
    for entry in status.get("outputs", []):
        if entry.get("name") == name:
            return entry.get("state")
    return None


@router.get("/")
def dashboard(request: Request, _=Depends(require_login)):
    snapshot = device_state.snapshot()
    status = snapshot["status"]

    outputs = [
        {"name": name, "label": OUTPUT_LABELS[name], "state": _find_state(status, name)}
        for name in settings.OUTPUT_NAMES
    ]

    return templates.TemplateResponse(
        request,
        "dashboard.html",
        {
            "show_nav": True,
            "active_nav": "dashboard",
            "availability": snapshot["availability"],
            "outputs": outputs,
            "float_state": _find_state(status, "float1"),
            "leak_lockout": status.get("leak_lockout") if status else None,
            "device_version": status.get("version") if status else None,
            "csrf_token": csrf_token_from_request(request),
        },
    )


class OverrideRequest(BaseModel):
    name: str
    override: int
    csrf_token: str


class CommandRequest(BaseModel):
    command: str
    csrf_token: str


class ResendRequest(BaseModel):
    csrf_token: str


@router.post("/override")
def post_override(request: Request, body: OverrideRequest, _=Depends(require_login)):
    if not verify_csrf_from_request(request, body.csrf_token):
        raise HTTPException(403, "invalid csrf token")
    try:
        mqtt_client.publish_override(body.name, body.override)
    except ValidationError as exc:
        raise HTTPException(400, "; ".join(exc.errors))
    return JSONResponse({"ok": True})


@router.post("/cmd")
def post_cmd(request: Request, body: CommandRequest, _=Depends(require_login)):
    if not verify_csrf_from_request(request, body.csrf_token):
        raise HTTPException(403, "invalid csrf token")
    try:
        mqtt_client.publish_command(body.command)
    except ValidationError as exc:
        raise HTTPException(400, "; ".join(exc.errors))
    return JSONResponse({"ok": True})


@router.post("/config/resend")
def post_resend_config(request: Request, body: ResendRequest, _=Depends(require_login)):
    if not verify_csrf_from_request(request, body.csrf_token):
        raise HTTPException(403, "invalid csrf token")
    mqtt_client.resend_config_and_request_status()
    return JSONResponse({"ok": True})
