from fastapi import APIRouter, Depends, Request
from fastapi.templating import Jinja2Templates

from app import auth, db
from app.auth import csrf_token_from_request, require_login, verify_csrf_from_request

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")


def _render(request: Request, error: str | None = None, saved: str | None = None, status_code: int = 200):
    return templates.TemplateResponse(
        request,
        "settings.html",
        {
            "show_nav": True,
            "active_nav": "settings",
            "mqtt_host": db.get_app_config("mqtt_host"),
            "mqtt_port": db.get_app_config("mqtt_port"),
            "mqtt_username": db.get_app_config("mqtt_username"),
            "error": error,
            "saved": saved,
            "csrf_token": csrf_token_from_request(request),
        },
        status_code=status_code,
    )


@router.get("/settings")
def settings_form(request: Request, _=Depends(require_login)):
    return _render(request)


@router.post("/settings/password")
async def change_password(request: Request, _=Depends(require_login)):
    form = await request.form()
    if not verify_csrf_from_request(request, form.get("csrf_token")):
        return _render(request, error="Invalid form submission, please retry.", status_code=403)

    current = form.get("current_password", "")
    new_password = form.get("new_password", "")
    confirm = form.get("confirm_password", "")

    if not auth.verify_admin_password(current):
        return _render(request, error="Current password is incorrect.", status_code=401)
    if len(new_password) < 8:
        return _render(request, error="New password must be at least 8 characters.", status_code=400)
    if new_password != confirm:
        return _render(request, error="New passwords do not match.", status_code=400)

    auth.set_admin_password(new_password)
    return _render(request, saved="Admin password updated.")


@router.post("/settings/broker")
async def change_broker(request: Request, _=Depends(require_login)):
    form = await request.form()
    if not verify_csrf_from_request(request, form.get("csrf_token")):
        return _render(request, error="Invalid form submission, please retry.", status_code=403)

    host = (form.get("mqtt_host") or "").strip()
    port_raw = form.get("mqtt_port") or ""
    username = (form.get("mqtt_username") or "").strip()
    password = form.get("mqtt_password") or ""

    if not host:
        return _render(request, error="Broker host is required.", status_code=400)
    if not port_raw.isdigit():
        return _render(request, error="Broker port must be a number.", status_code=400)

    db.set_app_config("mqtt_host", host)
    db.set_app_config("mqtt_port", port_raw)
    db.set_app_config("mqtt_username", username)
    if password:
        db.set_app_config("mqtt_password", password)

    return _render(
        request,
        saved="Broker settings saved. Restart the service for the new connection to take effect.",
    )
