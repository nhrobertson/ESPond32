import asyncio
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.responses import RedirectResponse
from fastapi.staticfiles import StaticFiles

from app import db
from app.auth import NotAuthenticated
from app.mqtt_client import mqtt_client
from app.routes import auth_routes, dashboard, events, schedule, settings_routes
from app.state import device_state

logging.basicConfig(level=logging.INFO)


@asynccontextmanager
async def lifespan(app: FastAPI):
    db.init_db()
    device_state.bind_loop(asyncio.get_running_loop())
    mqtt_client.connect()
    yield
    mqtt_client.disconnect()


app = FastAPI(lifespan=lifespan)
app.mount("/static", StaticFiles(directory="app/static"), name="static")

app.include_router(auth_routes.router)
app.include_router(dashboard.router)
app.include_router(schedule.router)
app.include_router(settings_routes.router)
app.include_router(events.router)


@app.exception_handler(NotAuthenticated)
async def not_authenticated_handler(request: Request, exc: NotAuthenticated):
    return RedirectResponse(url="/login", status_code=303)
