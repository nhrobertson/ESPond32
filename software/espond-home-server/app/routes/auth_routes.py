from fastapi import APIRouter, Form, Request
from fastapi.responses import RedirectResponse
from fastapi.templating import Jinja2Templates

from app import auth, settings

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")


@router.get("/setup")
def setup_form(request: Request):
    if auth.is_admin_configured():
        return RedirectResponse("/login", status_code=303)
    return templates.TemplateResponse(request, "setup.html", {})


@router.post("/setup")
def setup_submit(request: Request, password: str = Form(...), confirm_password: str = Form(...)):
    if auth.is_admin_configured():
        return RedirectResponse("/login", status_code=303)

    error = None
    if len(password) < 8:
        error = "Password must be at least 8 characters."
    elif password != confirm_password:
        error = "Passwords do not match."
    if error:
        return templates.TemplateResponse(
            request, "setup.html", {"error": error}, status_code=400
        )

    auth.set_admin_password(password)
    response = RedirectResponse("/", status_code=303)
    auth.set_session_cookie(response, auth.create_session_cookie_value())
    return response


@router.get("/login")
def login_form(request: Request):
    if not auth.is_admin_configured():
        return RedirectResponse("/setup", status_code=303)
    return templates.TemplateResponse(request, "login.html", {})


@router.post("/login")
def login_submit(request: Request, password: str = Form(...)):
    if not auth.is_admin_configured():
        return RedirectResponse("/setup", status_code=303)

    client_ip = request.client.host if request.client else "unknown"
    if auth.login_limiter.is_locked_out(client_ip):
        return templates.TemplateResponse(
            request,
            "login.html",
            {"error": "Too many failed attempts. Try again in a minute."},
            status_code=429,
        )

    if not auth.verify_admin_password(password):
        auth.login_limiter.record_failure(client_ip)
        return templates.TemplateResponse(
            request, "login.html", {"error": "Incorrect password."}, status_code=401
        )

    auth.login_limiter.reset(client_ip)
    response = RedirectResponse("/", status_code=303)
    auth.set_session_cookie(response, auth.create_session_cookie_value())
    return response


@router.post("/logout")
def logout():
    response = RedirectResponse("/login", status_code=303)
    response.delete_cookie(settings.SESSION_COOKIE_NAME)
    return response
