"""Kullanıcıya doğrudan gösterilebilecek hata tipleri."""

from __future__ import annotations


class OptimusError(Exception):
    """Tüm Optimus hatalarının atası. Mesajı kullanıcıya gösterilebilir."""


class NotConfigured(OptimusError):
    """Bir sağlayıcı için gereken API anahtarı / ayar eksik."""


class ProviderError(OptimusError):
    """Dış servis (görsel/video API'si) hata döndürdü."""


class FFmpegMissing(OptimusError):
    """ffmpeg bulunamadı; medya işleri yapılamaz."""


class FFmpegFailed(OptimusError):
    """ffmpeg çalıştı ama sıfırdan farklı bir kodla çıktı."""

    def __init__(self, message: str, *, command: list[str], stderr: str) -> None:
        super().__init__(message)
        self.command = command
        self.stderr = stderr


class InvalidTimeline(OptimusError):
    """Kurgu (timeline) tanımı geçersiz."""
