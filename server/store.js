import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { config } from './config.js';
import { isSafePath } from './fileblocks.js';

const META = 'project.json';
const FILES = 'files';

fs.mkdirSync(config.workspaceDir, { recursive: true });

const projectDir = (id) => path.join(config.workspaceDir, id);
const metaPath = (id) => path.join(projectDir(id), META);

function assertId(id) {
  if (!/^[a-z0-9]{6,32}$/.test(String(id || ''))) throw new Error('Gecersiz proje kimligi.');
  return id;
}

export async function createProject({ prompt, mode = 'site', title = '' }) {
  const id = crypto.randomBytes(6).toString('hex');
  const now = new Date().toISOString();
  const meta = {
    id,
    title: title || prompt.trim().slice(0, 60),
    mode,
    createdAt: now,
    updatedAt: now,
    turns: [{ role: 'user', prompt, at: now }],
  };
  await fsp.mkdir(path.join(projectDir(id), FILES), { recursive: true });
  await fsp.writeFile(metaPath(id), JSON.stringify(meta, null, 2));
  return meta;
}

export async function getProject(id) {
  assertId(id);
  try {
    return JSON.parse(await fsp.readFile(metaPath(id), 'utf8'));
  } catch {
    return null;
  }
}

export async function updateProject(id, patch) {
  const meta = await getProject(id);
  if (!meta) throw new Error('Proje bulunamadi.');
  const next = { ...meta, ...patch, id: meta.id, updatedAt: new Date().toISOString() };
  await fsp.writeFile(metaPath(id), JSON.stringify(next, null, 2));
  return next;
}

export async function addTurn(id, turn) {
  const meta = await getProject(id);
  if (!meta) throw new Error('Proje bulunamadi.');
  meta.turns.push({ ...turn, at: new Date().toISOString() });
  return updateProject(id, { turns: meta.turns });
}

export async function listProjects() {
  const entries = await fsp.readdir(config.workspaceDir, { withFileTypes: true });
  const metas = await Promise.all(
    entries.filter((e) => e.isDirectory()).map((e) => getProject(e.name).catch(() => null)),
  );
  return metas
    .filter(Boolean)
    .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt))
    .map(({ id, title, mode, updatedAt }) => ({ id, title, mode, updatedAt }));
}

export async function deleteProject(id) {
  assertId(id);
  await fsp.rm(projectDir(id), { recursive: true, force: true });
}

/** Proje icindeki bir dosyanin mutlak yolunu, kokten disari cikmadigini dogrulayarak dondurur. */
export function resolveProjectFile(id, relPath) {
  assertId(id);
  if (!isSafePath(relPath)) return null;
  const base = path.resolve(projectDir(id), FILES);
  const target = path.resolve(base, relPath);
  return target === base || target.startsWith(base + path.sep) ? target : null;
}

export async function writeFiles(id, files) {
  for (const file of files) {
    const target = resolveProjectFile(id, file.path);
    if (!target) continue;
    await fsp.mkdir(path.dirname(target), { recursive: true });
    await fsp.writeFile(target, file.content);
  }
}

export async function readFiles(id) {
  assertId(id);
  const base = path.join(projectDir(id), FILES);
  const out = [];
  async function walk(dir, prefix) {
    let entries;
    try {
      entries = await fsp.readdir(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
      const abs = path.join(dir, entry.name);
      if (entry.isDirectory()) await walk(abs, rel);
      else out.push({ path: rel, content: await fsp.readFile(abs, 'utf8') });
    }
  }
  await walk(base, '');
  return out.sort((a, b) => {
    if (a.path === 'index.html') return -1;
    if (b.path === 'index.html') return 1;
    return a.path.localeCompare(b.path);
  });
}

/**
 * Egitim verisi toplama: her basarili uretim, kendi modelinizi ince ayarlamak
 * icin kullanilabilecek bir SFT ornegi olarak saklanir.
 */
export async function appendDataset(record) {
  if (!config.collectDataset) return;
  await fsp.mkdir(config.datasetDir, { recursive: true });
  const line = JSON.stringify({ ...record, at: new Date().toISOString() }) + '\n';
  await fsp.appendFile(path.join(config.datasetDir, 'generations.jsonl'), line);
}
