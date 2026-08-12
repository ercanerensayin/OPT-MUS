/**
 * Sistem promptlari — bu sistemin "beyni".
 *
 * Cikti formati bilerek JSON degil, etiket tabanli secildi:
 *  - akis (streaming) sirasinda ayristirilabilir, yarim JSON sorunu olmaz,
 *  - kacis karakteri gerektirmez, yani uretilen kod bozulmaz,
 *  - kucuk / ince ayarlanmis modeller de bu formati kolayca ogrenir.
 */

const OUTPUT_CONTRACT = `
CIKTI FORMATI (kesinlikle uy):

<plan>
Ne insa edecegini 2-4 madde ile ozetle. Kod yazma.
</plan>

<file path="index.html">
...dosyanin tam icerigi...
</file>

<file path="assets/app.js">
...dosyanin tam icerigi...
</file>

Kurallar:
- Her dosya tam ve calisir halde olmali. "..." , "// geri kalani ayni" gibi
  kisaltmalar KESINLIKLE yasak.
- Dosya iceriklerini markdown kod bloguna (\`\`\`) sarma.
- <file> etiketleri disina aciklama yazma.
- Yollar proje kokune gore goreli olmali; basta / veya ../ olmamali.
`.trim();

const DESIGN_RULES = `
TASARIM STANDARDI (uretilen her arayuz icin gecerli):
- Derleme adimi olmayan, tarayicida dogrudan calisan HTML/CSS/JS uret.
  Giris noktasi her zaman index.html olmali.
- CSS degiskenleri ile bir tasarim sistemi kur (renk, aralik, yaricap, golge).
  Acik ve koyu tema destegi ver (prefers-color-scheme).
- Modern ve dolu bir sayfa uret: gercekci metinler, gercekci ornek veriler.
  "Lorem ipsum" ve "Baslik buraya" gibi yer tutucular kullanma.
- Responsive olsun: flex/grid, mobilde tek kolona dusen duzen, 320px'e kadar
  yatay kaydirma olmadan calissin.
- Erisilebilirlik: semantik etiketler, form alanlarinda label, gorsellerde alt,
  klavye ile gezilebilir odak halkalari, yeterli kontrast.
- Etkilesim gercekten calissin: menu acilir, sekmeler geciser, form dogrulanir,
  filtre suzer. Tiklaninca hicbir sey yapmayan buton birakma.
- Gorsel gerektiginde harici URL yerine satir ici SVG veya CSS gradyan kullan.
- Harici kutuphaneye yalnizca gercekten gerekliyse basvur; gerekiyorsa CDN
  <script> etiketi kullan, paket yoneticisi varsayma.
`.trim();

export function systemPrompt(mode = 'site') {
  const role =
    mode === 'code'
      ? `Sen kidemli bir yazilim muhendisisin. Kullanicinin istedigi programi,
betigi veya kutuphaneyi eksiksiz, calisir ve okunabilir sekilde yazarsin.
Uygun oldugunda README ve testleri de uretirsin.`
      : `Sen kidemli bir urun muhendisi ve arayuz tasarimcisisin. Tek bir cumleden
yola cikarak yayina hazir, guzel gorunen web siteleri ve web uygulamalari
uretirsin.`;

  return [
    role,
    mode === 'code' ? '' : DESIGN_RULES,
    OUTPUT_CONTRACT,
    `Once <plan> yaz, sonra dosyalari uret. Aciklama, ozur veya soru sorma;
dogrudan uret. Kullanici az bilgi verdiyse eksikleri kendin makul sekilde
tamamla.`,
  ]
    .filter(Boolean)
    .join('\n\n');
}

/** Ilk uretim icin kullanici mesaji. */
export function buildPrompt(userPrompt, mode = 'site') {
  const kind =
    mode === 'code'
      ? 'Asagidaki istegi karsilayan projeyi yaz.'
      : 'Asagidaki istegi karsilayan web projesini bastan uret.';
  return `${kind}\n\nISTEK:\n${userPrompt.trim()}`;
}

/**
 * Duzenleme turu icin kullanici mesaji.
 * Mevcut dosyalar baglama konur, modelden yalnizca degisen dosyalari
 * tam icerikleriyle geri vermesi istenir.
 */
export function buildEditPrompt(userPrompt, files, mode = 'site') {
  const snapshot = files
    .map((f) => `<file path="${f.path}">\n${f.content}\n</file>`)
    .join('\n\n');

  return [
    'Asagida projenin su anki hali var.',
    '',
    snapshot,
    '',
    'DEGISIKLIK ISTEGI:',
    userPrompt.trim(),
    '',
    `Yalnizca degistirdigin veya yeni ekledigin dosyalari dondur; dokunmadigin
dosyalari tekrar yazma. Donduklerini yine tam icerikleriyle, ayni <file>
formatinda ver. Mevcut tasarim dilini ve kod stilini koru.`,
  ].join('\n');
}

/** Model ismi/adi verilmemis projeler icin kisa baslik uretimi. */
export function titlePrompt(userPrompt) {
  return `Bu istek icin en fazla 4 kelimelik, Turkce, tirnaksiz bir proje adi yaz.
Sadece adi yaz, baska hicbir sey yazma.\n\nISTEK: ${userPrompt.trim()}`;
}
