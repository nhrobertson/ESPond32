import asyncio
import json

from fastapi import APIRouter, Depends, Request
from fastapi.responses import StreamingResponse

from app.auth import require_login
from app.state import device_state

router = APIRouter()


@router.get("/events")
async def stream_events(request: Request, _=Depends(require_login)):
    async def event_generator():
        queue = device_state.subscribe()
        try:
            yield f"data: {json.dumps(device_state.snapshot())}\n\n"
            while True:
                if await request.is_disconnected():
                    break
                try:
                    payload = await asyncio.wait_for(queue.get(), timeout=15)
                    yield f"data: {payload}\n\n"
                except asyncio.TimeoutError:
                    yield ": keepalive\n\n"
        finally:
            device_state.unsubscribe(queue)

    return StreamingResponse(event_generator(), media_type="text/event-stream")
