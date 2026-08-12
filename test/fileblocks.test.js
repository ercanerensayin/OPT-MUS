import test from 'node:test';
import assert from 'node:assert/strict';
import { FileBlockParser, parseFileBlocks, isSafePath, stripCodeFence } from '../server/fileblocks.js';

test('plan ve dosyalari ayristirir', () => {
  const { plan, files } = parseFileBlocks(
    '<plan>\nIki dosya.\n</plan>\n\n<file path="index.html">\n<h1>Merhaba</h1>\n</file>\n' +
      '<file path="assets/app.js">\nconsole.log(1);\n</file>',
  );
  assert.equal(plan, 'Iki dosya.');
  assert.deepEqual(
    files.map((f) => f.path),
    ['index.html', 'assets/app.js'],
  );
  assert.equal(files[0].content, '<h1>Merhaba</h1>');
  assert.equal(files[1].content, 'console.log(1);');
});

test('parcali akista da ayni sonucu verir', () => {
  const source =
    '<plan>Plan</plan><file path="a.css">body { color: red; }\n.x::after { content: "</fil"; }</file>';
  const events = [];
  const parser = new FileBlockParser({
    onFileStart: (p) => events.push(['start', p]),
    onFileEnd: (f) => events.push(['end', f.path]),
  });

  // Tek karakterlik parcalar: kapanis etiketinin bolunmesi dahil en kotu durum.
  for (const ch of source) parser.feed(ch);
  const { plan, files } = parser.end();

  assert.equal(plan, 'Plan');
  assert.equal(files.length, 1);
  assert.equal(files[0].content, 'body { color: red; }\n.x::after { content: "</fil"; }');
  assert.deepEqual(events, [
    ['start', 'a.css'],
    ['end', 'a.css'],
  ]);
});

test('akis deltalari birlestiginde dosya icerigini verir', () => {
  let streamed = '';
  const parser = new FileBlockParser({ onFileDelta: (_p, d) => (streamed += d) });
  const source = '<file path="a.txt">\nbir\niki\nuc\n</file>';
  for (let i = 0; i < source.length; i += 5) parser.feed(source.slice(i, i + 5));
  parser.end();
  assert.equal(streamed.replace(/\s+$/, ''), 'bir\niki\nuc');
});

test('yarim kalan blok kurtarilir ve isaretlenir', () => {
  const { files } = parseFileBlocks('<file path="a.js">const x = 1;');
  assert.equal(files.length, 1);
  assert.equal(files[0].content, 'const x = 1;');
  assert.equal(files[0].truncated, true);
});

test('guvensiz yollar reddedilir', () => {
  for (const bad of ['../gizli', '/etc/passwd', 'a/../../b', 'C:\\x', '', 'a//b']) {
    assert.equal(isSafePath(bad), false, `${bad} guvenli sayilmamali`);
  }
  for (const good of ['index.html', 'assets/app.js', 'src/lib/util.js']) {
    assert.equal(isSafePath(good), true, `${good} guvenli sayilmali`);
  }
});

test('guvensiz yollu bloklar dosya listesine girmez', () => {
  const { files } = parseFileBlocks('<file path="../kacis.txt">x</file><file path="ok.txt">y</file>');
  assert.deepEqual(
    files.map((f) => f.path),
    ['ok.txt'],
  );
});

test('markdown kod blogu sarmalayicisi temizlenir', () => {
  assert.equal(stripCodeFence('```js\nconst a = 1;\n```'), 'const a = 1;');
  assert.equal(stripCodeFence('const a = 1;'), 'const a = 1;');
  // Icinde ``` gecen ama sarmalanmamis icerik bozulmamali.
  assert.equal(stripCodeFence('# Baslik\n\n```\nkod\n```\n\nson'), '# Baslik\n\n```\nkod\n```\n\nson');
});
