"""Süre sınırı olmayan video üretimi.

Tek bir video modeli çağrısı bugün 5-10 saniye üretebiliyor. Buradaki hat
bunu şöyle aşıyor:

1. **Planla** — Claude, isteği N sahneye böler; ortak stil cümlesi ve
   karakter tarifleriyle sahneler arası tutarlılığı korur.
2. **Üret** — her sahne ayrı bir klip olarak üretilir.
3. **Zincirle** — bir klibin son karesi, bir sonraki klibin başlangıç
   görseli olarak verilir; görüntü kopmaz.
4. **Birleştir** — klipler geçişler, altyazılar ve müzikle tek videoya
   kurgulanır (`media/timeline.py`).

Toplam süreyi sınırlayan tek şey zaman ve API bütçesi; kodda üst sınır yok.
İşin durumu her adımda diske yazılır, böylece yarım kalan iş kaldığı yerden
devam edebilir.
"""

from __future__ import annotations

import json
import time
from collections.abc import Callable
from dataclasses import asdict, dataclass, field
from pathlib import Path

from ..config import Config, get_config
from ..errors import OptimusError
from ..llm import Brain, Scene, ScenePlan
from ..media import ffmpeg
from ..media.timeline import (
    PRESETS,
    AudioTrack,
    Clip,
    ClipFilters,
    Output,
    TextOverlay,
    Timeline,
    Transition,
    compile_timeline,
)

ProgressFn = Callable[[str, int, int, str], None]


@dataclass
class VideoRequest:
    prompt: str
    total_seconds: float = 60.0
    preset: str = "youtube"
    fps: int = 24
    segment_seconds: float | None = None
    transition: str = "fade"
    transition_duration: float = 0.5
    captions: bool = True
    music: str | None = None
    music_volume: float = 0.25
    ducking: bool = False

    def validate(self) -> None:
        if self.total_seconds <= 0:
            raise OptimusError("Toplam süre sıfırdan büyük olmalı.")
        if self.preset not in PRESETS:
            raise OptimusError(
                f"'{self.preset}' şablonu yok. Seçenekler: "
                + ", ".join(sorted(PRESETS))
            )


@dataclass
class JobState:
    job_id: str
    request: dict
    status: str = "pending"
    message: str = ""
    stage: str = "hazırlanıyor"
    done: int = 0
    total: int = 0
    scenes: list[dict] = field(default_factory=list)
    segments: list[str] = field(default_factory=list)
    output: str | None = None
    error: str | None = None
    started_at: float = field(default_factory=time.time)
    updated_at: float = field(default_factory=time.time)

    def to_dict(self) -> dict:
        return asdict(self)


