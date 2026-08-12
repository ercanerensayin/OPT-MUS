/**
 * `LLM_PROVIDER=mock` icin sabit yanit.
 * Amaci: API anahtari olmadan da akis, ayristirma, onizleme ve indirme
 * zincirinin ucundan uca calistigini gorebilmek.
 */
export const MOCK_RESPONSE = `<plan>
- Tek sayfalik bir tanitim sitesi kurulacak.
- Acik/koyu tema destekli CSS degisken sistemi eklenecek.
- Mobil menu ve iletisim formu dogrulamasi JS ile calisir hale getirilecek.
</plan>

<file path="index.html">
<!doctype html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OPT-MUS Demo</title>
<link rel="stylesheet" href="style.css">
</head>
<body>
<header class="site-header">
  <a class="logo" href="#">OPT<span>MUS</span></a>
  <button class="menu-toggle" aria-expanded="false" aria-controls="nav">Menu</button>
  <nav id="nav">
    <a href="#ozellikler">Ozellikler</a>
    <a href="#iletisim">Iletisim</a>
  </nav>
</header>

<main>
  <section class="hero">
    <h1>Tek cumleyle calisan bir site</h1>
    <p>Bu sayfa, sahte saglayici ile uretildi. Gercek modeli baglamak icin .env dosyasina anahtarinizi girin.</p>
    <a class="btn" href="#ozellikler">Kesfet</a>
  </section>

  <section id="ozellikler" class="grid">
    <article><h2>Akisli uretim</h2><p>Kod, model yazarken aninda ekrana duser.</p></article>
    <article><h2>Canli onizleme</h2><p>Uretilen proje aninda iframe icinde calisir.</p></article>
    <article><h2>Kendi modelin</h2><p>Toplanan veriyle kendi modelini ince ayarla.</p></article>
  </section>

  <section id="iletisim">
    <h2>Iletisim</h2>
    <form id="contact" novalidate>
      <label for="email">E-posta</label>
      <input id="email" name="email" type="email" required placeholder="ornek@site.com">
      <button class="btn" type="submit">Gonder</button>
      <p class="status" role="status"></p>
    </form>
  </section>
</main>

<footer>OPT-MUS ile uretildi.</footer>
<script src="app.js"></script>
</body>
</html>
</file>

<file path="style.css">
:root {
  --bg: #f7f8fb;
  --surface: #ffffff;
  --text: #14161c;
  --muted: #5b6172;
  --accent: #4f46e5;
  --border: #e4e6ef;
  --radius: 14px;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0e1016;
    --surface: #161a23;
    --text: #eef0f6;
    --muted: #a2a9bd;
    --accent: #8b85ff;
    --border: #262c3a;
  }
}
* { box-sizing: border-box; }
body {
  margin: 0;
  font: 16px/1.6 system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
  background: var(--bg);
  color: var(--text);
}
.site-header {
  display: flex; align-items: center; gap: 16px;
  padding: 16px 24px; border-bottom: 1px solid var(--border);
  background: var(--surface); position: sticky; top: 0;
}
.logo { font-weight: 700; text-decoration: none; color: var(--text); letter-spacing: .5px; }
.logo span { color: var(--accent); }
nav { margin-left: auto; display: flex; gap: 18px; }
nav a { color: var(--muted); text-decoration: none; }
nav a:hover { color: var(--text); }
.menu-toggle { display: none; margin-left: auto; }
.hero { padding: 88px 24px; text-align: center; }
.hero h1 { font-size: clamp(2rem, 5vw, 3.2rem); margin: 0 0 12px; }
.hero p { color: var(--muted); max-width: 620px; margin: 0 auto 24px; }
.btn {
  display: inline-block; padding: 12px 22px; border: 0; cursor: pointer;
  border-radius: 999px; background: var(--accent); color: #fff;
  text-decoration: none; font-weight: 600;
}
.btn:focus-visible, a:focus-visible, input:focus-visible { outline: 3px solid var(--accent); outline-offset: 2px; }
.grid {
  display: grid; gap: 16px; padding: 24px;
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
  max-width: 1040px; margin: 0 auto;
}
.grid article { background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius); padding: 20px; }
.grid h2 { margin-top: 0; font-size: 1.1rem; }
#iletisim { max-width: 480px; margin: 48px auto 80px; padding: 0 24px; }
form { display: grid; gap: 10px; }
input {
  padding: 12px; border-radius: 10px; border: 1px solid var(--border);
  background: var(--surface); color: var(--text);
}
.status { min-height: 22px; color: var(--muted); margin: 0; }
footer { padding: 24px; text-align: center; color: var(--muted); border-top: 1px solid var(--border); }
@media (max-width: 640px) {
  .menu-toggle { display: block; }
  nav { display: none; width: 100%; flex-direction: column; }
  .site-header.open nav { display: flex; }
  .site-header { flex-wrap: wrap; }
}
</file>

<file path="app.js">
const header = document.querySelector('.site-header');
const toggle = document.querySelector('.menu-toggle');

toggle.addEventListener('click', () => {
  const open = header.classList.toggle('open');
  toggle.setAttribute('aria-expanded', String(open));
});

const form = document.getElementById('contact');
const status = form.querySelector('.status');

form.addEventListener('submit', (event) => {
  event.preventDefault();
  const email = form.email.value.trim();
  const valid = /^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(email);
  status.textContent = valid ? 'Tesekkurler, kaydedildi.' : 'Gecerli bir e-posta girin.';
  status.style.color = valid ? 'var(--accent)' : '#e5484d';
  if (valid) form.reset();
});
</file>`;
