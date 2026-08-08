"""Kurgu derleyicisi testleri.

Derleyici saf olduğu için ffmpeg kurulu olmadan da çalışır: ürettiği filtre
grafiğini string olarak doğruluyoruz.
"""

from __future__ import annotations

import pytest

from optimus_ai.errors import InvalidTimeline
from optimus_ai.media.timeline import Timeline, compile_timeline


@pytest.fixture()
def clips(tmp_path):
    paths = []
    for name in ("a.mp4", "b.mp4", "c.mp4"):
        path = tmp_path / name
        path.write_bytes(b"fake")
        paths.append(path)
    return paths


def build(tmp_path, sources, **overrides):
    data = {
        "output": {"path": str(tmp_path / "out.mp4"), "fps": 30},
        "clips": [
            {"src": str(sources[0]), "start": 0, "end": 4},
            {"src": str(sources[1]), "start": 0, "end": 6},
        ],
    }
    data.update(overrides)
    return compile_timeline(Timeline.parse(data), workdir=tmp_path)


def test_cuts_use_concat_filter(tmp_path, clips):
    plan = build(tmp_path, clips)
    assert "concat=n=2:v=1:a=1" in plan.filter_complex
    assert "xfade" not in plan.filter_complex
    assert plan.total_duration == pytest.approx(10.0)


def test_transition_shortens_total_duration(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[
            {"src": str(clips[0]), "start": 0, "end": 4},
            {
                "src": str(clips[1]),
                "start": 0,
                "end": 6,
                "transition": {"type": "fade", "duration": 1.0},
            },
        ],
    )
    # Geçiş süresi iki klibin üstünde çakışır: 4 + 6 - 1 = 9
    assert plan.total_duration == pytest.approx(9.0)
    assert "xfade=transition=fade:duration=1:offset=3" in plan.filter_complex
    assert "acrossfade=d=1" in plan.filter_complex


def test_speed_adjusts_duration_and_audio_tempo(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[{"src": str(clips[0]), "start": 0, "end": 10, "filters": {"speed": 2.0}}],
    )
    assert plan.total_duration == pytest.approx(5.0)
    assert "setpts=PTS/2" in plan.filter_complex
    assert "atempo=2" in plan.filter_complex


def test_extreme_speed_chains_atempo(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[{"src": str(clips[0]), "start": 0, "end": 10, "filters": {"speed": 8.0}}],
    )
    # atempo tek başına 2.0'a kadar güvenli; 8x için zincirlenmeli.
    assert plan.filter_complex.count("atempo=2") == 3


def test_color_grade_emits_eq_filter(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[
            {
                "src": str(clips[0]),
                "start": 0,
                "end": 3,
                "filters": {"brightness": 0.1, "contrast": 1.2, "saturation": 1.3},
            }
        ],
    )
    assert "eq=brightness=0.1:contrast=1.2:saturation=1.3" in plan.filter_complex


def test_drawtext_escapes_special_characters(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[
            {
                "src": str(clips[0]),
                "start": 0,
                "end": 3,
                "text": [{"content": "50%: 'iyi', süper", "start": 0, "end": 2}],
            }
        ],
    )
    assert r"\%" in plan.filter_complex
    assert r"\'" in plan.filter_complex
    assert "enable='between(t,0,2)'" in plan.filter_complex


def test_silent_clip_uses_null_audio_source(tmp_path, clips):
    plan = build(
        tmp_path,
        clips,
        clips=[{"src": str(clips[0]), "start": 0, "end": 3, "has_audio": False}],
    )
    assert "anullsrc" in " ".join(plan.args)
    # Tek klip + müzik yok → sessizlik girdisi 1. sırada.
    assert "[1:a]atrim=start=0:end=3" in plan.filter_complex


def test_music_with_ducking_uses_sidechain(tmp_path, clips):
    music = tmp_path / "m.mp3"
    music.write_bytes(b"fake")
    plan = build(
        tmp_path,
        clips,
        audio={"music": str(music), "music_volume": 0.3, "ducking": True},
    )
    assert "sidechaincompress" in plan.filter_complex
    assert "volume=0.3" in plan.filter_complex
    assert "amix=inputs=2" in plan.filter_complex


def test_preset_sets_canvas_size(tmp_path, clips):
    plan = build(tmp_path, clips, output={"path": str(tmp_path / "o.mp4"), "preset": "shorts"})
    assert "scale=1080:1920" in plan.filter_complex


def test_missing_file_is_reported_clearly(tmp_path, clips):
    with pytest.raises(InvalidTimeline, match="Klip bulunamadı"):
        build(tmp_path, clips, clips=[{"src": "yok.mp4", "start": 0, "end": 3}])


def test_transition_requires_known_clip_length(tmp_path, clips):
    with pytest.raises(InvalidTimeline, match="'end' verilmeli"):
        build(
            tmp_path,
            clips,
            clips=[
                {"src": str(clips[0]), "start": 0, "end": 4},
                {
                    "src": str(clips[1]),
                    "start": 0,
                    "transition": {"type": "fade", "duration": 1.0},
                },
            ],
        )


def test_invalid_transition_name_is_rejected(tmp_path, clips):
    with pytest.raises(InvalidTimeline, match="desteklenmiyor"):
        build(
            tmp_path,
            clips,
            clips=[
                {"src": str(clips[0]), "start": 0, "end": 4},
                {
                    "src": str(clips[1]),
                    "start": 0,
                    "end": 4,
                    "transition": {"type": "warp-speed", "duration": 1},
                },
            ],
        )


def test_out_point_before_in_point_is_rejected(tmp_path, clips):
    with pytest.raises(InvalidTimeline, match="sonra olmalı"):
        build(tmp_path, clips, clips=[{"src": str(clips[0]), "start": 5, "end": 2}])


def test_encoder_args_are_present(tmp_path, clips):
    plan = build(tmp_path, clips)
    args = plan.args
    assert "-c:v" in args and "libx264" in args
    assert "-pix_fmt" in args and "yuv420p" in args
    assert args[-1].endswith("out.mp4")
