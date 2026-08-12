#!/usr/bin/env python3
"""
Kendi kod/site uretim modelinizi QLoRA ile ince ayarlar.

Sifirdan bir temel model egitmek milyonlarca dolarlik GPU saati ister;
dogru yol, guclu bir acik modeli kendi verinizle uzmanlastirmaktir.
Bu betik tam olarak bunu yapar: temel model dondurulur, yalnizca kucuk
LoRA katmanlari egitilir (7B model, tek 16 GB GPU'da calisir).

Kullanim:
    pip install -r training/requirements.txt
    python3 training/build_dataset.py
    python3 training/train_lora.py --epochs 3

Cikti: training/out/optmus-lora  (LoRA adaptoru)
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

BASE_DIR = Path(__file__).parent
DEFAULT_MODEL = "Qwen/Qwen2.5-Coder-7B-Instruct"


def load_jsonl(path: Path) -> list[dict]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            rows.append(json.loads(line))
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description="OPT-MUS QLoRA ince ayar")
    parser.add_argument("--model", default=DEFAULT_MODEL,
                        help="temel model (kucuk GPU icin Qwen2.5-Coder-1.5B-Instruct deneyin)")
    parser.add_argument("--train", default=str(BASE_DIR / "data" / "train.jsonl"))
    parser.add_argument("--val", default=str(BASE_DIR / "data" / "val.jsonl"))
    parser.add_argument("--out", default=str(BASE_DIR / "out" / "optmus-lora"))
    parser.add_argument("--epochs", type=float, default=3.0)
    parser.add_argument("--lr", type=float, default=1e-4)
    parser.add_argument("--batch", type=int, default=1)
    parser.add_argument("--grad-accum", type=int, default=8)
    parser.add_argument("--max-seq-len", type=int, default=8192,
                        help="uretilen dosyalar uzun oldugu icin genis tutuldu")
    parser.add_argument("--lora-r", type=int, default=32)
    parser.add_argument("--lora-alpha", type=int, default=64)
    parser.add_argument("--no-4bit", action="store_true", help="4-bit nicemlemeyi kapat")
    args = parser.parse_args()

    # Agir bagimliliklar yalnizca gercekten egitim yapilirken yuklenir.
    import torch
    from datasets import Dataset
    from peft import LoraConfig
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from trl import SFTConfig, SFTTrainer

    train_path, val_path = Path(args.train), Path(args.val)
    if not train_path.exists():
        raise SystemExit(f"{train_path} yok. Once `python3 training/build_dataset.py` calistirin.")

    train_rows = load_jsonl(train_path)
    val_rows = load_jsonl(val_path) if val_path.exists() else []
    print(f"Egitim ornegi: {len(train_rows)}   Dogrulama: {len(val_rows)}")
    if len(train_rows) < 50:
        print("Uyari: ornek sayisi cok az. distill.py ile veriyi cogaltmaniz onerilir.")

    tokenizer = AutoTokenizer.from_pretrained(args.model, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    quantization_config = None
    if not args.no_4bit and torch.cuda.is_available():
        from transformers import BitsAndBytesConfig

        quantization_config = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_use_double_quant=True,
            bnb_4bit_compute_dtype=torch.bfloat16,
        )

    model = AutoModelForCausalLM.from_pretrained(
        args.model,
        quantization_config=quantization_config,
        dtype=torch.bfloat16 if torch.cuda.is_available() else torch.float32,
        device_map="auto" if torch.cuda.is_available() else None,
        trust_remote_code=True,
        attn_implementation="sdpa",
    )
    model.config.use_cache = False

    peft_config = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=0.05,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=[
            "q_proj", "k_proj", "v_proj", "o_proj",
            "gate_proj", "up_proj", "down_proj",
        ],
    )

    def to_text(rows: list[dict]) -> Dataset:
        # Yalnizca "messages" alani birakilir; SFTTrainer sohbet sablonunu kendisi uygular.
        return Dataset.from_list([{"messages": row["messages"]} for row in rows])

    sft_config = SFTConfig(
        output_dir=args.out,
        num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch,
        gradient_accumulation_steps=args.grad_accum,
        learning_rate=args.lr,
        lr_scheduler_type="cosine",
        warmup_ratio=0.03,
        logging_steps=5,
        save_strategy="epoch",
        eval_strategy="epoch" if val_rows else "no",
        bf16=torch.cuda.is_available(),
        max_length=args.max_seq_len,
        gradient_checkpointing=True,
        report_to=[],
        # Kayip yalnizca asistan cevabi uzerinden hesaplanir: model istegi
        # ezberlemek yerine dogru ciktiyi uretmeyi ogrenir.
        assistant_only_loss=True,
    )

    trainer = SFTTrainer(
        model=model,
        args=sft_config,
        train_dataset=to_text(train_rows),
        eval_dataset=to_text(val_rows) if val_rows else None,
        peft_config=peft_config,
        processing_class=tokenizer,
    )

    trainer.train()
    trainer.save_model(args.out)
    tokenizer.save_pretrained(args.out)

    print(f"\nBitti. LoRA adaptoru: {args.out}")
    print("Sunmak icin:")
    print(f"  python3 training/serve_openai.py --base {args.model} --adapter {args.out}")


if __name__ == "__main__":
    main()
