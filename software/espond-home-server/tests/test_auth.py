import time

from app import auth


def test_admin_not_configured_until_password_set(temp_db):
    assert auth.is_admin_configured() is False
    auth.set_admin_password("hunter22")
    assert auth.is_admin_configured() is True


def test_verify_admin_password_roundtrip(temp_db):
    auth.set_admin_password("correct-horse")
    assert auth.verify_admin_password("correct-horse") is True
    assert auth.verify_admin_password("wrong") is False


def test_session_cookie_roundtrip(temp_db):
    token = auth.create_session_cookie_value()
    assert auth.verify_session_cookie(token) is True
    assert auth.verify_session_cookie("garbage") is False
    assert auth.verify_session_cookie(None) is False


def test_csrf_token_matches_only_for_its_own_session(temp_db):
    session_a = auth.create_session_cookie_value()
    session_b = auth.create_session_cookie_value()
    token_a = auth.csrf_token_for(session_a)

    assert auth.verify_csrf(session_a, token_a) is True
    assert auth.verify_csrf(session_b, token_a) is False
    assert auth.verify_csrf(session_a, "wrong-token") is False


def test_login_limiter_locks_out_after_max_attempts():
    limiter = auth.LoginLimiter(max_attempts=3, lockout_seconds=60)
    assert limiter.is_locked_out("1.2.3.4") is False
    for _ in range(3):
        limiter.record_failure("1.2.3.4")
    assert limiter.is_locked_out("1.2.3.4") is True


def test_login_limiter_reset_clears_lockout():
    limiter = auth.LoginLimiter(max_attempts=1, lockout_seconds=60)
    limiter.record_failure("5.6.7.8")
    assert limiter.is_locked_out("5.6.7.8") is True
    limiter.reset("5.6.7.8")
    assert limiter.is_locked_out("5.6.7.8") is False


def test_login_limiter_expires_after_lockout_window():
    limiter = auth.LoginLimiter(max_attempts=1, lockout_seconds=0.05)
    limiter.record_failure("9.9.9.9")
    assert limiter.is_locked_out("9.9.9.9") is True
    time.sleep(0.1)
    assert limiter.is_locked_out("9.9.9.9") is False
