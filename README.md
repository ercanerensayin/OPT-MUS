# OPT-MUS — Vulkan TPS Iskeleti

Motor kullanmadan, sifirdan C++20 + GLFW + Vulkan ile yazilmis 3D ucuncu sahis
(TPS) oyun iskeleti. **OpenGL kullanilmaz**; pencere `GLFW_NO_API` ile acilir.

## Icerik

| Katman | Dosyalar | Sorumluluk |
|---|---|---|
| `core/` | `Window`, `Application`, `Math` | GLFW penceresi, girdi, oyun dongusu, GLM ayarlari |
| `scene/` | `Camera`, `Player` | TPS yorunge kamerasi, sabit adimli karakter denetleyicisi |
| `render/` | `VulkanInstance`, `VulkanSurface`, `VulkanDevice`, `VulkanSwapchain`, `VulkanPipeline`, `VulkanBuffer`, `Mesh`, `Renderer` | RAII Vulkan altyapisi ve kare senkronizasyonu |
| `shaders/` | `shader.vert`, `shader.frag` | Push constant tabanli, yarim-lambert isiklandirma |

Her Vulkan sinifi tek bir handle grubunun sahibidir, kopyalanamaz, yikiciyla
temizlenir. `Renderer` bunlari dogru olusturma sirasinda tutar; C++ uyeleri ters
sirada yok ettigi icin yikim sirasi (swapchain -> device -> surface -> instance)
ucretsiz olarak dogrudur.

## Uc temel tasarim karari

### 1. Sabit zaman adimi (`core/Application.cpp`)

Mantik/fizik daima 1/60 sn araliklarla ilerler, render ekranin verdigi hizda
calisir. Artik zaman bir birikticide (accumulator) tutulur ve
`alpha = accumulator / kFixedDelta` orani ile onceki/guncel durum arasinda
interpolasyon yapilir — 300 FPS'te de 60 FPS'te de fizik ayni sonucu uretir.

"Olum sarmali" korumasi iki katmanlidir: kare suresi `kMaxFrameTime` (0.25 sn) ile
kirpilir ve kare basina en fazla `kMaxStepsPerFrame` (8) adim atilir.

Fare bakisi bilerek sabit adimda **degil**, kare basina uygulanir: fare girdisi
zaten kareye gore orneklenir, sabit adimda tekrar uygulamak hassasiyeti kare
hizina baglardi.

### 2. Kare senkronizasyonu (`render/Renderer.cpp`)

Ayni anda 2 kare "ucusta" (frames in flight):

| Nesne | Yon | Gorevi |
|---|---|---|
| `inFlight` fence | GPU → CPU | Kare yuvasinin komut tamponu yeniden kullanilmadan once GPU'nun bitirmesini garanti eder |
| `imageAvailable` semaphore | Sunum → GPU | Swapchain goruntusu hazir olmadan renk ekine yazilmaz |
| `renderFinished` semaphore | GPU → Sunum | Cizim bitmeden sunum yapilmaz — **kare basina degil, goruntu basina** tutulur |
| `imagesInFlight` | — | Ayni swapchain goruntusune iki karenin ust uste yazmasini engeller |

`vkQueueSubmit` beklemez; CPU hemen bir sonraki karenin komutlarini kaydeder.
Bekleme yalnizca `beginFrame` basindaki tek `vkWaitForFences` cagrisindadir ve bu,
CPU'nun GPU'yu en fazla 2 kare gecmesini saglayan kasitli geri baskidir.

Fence, goruntu **basariyla alindiktan sonra** sifirlanir; `VK_ERROR_OUT_OF_DATE_KHR`
durumunda sifirlanmadigi icin swapchain yeniden olusturmada kilitlenme olusmaz.

### 3. Vulkan NDC ve Y-flip (`scene/Camera.cpp`, `core/Math.hpp`)

Vulkan NDC'si OpenGL'den iki noktada ayrilir:

