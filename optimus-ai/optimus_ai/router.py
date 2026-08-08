"""İstek yönlendirme: kullanıcı ne istiyor?

Anahtar kelimeye dayalı, deterministik ve testlenebilir bir sınıflandırıcı.
Emin olamazsa `chat` döner — sohbet her isteği karşılayabilecek tek yetenek,
yanlış yönlendirmenin en ucuz olduğu yer orası.
"""

from __future__ import annotations

import re
import unicodedata
from dataclasses import dataclass

Intent = str

INTENTS: tuple[Intent, ...] = ("chat", "code", "app", "game", "image", "video", "edit")

#: Her niyet için tetikleyici kalıplar. Türkçe ekler için gövde eşleşmesi
#: yeterli (ör. "oyun" → "oyunu", "oyununu").
KEYWORDS: dict[Intent, tuple[str, ...]] = {
    "game": ("oyun", "game", "platformer", "yilan oyunu", "snake", "tetris", "arcade"),
    "video": (
        "video", "klip", "film", "reels", "shorts", "tiktok", "animasyon",
        "reklam filmi", "tanitim filmi",
    ),
    "edit": (
        "kurgu", "montaj", "edit", "kes ", "kesme", "gecis", "altyazi",
        "renk duzelt", "lut", "birlestir", "premiere", "after effects",
        "seslendirme", "muzik ekle",
    ),
    "image": (
        "gorsel", "resim", "fotograf", "image", "afis", "poster", "logo",
        "illustrasyon", "kapak",
    ),
    "app": (
        "uygulama", "app", "web sitesi", "website", "site yap", "dashboard",
        "arayuz", "api yaz", "backend", "frontend",
    ),
    "code": (
        "kod", "code", "fonksiyon", "script", "algoritma", "hata", "debug",
        "refactor", "test yaz", "sql", "regex",
    ),
}

#: Aynı metin birden çok niyeti tetiklerse bu sıra kazanır. Daha spesifik
#: olan üstte: "oyunun tanıtım videosu" isteği video değil oyun değil —
#: video, çünkü asıl çıktı video.
PRIORITY: tuple[Intent, ...] = ("edit", "video", "game", "image", "app", "code")


@dataclass(frozen=True)
class Route:
    intent: Intent
    matched: tuple[str, ...]

    @property
    def is_fallback(self) -> bool:
        return self.intent == "chat"


def normalize(text: str) -> str:
    """Türkçe karakterleri sadeleştirir ki 'görsel' ~ 'gorsel' eşleşsin."""
    lowered = text.casefold()
    lowered = lowered.replace("ı", "i").replace("İ", "i")
    decomposed = unicodedata.normalize("NFD", lowered)
    stripped = "".join(ch for ch in decomposed if not unicodedata.combining(ch))
    return re.sub(r"\s+", " ", stripped)


def _matches(keyword: str, haystack: str) -> bool:
    """Kelime başından eşleşir; Türkçe ekler serbest.

    Sözcük ortasında eşleşmeyi engelliyoruz: aksi hâlde "kod" → "makrokodu"
    gibi yanlış tetiklemeler oluyor. Sonek serbest bırakılıyor ki "oyun"
    kelimesi "oyununu" içinde de yakalansın.
    """
    pattern = r"(?<![a-z0-9])" + re.escape(normalize(keyword).strip())
    return re.search(pattern, haystack) is not None


def route(text: str) -> Route:
    """Metni bir niyete yönlendirir."""
    haystack = normalize(text)
    hits: dict[Intent, list[str]] = {}

    for intent, keywords in KEYWORDS.items():
        found = [word for word in keywords if _matches(word, haystack)]
        if found:
            hits[intent] = found

    for intent in PRIORITY:
        if intent in hits:
            return Route(intent=intent, matched=tuple(hits[intent]))

    return Route(intent="chat", matched=())
