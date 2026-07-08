import hashlib
import hmac
import secrets
import time

import bcrypt
from fastapi import Request
from itsdangerous import BadSignature, SignatureExpired, URLSafeTimedSerializer

from app import db, settings

ADMIN_PASSWORD_HASH_KEY = "admin_password_hash"
SESSION_SECRET_KEY = "session_secret"
SESSION_SALT = "espond-session"


class NotAuthenticated(Exception):
    """Raised by require_login; main.py maps this to a redirect to /login."""


def _get_or_create_secret() -> str:
    secret = db.get_app_config(SESSION_SECRET_KEY)
    if secret is None:
        secret = secrets.token_hex(32)
        db.set_app_config(SESSION_SECRET_KEY, secret)
    return secret


def _serializer() -> URLSafeTimedSerializer:
    return URLSafeTimedSerializer(_get_or_create_secret(), salt=SESSION_SALT)


def is_admin_configured() -> bool:
    return db.get_app_config(ADMIN_PASSWORD_HASH_KEY) is not None


def set_admin_password(password: str) -> None:
    hashed = bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()
    db.set_app_config(ADMIN_PASSWORD_HASH_KEY, hashed)


def verify_admin_password(password: str) -> bool:
    hashed = db.get_app_config(ADMIN_PASSWORD_HASH_KEY)
    if hashed is None:
        return False
    return bcrypt.checkpw(password.encode(), hashed.encode())


def create_session_cookie_value() -> str:
    return _serializer().dumps({"authenticated": True, "nonce": secrets.token_hex(8)})


def verify_session_cookie(value: str | None) -> bool:
    if not value:
        return False
    try:
        data = _serializer().loads(value, max_age=settings.SESSION_MAX_AGE_SECONDS)
    except (BadSignature, SignatureExpired):
        return False
    return bool(data.get("authenticated"))


def csrf_token_for(session_cookie_value: str) -> str:
    secret = _get_or_create_secret().encode()
    return hmac.new(secret, session_cookie_value.encode(), hashlib.sha256).hexdigest()


def verify_csrf(session_cookie_value: str | None, token: str | None) -> bool:
    if not session_cookie_value or not token:
        return False
    expected = csrf_token_for(session_cookie_value)
    return hmac.compare_digest(expected, token)


def csrf_token_from_request(request: Request) -> str:
    cookie_value = request.cookies.get(settings.SESSION_COOKIE_NAME)
    return csrf_token_for(cookie_value) if cookie_value else ""


def verify_csrf_from_request(request: Request, token: str | None) -> bool:
    cookie_value = request.cookies.get(settings.SESSION_COOKIE_NAME)
    return verify_csrf(cookie_value, token)


class LoginLimiter:
    """In-memory per-IP lockout after repeated failures. This is a single-Pi,
    single-admin app, so a process-local dict is enough — no shared cache needed.
    """

    def __init__(self, max_attempts: int, lockout_seconds: int):
        self.max_attempts = max_attempts
        self.lockout_seconds = lockout_seconds
        self._failures: dict[str, list[float]] = {}
        self._locked_until: dict[str, float] = {}

    def is_locked_out(self, key: str) -> bool:
        until = self._locked_until.get(key)
        if until is None:
            return False
        if time.monotonic() >= until:
            del self._locked_until[key]
            self._failures.pop(key, None)
            return False
        return True

    def record_failure(self, key: str) -> None:
        now = time.monotonic()
        window_start = now - self.lockout_seconds
        attempts = [t for t in self._failures.get(key, []) if t >= window_start]
        attempts.append(now)
        self._failures[key] = attempts
        if len(attempts) >= self.max_attempts:
            self._locked_until[key] = now + self.lockout_seconds

    def reset(self, key: str) -> None:
        self._failures.pop(key, None)
        self._locked_until.pop(key, None)


login_limiter = LoginLimiter(settings.LOGIN_MAX_ATTEMPTS, settings.LOGIN_LOCKOUT_SECONDS)


def require_login(request: Request) -> None:
    cookie_value = request.cookies.get(settings.SESSION_COOKIE_NAME)
    if not verify_session_cookie(cookie_value):
        raise NotAuthenticated()


def set_session_cookie(response, session_value: str) -> None:
    response.set_cookie(
        settings.SESSION_COOKIE_NAME,
        session_value,
        max_age=settings.SESSION_MAX_AGE_SECONDS,
        httponly=True,
        samesite="lax",
        secure=settings.SESSION_COOKIE_SECURE,
    )
