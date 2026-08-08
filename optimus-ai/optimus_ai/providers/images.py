"""Görsel üretimi. Sağlayıcı .env içindeki IMAGE_PROVIDER ile seçilir."""

from __future__ import annotations

import hashlib
from pathlib import Path

from ..config import Config, get_config
from ..errors import NotConfigured, ProviderError
from .base import download, post_json, write_base64

SIZES = {
    "square": (1024, 1024),
    "landscape": (1536, 1024),
    "portrait": (1024, 1536),
}


def generate_image(
    prompt: str,
    dest: Path,
    *,
    shape: str = "square",
    config: Config | None = None,
) -> Path:
    """Prompt'tan görsel üretip `dest` yoluna yazar."""
    config = config or get_config()
    width, height = SIZES.get(shape, SIZES["square"])
    provider = config.image_provider

    if provider == "demo":
        return _demo(prompt, dest, width, height)
    if provider == "openai":
        return _openai(prompt, dest, width, height, config)
    if provider == "stability":
        return _stability(prompt, dest, config)
    if provider == "replicate":
        return _replicate(prompt, dest, width, height, config)
    raise NotConfigured(
        f"IMAGE_PROVIDER='{provider}' bilinmiyor. "
        "Seçenekler: demo, openai, stability, replicate."
    )


def _demo(prompt: str, dest: Path, width: int, height: int) -> Path:
    """Anahtarsız yerel görsel: prompt'tan türetilmiş renkli bir kart."""
    from ..media.ffmpeg import make_color_clip  # gecikmeli import: ffmpeg opsiyonel

    digest = hashlib.sha256(prompt.encode()).hexdigest()
    color = f"0x{digest[:6]}"
    temp = dest.with_suffix(".demo.mp4")
    make_color_clip(
        temp,
        seconds=0.2,
        width=width,
        height=height,
        fps=5,
        color=color,
        label=prompt[:60],
    )
    from ..media.ffmpeg import run

    run(["-i", str(temp), "-frames:v", "1", str(dest)])
    temp.unlink(missing_ok=True)
    return dest


def _openai(prompt: str, dest: Path, width: int, height: int, config: Config) -> Path:
    if not config.openai_api_key:
        raise NotConfigured("OPENAI_API_KEY tanımlı değil (IMAGE_PROVIDER=openai).")
    body = post_json(
        "https://api.openai.com/v1/images/generations",
        headers={"Authorization": f"Bearer {config.openai_api_key}"},
        payload={
            "model": config.openai_image_model,
            "prompt": prompt,
            "size": f"{width}x{height}",
            "n": 1,
        },
        label="OpenAI Images",
    )
    item = (body.get("data") or [{}])[0]
    if item.get("b64_json"):
        return write_base64(item["b64_json"], dest)
    if item.get("url"):
        return download(item["url"], dest)
    raise ProviderError("OpenAI görsel döndürmedi.")


def _stability(prompt: str, dest: Path, config: Config) -> Path:
    if not config.stability_api_key:
        raise NotConfigured(
            "STABILITY_API_KEY tanımlı değil (IMAGE_PROVIDER=stability)."
        )
    import httpx

    from .base import TIMEOUT

    try:
        response = httpx.post(
            "https://api.stability.ai/v2beta/stable-image/generate/core",
            headers={
                "Authorization": f"Bearer {config.stability_api_key}",
                "Accept": "image/*",
            },
            files={"none": ""},
            data={"prompt": prompt, "output_format": "png"},
            timeout=TIMEOUT,
        )
    except httpx.HTTPError as exc:
        raise ProviderError(f"Stability servisine ulaşılamadı: {exc}") from exc
    if response.status_code >= 400:
        raise ProviderError(
            f"Stability hata döndürdü ({response.status_code}): {response.text[:300]}"
        )
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(response.content)
    return dest


def _replicate(
    prompt: str, dest: Path, width: int, height: int, config: Config
) -> Path:
    if not config.replicate_token:
        raise NotConfigured(
            "REPLICATE_API_TOKEN tanımlı değil (IMAGE_PROVIDER=replicate)."
        )
    from .base import poll_until_done

    headers = {
        "Authorization": f"Bearer {config.replicate_token}",
        "Content-Type": "application/json",
    }
    created = post_json(
        f"https://api.replicate.com/v1/models/{config.replicate_image_model}/predictions",
        headers=headers,
        payload={"input": {"prompt": prompt, "width": width, "height": height}},
        label="Replicate",
    )
    poll_url = created.get("urls", {}).get("get")
    if not poll_url:
        raise ProviderError("Replicate yoklama adresi döndürmedi.")
    done = poll_until_done(poll_url, headers=headers, label="Replicate")
    output = done.get("output")
    url = output[0] if isinstance(output, list) and output else output
    if not isinstance(url, str):
        raise ProviderError("Replicate görsel bağlantısı döndürmedi.")
    return download(url, dest)
