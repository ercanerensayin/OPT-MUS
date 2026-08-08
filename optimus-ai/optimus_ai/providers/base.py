"""Sağlayıcılar arasında paylaşılan yardımcılar."""

from __future__ import annotations

import base64
import time
from pathlib import Path

import httpx

from ..errors import ProviderError

#: Görsel/video üretimi dakikalar sürebilir; cömert bir zaman aşımı.
TIMEOUT = httpx.Timeout(connect=15.0, read=600.0, write=120.0, pool=15.0)


def download(url: str, dest: Path) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    try:
        with httpx.stream("GET", url, timeout=TIMEOUT, follow_redirects=True) as r:
            r.raise_for_status()
            with dest.open("wb") as handle:
                for chunk in r.iter_bytes():
                    handle.write(chunk)
    except httpx.HTTPError as exc:
        raise ProviderError(f"Dosya indirilemedi ({url}): {exc}") from exc
    return dest


def write_base64(data: str, dest: Path) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(base64.b64decode(data))
    return dest


def post_json(
    url: str, *, headers: dict[str, str], payload: dict, label: str
) -> dict:
    try:
        response = httpx.post(url, headers=headers, json=payload, timeout=TIMEOUT)
    except httpx.HTTPError as exc:
        raise ProviderError(f"{label} servisine ulaşılamadı: {exc}") from exc
    if response.status_code >= 400:
        raise ProviderError(
            f"{label} hata döndürdü ({response.status_code}): {response.text[:400]}"
        )
    return response.json()


def poll_until_done(
    url: str,
    *,
    headers: dict[str, str],
    label: str,
    status_key: str = "status",
    done_values: tuple[str, ...] = ("succeeded", "completed"),
    failed_values: tuple[str, ...] = ("failed", "canceled", "error"),
    interval: float = 3.0,
    max_wait: float = 900.0,
) -> dict:
    """İş bitene kadar yoklar. Uzun video üretiminde tek klip dakikalar sürebilir."""
    deadline = time.monotonic() + max_wait
    while True:
        try:
            response = httpx.get(url, headers=headers, timeout=TIMEOUT)
        except httpx.HTTPError as exc:
            raise ProviderError(f"{label} durumu okunamadı: {exc}") from exc
        if response.status_code >= 400:
            raise ProviderError(
                f"{label} durumu okunamadı ({response.status_code}): "
                f"{response.text[:300]}"
            )
        body = response.json()
        state = str(body.get(status_key, "")).lower()
        if state in done_values:
            return body
        if state in failed_values:
            detail = body.get("error") or body.get("failure_reason") or state
            raise ProviderError(f"{label} işi başarısız oldu: {detail}")
        if time.monotonic() > deadline:
            raise ProviderError(
                f"{label} işi {max_wait:.0f} saniyede bitmedi (son durum: {state})."
            )
        time.sleep(interval)
