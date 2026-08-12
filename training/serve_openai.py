#!/usr/bin/env python3
"""
Ince ayarlanmis modeli OpenAI-uyumlu bir API olarak sunar.

Boylece OPT-MUS uygulamasi hicbir kod degisikligi olmadan kendi modelinizi
kullanir:

    .env:
      LLM_PROVIDER=openai
      OPENAI_BASE_URL=http://localhost:8000/v1
      OPENAI_MODEL=optmus-coder
      OPENAI_API_KEY=local

Kullanim:
    python3 training/serve_openai.py --base Qwen/Qwen2.5-Coder-7B-Instruct \\
        --adapter training/out/optmus-lora
"""

from __future__ import annotations

import argparse
import json
import time
import uuid
from threading import Thread

import torch
import uvicorn
from fastapi import FastAPI
from fastapi.responses import StreamingResponse
from transformers import AutoModelForCausalLM, AutoTokenizer, TextIteratorStreamer

STATE: dict = {}


def build_app(model_name: str) -> FastAPI:
    app = FastAPI(title="OPT-MUS model sunucusu")

    @app.get("/v1/models")
    def list_models() -> dict:
        return {
            "object": "list",
            "data": [{"id": model_name, "object": "model", "owned_by": "optmus"}],
        }

    @app.post("/v1/chat/completions")
    async def chat_completions(body: dict):
        tokenizer, model = STATE["tokenizer"], STATE["model"]

        messages = body.get("messages", [])
        stream = bool(body.get("stream"))
        max_new_tokens = int(body.get("max_tokens") or 8192)
        temperature = float(body.get("temperature", 0.4))

        prompt = tokenizer.apply_chat_template(
            messages, tokenize=False, add_generation_prompt=True
        )
        inputs = tokenizer(prompt, return_tensors="pt").to(model.device)

        generation_kwargs = dict(
            **inputs,
            max_new_tokens=max_new_tokens,
            do_sample=temperature > 0,
            temperature=max(temperature, 1e-5),
            top_p=float(body.get("top_p", 0.95)),
            repetition_penalty=1.02,
            pad_token_id=tokenizer.pad_token_id or tokenizer.eos_token_id,
        )

        completion_id = f"chatcmpl-{uuid.uuid4().hex[:24]}"
        created = int(time.time())

        if not stream:
            with torch.inference_mode():
                output = model.generate(**generation_kwargs)
            text = tokenizer.decode(
                output[0][inputs["input_ids"].shape[-1]:], skip_special_tokens=True
            )
            return {
                "id": completion_id,
                "object": "chat.completion",
                "created": created,
                "model": model_name,
                "choices": [
                    {
                        "index": 0,
                        "message": {"role": "assistant", "content": text},
                        "finish_reason": "stop",
                    }
                ],
            }

        streamer = TextIteratorStreamer(
            tokenizer, skip_prompt=True, skip_special_tokens=True
        )
        Thread(
            target=lambda: model.generate(**generation_kwargs, streamer=streamer),
            daemon=True,
        ).start()

        def event_stream():
            for token in streamer:
                if not token:
                    continue
                chunk = {
                    "id": completion_id,
                    "object": "chat.completion.chunk",
                    "created": created,
                    "model": model_name,
                    "choices": [{"index": 0, "delta": {"content": token}, "finish_reason": None}],
                }
                yield f"data: {json.dumps(chunk, ensure_ascii=False)}\n\n"

            final = {
                "id": completion_id,
                "object": "chat.completion.chunk",
                "created": created,
                "model": model_name,
                "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
            }
            yield f"data: {json.dumps(final)}\n\n"
            yield "data: [DONE]\n\n"

        return StreamingResponse(event_stream(), media_type="text/event-stream")

    return app


def main() -> None:
    parser = argparse.ArgumentParser(description="OPT-MUS model sunucusu")
    parser.add_argument("--base", required=True, help="temel model kimligi veya yolu")
    parser.add_argument("--adapter", default="", help="LoRA adaptor klasoru (bos ise sade temel model)")
    parser.add_argument("--model-name", default="optmus-coder", help="API'de gorunecek ad")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--load-4bit", action="store_true")
    args = parser.parse_args()

    print(f"Model yukleniyor: {args.base}" + (f" + {args.adapter}" if args.adapter else ""))

    tokenizer = AutoTokenizer.from_pretrained(args.adapter or args.base, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    quantization_config = None
    if args.load_4bit and torch.cuda.is_available():
        from transformers import BitsAndBytesConfig

        quantization_config = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.bfloat16,
        )

    model = AutoModelForCausalLM.from_pretrained(
        args.base,
        quantization_config=quantization_config,
        dtype=torch.bfloat16 if torch.cuda.is_available() else torch.float32,
        device_map="auto" if torch.cuda.is_available() else None,
        trust_remote_code=True,
    )

    if args.adapter:
        from peft import PeftModel

        model = PeftModel.from_pretrained(model, args.adapter)
        model = model.merge_and_unload()  # birlestirilince cikarim hizlanir

    model.eval()
    STATE["tokenizer"] = tokenizer
    STATE["model"] = model

    print(f"Hazir -> http://{args.host}:{args.port}/v1")
    uvicorn.run(build_app(args.model_name), host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
