"""Ortam değişkenlerinden okunan tekil yapılandırma."""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path

from dotenv import load_dotenv

load_dotenv()

VALID_EFFORTS = ("low", "medium", "high", "xhigh", "max")


def _env(name: str, default: str = "") -> str:
    return os.environ.get(name, default).strip()


def _int_env(name: str, default: int) -> int:
    raw = _env(name)
    if not raw:
        return default
    try:
        return int(raw)
    except ValueError:
        return default


@dataclass(frozen=True)
class Config:
    anthropic_api_key: str
    model: str
    effort: str

    image_provider: str
    openai_api_key: str
    openai_image_model: str
    stability_api_key: str
    replicate_token: str
    replicate_image_model: str

    video_provider: str
    replicate_video_model: str
    luma_api_key: str
    video_segment_seconds: int

    workspace: Path
    ffmpeg: str
    ffprobe: str
    host: str
    port: int

    #: Kurulum sırasında toplanan uyarılar; arayüzde "eksikler" olarak gösterilir.
    warnings: tuple[str, ...] = field(default=())

    @property
    def media_dir(self) -> Path:
        return self.workspace / "media"

    @property
    def projects_dir(self) -> Path:
        return self.workspace / "projects"

    @property
    def jobs_dir(self) -> Path:
        return self.workspace / "jobs"

    def ensure_dirs(self) -> None:
        for path in (self.media_dir, self.projects_dir, self.jobs_dir):
            path.mkdir(parents=True, exist_ok=True)


def load_config() -> Config:
    warnings: list[str] = []

    effort = _env("OPTIMUS_EFFORT", "high").lower()
    if effort not in VALID_EFFORTS:
        warnings.append(
            f"OPTIMUS_EFFORT='{effort}' geçersiz, 'high' kullanılıyor "
            f"(geçerli: {', '.join(VALID_EFFORTS)})."
        )
        effort = "high"

    if not _env("ANTHROPIC_API_KEY"):
        warnings.append(
            "ANTHROPIC_API_KEY tanımlı değil. Sohbet, kod yazma ve video "
            "planlama çalışmaz. `ant auth login` ile giriş yaptıysan bu uyarıyı "
            "yok sayabilirsin."
        )

    image_provider = _env("IMAGE_PROVIDER", "demo").lower()
    video_provider = _env("VIDEO_PROVIDER", "demo").lower()

    segment = _int_env("VIDEO_SEGMENT_SECONDS", 5)
    if segment < 1:
        warnings.append("VIDEO_SEGMENT_SECONDS en az 1 olmalı, 5 kullanılıyor.")
        segment = 5

    workspace = Path(_env("OPTIMUS_WORKSPACE", "./workspace")).expanduser().resolve()

    return Config(
        anthropic_api_key=_env("ANTHROPIC_API_KEY"),
        model=_env("OPTIMUS_MODEL", "claude-opus-5"),
        effort=effort,
        image_provider=image_provider,
        openai_api_key=_env("OPENAI_API_KEY"),
        openai_image_model=_env("OPENAI_IMAGE_MODEL", "gpt-image-1"),
        stability_api_key=_env("STABILITY_API_KEY"),
        replicate_token=_env("REPLICATE_API_TOKEN"),
        replicate_image_model=_env(
            "REPLICATE_IMAGE_MODEL", "black-forest-labs/flux-1.1-pro"
        ),
        video_provider=video_provider,
        replicate_video_model=_env("REPLICATE_VIDEO_MODEL", "minimax/video-01"),
        luma_api_key=_env("LUMA_API_KEY"),
        video_segment_seconds=segment,
        workspace=workspace,
        ffmpeg=_env("FFMPEG_BIN", "ffmpeg"),
        ffprobe=_env("FFPROBE_BIN", "ffprobe"),
        host=_env("OPTIMUS_HOST", "127.0.0.1"),
        port=_int_env("OPTIMUS_PORT", 8000),
        warnings=tuple(warnings),
    )


@lru_cache(maxsize=1)
def get_config() -> Config:
    config = load_config()
    config.ensure_dirs()
    return config
