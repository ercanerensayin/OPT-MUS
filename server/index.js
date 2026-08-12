import express from 'express';
import fs from 'node:fs';
import path from 'node:path';
import { config, providerStatus, ROOT } from './config.js';
import * as store from './store.js';
import { runGeneration } from './pipeline.js';
import { createZip } from './zip.js';
import { complete } from './llm.js';
import { titlePrompt } from './prompts.js';

const app = express();
app.use(express.json({ limit: '4mb' }));
app.use(express.static(path.join(ROOT, 'web')));

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.webp': 'image/webp',
  '.ico': 'image/x-icon',
  '.txt': 'text/plain; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
};

const wrap = (handler) => (req, res, next) => Promise.resolve(handler(req, res)).catch(next);

app.get('/api/status', (req, res) => {
  const status = providerStatus();
  res.json({ provider: config.provider, ...status });
});

app.get('/api/projects', wrap(async (req, res) => {
  res.json({ projects: await store.listProjects() });
}));

app.post('/api/projects', wrap(async (req, res) => {
  const prompt = String(req.body?.prompt || '').trim();
  const mode = ['site', 'app', 'code'].includes(req.body?.mode) ? req.body.mode : 'site';
  if (!prompt) return res.status(400).json({ error: 'Prompt bos olamaz.' });
  const meta = await store.createProject({ prompt, mode });
  res.json({ project: meta });
}));

app.get('/api/projects/:id', wrap(async (req, res) => {
  const meta = await store.getProject(req.params.id);
  if (!meta) return res.status(404).json({ error: 'Proje bulunamadi.' });
  res.json({ project: meta, files: await store.readFiles(meta.id) });
}));

app.delete('/api/projects/:id', wrap(async (req, res) => {
  await store.deleteProject(req.params.id);
  res.json({ ok: true });
}));

app.get('/api/projects/:id/zip', wrap(async (req, res) => {
  const meta = await store.getProject(req.params.id);
  if (!meta) return res.status(404).json({ error: 'Proje bulunamadi.' });
  const files = await store.readFiles(meta.id);
  if (!files.length) return res.status(409).json({ error: 'Projede dosya yok.' });
  const name = (meta.title || 'proje').replace(/[^a-zA-Z0-9_-]+/g, '-').slice(0, 40) || 'proje';
  res.setHeader('content-type', 'application/zip');
  res.setHeader('content-disposition', `attachment; filename="${name}.zip"`);
  res.send(createZip(files));
}));

/** Akisli uretim. Yanit govdesi SSE bicimindedir. */
app.post('/api/projects/:id/generate', wrap(async (req, res) => {
  const meta = await store.getProject(req.params.id);
  if (!meta) return res.status(404).json({ error: 'Proje bulunamadi.' });

  const prompt = String(req.body?.prompt || '').trim();
  if (!prompt) return res.status(400).json({ error: 'Prompt bos olamaz.' });

  const status = providerStatus();
  if (!status.ready) return res.status(503).json({ error: status.hint });

  const existing = await store.readFiles(meta.id);
  const isEdit = existing.length > 0;

  res.setHeader('content-type', 'text/event-stream; charset=utf-8');
  res.setHeader('cache-control', 'no-cache, no-transform');
  res.setHeader('connection', 'keep-alive');
  res.setHeader('x-accel-buffering', 'no');
  res.flushHeaders?.();

  const emit = (event, data) => {
    res.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
  };

  const controller = new AbortController();
  req.on('close', () => controller.abort());

  try {
    if (isEdit) await store.addTurn(meta.id, { role: 'user', prompt });
    emit('start', { projectId: meta.id, mode: meta.mode, edit: isEdit });

    const result = await runGeneration({
      projectId: meta.id,
      prompt,
      mode: meta.mode,
      isEdit,
      emit,
      signal: controller.signal,
    });

    // Ilk uretimden sonra projeye kisa bir baslik ver.
    if (!isEdit && result.files.length) {
      try {
        const answer = await complete({
          system: 'Kisa ve net cevap ver.',
          prompt: titlePrompt(prompt),
        });
        // Model talimati yok sayip proje uretmeye kalkarsa basligi kullanma.
        const usable = !/<(file|plan)\b/i.test(answer);
        const title = (answer.split('\n').find((line) => line.trim() && !line.includes('<')) || '')
          .trim()
          .replace(/^["'*\-\s]+|["']+$/g, '')
          .slice(0, 60);
        if (usable && title) await store.updateProject(meta.id, { title });
      } catch {
        /* baslik uretilemedi, sorun degil */
      }
    }

    emit('done', {
      plan: result.plan,
      files: result.allFiles.map((f) => f.path),
      warnings: result.warnings,
      previewUrl: `/p/${meta.id}/`,
    });
  } catch (error) {
    emit('error', { message: error?.message || 'Bilinmeyen hata.' });
  } finally {
    res.end();
  }
}));

/** Uretilen projenin canli onizlemesi. */
app.get('/p/:id/*', wrap(async (req, res) => {
  const rel = req.params[0] || 'index.html';
  const target = store.resolveProjectFile(req.params.id, rel === '' ? 'index.html' : rel);
  if (!target) return res.status(400).send('Gecersiz yol.');

  let file = target;
  if (!fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    const candidate = path.join(target, 'index.html');
    if (!fs.existsSync(candidate)) {
      return res.status(404).send('Onizlenecek dosya yok. Once bir proje uretin.');
    }
    file = candidate;
  }

  res.setHeader('content-type', MIME[path.extname(file).toLowerCase()] || 'application/octet-stream');
  res.setHeader('cache-control', 'no-store');
  fs.createReadStream(file).pipe(res);
}));

app.get('/p/:id', (req, res) => res.redirect(`/p/${req.params.id}/`));

app.use((error, req, res, next) => {
  console.error(error);
  if (res.headersSent) return next(error);
  res.status(500).json({ error: error?.message || 'Sunucu hatasi.' });
});

app.listen(config.port, () => {
  const status = providerStatus();
  console.log(`\n  OPT-MUS  ->  http://localhost:${config.port}`);
  console.log(`  Saglayici: ${config.provider} (${status.model})`);
  if (!status.ready) console.log(`  Uyari: ${status.hint}`);
  console.log('');
});
