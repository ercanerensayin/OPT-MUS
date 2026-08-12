import { config } from './config.js';
import { MOCK_RESPONSE } from './mock.js';

/** HTTP govdesini satir satir okuyan yardimci (SSE ve NDJSON icin). */
async function* readLines(response) {
  const decoder = new TextDecoder();
  let buf = '';
  for await (const chunk of response.body) {
    buf += decoder.decode(chunk, { stream: true });
    let nl;
    while ((nl = buf.indexOf('\n')) !== -1) {
      yield buf.slice(0, nl).replace(/\r$/, '');
      buf = buf.slice(nl + 1);
    }
  }
  if (buf.trim()) yield buf.trim();
}

async function ensureOk(response, provider) {
  if (response.ok) return;
  let detail = '';
  try {
    detail = (await response.text()).slice(0, 600);
  } catch {
    /* govde okunamadi */
  }
  throw new Error(`${provider} hatasi (${response.status}): ${detail || response.statusText}`);
}

async function* streamAnthropic({ system, prompt, signal }) {
  const { apiKey, model, baseUrl } = config.anthropic;
  if (!apiKey) throw new Error('ANTHROPIC_API_KEY tanimli degil.');

  const res = await fetch(`${baseUrl}/v1/messages`, {
    method: 'POST',
    signal,
    headers: {
      'content-type': 'application/json',
      'x-api-key': apiKey,
      'anthropic-version': '2023-06-01',
    },
    body: JSON.stringify({
      model,
      max_tokens: config.maxTokens,
      temperature: config.temperature,
      system,
      messages: [{ role: 'user', content: prompt }],
      stream: true,
    }),
  });
  await ensureOk(res, 'Anthropic');

  for await (const line of readLines(res)) {
    if (!line.startsWith('data:')) continue;
    const payload = line.slice(5).trim();
    if (!payload || payload === '[DONE]') continue;
    let event;
    try {
      event = JSON.parse(payload);
    } catch {
      continue;
    }
    if (event.type === 'content_block_delta' && event.delta?.type === 'text_delta') {
      yield event.delta.text;
    } else if (event.type === 'error') {
      throw new Error(`Anthropic akis hatasi: ${event.error?.message || 'bilinmiyor'}`);
    }
  }
}

async function* streamOpenAI({ system, prompt, signal }) {
  const { apiKey, model, baseUrl } = config.openai;

  const res = await fetch(`${baseUrl}/chat/completions`, {
    method: 'POST',
    signal,
    headers: {
      'content-type': 'application/json',
      authorization: `Bearer ${apiKey || 'local'}`,
    },
    body: JSON.stringify({
      model,
      max_tokens: config.maxTokens,
      temperature: config.temperature,
      messages: [
        { role: 'system', content: system },
        { role: 'user', content: prompt },
      ],
      stream: true,
    }),
  });
  await ensureOk(res, 'OpenAI-uyumlu saglayici');

  for await (const line of readLines(res)) {
    if (!line.startsWith('data:')) continue;
    const payload = line.slice(5).trim();
    if (!payload || payload === '[DONE]') continue;
    let event;
    try {
      event = JSON.parse(payload);
    } catch {
      continue;
    }
    const delta = event.choices?.[0]?.delta?.content;
    if (delta) yield delta;
  }
}

async function* streamOllama({ system, prompt, signal }) {
  const { model, baseUrl } = config.ollama;

  const res = await fetch(`${baseUrl}/api/chat`, {
    method: 'POST',
    signal,
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({
      model,
      stream: true,
      options: { temperature: config.temperature, num_predict: config.maxTokens },
      messages: [
        { role: 'system', content: system },
        { role: 'user', content: prompt },
      ],
    }),
  });
  await ensureOk(res, 'Ollama');

  for await (const line of readLines(res)) {
    if (!line.trim()) continue;
    let event;
    try {
      event = JSON.parse(line);
    } catch {
      continue;
    }
    if (event.error) throw new Error(`Ollama hatasi: ${event.error}`);
    const delta = event.message?.content;
    if (delta) yield delta;
  }
}

/** API anahtari olmadan tum akisi ucundan uca denemek icin. */
async function* streamMock() {
  for (const piece of MOCK_RESPONSE.match(/[\s\S]{1,60}/g) || []) {
    await new Promise((r) => setTimeout(r, 12));
    yield piece;
  }
}

/**
 * Secili saglayicidan metin akisi uretir.
 * @returns {AsyncGenerator<string>}
 */
export function streamText(options) {
  switch (config.provider) {
    case 'anthropic':
      return streamAnthropic(options);
    case 'openai':
      return streamOpenAI(options);
    case 'ollama':
      return streamOllama(options);
    case 'mock':
      return streamMock(options);
    default:
      throw new Error(`Bilinmeyen saglayici: ${config.provider}`);
  }
}

/** Akisi toplayip tek parca metin dondurur. */
export async function complete(options) {
  let out = '';
  for await (const delta of streamText(options)) out += delta;
  return out;
}
