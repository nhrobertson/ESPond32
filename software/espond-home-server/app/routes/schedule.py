from fastapi import APIRouter, Depends, Request
from fastapi.templating import Jinja2Templates

from app import db, settings
from app.auth import csrf_token_from_request, require_login, verify_csrf_from_request
from app.mqtt_client import mqtt_client
from app.validators import ValidationError

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")

OUTPUT_LABELS = {
    "pump1": "Pump 1",
    "pump2": "Pump 2",
    "valve1": "Valve 1 (fill)",
    "light1": "Light 1",
}
DAY_LABELS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]


def _render(request: Request, outputs: list[dict], float_sens: dict, error: str | None = None, saved: bool = False, status_code: int = 200):
    view_outputs = []
    for name in settings.OUTPUT_NAMES:
        entry = next((o for o in outputs if o.get("name") == name), {"name": name, "on": None, "off": None, "days": []})
        view_outputs.append(
            {
                "name": name,
                "label": OUTPUT_LABELS[name],
                "on": entry.get("on") or "",
                "off": entry.get("off") or "",
                "no_schedule": entry.get("on") is None,
                "days": set(entry.get("days") or []),
            }
        )
    return templates.TemplateResponse(
        request,
        "schedule.html",
        {
            "show_nav": True,
            "active_nav": "schedule",
            "outputs": view_outputs,
            "day_labels": list(enumerate(DAY_LABELS)),
            "float_sens": float_sens,
            "error": error,
            "saved": saved,
            "csrf_token": csrf_token_from_request(request),
        },
        status_code=status_code,
    )


@router.get("/schedule")
def schedule_form(request: Request, _=Depends(require_login)):
    cfg = db.get_pond_config()
    return _render(request, cfg["outputs"], cfg["float_sens"])


@router.post("/schedule")
async def schedule_submit(request: Request, _=Depends(require_login)):
    form = await request.form()

    if not verify_csrf_from_request(request, form.get("csrf_token")):
        cfg = db.get_pond_config()
        return _render(request, cfg["outputs"], cfg["float_sens"], error="Invalid form submission, please retry.", status_code=403)

    outputs = []
    for name in settings.OUTPUT_NAMES:
        no_schedule = form.get(f"no_schedule_{name}") == "on"
        on = None if no_schedule else (form.get(f"on_{name}") or None)
        off = None if no_schedule else (form.get(f"off_{name}") or None)
        days = [int(d) for d in form.getlist(f"days_{name}") if d.isdigit()]
        outputs.append({"name": name, "on": on, "off": off, "days": days})

    try:
        float_sens = {
            "threshold_min": int(form.get("threshold_min", "")),
            "overflow_min": int(form.get("overflow_min", "")),
            "max_fills_per_day": int(form.get("max_fills_per_day", "")),
        }
    except ValueError:
        return _render(request, outputs, {
            "threshold_min": form.get("threshold_min", ""),
            "overflow_min": form.get("overflow_min", ""),
            "max_fills_per_day": form.get("max_fills_per_day", ""),
        }, error="Float sensor fields must be whole numbers.", status_code=400)

    try:
        mqtt_client.push_new_config(outputs, float_sens)
    except ValidationError as exc:
        return _render(request, outputs, float_sens, error="; ".join(exc.errors), status_code=400)

    return _render(request, outputs, float_sens, error=None, saved=True)
