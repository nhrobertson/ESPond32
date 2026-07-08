import threading

import pytest

from app import db, settings


@pytest.fixture()
def temp_db(tmp_path, monkeypatch):
    monkeypatch.setattr(settings, "DB_PATH", tmp_path / "test.db")
    monkeypatch.setattr(db, "_local", threading.local())
    db.init_db()
    yield db
