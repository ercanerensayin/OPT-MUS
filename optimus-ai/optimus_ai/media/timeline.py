"""Kurgu derleyicisi: JSON timeline → ffmpeg filter graph.

Bu modül projenin "Adobe" tarafı. Bir NLE'de fare ile yaptığın işleri
(kesme, geçiş, hız, renk düzeltme, LUT, yazı, müzik, ducking, dışa aktarma
şablonu) veri olarak tanımlayıp tek bir ffmpeg komutuna derler.

Derleyici saf: ffmpeg kurulu olmasa da çalışır ve komutu üretir. Böylece
filtre grafiği testlerle doğrulanabilir.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field, ValidationError, model_validator

from ..errors import InvalidTimeline
from .ffmpeg import escape_drawtext

#: Sosyal medya / yayın hedeflerine göre hazır tuval boyutları.
PRESETS: dict[str, tuple[int, int]] = {
    "youtube": (1920, 1080),
    "youtube4k": (3840, 2160),
    "shorts": (1080, 1920),
    "reels": (1080, 1920),
    "tiktok": (1080, 1920),
    "square": (1080, 1080),
    "cinema": (2560, 1080),
}

#: xfade filtresinin desteklediği, sık kullanılan geçişler.
TRANSITIONS = (
    "cut", "fade", "fadeblack", "fadewhite", "dissolve",
    "wipeleft", "wiperight", "wipeup", "wipedown",
    "slideleft", "slideright", "slideup", "slidedown",
    "circleopen", "circleclose", "radial", "smoothleft", "smoothright",
)


class TextOverlay(BaseModel):
    content: str
    x: str = "(w-text_w)/2"
    y: str = "h*0.82"
    size: int = 48
    color: str = "white"
    start: float = 0.0
    end: float | None = None
    box: bool = True
    box_color: str = "black@0.45"


class ClipFilters(BaseModel):
    speed: float = Field(default=1.0, gt=0.0, le=100.0)
    brightness: float = Field(default=0.0, ge=-1.0, le=1.0)
    contrast: float = Field(default=1.0, ge=0.0, le=3.0)
    saturation: float = Field(default=1.0, ge=0.0, le=3.0)
    gamma: float = Field(default=1.0, ge=0.1, le=10.0)
    blur: float = Field(default=0.0, ge=0.0, le=50.0)
    vignette: bool = False
    lut: str | None = Field(default=None, description=".cube LUT dosyası yolu")
    fade_in: float = Field(default=0.0, ge=0.0)
    fade_out: float = Field(default=0.0, ge=0.0)

    def is_identity(self) -> bool:
        return (
            self.speed == 1.0
            and self.brightness == 0.0
            and self.contrast == 1.0
            and self.saturation == 1.0
            and self.gamma == 1.0
            and self.blur == 0.0
            and not self.vignette
            and self.lut is None
        )


class Transition(BaseModel):
    type: str = "cut"
    duration: float = Field(default=0.0, ge=0.0, le=10.0)

    @model_validator(mode="after")
    def _check_type(self) -> Transition:
        if self.type not in TRANSITIONS:
            raise ValueError(
                f"'{self.type}' geçişi desteklenmiyor. Seçenekler: "
                + ", ".join(TRANSITIONS)
            )
        if self.type == "cut":
            self.duration = 0.0
        return self


class Clip(BaseModel):
    src: str
    start: float = Field(default=0.0, ge=0.0, description="Kaynakta giriş noktası")
    end: float | None = Field(default=None, description="Kaynakta çıkış noktası")
    transition: Transition = Field(default_factory=Transition)
    filters: ClipFilters = Field(default_factory=ClipFilters)
    text: list[TextOverlay] = Field(default_factory=list)
    volume: float = Field(default=1.0, ge=0.0, le=8.0)
    mute: bool = False
    has_audio: bool = True

    @model_validator(mode="after")
    def _check_range(self) -> Clip:
        if self.end is not None and self.end <= self.start:
            raise ValueError(
                f"'{self.src}' klibinde çıkış ({self.end}s) girişten "
                f"({self.start}s) sonra olmalı."
            )
        return self

    def source_duration(self) -> float | None:
        """Kaynaktan kesilen ham süre. `end` verilmemişse bilinmiyor."""
        return None if self.end is None else self.end - self.start

    def timeline_duration(self) -> float | None:
        """Hız uygulandıktan sonra kurguda kapladığı süre."""
        raw = self.source_duration()
        return None if raw is None else raw / self.filters.speed


class AudioTrack(BaseModel):
    music: str | None = None
    music_volume: float = Field(default=0.25, ge=0.0, le=4.0)
    ducking: bool = Field(
        default=False,
        description="Klip sesi varken müziği otomatik kıs (sidechain kompresör)",
    )
    fade_in: float = Field(default=0.0, ge=0.0)
    fade_out: float = Field(default=0.0, ge=0.0)


class Output(BaseModel):
    path: str = "output.mp4"
    preset: str | None = None
    width: int = Field(default=1920, ge=16, le=7680)
    height: int = Field(default=1080, ge=16, le=4320)
    fps: int = Field(default=30, ge=1, le=120)
    crf: int = Field(default=20, ge=0, le=51)
    encode_preset: Literal[
        "ultrafast", "veryfast", "faster", "fast", "medium", "slow", "slower"
    ] = "medium"
    background: str = "black"

    @model_validator(mode="after")
    def _apply_preset(self) -> Output:
        if self.preset:
            key = self.preset.lower()
            if key not in PRESETS:
                raise ValueError(
                    f"'{self.preset}' şablonu yok. Seçenekler: "
                    + ", ".join(sorted(PRESETS))
                )
            self.width, self.height = PRESETS[key]
        return self


class Timeline(BaseModel):
    output: Output = Field(default_factory=Output)
    clips: list[Clip] = Field(min_length=1)
    audio: AudioTrack = Field(default_factory=AudioTrack)

    @classmethod
    def parse(cls, data: dict) -> Timeline:
        try:
            return cls.model_validate(data)
        except ValidationError as exc:
            raise InvalidTimeline(_format_validation_error(exc)) from exc


def _format_validation_error(exc: ValidationError) -> str:
    lines = ["Kurgu tanımı geçersiz:"]
    for error in exc.errors():
        where = ".".join(str(part) for part in error["loc"]) or "(kök)"
        lines.append(f"  - {where}: {error['msg']}")
    return "\n".join(lines)


@dataclass(frozen=True)
class RenderPlan:
    """Derlenmiş ffmpeg çağrısı."""

    args: list[str]
    filter_complex: str
    output_path: Path
    total_duration: float | None

    def command_preview(self) -> str:
        return "ffmpeg " + " ".join(
            arg if " " not in arg else f'"{arg}"' for arg in self.args
        )


def _atempo_chain(speed: float) -> list[str]:
    """atempo tek seferde 0.5–2.0 aralığını güvenle işler; dışını zincirle."""
    if speed == 1.0:
        return []
    chain: list[str] = []
    remaining = speed
    while remaining > 2.0:
        chain.append("atempo=2.0")
        remaining /= 2.0
    while remaining < 0.5:
        chain.append("atempo=0.5")
        remaining /= 0.5
    chain.append(f"atempo={remaining:.6g}")
    return chain


def _video_chain(clip: Clip, out: Output, index: int, label: str) -> str:
    steps: list[str] = []

    trim = f"trim=start={clip.start:.6g}"
    if clip.end is not None:
        trim += f":end={clip.end:.6g}"
    steps.append(trim)
    steps.append("setpts=PTS-STARTPTS")

    if clip.filters.speed != 1.0:
        steps.append(f"setpts=PTS/{clip.filters.speed:.6g}")

    steps.append(f"fps={out.fps}")
    steps.append(
        f"scale={out.width}:{out.height}:force_original_aspect_ratio=decrease"
    )
    steps.append(
        f"pad={out.width}:{out.height}:(ow-iw)/2:(oh-ih)/2:color={out.background}"
    )
    steps.append("setsar=1")

    f = clip.filters
    if (f.brightness, f.contrast, f.saturation, f.gamma) != (0.0, 1.0, 1.0, 1.0):
        steps.append(
            f"eq=brightness={f.brightness:.6g}:contrast={f.contrast:.6g}"
            f":saturation={f.saturation:.6g}:gamma={f.gamma:.6g}"
        )
    if f.blur > 0:
        steps.append(f"gblur=sigma={f.blur:.6g}")
    if f.vignette:
        steps.append("vignette")
    if f.lut:
        lut_path = str(f.lut).replace("\\", "/").replace(":", "\\:")
        steps.append(f"lut3d=file='{lut_path}'")

    for overlay in clip.text:
        steps.append(_drawtext(overlay))

    duration = clip.timeline_duration()
    if f.fade_in > 0:
        steps.append(f"fade=t=in:st=0:d={f.fade_in:.6g}")
    if f.fade_out > 0:
        if duration is None:
            raise InvalidTimeline(
                f"{index}. klipte fade_out var ama 'end' verilmemiş. "
                "Çıkış kararması için klip süresi bilinmeli."
            )
        steps.append(f"fade=t=out:st={max(0.0, duration - f.fade_out):.6g}"
                     f":d={f.fade_out:.6g}")

    steps.append("format=yuv420p")
    return f"[{index}:v]" + ",".join(steps) + f"[{label}]"


def _drawtext(overlay: TextOverlay) -> str:
    parts = [
        f"text='{escape_drawtext(overlay.content)}'",
        f"fontsize={overlay.size}",
        f"fontcolor={overlay.color}",
        f"x={overlay.x}",
        f"y={overlay.y}",
    ]
    if overlay.box:
        parts += [f"box=1", f"boxcolor={overlay.box_color}", "boxborderw=16"]
    if overlay.start > 0 or overlay.end is not None:
        end = overlay.end if overlay.end is not None else 1e9
        parts.append(f"enable='between(t,{overlay.start:.6g},{end:.6g})'")
    return "drawtext=" + ":".join(parts)


def _audio_chain(clip: Clip, index: int, silence_index: int, label: str) -> str:
    use_silence = clip.mute or not clip.has_audio
    source = silence_index if use_silence else index

    duration = clip.timeline_duration()
    steps: list[str] = []
    if use_silence:
        # Sessizlik kaynağı tek ve uzun; her klip için baştan kendi payını keser.
        length = duration if duration is not None else 3600.0
        steps.append(f"atrim=start=0:end={length:.6g}")
    else:
        trim = f"atrim=start={clip.start:.6g}"
        if clip.end is not None:
            trim += f":end={clip.end:.6g}"
        steps.append(trim)

    steps.append("asetpts=PTS-STARTPTS")
    if not use_silence:
        steps.extend(_atempo_chain(clip.filters.speed))
        if clip.volume != 1.0:
            steps.append(f"volume={clip.volume:.6g}")
    steps.append("aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo")
    return f"[{source}:a]" + ",".join(steps) + f"[{label}]"


def compile_timeline(timeline: Timeline, *, workdir: Path | None = None) -> RenderPlan:
    """Timeline'ı çalıştırılabilir bir ffmpeg argüman listesine çevirir."""
    out = timeline.output
    clips = timeline.clips
    base = workdir or Path.cwd()

    for clip in clips:
        path = Path(clip.src)
        if not path.is_absolute():
            path = base / path
        if not path.exists():
            raise InvalidTimeline(f"Klip bulunamadı: {clip.src}")

    inputs: list[str] = []
    for clip in clips:
        path = Path(clip.src)
        inputs += ["-i", str(path if path.is_absolute() else base / path)]

    music_index: int | None = None
    if timeline.audio.music:
        music_path = Path(timeline.audio.music)
        if not music_path.is_absolute():
            music_path = base / music_path
        if not music_path.exists():
            raise InvalidTimeline(f"Müzik dosyası bulunamadı: {timeline.audio.music}")
        music_index = len(clips)
        inputs += ["-i", str(music_path)]

    silence_index = len(clips) + (1 if music_index is not None else 0)
    inputs += [
        "-f", "lavfi",
        "-i", "anullsrc=channel_layout=stereo:sample_rate=48000",
    ]

    graph: list[str] = []
    for i, clip in enumerate(clips):
        graph.append(_video_chain(clip, out, i, f"v{i}"))
        graph.append(_audio_chain(clip, i, silence_index, f"a{i}"))

    use_xfade = any(clip.transition.duration > 0 for clip in clips[1:])
    if use_xfade:
        video_label, audio_label, total = _join_with_transitions(clips, out, graph)
    else:
        video_label, audio_label, total = _join_with_cuts(clips, graph)

    if music_index is not None:
        audio_label = _mix_music(
            timeline.audio, music_index, audio_label, total, graph
        )

    fades = []
    if timeline.audio.fade_in > 0:
        fades.append(f"afade=t=in:st=0:d={timeline.audio.fade_in:.6g}")
    if timeline.audio.fade_out > 0:
        if total is None:
            raise InvalidTimeline(
                "Ses çıkış kararması için tüm kliplerde 'end' verilmeli "
                "(toplam süre hesaplanamıyor)."
            )
        start = max(0.0, total - timeline.audio.fade_out)
        fades.append(f"afade=t=out:st={start:.6g}:d={timeline.audio.fade_out:.6g}")
    if fades:
        graph.append(f"[{audio_label}]" + ",".join(fades) + "[afinal]")
        audio_label = "afinal"

    filter_complex = ";".join(graph)
    output_path = Path(out.path)
    if not output_path.is_absolute():
        output_path = base / output_path
    output_path.parent.mkdir(parents=True, exist_ok=True)

    args = [
        *inputs,
        "-filter_complex", filter_complex,
        "-map", f"[{video_label}]",
        "-map", f"[{audio_label}]",
        "-r", str(out.fps),
        "-c:v", "libx264",
        "-preset", out.encode_preset,
        "-crf", str(out.crf),
        "-pix_fmt", "yuv420p",
        "-c:a", "aac",
        "-b:a", "192k",
        "-movflags", "+faststart",
    ]
    if total is not None:
        args += ["-t", f"{total:.6g}"]
    args.append(str(output_path))

    return RenderPlan(
        args=args,
        filter_complex=filter_complex,
        output_path=output_path,
        total_duration=total,
    )


