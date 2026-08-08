"""ffmpeg yardımcıları ve proje yazımı için testler."""

from __future__ import annotations

import pytest

from optimus_ai.errors import OptimusError
from optimus_ai.media import ffmpeg
from optimus_ai.skills.builder import safe_join, slugify

FFMPEG_LOG = """\
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'klip.mp4':
  Duration: 00:01:23.45, start: 0.000000, bitrate: 1200 kb/s
  Stream #0:0[0x1](und): Video: h264 (High), yuv420p, 1920x1080, 30 fps
  Stream #0:1[0x2](und): Audio: aac (LC), 48000 Hz, stereo, fltp, 128 kb/s
At least one output file must be specified
"""

SILENT_LOG = """\
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'sessiz.mp4':
  Duration: 00:00:05.00, start: 0.000000, bitrate: 800 kb/s
  Stream #0:0[0x1](und): Video: h264 (High), yuv420p, 1280x720, 24 fps
At least one output file must be specified
"""


def test_ffmpeg_fallback_probe_reads_duration(monkeypatch, tmp_path):
    """ffprobe yoksa süre ffmpeg logundan çıkarılabilmeli."""
    monkeypatch.setattr(ffmpeg, "_identify", lambda path: FFMPEG_LOG)
    info = ffmpeg._probe_with_ffmpeg(tmp_path / "klip.mp4")
    assert float(info["format"]["duration"]) == pytest.approx(83.45)


def test_ffmpeg_fallback_probe_lists_stream_types(monkeypatch, tmp_path):
    monkeypatch.setattr(ffmpeg, "_identify", lambda path: FFMPEG_LOG)
    kinds = [s["codec_type"] for s in ffmpeg._probe_with_ffmpeg(tmp_path / "k.mp4")["streams"]]
    assert kinds == ["video", "audio"]


def test_ffmpeg_fallback_probe_detects_missing_audio(monkeypatch, tmp_path):
    monkeypatch.setattr(ffmpeg, "_identify", lambda path: SILENT_LOG)
    kinds = [s["codec_type"] for s in ffmpeg._probe_with_ffmpeg(tmp_path / "s.mp4")["streams"]]
    assert kinds == ["video"]


@pytest.mark.parametrize(
    ("raw", "expected_fragment"),
    [
        ("50%", r"\%"),
        ("'tek tırnak'", r"\'"),
        ("saat 12:30", r"\:"),
        (r"ters\bölü", r"\\"),
        ("iki\nsatır", "iki satır"),
    ],
)
def test_drawtext_escaping(raw, expected_fragment):
    assert expected_fragment in ffmpeg.escape_drawtext(raw)


def test_drawtext_escapes_backslash_before_other_characters():
    # Ters bölü önce kaçışlanmazsa, sonradan eklediğimiz kaçışları bozar.
    assert ffmpeg.escape_drawtext("a:b") == r"a\:b"
    assert ffmpeg.escape_drawtext("\\") == "\\\\"


@pytest.mark.parametrize(
    ("name", "expected"),
    [
        ("Yılan Oyunu", "y-lan-oyunu"),
        ("  Todo App!! ", "todo-app"),
        ("///", "proje"),
    ],
)
def test_slugify(name, expected):
    assert slugify(name) == expected


@pytest.mark.parametrize(
    "path",
    ["../kacis.txt", "/etc/passwd", "a/../../b.txt", ".", ""],
)
def test_safe_join_rejects_escapes(tmp_path, path):
    with pytest.raises(OptimusError, match="Güvenli olmayan"):
        safe_join(tmp_path, path)


def test_safe_join_allows_nested_paths(tmp_path):
    target = safe_join(tmp_path, "src/game/main.js")
    assert target.parent.name == "game"
    assert tmp_path.resolve() in target.parents