* **Derinlik araligi [0, 1]** → `GLM_FORCE_DEPTH_ZERO_TO_ONE`, GLM dahil edilmeden
  once `core/Math.hpp` icinde tanimlanir (CMake de ayni tanimi verir).
* **+Y ekseni ekranda asagi** → `m_projection[1][1] *= -1.0F`.

Y-flip sarim yonunu ters cevirdigi icin pipeline'da on yuz
`VK_FRONT_FACE_CLOCKWISE` olarak isaretlenmistir. (Alternatif yaklasim negatif
viewport yuksekligi kullanmaktir; ikisi ayni anda uygulanirsa etki iptal olur.)

## Derleme

Gereksinimler: CMake 3.20+, C++20 derleyici, Vulkan SDK (veya `libvulkan-dev` +
`glslangValidator`). GLFW ve GLM sistemde yoksa CMake bunlari otomatik indirir.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/opt_mus
```

Shader'lar yapinin bir parcasi olarak SPIR-V'ye derlenir ve calistirilabilir
dosyanin yanindaki `shaders/` klasorune kopyalanir; `main.cpp` klasoru
yurutulebilir dosyaya gore cozer, calisma dizini fark etmez.

Validation layer'lar Debug yapida otomatik acilir, bulunamazsa uyari verip devam
edilir. Release (`NDEBUG`) yapida kapalidir.

## Kontroller

| Tus | Islev |
|---|---|
| `W` `A` `S` `D` | Kamera yonune gore hareket |
| `Shift` | Kosma |
| `Space` | Ziplama |
| Fare | Kamera bakisi |
| `Tab` | Imlec kilidini ac/kapa |
| `P` | Duraklat |
| `Esc` | Cikis |

## Dogrulama durumu

Linux/x86-64 uzerinde `llvmpipe` (Mesa yazilim rasterlestiricisi) ve Xvfb ile
calistirilarak dogrulandi:

* Temiz derleme — `-Wall -Wextra -Wpedantic` ile sifir uyari.
* Render ~120–600 FPS arasinda degisirken sabit adim sayaci **her saniye 60**
  degerinde kaldi; sabit adim/render ayrimi calisiyor.
* Senkronizasyon dogrulamasi (`VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`)
  acikken **sifir hazard, sifir hata**, 5 ardisik pencere yeniden boyutlandirma
  (swapchain recreate) dahil.

Ilk olcumde derinlik ekinde her karede `SYNC-HAZARD-WRITE-AFTER-WRITE` tespit
edildi: derinlik goruntusu swapchain'e ait olmadigi icin hicbir semaphore
tarafindan korunmuyordu. Render pass'in dis bagimliligina `LATE_FRAGMENT_TESTS`
asamasi ve `DEPTH_STENCIL_ATTACHMENT_WRITE` erisimi eklenerek giderildi
(`VulkanSwapchain::createRenderPass`).

Gercek bir GPU surucusunde (NVIDIA/AMD/Intel) ayrica denenmedi.

## Bilinerek ertelenenler

Bu iskelet oyun dongusu ve render altyapisina odaklanir. Bir TPS oyununun
ihtiyac duyacagi su parcalar henuz yok:

* Descriptor set / uniform buffer — sahne verisi simdilik 128 baytlik push
  constant blogunda tasiniyor (kamera konumu bile gecmiyor; sis, gorus uzayi
  derinliginden hesaplaniyor).
* Doku, model yukleme (glTF), malzeme sistemi — geometri kod icinde uretiliyor.
* Bellek alt-tahsisi: her tampon icin ayri `vkAllocateMemory` yapiliyor. Vulkan'in
  `maxMemoryAllocationCount` siniri (~4096) nedeniyle uretimde VMA benzeri bir
  alt-tahsisci sart.
* Gercek carpisma tespiti — zemin `y = 0` duzlemi olarak sabit.
* Golge haritasi, MSAA, karakter animasyonu, ses, ag katmani.
