"""FastAPI sunucusu: tüm yetenekleri tek arayüzde toplar."""

from __future__ import annotations

import threading
import uuid
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from .config import get_config
from .errors import OptimusError
from .llm import Brain
from .media import ffmpeg
from .media.timeline import PRESETS, Timeline, compile_timeline
from .router import route
from .skills import builder, longvideo

app = FastAPI(title="OPTIMUS AI", version="0.1.0")
WEB_DIR = Path(__file__).parent / "web"

#: Arka planda çalışan uzun video işleri.
_jobs: dict[str, longvideo.LongVideoJob] = {}
_jobs_lock = threading.Lock()


def _fail(exc: Exception) -> HTTPException:
    if isinstance(exc, OptimusError):
        return HTTPException(status_code=400, detail=str(exc))
    return HTTPException(status_code=500, detail=f"Beklenmeyen hata: {exc}")


# -- şemalar ------------------------------------------------------------------


class ChatIn(BaseModel):
    message: str
    history: list[dict[str, Any]] = Field(default_factory=list)


class BuildIn(BaseModel):
    request: str
    kind: str = Field(default="app", pattern="^(app|game)$")


class ImageIn(BaseModel):
    prompt: str
    shape: str = Field(default="square", pattern="^(square|landscape|portrait)$")


class VideoIn(BaseModel):
    prompt: str
    total_seconds: float = Field(default=60.0, gt=0)
    preset: str = "youtube"
    fps: int = Field(default=24, ge=1, le=60)
    segment_seconds: float | None = Field(default=None, gt=0)
    transition: str = "fade"
    transition_duration: float = Field(default=0.5, ge=0, le=5)
    captions: bool = True
    music: str | None = None
    music_volume: float = Field(default=0.25, ge=0, le=4)
    ducking: bool = False


class EditIn(BaseModel):
    timeline: dict
    dry_run: bool = Field(
        default=False, description="Sadece ffmpeg komutunu döndür, çalıştırma"
    )


# -- uçlar --------------------------------------------------------------------


@app.get("/api/capabilities")
def capabilities() -> dict:
    config = get_config()
    return {
        "model": config.model,
        "effort": config.effort,
        "image_provider": config.image_provider,
        "video_provider": config.video_provider,
        "segment_seconds": config.video_segment_seconds,
        "ffmpeg": ffmpeg.available(),
        "presets": sorted(PRESETS),
        "workspace": str(config.workspace),
        "warnings": list(config.warnings)
        + ([] if ffmpeg.available() else [ffmpeg.INSTALL_HINT]),
    }


@app.post("/api/route")
def classify(payload: ChatIn) -> dict:
    result = route(payload.message)
    return {"intent": result.intent, "matched": list(result.matched)}


@app.post("/api/chat")
def chat(payload: ChatIn) -> StreamingResponse:
    messages = [*payload.history, {"role": "user", "content": payload.message}]

    def stream():
        try:
            for chunk in Brain().chat_stream(messages):
                yield chunk
        except OptimusError as exc:
            yield f"\n\n[Hata] {exc}"

    return StreamingResponse(stream(), media_type="text/plain; charset=utf-8")


@app.post("/api/build")
def build(payload: BuildIn) -> dict:
    try:
        return builder.build(payload.request, kind=payload.kind).as_dict()
    except Exception as exc:
        raise _fail(exc) from exc


@app.post("/api/image")
def image(payload: ImageIn) -> dict:
    from .providers import generate_image

    config = get_config()
    dest = config.media_dir / f"img-{uuid.uuid4().hex[:10]}.png"
    try:
        generate_image(payload.prompt, dest, shape=payload.shape, config=config)
    except Exception as exc:
        raise _fail(exc) from exc
    return {"path": str(dest), "url": f"/files?path={dest}"}


@app.post("/api/video")
def video(payload: VideoIn) -> dict:
    request = longvideo.VideoRequest(**payload.model_dump())
    job_id = uuid.uuid4().hex[:12]
    try:
        job = longvideo.LongVideoJob(job_id, request)
    except Exception as exc:
        raise _fail(exc) from exc

    with _jobs_lock:
        _jobs[job_id] = job
    job.save()

    def worker() -> None:
        try:
            job.run()
        except Exception:  # durum dosyasına zaten yazıldı
            pass

    threading.Thread(target=worker, daemon=True, name=f"video-{job_id}").start()

    segment = request.segment_seconds or get_config().video_segment_seconds
    return {
        "job_id": job_id,
        "estimated_segments": max(1, round(request.total_seconds / segment)),
        "poll": f"/api/jobs/{job_id}",
    }


@app.get("/api/jobs/{job_id}")
def job_status(job_id: str) -> dict:
    with _jobs_lock:
        job = _jobs.get(job_id)
    if job is not None:
        return job.state.to_dict()
    state = longvideo.load_state(job_id)
    if state is None:
        raise HTTPException(status_code=404, detail="Böyle bir iş yok.")
    return state


@app.post("/api/edit")
def edit(payload: EditIn) -> dict:
    config = get_config()
    try:
        timeline = Timeline.parse(payload.timeline)
        plan = compile_timeline(timeline, workdir=config.workspace)
    except Exception as exc:
        raise _fail(exc) from exc

    if payload.dry_run:
        return {
            "dry_run": True,
            "command": plan.command_preview(),
            "filter_complex": plan.filter_complex,
            "duration": plan.total_duration,
        }

    try:
        ffmpeg.run(plan.args, timeout=7200)
    except Exception as exc:
        raise _fail(exc) from exc
    return {
        "path": str(plan.output_path),
        "url": f"/files?path={plan.output_path}",
        "duration": plan.total_duration,
    }


@app.get("/files")
def files(path: str) -> FileResponse:
    """Çalışma alanı içindeki üretilmiş dosyaları servis eder."""
    config = get_config()
    target = Path(path).resolve()
    if config.workspace not in target.parents and target != config.workspace:
        raise HTTPException(status_code=403, detail="Çalışma alanı dışı erişim.")
    if not target.is_file():
        raise HTTPException(status_code=404, detail="Dosya yok.")
    return FileResponse(target)


if WEB_DIR.is_dir():
    app.mount("/", StaticFiles(directory=WEB_DIR, html=True), name="web")


def main() -> None:
    import uvicorn

    config = get_config()
    for warning in config.warnings:
        print(f"[uyarı] {warning}")
    if not ffmpeg.available():
        print("[uyarı] ffmpeg yok — video ve kurgu yetenekleri kapalı.")
    print(f"OPTIMUS AI → http://{config.host}:{config.port}")
    uvicorn.run(app, host=config.host, port=config.port, log_level="info")


if __name__ == "__main__":
    main()
