"""Video klip üretimi.

Önemli: bugünkü video modellerinin hiçbiri tek çağrıda dakikalarca video
üretmiyor — tipik sınır 5-10 saniye. "Sınırsız süre" bu katmanda değil,
`skills/longvideo.py` içinde çözülüyor: sahnelere böl, tek tek üret, son
kareyi bir sonrakine başlangıç görseli olarak ver, hepsini birleştir.
"""

from __future__ import annotations

import hashlib
from pathlib import Path

from ..config import Config, get_config
from ..errors import NotConfigured, ProviderError
from .base import download, poll_until_done, post_json


def generate_clip(
    prompt: str,
    dest: Path,
    *,
    seconds: float,
    width: int = 1280,
    height: int = 720,
    fps: int = 24,
    init_image: Path | None = None,
    config: Config | None = None,
) -> Path:
    """Tek bir kısa klip üretir.

    `init_image` verilirse klip o kareden devam eder (görsel süreklilik).
    """
    config = config or get_config()
    provider = config.video_provider

    if provider == "demo":
        return _demo(prompt, dest, seconds, width, height, fps)
    if provider == "replicate":
        return _replicate(prompt, dest, seconds, init_image, config)
    if provider == "luma":
        return _luma(prompt, dest, init_image, config)
    raise NotConfigured(
        f"VIDEO_PROVIDER='{provider}' bilinmiyor. "
        "Seçenekler: demo, replicate, luma."
    )


def _demo(
    prompt: str, dest: Path, seconds: float, width: int, height: int, fps: int
) -> Path:
    """API anahtarı olmadan yerel placeholder klip.

    Uzun video hattını (planlama → parça üretimi → birleştirme → kurgu)
    tek kuruş harcamadan uçtan uca denemek için.
    """
    from ..media.ffmpeg import make_color_clip

    digest = hashlib.sha256(prompt.encode()).hexdigest()
    return make_color_clip(
        dest,
        seconds=seconds,
        width=width,
        height=height,
        fps=fps,
        color=f"0x{digest[:6]}",
        label=prompt[:70],
    )


def _replicate(
    prompt: str,
    dest: Path,
    seconds: float,
    init_image: Path | None,
    config: Config,
) -> Path:
    if not config.replicate_token:
        raise NotConfigured(
            "REPLICATE_API_TOKEN tanımlı değil (VIDEO_PROVIDER=replicate)."
        )
    headers = {
        "Authorization": f"Bearer {config.replicate_token}",
        "Content-Type": "application/json",
    }
    payload: dict = {"input": {"prompt": prompt}}
    if init_image is not None and init_image.exists():
        payload["input"]["first_frame_image"] = _data_uri(init_image)

    created = post_json(
        f"https://api.replicate.com/v1/models/{config.replicate_video_model}/predictions",
        headers=headers,
        payload=payload,
        label="Replicate video",
    )
    poll_url = created.get("urls", {}).get("get")
    if not poll_url:
        raise ProviderError("Replicate yoklama adresi döndürmedi.")
    done = poll_until_done(
        poll_url, headers=headers, label="Replicate video", max_wait=1800
    )
    output = done.get("output")
    url = output[0] if isinstance(output, list) and output else output
    if not isinstance(url, str):
        raise ProviderError("Replicate video bağlantısı döndürmedi.")
    return download(url, dest)


def _luma(
    prompt: str, dest: Path, init_image: Path | None, config: Config
) -> Path:
    if not config.luma_api_key:
        raise NotConfigured("LUMA_API_KEY tanımlı değil (VIDEO_PROVIDER=luma).")
    headers = {
        "Authorization": f"Bearer {config.luma_api_key}",
        "Content-Type": "application/json",
    }
    payload: dict = {"prompt": prompt, "model": "ray-2"}
    if init_image is not None and init_image.exists():
        payload["keyframes"] = {
            "frame0": {"type": "image", "url": _data_uri(init_image)}
        }

    created = post_json(
        "https://api.lumalabs.ai/dream-machine/v1/generations",
        headers=headers,
        payload=payload,
        label="Luma",
    )
    generation_id = created.get("id")
    if not generation_id:
        raise ProviderError("Luma iş kimliği döndürmedi.")
    done = poll_until_done(
        f"https://api.lumalabs.ai/dream-machine/v1/generations/{generation_id}",
        headers=headers,
        label="Luma",
        status_key="state",
        done_values=("completed",),
        failed_values=("failed",),
        max_wait=1800,
    )
    url = (done.get("assets") or {}).get("video")
    if not url:
        raise ProviderError("Luma video bağlantısı döndürmedi.")
    return download(url, dest)


def _data_uri(image: Path) -> str:
    import base64
    import mimetypes

    mime = mimetypes.guess_type(image.name)[0] or "image/png"
    encoded = base64.b64encode(image.read_bytes()).decode()
    return f"data:{mime};base64,{encoded}"
