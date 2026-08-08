"""İstek yönlendirme testleri."""

from __future__ import annotations

import pytest

from optimus_ai.router import normalize, route


@pytest.mark.parametrize(
    ("text", "expected"),
    [
        ("Bana bir yılan oyunu yap", "game"),
        ("Snake game yaz", "game"),
        ("2 dakikalık tanıtım videosu üret", "video"),
        ("TikTok için dikey klip", "video"),
        ("Şu iki klibi birleştir, araya geçiş koy", "edit"),
        ("Videoya altyazı ekle", "edit"),
        ("Gün batımı görseli üret", "image"),
        ("Bir poster tasarla", "image"),
        ("Todo uygulaması yap", "app"),
        ("Şu fonksiyondaki hatayı bul", "code"),
        ("Merhaba, nasılsın?", "chat"),
        ("Fransa'nın başkenti neresi", "chat"),
    ],
)
def test_routing(text, expected):
    assert route(text).intent == expected


def test_turkish_characters_are_normalized():
    assert normalize("Görsel Üretimi") == "gorsel uretimi"
    assert route("GÖRSEL üret").intent == "image"


def test_edit_wins_over_video_when_both_match():
    # Elde klip varsa asıl iş kurgu; "video" kelimesi geçmesi bunu değiştirmez.
    result = route("videoyu kes ve montajla")
    assert result.intent == "edit"


def test_unmatched_text_falls_back_to_chat():
    result = route("bugün hava nasıl")
    assert result.intent == "chat"
    assert result.is_fallback
    assert result.matched == ()
