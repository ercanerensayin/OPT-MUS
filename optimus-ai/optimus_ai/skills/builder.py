"""İstekten çalışır proje (uygulama / oyun) üretimi ve diske yazımı."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from ..config import Config, get_config
from ..errors import OptimusError
from ..llm import Brain, GeneratedProject

#: Tek bir dosya için üst sınır — model kaçarsa diski doldurmasın.
MAX_FILE_BYTES = 2_000_000
MAX_FILES = 60


@dataclass(frozen=True)
class BuiltProject:
    name: str
    summary: str
    directory: Path
    entrypoint: Path | None
    run_command: str
    files: list[Path]

    def as_dict(self) -> dict:
        return {
            "name": self.name,
            "summary": self.summary,
            "directory": str(self.directory),
            "entrypoint": str(self.entrypoint) if self.entrypoint else None,
            "run_command": self.run_command,
            "files": [str(path) for path in self.files],
        }


def slugify(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return slug or "proje"


def safe_join(root: Path, relative: str) -> Path:
    """Yol kaçışını (`..`, mutlak yol, sembolik link) engelleyerek birleştirir."""
    candidate = (root / relative).resolve()
    root_resolved = root.resolve()
    if candidate == root_resolved or root_resolved not in candidate.parents:
        raise OptimusError(
            f"Güvenli olmayan dosya yolu reddedildi: {relative!r}"
        )
    return candidate


def write_project(
    project: GeneratedProject, *, config: Config | None = None
) -> BuiltProject:
    """Üretilen dosyaları çalışma alanına yazar."""
    config = config or get_config()
    if not project.files:
        raise OptimusError("Model hiç dosya üretmedi.")
    if len(project.files) > MAX_FILES:
        raise OptimusError(
            f"Model {len(project.files)} dosya üretti (sınır {MAX_FILES}). "
            "İsteği daraltıp tekrar dene."
        )

    directory = _unique_dir(config.projects_dir / slugify(project.name))
    directory.mkdir(parents=True)

    written: list[Path] = []
    for item in project.files:
        if len(item.content.encode()) > MAX_FILE_BYTES:
            raise OptimusError(f"'{item.path}' dosyası fazla büyük, yazılmadı.")
        target = safe_join(directory, item.path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(item.content, encoding="utf-8")
        written.append(target)

    entrypoint: Path | None = None
    if project.entrypoint:
        try:
            candidate = safe_join(directory, project.entrypoint)
        except OptimusError:
            candidate = None
        if candidate is not None and candidate.exists():
            entrypoint = candidate

    return BuiltProject(
        name=project.name,
        summary=project.summary,
        directory=directory,
        entrypoint=entrypoint,
        run_command=project.run_command,
        files=written,
    )


def _unique_dir(base: Path) -> Path:
    if not base.exists():
        return base
    for counter in range(2, 1000):
        candidate = base.with_name(f"{base.name}-{counter}")
        if not candidate.exists():
            return candidate
    raise OptimusError(f"'{base.name}' için boş klasör adı bulunamadı.")


def build(
    request: str, *, kind: str = "app", brain: Brain | None = None
) -> BuiltProject:
    """İsteği projeye çevirip diske yazar."""
    brain = brain or Brain()
    generated = brain.build_project(request, kind=kind)
    return write_project(generated, config=brain.config)
