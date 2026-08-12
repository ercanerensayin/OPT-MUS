import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { createZip } from '../server/zip.js';

test('gecerli bir zip uretir ve icerik korunur', () => {
  const files = [
    { path: 'index.html', content: '<h1>Merhaba dunya</h1>\n' },
    { path: 'assets/app.js', content: 'console.log("Turkce icerik: ışğüçö");\n' },
  ];

  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'optmus-zip-'));
  const zipPath = path.join(dir, 'out.zip');
  fs.writeFileSync(zipPath, createZip(files));

  let unzipAvailable = true;
  try {
    execFileSync('unzip', ['-tq', zipPath], { stdio: 'pipe' });
  } catch (error) {
    if (error.code === 'ENOENT') unzipAvailable = false;
    else throw error;
  }

  if (unzipAvailable) {
    execFileSync('unzip', ['-qq', '-o', zipPath, '-d', dir]);
    for (const file of files) {
      assert.equal(fs.readFileSync(path.join(dir, file.path), 'utf8'), file.content);
    }
  }

  const bytes = fs.readFileSync(zipPath);
  assert.equal(bytes.readUInt32LE(0), 0x04034b50, 'yerel baslik imzasi');
  assert.equal(bytes.readUInt16LE(bytes.length - 14), files.length, 'merkezi dizin kayit sayisi');

  fs.rmSync(dir, { recursive: true, force: true });
});
