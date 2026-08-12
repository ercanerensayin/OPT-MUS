#!/usr/bin/env python3
"""
Toplanan uretimleri egitilebilir bir SFT veri kumesine cevirir.

Girdi : training/data/generations.jsonl   (uygulama calisirken otomatik dolar)
Cikti : training/data/train.jsonl , training/data/val.jsonl

Kalite filtreleri, modele kotu aliskanlik ogretmemek icin bilerek serttir:
kisaltilmis kod, bos cikti, bozuk format ve tekrarlar elenir.

Kullanim:
    python3 training/build_dataset.py --min-files 1 --val-ratio 0.05
"""

from __future__ import annotations

import argparse
import hashlib
import json
import random
import re
from pathlib import Path

DATA_DIR = Path(__file__).parent / "data"

FILE_BLOCK = re.compile(r'<file\s+path\s*=\s*"([^"]+)"\s*>(.*?)</file>', re.S | re.I)

# Modelin ogrenmemesi gereken "tembellik" kaliplari.
LAZY_PATTERNS = [
    re.compile(r"^\s*(//|#|/\*)?\s*\.\.\.\s*(geri kalan|rest of|remaining)", re.I | re.M),
    re.compile(r"(kodun geri kalani|rest of the code|unchanged|ayni kalacak|as before)", re.I),
    re.compile(r"^\s*(//|#)\s*TODO\b", re.I | re.M),
]


def parse_files(output: str) -> list[tuple[str, str]]:
    return [(m.group(1).strip(), m.group(2).strip()) for m in FILE_BLOCK.finditer(output)]


def is_lazy(text: str) -> bool:
    return any(p.search(text) for p in LAZY_PATTERNS)


def quality_check(record: dict, min_files: int) -> str | None:
    """Kayit uygunsa None, degilse red sebebini dondurur."""
    output = record.get("output", "")
    instruction = record.get("instruction", "")

    if not output.strip() or not instruction.strip():
        return "bos alan"
    if "<file" not in output:
        return "dosya blogu yok"

    files = parse_files(output)
    if len(files) < min_files:
        return f"yetersiz dosya ({len(files)})"
    if any(not content.strip() for _, content in files):
        return "bos dosya icerigi"
    if is_lazy(output):
        return "kisaltilmis kod"
    # Kapanmamis blok: acilis sayisi kapanis sayisindan fazlaysa cikti yarim kalmistir.
    if output.lower().count("<file ") != output.lower().count("</file>"):
        return "yarim blok"
    return None


def to_chat(record: dict) -> dict:
    return {
        "messages": [
            {"role": "system", "content": record["system"]},
            {"role": "user", "content": record["instruction"]},
            {"role": "assistant", "content": record["output"].strip()},
        ],
        "meta": {
            "mode": record.get("mode", "site"),
            "kind": record.get("kind", "create"),
            "files": record.get("files", []),
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="OPT-MUS SFT veri kumesi olusturucu")
    parser.add_argument("--input", default=str(DATA_DIR / "generations.jsonl"))
    parser.add_argument("--out-dir", default=str(DATA_DIR))
    parser.add_argument("--min-files", type=int, default=1)
    parser.add_argument("--val-ratio", type=float, default=0.05)
    parser.add_argument("--seed", type=int, default=13)
    args = parser.parse_args()

    src = Path(args.input)
    if not src.exists():
        raise SystemExit(
            f"{src} yok.\n"
            "Once uygulamayi calistirip birkac uretim yapin ya da "
            "`python3 training/distill.py` ile ogretmen modelden veri uretin."
        )

    seen: set[str] = set()
    kept: list[dict] = []
    rejected: dict[str, int] = {}

    for line_no, line in enumerate(src.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            rejected["bozuk json"] = rejected.get("bozuk json", 0) + 1
            continue

        reason = quality_check(record, args.min_files)
        if reason:
            rejected[reason] = rejected.get(reason, 0) + 1
            continue

        key = hashlib.sha256(record["instruction"].strip().encode("utf-8")).hexdigest()
        if key in seen:
            rejected["tekrar"] = rejected.get("tekrar", 0) + 1
            continue
        seen.add(key)
        kept.append(to_chat(record))

    if not kept:
        raise SystemExit("Kaliteli ornek bulunamadi. Redler: " + json.dumps(rejected, ensure_ascii=False))

    random.Random(args.seed).shuffle(kept)
    val_size = max(1, int(len(kept) * args.val_ratio)) if len(kept) > 20 else 0
    val, train = kept[:val_size], kept[val_size:]

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, rows in (("train", train), ("val", val)):
        if not rows:
            continue
        path = out_dir / f"{name}.jsonl"
        with path.open("w", encoding="utf-8") as fh:
            for row in rows:
                fh.write(json.dumps(row, ensure_ascii=False) + "\n")
        print(f"{path}  ->  {len(rows)} ornek")

    print(f"\nToplam kabul: {len(kept)}")
    if rejected:
        print("Redler:")
        for reason, count in sorted(rejected.items(), key=lambda kv: -kv[1]):
            print(f"  {count:5d}  {reason}")
    if len(kept) < 200:
        print(
            "\nUyari: 200'den az ornek var. Kullanilabilir bir model icin en az "
            "500-2000 ornek hedefleyin (distill.py ile cogaltabilirsiniz)."
        )


if __name__ == "__main__":
    main()