def _join_with_cuts(
    clips: list[Clip], graph: list[str]
) -> tuple[str, str, float | None]:
    if len(clips) == 1:
        return "v0", "a0", clips[0].timeline_duration()

    streams = "".join(f"[v{i}][a{i}]" for i in range(len(clips)))
    graph.append(f"{streams}concat=n={len(clips)}:v=1:a=1[vout][aout]")

    durations = [clip.timeline_duration() for clip in clips]
    total = None if any(d is None for d in durations) else sum(durations)  # type: ignore[arg-type]
    return "vout", "aout", total


def _join_with_transitions(
    clips: list[Clip], out: Output, graph: list[str]
) -> tuple[str, str, float | None]:
    durations = [clip.timeline_duration() for clip in clips]
    missing = [i for i, d in enumerate(durations) if d is None]
    if missing:
        raise InvalidTimeline(
            "Geçiş kullanırken her klipte 'end' verilmeli — xfade, klibin tam "
            f"süresini bilmek zorunda. Eksik klip(ler): {missing}"
        )

    # Kesme (duration=0) xfade ile ifade edilemez; bir karelik erime kullanıyoruz.
    min_transition = 1.0 / out.fps
    video_label = "v0"
    audio_label = "a0"
    elapsed: float = durations[0]  # type: ignore[assignment]

    for i in range(1, len(clips)):
        transition = clips[i].transition
        kind = "fade" if transition.type == "cut" else transition.type
        seconds = max(transition.duration, min_transition)
        offset = max(0.0, elapsed - seconds)

        next_video = f"xv{i}"
        next_audio = f"xa{i}"
        graph.append(
            f"[{video_label}][v{i}]xfade=transition={kind}"
            f":duration={seconds:.6g}:offset={offset:.6g}[{next_video}]"
        )
        graph.append(
            f"[{audio_label}][a{i}]acrossfade=d={seconds:.6g}:c1=tri:c2=tri"
            f"[{next_audio}]"
        )
        video_label, audio_label = next_video, next_audio
        elapsed = elapsed + durations[i] - seconds  # type: ignore[operator]

    return video_label, audio_label, elapsed


def _mix_music(
    audio: AudioTrack,
    music_index: int,
    voice_label: str,
    total: float | None,
    graph: list[str],
) -> str:
    steps = ["aloop=loop=-1:size=2147483647"]
    if total is not None:
        steps.append(f"atrim=start=0:end={total:.6g}")
    steps.append("asetpts=PTS-STARTPTS")
    steps.append(f"volume={audio.music_volume:.6g}")
    steps.append("aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo")
    graph.append(f"[{music_index}:a]" + ",".join(steps) + "[music]")

    if audio.ducking:
        # Klip sesi yükseldikçe müziği otomatik kıs. Sidechain girdisi olarak
        # klip sesinin bir kopyasını kullanıyoruz.
        graph.append(f"[{voice_label}]asplit=2[voice][sc]")
        graph.append(
            "[music][sc]sidechaincompress=threshold=0.03:ratio=12:attack=20"
            ":release=400[ducked]"
        )
        graph.append("[voice][ducked]amix=inputs=2:duration=first:normalize=0[amix]")
    else:
        graph.append(
            f"[{voice_label}][music]amix=inputs=2:duration=first:normalize=0[amix]"
        )
    return "amix"
