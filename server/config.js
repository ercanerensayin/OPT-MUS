import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

// Minik .env okuyucu — tek bir bagimliligi bile bosuna eklememek icin.
function loadDotEnv() {
  const file = path.join(ROOT, '.env');
  if (!fs.existsSync(file)) return;
  for (const raw of fs.readFileSync(file, 'utf8').split('\n')) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const eq = line.indexOf('=');
    if (eq === -1) continue;
    const key = line.slice(0, eq).trim();
    let value = line.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    if (process.env[key] === undefined) process.env[key] = value;
  }
}

loadDotEnv();

const env = process.env;

export const config = {
  port: Number(env.PORT || 3000),
  provider: (env.LLM_PROVIDER || 'anthropic').toLowerCase(),
  maxTokens: Number(env.MAX_TOKENS || 16000),
  temperature: Number(env.TEMPERATURE ?? 0.4),
  collectDataset: env.COLLECT_DATASET !== 'false',
  workspaceDir: path.join(ROOT, 'workspace'),
  datasetDir: path.join(ROOT, 'training', 'data'),

  anthropic: {
    apiKey: env.ANTHROPIC_API_KEY || '',
    model: env.ANTHROPIC_MODEL || 'claude-sonnet-5',
    baseUrl: (env.ANTHROPIC_BASE_URL || 'https://api.anthropic.com').replace(/\/$/, ''),
  },
  openai: {
    apiKey: env.OPENAI_API_KEY || '',
    model: env.OPENAI_MODEL || 'gpt-4o-mini',
    baseUrl: (env.OPENAI_BASE_URL || 'https://api.openai.com/v1').replace(/\/$/, ''),
  },
  ollama: {
    model: env.OLLAMA_MODEL || 'qwen2.5-coder:7b',
    baseUrl: (env.OLLAMA_BASE_URL || 'http://localhost:11434').replace(/\/$/, ''),
  },
};

/** Aktif saglayicinin kullanilabilir olup olmadigini dondurur. */
export function providerStatus() {
  switch (config.provider) {
    case 'anthropic':
      return {
        ready: Boolean(config.anthropic.apiKey),
        model: config.anthropic.model,
        hint: 'ANTHROPIC_API_KEY tanimli degil. .env dosyasina ekleyin.',
      };
    case 'openai':
      return {
        ready: Boolean(config.openai.apiKey),
        model: config.openai.model,
        hint: 'OPENAI_API_KEY tanimli degil. .env dosyasina ekleyin (yerel sunucu icin herhangi bir deger yeterli).',
      };
    case 'ollama':
      return { ready: true, model: config.ollama.model, hint: '' };
    case 'mock':
      return { ready: true, model: 'mock', hint: '' };
    default:
      return { ready: false, model: '?', hint: `Bilinmeyen saglayici: ${config.provider}` };
  }
}
