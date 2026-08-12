# OPT-MUS

Tek bir cümleden çalışır web sitesi, web uygulaması veya kod üreten yapay zekâ sistemi —
ve ürettiklerini veri olarak toplayıp **kendi modelini eğitmen** için hazır bir eğitim hattı.

```
"kahve dükkanım için menü ve rezervasyon formu olan bir site yap"
        │
        ▼
  akışlı üretim  →  canlı önizleme  →  ZIP indir
        │
        └──────────► training/data/generations.jsonl  →  kendi modelin
```

## Ne yapar

- **Tek prompt → tam proje.** Model çok dosyalı bir proje üretir (`index.html`, `style.css`,
  `app.js`, alt klasörler…), dosyalar diske yazılır.
- **Akışlı yazım.** Kod, model üretirken karakter karakter ekrana düşer.
- **Canlı önizleme.** Üretilen proje gerçek bir sunucudan `iframe` içinde çalışır;
  masaüstü / tablet / telefon genişliğinde denenebilir.
- **Konuşarak düzenleme.** İkinci prompt ile projeyi değiştirirsin; model yalnızca
  değişen dosyaları döner, gerisine dokunulmaz.
- **ZIP indirme.** Proje tek tıkla indirilir, herhangi bir statik hosting'e atılabilir.
- **Üç mod.** Web sitesi · Web uygulaması · Kod/betik (Python, Node, Bash…).
- **Sağlayıcı bağımsız.** Claude, OpenAI-uyumlu her servis, Ollama — veya kendi eğittiğin model.

## İki sürüm var

| | Kurulumsuz sürüm | Tam sürüm |
|---|---|---|
| Dosya | `docs/index.html` (tek dosya) | `server/` + `web/` |
| Gereksinim | Sadece tarayıcı | Node.js 20+ |
| Anahtar nerede | Senin tarayıcının `localStorage`'ında | Sunucudaki `.env` |
| Projeler nerede | Tarayıcı depolamasında | Diskte, `workspace/` |
| Veri toplama / eğitim | Yok | Var |

Sadece kullanmak istiyorsan **kurulumsuz sürüm** yeter: `docs/index.html` dosyasına
çift tıkla, açılır. Ya da GitHub Pages'e koyup gerçek bir adresten çalıştır
(Settings → Pages → Source: `main` / `/docs`).

> Kurulumsuz sürüm anahtarını doğrudan sağlayıcının API'sine gönderir ve yalnızca
> senin tarayıcında saklar. Kişisel kullanım için tasarlandı — anahtarının yazılı
> olduğu tarayıcıyı başkasıyla paylaşma.

## Hızlı başlangıç (tam sürüm)

```bash
npm install
cp .env.example .env      # ANTHROPIC_API_KEY değerini gir
npm start                 # http://localhost:3000
```

API anahtarın yoksa önce sahte sağlayıcıyla dene — tüm akış (üretim → önizleme → ZIP) çalışır:

```bash
echo "LLM_PROVIDER=mock" > .env && npm start
```

## Sağlayıcı seçimi

`.env` içinde `LLM_PROVIDER` değerini değiştirmen yeterli:

| Sağlayıcı | Ne için | Ayarlar |
|---|---|---|
| `anthropic` | En iyi sonuç (önerilen) | `ANTHROPIC_API_KEY`, `ANTHROPIC_MODEL` |
| `openai` | OpenAI, OpenRouter, Together, Groq, DeepSeek, vLLM, **kendi modelin** | `OPENAI_API_KEY`, `OPENAI_BASE_URL`, `OPENAI_MODEL` |
| `ollama` | Yerelde ücretsiz çalıştırma | `OLLAMA_BASE_URL`, `OLLAMA_MODEL` |
| `mock` | Anahtarsız deneme | — |

## Kendi modelini eğitmek

Sıfırdan temel model eğitmek milyonlarca dolarlık GPU saati ister; anlamlı ve
ulaşılabilir yol, güçlü bir açık modeli **kendi verinle uzmanlaştırmak**. Bu depo bunun
tüm hattını içerir → **[training/README.md](training/README.md)**

```bash
python3 training/distill.py --count 300      # öğretmen modelden veri üret
python3 training/build_dataset.py            # temizle, filtrele, böl
python3 training/train_lora.py --epochs 3    # QLoRA ile ince ayar
python3 training/serve_openai.py --base Qwen/Qwen2.5-Coder-7B-Instruct \
    --adapter training/out/optmus-lora       # kendi modelini sun
```

Sonra `.env` içinde `LLM_PROVIDER=openai`, `OPENAI_BASE_URL=http://localhost:8000/v1`
yaz — uygulama artık **senin modelinle** çalışır.

## Mimari

```
server/
  index.js       HTTP API, SSE akışı, önizleme sunucusu, ZIP
  llm.js         sağlayıcı katmanı (anthropic / openai / ollama / mock)
  prompts.js     sistem promptları ve çıktı sözleşmesi  ← sistemin beyni
  fileblocks.js  akış halinde <file> ayrıştırıcı + yol güvenliği
  pipeline.js    prompt → model → dosyalar → disk → veri kümesi
  store.js       proje kalıcılığı, sürüm geçmişi, veri toplama
  zip.js         bağımlılıksız ZIP üretici
web/             arayüz (derleme adımı yok, saf HTML/CSS/JS)
docs/index.html  kurulumsuz sürüm — tüm uygulama tek dosyada, tarayıcıda çalışır
training/        veri damıtma, veri kümesi, QLoRA eğitimi, model sunucusu
test/            ayrıştırıcı ve ZIP birim testleri
```

### Neden JSON değil, `<file>` etiketi

Model çıktısı şu sözleşmeye uyar:

```
<plan>Ne yapılacağı</plan>

<file path="index.html">
...tam içerik...
</file>
```

JSON'a göre üç avantajı var: **akış sırasında ayrıştırılabilir** (yarım JSON sorunu yok),
**kaçış karakteri gerektirmez** (üretilen kod bozulmaz), ve **küçük modeller bu formatı
kolayca öğrenir** — kendi modelini eğitirken bu önemli.

## API

| Uç | Açıklama |
|---|---|
| `GET /api/status` | Sağlayıcı durumu |
| `GET /api/projects` | Proje listesi |
| `POST /api/projects` | Proje oluştur `{prompt, mode}` |
| `POST /api/projects/:id/generate` | Üret / düzenle — **SSE akışı** |
| `GET /api/projects/:id` | Proje + dosyalar |
| `GET /api/projects/:id/zip` | ZIP indir |
| `DELETE /api/projects/:id` | Sil |
| `GET /p/:id/*` | Canlı önizleme |

SSE olayları: `start` · `plan` · `file_start` · `file_delta` · `file_end` · `done` · `error`

## Testler

```bash
npm test
```

Ayrıştırıcının en zor durumu da kapsanır: kapanış etiketi akışta ikiye bölündüğünde
(`</fi` + `le>`) ve üretilen CSS içinde `"</fil"` metni geçtiğinde doğru çalışması.

## Güvenlik notu

Üretilen kod `sandbox` özniteliği olan bir `iframe` içinde çalışır ve
`allow-same-origin` **verilmez**. Dosya yolları hem yazarken hem sunarken proje
kökünün dışına çıkmayacak şekilde doğrulanır. Yine de bu araç kişisel/geliştirme
kullanımı için tasarlandı; herkese açık bir sunucuya koyacaksan kimlik doğrulama
ve hız sınırlaması ekle.
