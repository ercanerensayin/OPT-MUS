import { streamText } from './llm.js';
import { FileBlockParser } from './fileblocks.js';
import { buildPrompt, buildEditPrompt, systemPrompt } from './prompts.js';
import * as store from './store.js';

/** Uretim sonrasi hizli saglik kontrolu. */
function inspect(files, mode) {
  const warnings = [];
  if (!files.length) {
    warnings.push('Model hic dosya uretmedi. Istegi biraz daha acik yazip tekrar deneyin.');
    return warnings;
  }
  for (const file of files) {
    if (file.truncated) {
      warnings.push(`${file.path} yarim kalmis olabilir (token siniri). MAX_TOKENS degerini artirin.`);
    }
  }
  if (mode !== 'code' && !files.some((f) => f.path === 'index.html')) {
    warnings.push('index.html uretilmedi; onizleme calismayabilir.');
  }
  return warnings;
}

/**
 * Bir istegi ucundan uca calistirir: prompt -> model akisi -> dosyalar -> disk.
 *
 * @param {object} options
 * @param {string} options.projectId
 * @param {string} options.prompt
 * @param {'site'|'app'|'code'} options.mode
 * @param {boolean} options.isEdit  mevcut projeyi duzenleme turu mu
 * @param {(event: string, data: object) => void} options.emit
 * @param {AbortSignal} [options.signal]
 */
export async function runGeneration({ projectId, prompt, mode, isEdit, emit, signal }) {
  const existing = isEdit ? await store.readFiles(projectId) : [];
  const userMessage = isEdit
    ? buildEditPrompt(prompt, existing, mode)
    : buildPrompt(prompt, mode);

  const parser = new FileBlockParser({
    onPlan: (delta) => emit('plan', { delta }),
    onFileStart: (path) => emit('file_start', { path }),
    onFileDelta: (path, delta) => emit('file_delta', { path, delta }),
    onFileEnd: (file) => emit('file_end', { path: file.path, content: file.content }),
  });

  let raw = '';
  for await (const delta of streamText({
    system: systemPrompt(mode),
    prompt: userMessage,
    signal,
  })) {
    raw += delta;
    parser.feed(delta);
  }

  const { plan, files } = parser.end();
  const warnings = inspect(files, mode);

  if (files.length) {
    await store.writeFiles(projectId, files);
    await store.addTurn(projectId, {
      role: 'assistant',
      prompt,
      plan,
      files: files.map((f) => f.path),
    });
    await store.appendDataset({
      mode,
      kind: isEdit ? 'edit' : 'create',
      system: systemPrompt(mode),
      instruction: userMessage,
      user_prompt: prompt,
      output: raw,
      files: files.map((f) => f.path),
    });
  }

  const allFiles = await store.readFiles(projectId);
  return { plan, files, allFiles, warnings };
}
