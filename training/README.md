# Kendi modelini eğitmek

## Önce gerçekçi beklenti

Claude/GPT seviyesinde bir modeli **sıfırdan** eğitmek onbinlerce GPU'da aylarca
eğitim, trilyonlarca token veri ve milyonlarca dolar demektir. Tek kişilik bir
projede yapılabilir değil.

Yapılabilir ve gerçekten işe yarayan yol: **güçlü bir açık temel modeli, kendi
göreve özel verinle uzmanlaştırmak.** Sonuçta ortaya senin verinle, senin çıktı
formatını, senin tasarım standardını öğrenmiş, kendi donanımında ücretsiz çalışan
bir model çıkar. "Kendi modelim" budur ve bu depodaki hat tam olarak bunu üretir.

Ne bekleyebilirsin: 7B'lik ince ayarlı bir model, bu dar görevde (tek promptla
tek sayfalık site üretmek) şaşırtıcı derecede iyi olur. Karmaşık uygulama
mimarisi ve uzun refactor işlerinde büyük modellerin gerisinde kalır.

## Hat

```
1. distill.py        öğretmen modelden veri üret   →  data/generations.jsonl
2. build_dataset.py  temizle + filtrele + böl      →  data/train.jsonl, val.jsonl
3. train_lora.py     QLoRA ince ayar               →  out/optmus-lora
4. serve_openai.py   OpenAI-uyumlu sunucu          →  localhost:8000/v1
5. .env              uygulamayı kendi modeline bağla
```

## 1. Veri

İki kaynak var, ikisi de aynı dosyada birikir:

**a) Gerçek kullanım.** Uygulamayı normal kullandığın her an, her başarılı üretim
`data/generations.jsonl` dosyasına bir eğitim örneği olarak yazılır
(`.env` → `COLLECT_DATASET=true`). Bedava veri.

**b) Damıtma (distillation).** Öğretmen model, öğrenci modelin verisini üretir:

```bash
npm start                                        # güçlü bir sağlayıcıyla
python3 training/distill.py --count 300 --concurrency 3
```

`distill.py` 250 farklı Türkçe istekten oluşan bir prompt bankasını çalışan
sunucuya gönderir; sunucu üretir ve veri kümesi kendiliğinden dolar.

> Damıtma yaparken sağlayıcının **güçlü** olması şart. Kendi küçük modelinle
> veri üretip yine onunla eğitmek hiçbir şey öğretmez.

**Ne kadar veri?** 500 örnek altı zayıf kalır; 1000–3000 örnek bu görev için iyi
bir hedeftir. Çeşitlilik, miktardan önemlidir — aynı konunun 50 varyasyonu yerine
50 farklı konu.

## 2. Veri kümesi

```bash
python3 training/build_dataset.py
```

Bu adım bilerek acımasız: boş çıktı, bozuk format, yarım kalmış blok, `"kodun
geri kalanı aynı"` gibi kısaltma içeren veya tekrar eden örnekler elenir.
Modele öğrettiğin her kötü örnek, sonsuza kadar üreteceği bir kusurdur.

## 3. Eğitim

```bash
pip install -r training/requirements.txt
python3 training/train_lora.py --epochs 3
```

| Donanım | Öneri |
|---|---|
| 24 GB VRAM (RTX 3090/4090) | `Qwen2.5-Coder-7B-Instruct`, varsayılan ayarlar |
| 16 GB VRAM | aynı model, `--max-seq-len 4096` |
| 8–12 GB VRAM | `--model Qwen/Qwen2.5-Coder-1.5B-Instruct` |
| GPU yok | Colab/Kaggle/RunPod kirala; CPU'da pratik değil |

Ayarların anlamı:

- **QLoRA**: temel model 4-bit dondurulur, yalnızca küçük LoRA katmanları eğitilir.
  Bellek ~4 kat düşer, kalite kaybı çok azdır.
- `assistant_only_loss=True`: kayıp yalnızca asistan cevabından hesaplanır — model
  istekleri ezberlemez, doğru **çıktıyı** üretmeyi öğrenir.
- `--max-seq-len 8192`: üretilen dosyalar uzun; pencere darsa model kesik kod
  üretmeyi öğrenir.

Eğitim sırasında `eval_loss` düşüşü durur ve yükselmeye başlarsa (ezberleme),
epoch sayısını azalt veya veriyi artır.

## 4. Sunma ve bağlama

```bash
python3 training/serve_openai.py \
    --base Qwen/Qwen2.5-Coder-7B-Instruct \
    --adapter training/out/optmus-lora
```

`.env`:

```env
LLM_PROVIDER=openai
OPENAI_BASE_URL=http://localhost:8000/v1
OPENAI_MODEL=optmus-coder
OPENAI_API_KEY=local
```

`npm start` — uygulama artık senin modelinle çalışıyor.

Yüksek eşzamanlılık gerekiyorsa aynı adaptörü vLLM ile de sunabilirsin
(`vllm serve <base> --enable-lora --lora-modules optmus=out/optmus-lora`);
uygulama tarafında değişen hiçbir şey olmaz.

## 5. Değerlendirme

İnce ayarın işe yarayıp yaramadığını ölçmenin en pratik yolu:

1. `data/val.jsonl` içindeki istekleri hem temel modele hem ince ayarlı modele ver.
2. Otomatik kontrol: çıktı `<file>` sözleşmesine uyuyor mu, `index.html` var mı,
   HTML/CSS/JS sözdizimi geçerli mi, kısaltma kalıbı içeriyor mu.
3. Gözle kontrol: 10–20 çıktıyı tarayıcıda aç, tasarım ve etkileşim çalışıyor mu.

Format uyumu ve "ilk denemede çalışan sayfa" oranı, ince ayarın en hızlı
iyileştirdiği iki metriktir.

## Sık karşılaşılan sorunlar

| Belirti | Sebep / çözüm |
|---|---|
| Çıktı yarım kalıyor | `.env` → `MAX_TOKENS` artır; eğitimde `--max-seq-len` artır |
| Model formatı bozuyor | Veri az veya kirli; `build_dataset.py` redlerine bak |
| `bitsandbytes` hatası | CUDA gerekir; CPU'da `--no-4bit` ile dene (yavaş) |
| CUDA OOM | `--max-seq-len` düşür, `--grad-accum` artır, küçük modele geç |
| Aynı cümleleri tekrarlıyor | Aşırı eğitim; epoch azalt veya veriyi çeşitlendir |
