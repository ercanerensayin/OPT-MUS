# OPTIMUS AI

Tek arayüzde: **sohbet**, **kod yazma**, **uygulama/oyun üretme**, **görsel
üretme**, **süre sınırı olmayan video üretme** ve **video kurgu/montaj**.

```
┌── Sohbet & Kod ──────────┐   Claude (claude-opus-5), streaming
├── Uygulama / Oyun ───────┤   istek → çalışan dosya ağacı → diske yaz
├── Görsel ────────────────┤   OpenAI / Stability / Replicate  (+demo)
├── Video (sınırsız süre) ─┤   planla → parça üret → zincirle → birleştir
└── Kurgu (NLE) ───────────┘   JSON timeline → ffmpeg filter graph
```

---

## Baştan söylemek gerekenler

İstediğin "her şeyi bilen" tanımına dürüst karşılık şu: bu sistem her şeyi
bilmiyor. Bir dil modeline ve birkaç üretim servisine bağlı; eğitim verisinin
bir kesme tarihi var, yanılabiliyor, güncel bilgi için arama gerekiyor.
Sistem prompt'u modele bunu söylüyor — bilmediğine "bilmiyorum" demesi
isteniyor.

"Adobe editi" derken kastettiğin şeyi Premiere/After Effects'i çalıştırarak
değil, aynı işleri **programatik olarak ffmpeg üzerinden** yaparak
karşılıyorum: kesme, geçiş, hız, renk düzeltme, LUT, yazı bindirme, müzik,
otomatik ducking, dışa aktarma şablonları. Adobe'nin kendi dosya formatlarını
(.prproj / .aep) açmıyor.

**Sınırsız video süresi gerçek, ama şu şekilde çalışıyor:** bugün hiçbir video
modeli tek çağrıda dakikalarca video üretmiyor — sınır tipik olarak 5-10
saniye. Bu proje süreyi şöyle açıyor:

1. Claude, isteği N sahnelik bir senaryoya bölüyor (ortak stil, aynı
   karakterler, aynı kamera dili).
2. Her sahne ayrı bir klip olarak üretiliyor.
3. Her klibin **son karesi**, bir sonraki klibin başlangıç görseli olarak
   veriliyor — görüntü kopmuyor.
4. Klipler geçişler, altyazı ve müzikle tek videoya kurgulanıyor.

Kodda süre üst sınırı yok. Pratikte süreyi **bekleme süresi ve API bütçen**
sınırlıyor: 10 dakikalık video, 5 saniyelik parçalarla 120 model çağrısı
demek. Yarım kalan iş diske yazılıyor, tekrar çalıştırınca kaldığı yerden
devam ediyor.

---

## Kurulum

```bash
cd optimus-ai
./run.sh                # sanal ortam + bağımlılıklar + sunucu
```

İlk çalıştırma `.env` dosyasını oluşturur. İçinde en az şunu doldur:

```
ANTHROPIC_API_KEY=sk-ant-...
```

Sonra tekrar `./run.sh` → <http://127.0.0.1:8000>

**ffmpeg gerekli** (video ve kurgu için):

```bash
sudo apt install ffmpeg      # Debian/Ubuntu
brew install ffmpeg          # macOS
winget install Gyan.FFmpeg   # Windows
```

Yazı bindirme (altyazı) için ffmpeg'in `drawtext` filtresiyle derlenmiş
olması gerekir. Dağıtımların standart paketlerinde var; bazı statik
derlemelerde yok — o durumda altyazı sessizce atlanır, gerisi çalışır.

### Anahtarsız deneme

`.env` içinde `IMAGE_PROVIDER=demo` ve `VIDEO_PROVIDER=demo` bırakırsan
görsel/video yerine yerel placeholder'lar üretilir. Uzun video hattını
(planlama → parça üretimi → zincirleme → kurgu) tek kuruş harcamadan uçtan
uca denemek için.

---

## Sağlayıcılar

| Yetenek | Seçenekler | Ayar |
|---|---|---|
| Beyin | Claude | `ANTHROPIC_API_KEY`, `OPTIMUS_MODEL`, `OPTIMUS_EFFORT` |
| Görsel | `demo`, `openai`, `stability`, `replicate` | `IMAGE_PROVIDER` |
| Video | `demo`, `replicate`, `luma` | `VIDEO_PROVIDER` |

Sağlayıcılar `optimus_ai/providers/` altında; yenisini eklemek tek fonksiyon
yazmak demek.

`OPTIMUS_EFFORT` modelin ne kadar düşüneceğini ayarlar: kod ve ajan işlerinde
`xhigh`, günlük kullanımda `high`, ucuz/hızlı isteklerde `low`.

---

## Kurgu (timeline) formatı

Kurgu sekmesine yapıştırdığın JSON, tek bir ffmpeg komutuna derlenir.
**Önizle** komutu gösterir (hiçbir şey çalıştırmaz), **Render** çalıştırır.

