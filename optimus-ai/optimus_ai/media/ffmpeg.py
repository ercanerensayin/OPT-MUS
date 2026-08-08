"""ffmpeg / ffprobe sarmalayıcısı.

ffmpeg kurulu değilse net bir hata veriyoruz — sessizce yanlış çıktı üretmek
yerine ne kurulması gerektiğini söylemek daha iyi.
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
from functools import lru_cache
from pathlib import Path

from ..config import get_config
from ..errors import FFmpegFailed, FFmpegMissing

INSTALL_HINT = (
    "ffmpeg bulunamadı. Kurulum:\n"
    "  Debian/Ubuntu : sudo apt install ffmpeg\n"
    "  macOS         : brew install ffmpeg\n"
    "  Windows       : winget install Gyan.FFmpeg\n"
    "Farklı bir yoldaysa .env içinde FFMPEG_BIN ile göster."
)


def ffmpeg_path() -> str:
    binary = get_config().ffmpeg
    resolved = shutil.which(binary)
    if not resolved:
        raise FFmpegMissing(INSTALL_HINT)
    return resolved


def ffprobe_path() -> str:
    binary = get_config().ffprobe
    resolved = shutil.which(binary)
    if not resolved:
        raise FFmpegMissing(INSTALL_HINT.replace("ffmpeg", "ffprobe", 1))
    return resolved


def available() -> bool:
    """ffmpeg kullanılabilir mi? Arayüzde yetenek göstermek için."""
    try:
        ffmpeg_path()
    except FFmpegMissing:
        return False
    return True


@lru_cache(maxsize=None)
def has_filter(name: str) -> bool:
    """Bu ffmpeg derlemesi verilen filtreyi içeriyor mu?

    Bazı statik derlemeler libfreetype'sız gelir ve `drawtext` bulunmaz;
    yazı bindirmeyi denemeden önce sormak, ortada çöken bir render'dan iyi.
    """
    try:
        binary = ffmpeg_path()
    except FFmpegMissing:
        return False
    process = subprocess.run(  # noqa: S603
        [binary, "-hide_banner", "-filters"],
        capture_output=True,
        text=True,
        timeout=60,
    )
    return re.search(rf"^\s*\S+\s+{re.escape(name)}\s", process.stdout, re.M) is not None


def run(args: list[str], *, timeout: int = 3600) -> str:
    """ffmpeg'i çalıştırır ve stderr'ı döndürür (ffmpeg logu oraya yazar)."""
    command = [ffmpeg_path(), "-hide_banner", "-loglevel", "error", "-y", *args]
    process = subprocess.run(  # noqa: S603 - komut bizim ürettiğimiz argv listesi
        command,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    if process.returncode != 0:
        tail = "\n".join(process.stderr.strip().splitlines()[-15:])
        raise FFmpegFailed(
            f"ffmpeg hata verdi (kod {process.returncode}):\n{tail}",
            command=command,
            stderr=process.stderr,
        )
    return process.stderr


def has_ffprobe() -> bool:
    try:
        ffprobe_path()
    except FFmpegMissing:
        return False
    return True


def _identify(path: Path) -> str:
    """ffprobe yokken ffmpeg'in dosya hakkında yazdığı logu döndürür.

    `ffmpeg -i <dosya>` çıktı hedefi olmadan 1 ile çıkar ama akış bilgisini
    stderr'a yazar; bize gereken de o.
    """
    process = subprocess.run(  # noqa: S603
        [ffmpeg_path(), "-hide_banner", "-i", str(path)],
        capture_output=True,
        text=True,
        timeout=120,
    )
    return process.stderr


def probe(path: Path) -> dict:
    """Bir medya dosyasının süre/çözünürlük bilgisini döndürür."""
    if not has_ffprobe():
        return _probe_with_ffmpeg(path)

    command = [
        ffprobe_path(),
        "-v", "error",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        str(path),
    ]
    process = subprocess.run(command, capture_output=True, text=True, timeout=120)  # noqa: S603
    if process.returncode != 0:
        raise FFmpegFailed(
            f"ffprobe '{path.name}' dosyasını okuyamadı.",
            command=command,
            stderr=process.stderr,
        )
    return json.loads(process.stdout)


def _probe_with_ffmpeg(path: Path) -> dict:
    """ffprobe kurulu değilken ffmpeg logundan asgari bilgiyi çıkarır.

    ffprobe'un JSON'u kadar zengin değil; yalnız süre ve akış tiplerini
    verir — hattın ihtiyacı olan da bu.
    """
    log = _identify(path)

    duration = 0.0
    match = re.search(r"Duration:\s*(\d+):(\d\d):(\d\d(?:\.\d+)?)", log)
    if match:
        hours, minutes, seconds = match.groups()
        duration = int(hours) * 3600 + int(minutes) * 60 + float(seconds)
    elif "Duration: N/A" not in log and not re.search(r"Stream #\d", log):
        raise FFmpegFailed(
            f"'{path.name}' okunamadı.",
            command=[ffmpeg_path(), "-i", str(path)],
            stderr=log,
        )

    streams = [
        {"codec_type": kind.lower()}
        for kind in re.findall(r"Stream #\d+:\d+.*?: (Video|Audio)", log)
    ]
    return {"format": {"duration": str(duration)}, "streams": streams}


def duration_seconds(path: Path) -> float:
    info = probe(path)
    return float(info.get("format", {}).get("duration", 0.0))


def extract_last_frame(video: Path, out_image: Path) -> Path:
    """Videonun son karesini alır.

    Uzun video üretiminde bir sonraki klibin başlangıç görseli olarak
    kullanılır; sahneler arası görsel süreklilik böyle kurulur.
    """
    total = duration_seconds(video)
    seek = max(0.0, total - 0.04)
    out_image.parent.mkdir(parents=True, exist_ok=True)
    run(["-ss", f"{seek:.3f}", "-i", str(video), "-frames:v", "1", str(out_image)])
    return out_image


def make_color_clip(
    out_path: Path,
    *,
    seconds: float,
    width: int,
    height: int,
    fps: int,
    color: str,
    label: str = "",
) -> Path:
    """Anahtarsız demo modu için yerel placeholder klip üretir."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    filters = ["format=yuv420p"]
    if label and has_filter("drawtext"):
        safe = escape_drawtext(label)
        filters.insert(
            0,
            "drawtext=text='%s':fontcolor=white:fontsize=%d:x=(w-text_w)/2:"
            "y=(h-text_h)/2:box=1:boxcolor=black@0.4:boxborderw=20"
            % (safe, max(18, height // 18)),
        )
    run([
        "-f", "lavfi",
        "-i", f"color=c={color}:s={width}x{height}:r={fps}:d={seconds:.3f}",
        "-f", "lavfi",
        "-i", f"anullsrc=channel_layout=stereo:sample_rate=48000",
        "-shortest",
        "-vf", ",".join(filters),
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "23",
        "-c:a", "aac", "-b:a", "128k",
        str(out_path),
    ])
    return out_path


def escape_drawtext(text: str) -> str:
    """drawtext filtresi için metni kaçışlar.

    Sıra önemli: önce ters bölü, sonra diğerleri. Aksi hâlde eklediğimiz
    kaçış karakterlerini yeniden kaçışlarız.
    """
    out = text.replace("\\", "\\\\")
    for char in ("'", ":", "%", ",", "[", "]", ";"):
        out = out.replace(char, "\\" + char)
    return out.replace("\n", " ")