class LongVideoJob:
    """Uzun video işini yürütür ve durumunu diskte tutar."""

    def __init__(
        self,
        job_id: str,
        request: VideoRequest,
        *,
        config: Config | None = None,
        brain: Brain | None = None,
        on_progress: ProgressFn | None = None,
    ) -> None:
        request.validate()
        self.config = config or get_config()
        self.request = request
        self.brain = brain
        self.on_progress = on_progress
        self.dir = self.config.jobs_dir / job_id
        self.segments_dir = self.dir / "segments"
        self.frames_dir = self.dir / "frames"
        self.state = JobState(job_id=job_id, request=asdict(request))

    # -- durum --------------------------------------------------------------

    @property
    def state_path(self) -> Path:
        return self.dir / "state.json"

    def save(self) -> None:
        self.state.updated_at = time.time()
        self.dir.mkdir(parents=True, exist_ok=True)
        self.state_path.write_text(
            json.dumps(self.state.to_dict(), ensure_ascii=False, indent=2),
            encoding="utf-8",
        )

    def _progress(self, stage: str, done: int, total: int, message: str) -> None:
        self.state.stage = stage
        self.state.done = done
        self.state.total = total
        self.state.message = message
        self.save()
        if self.on_progress:
            self.on_progress(stage, done, total, message)

    # -- hat ----------------------------------------------------------------

    def run(self) -> Path:
        self.state.status = "running"
        self.segments_dir.mkdir(parents=True, exist_ok=True)
        self.frames_dir.mkdir(parents=True, exist_ok=True)

        try:
            plan = self._plan()
            clips = self._render_segments(plan)
            output = self._assemble(plan, clips)
        except Exception as exc:  # kullanıcıya taşınacak, yutulmayacak
            self.state.status = "failed"
            self.state.error = str(exc)
            self.save()
            raise

        self.state.status = "done"
        self.state.output = str(output)
        self._progress("bitti", self.state.total, self.state.total, str(output))
        return output

    def _plan(self) -> ScenePlan:
        segment = self.request.segment_seconds or self.config.video_segment_seconds
        count = max(1, round(self.request.total_seconds / segment))
        self._progress(
            "planlama", 0, count, f"{count} sahnelik senaryo hazırlanıyor…"
        )

        try:
            brain = self.brain or Brain(self.config)
            plan = brain.plan_scenes(
                self.request.prompt,
                total_seconds=self.request.total_seconds,
                segment_seconds=segment,
            )
        except Exception as exc:  # noqa: BLE001 - yedek planlayıcı var, hat durmamalı
            # Claude erişimi yoksa (anahtar eksik, ağ yok) düz bölmeye düş.
            plan = _naive_plan(self.request.prompt, count, segment)
            self._progress(
                "planlama",
                0,
                count,
                f"Senaryo modelden alınamadı ({exc}); düz bölme kullanılıyor.",
            )

        if not plan.scenes:
            raise OptimusError("Senaryo boş döndü.")

        self.state.scenes = [scene.model_dump() for scene in plan.scenes]
        self.state.total = len(plan.scenes)
        self.save()
        return plan

    def _render_segments(self, plan: ScenePlan) -> list[Path]:
        from ..providers import generate_clip

        width, height = PRESETS[self.request.preset]
        total = len(plan.scenes)
        paths: list[Path] = []
        previous_frame: Path | None = None

        for scene in plan.scenes:
            target = self.segments_dir / f"{scene.index:04d}.mp4"

            if target.exists() and ffmpeg.available():
                # Yarım kalan iş: üretilmiş parçayı yeniden üretme.
                self._progress(
                    "üretim", scene.index + 1, total,
                    f"Sahne {scene.index + 1}/{total} zaten var, atlanıyor.",
                )
            else:
                self._progress(
                    "üretim", scene.index, total,
                    f"Sahne {scene.index + 1}/{total} üretiliyor…",
                )
                generate_clip(
                    f"{plan.style}. {scene.prompt}",
                    target,
                    seconds=scene.seconds,
                    width=width,
                    height=height,
                    fps=self.request.fps,
                    init_image=previous_frame,
                    config=self.config,
                )

            paths.append(target)
            self.state.segments = [str(p) for p in paths]
            self.save()

            if ffmpeg.available():
                previous_frame = ffmpeg.extract_last_frame(
                    target, self.frames_dir / f"{scene.index:04d}.png"
                )

        return paths

    def _assemble(self, plan: ScenePlan, paths: list[Path]) -> Path:
        self._progress(
            "kurgu", len(paths), len(paths), "Klipler tek videoya birleştiriliyor…"
        )
        width, height = PRESETS[self.request.preset]
        output_path = self.dir / "final.mp4"

        # Bazı ffmpeg derlemelerinde drawtext yok; altyazıyı sessizce düşür.
        captions = self.request.captions and ffmpeg.has_filter("drawtext")
        if self.request.captions and not captions:
            self.state.message = (
                "Altyazı atlandı: bu ffmpeg derlemesinde 'drawtext' filtresi yok."
            )

        clips: list[Clip] = []
        for index, (scene, path) in enumerate(zip(plan.scenes, paths, strict=False)):
            duration = ffmpeg.duration_seconds(path)
            overlays: list[TextOverlay] = []
            if captions and scene.caption:
                overlays.append(
                    TextOverlay(
                        content=scene.caption,
                        size=max(24, height // 22),
                        start=0.2,
                        end=max(0.4, duration - 0.2),
                    )
                )
            clips.append(
                Clip(
                    src=str(path),
                    start=0.0,
                    end=duration,
                    transition=Transition(
                        type="cut" if index == 0 else self.request.transition,
                        duration=0.0
                        if index == 0
                        else min(
                            self.request.transition_duration,
                            max(0.0, duration / 2 - 0.05),
                        ),
                    ),
                    filters=ClipFilters(
                        fade_in=0.4 if index == 0 else 0.0,
                        fade_out=0.6 if index == len(paths) - 1 else 0.0,
                    ),
                    text=overlays,
                    has_audio=_has_audio(path),
                )
            )

        timeline = Timeline(
            output=Output(
                path=str(output_path),
                preset=self.request.preset,
                fps=self.request.fps,
            ),
            clips=clips,
            audio=AudioTrack(
                music=self.request.music,
                music_volume=self.request.music_volume,
                ducking=self.request.ducking,
                fade_in=1.0 if self.request.music else 0.0,
                fade_out=1.5 if self.request.music else 0.0,
            ),
        )

        plan_obj = compile_timeline(timeline, workdir=self.dir)
        ffmpeg.run(plan_obj.args, timeout=7200)
        return plan_obj.output_path


def _has_audio(path: Path) -> bool:
    try:
        info = ffmpeg.probe(path)
    except OptimusError:
        return False
    return any(
        stream.get("codec_type") == "audio" for stream in info.get("streams", [])
    )


def _naive_plan(prompt: str, count: int, segment: float) -> ScenePlan:
    """Claude yokken kullanılan yedek planlayıcı."""
    return ScenePlan(
        title=prompt[:60],
        style="cinematic, consistent color palette, steady camera",
        scenes=[
            Scene(
                index=i,
                prompt=f"{prompt} — part {i + 1} of {count}",
                caption="",
                seconds=segment,
            )
            for i in range(count)
        ],
    )


def load_state(job_id: str, config: Config | None = None) -> dict | None:
    config = config or get_config()
    path = config.jobs_dir / job_id / "state.json"
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))