```json
{
  "output": { "path": "kurgu.mp4", "preset": "shorts", "fps": 30, "crf": 20 },
  "clips": [
    {
      "src": "media/klip1.mp4",
      "start": 0, "end": 5,
      "filters": {
        "speed": 1.0, "brightness": 0.05, "contrast": 1.1,
        "saturation": 1.2, "gamma": 1.0, "blur": 0, "vignette": false,
        "lut": "look.cube", "fade_in": 0.5, "fade_out": 0
      },
      "text": [
        { "content": "Merhaba", "x": "(w-text_w)/2", "y": "h*0.82",
          "size": 48, "color": "white", "start": 0.5, "end": 3 }
      ],
      "volume": 1.0, "mute": false
    },
    {
      "src": "media/klip2.mp4",
      "start": 2, "end": 8,
      "transition": { "type": "fade", "duration": 0.75 },
      "filters": { "speed": 1.5 }
    }
  ],
  "audio": {
    "music": "media/muzik.mp3", "music_volume": 0.3,
    "ducking": true, "fade_in": 1, "fade_out": 2
  }
}
```

**Şablonlar:** `youtube` (1920×1080), `youtube4k`, `shorts` / `reels` /
`tiktok` (1080×1920), `square`, `cinema`.

**Geçişler:** `cut`, `fade`, `fadeblack`, `fadewhite`, `dissolve`,
`wipeleft/right/up/down`, `slideleft/right/up/down`, `circleopen`,
`circleclose`, `radial`, `smoothleft`, `smoothright`.

Bilmesi gerekenler:

- Geçiş süresi iki klibin **üstünde çakışır**: 4sn + 6sn arada 1sn fade =
  toplam 9sn.
- Geçiş kullanıyorsan her klipte `end` vermek zorundasın — `xfade` klip
  süresini bilmek zorunda.
- Aynı kurguda geçiş varken bir yerde `cut` istersen, o kesme tek karelik
  bir erimeye dönüşür (gözle fark edilmez); ffmpeg sıfır süreli `xfade`
  kabul etmiyor.
- `ducking: true` → klip sesi yükseldikçe müzik otomatik kısılır
  (sidechain kompresör).
- Klip sesi yoksa veya `mute` ise sessizlik kaynağı devreye girer;
  ses/görüntü senkronu bozulmaz.

---

## HTTP API

| Uç | İş |
|---|---|
| `GET /api/capabilities` | Aktif model, sağlayıcılar, ffmpeg durumu, uyarılar |
| `POST /api/chat` | Sohbet (düz metin stream) |
| `POST /api/route` | İsteği niyete sınıflandır |
| `POST /api/build` | `{request, kind: app\|game}` → diske yazılmış proje |
| `POST /api/image` | `{prompt, shape}` → görsel |
| `POST /api/video` | `{prompt, total_seconds, ...}` → arka plan işi başlatır |
| `GET /api/jobs/{id}` | İş durumu / ilerleme |
| `POST /api/edit` | `{timeline, dry_run}` → ffmpeg komutu ya da render |
| `GET /files?path=` | Çalışma alanındaki üretilmiş dosya |

Örnek — 3 dakikalık dikey video:

```bash
curl -s localhost:8000/api/video -H 'content-type: application/json' -d '{
  "prompt": "bir kahve çekirdeğinin tarladan fincana yolculuğu",
  "total_seconds": 180, "preset": "shorts", "transition": "dissolve"
}'
# {"job_id":"...","estimated_segments":36,"poll":"/api/jobs/..."}
```

---

## Güvenlik sınırları

- Üretilen proje dosyaları çalışma alanına yazılır; `..` ve mutlak yollar
  reddedilir (`builder.safe_join`).
- `/files` yalnızca çalışma alanı içindeki dosyaları servis eder.
- Sunucu varsayılan olarak `127.0.0.1`'e bağlanır. Dışarı açacaksan önüne
  kimlik doğrulama koy — bu uçlar kimliksiz.
- Kurgu JSON'undaki dosya yolları doğrudan ffmpeg'e gider; güvenmediğin
  kullanıcıdan gelen timeline'ı çalıştırma.

## Testler

```bash
./.venv/bin/python -m pytest -q
```

29 test var; kurgu derleyicisi ffmpeg kurulu olmadan da test edilir
(üretilen filtre grafiği string olarak doğrulanır).

## Dosya düzeni

```
optimus_ai/
  config.py            ortam değişkenleri, çalışma alanı
  llm.py               Claude: sohbet, proje üretimi, sahne planlama
  router.py            istek → niyet sınıflandırma
  server.py            FastAPI uçları
  media/ffmpeg.py      ffmpeg sarmalayıcı, kare çıkarma, yetenek tespiti
  media/timeline.py    kurgu derleyicisi  ← projenin çekirdeği
  providers/images.py  görsel sağlayıcıları
  providers/video.py   video sağlayıcıları
  skills/builder.py    proje/oyun üretimi ve diske yazımı
  skills/longvideo.py  sınırsız süreli video hattı
  web/                 arayüz (kurulum gerektirmeyen HTML/CSS/JS)
```
