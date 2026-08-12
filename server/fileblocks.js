/**
 * `<plan>` ve `<file path="...">` bloklarini AKIS halinde ayristirir.
 * Model token uretirken dosyalar aninda ekrana dusebilsin diye yazildi.
 */

const FILE_OPEN = /<file\s+path\s*=\s*"([^"]+)"\s*>/i;
const PLAN_OPEN = '<plan>';
const PLAN_CLOSE = '</plan>';
const FILE_CLOSE = '</file>';

/** Yol guvenligi: proje kokunun disina cikan yollari reddeder. */
export function isSafePath(p) {
  if (!p || typeof p !== 'string') return false;
  if (p.length > 200) return false;
  if (p.startsWith('/') || p.startsWith('\\')) return false;
  if (/^[a-zA-Z]:/.test(p)) return false;
  if (p.includes('\0')) return false;
  return !p.split(/[\\/]/).some((seg) => seg === '..' || seg === '');
}

/** Model icerigi yanlislikla markdown blogu icine sararsa temizler. */
export function stripCodeFence(text) {
  const trimmed = text.replace(/^\n+/, '').replace(/\s+$/, '');
  const m = /^```[a-zA-Z0-9#+._-]*\n([\s\S]*?)\n?```$/.exec(trimmed);
  return m ? m[1] : trimmed;
}

export class FileBlockParser {
  /**
   * @param {object} handlers
   * @param {(delta: string) => void} [handlers.onPlan]
   * @param {(path: string) => void} [handlers.onFileStart]
   * @param {(path: string, delta: string) => void} [handlers.onFileDelta]
   * @param {(file: {path: string, content: string}) => void} [handlers.onFileEnd]
   */
  constructor(handlers = {}) {
    this.h = handlers;
    this.buf = '';
    this.state = 'outside';
    this.currentPath = null;
    this.currentContent = '';
    this.plan = '';
    this.files = [];
  }

  feed(chunk) {
    this.buf += chunk;
    let progressed = true;
    while (progressed) {
      progressed = this.state === 'outside' ? this.#stepOutside() : this.#stepInside();
    }
  }

  end() {
    // Model bloklari kapatmadan bitirdiyse elde kalani kurtar.
    if (this.state !== 'outside') this.#close(this.buf, true);
    this.buf = '';
    return { plan: this.plan.trim(), files: this.files };
  }

  #stepOutside() {
    const planAt = this.buf.indexOf(PLAN_OPEN);
    const fileMatch = FILE_OPEN.exec(this.buf);
    const fileAt = fileMatch ? fileMatch.index : -1;

    const openPlanFirst = planAt !== -1 && (fileAt === -1 || planAt < fileAt);
    if (openPlanFirst) {
      this.buf = this.buf.slice(planAt + PLAN_OPEN.length);
      this.state = 'plan';
      return true;
    }
    if (fileAt !== -1) {
      this.buf = this.buf.slice(fileAt + fileMatch[0].length).replace(/^\r?\n/, '');
      this.currentPath = fileMatch[1].trim();
      this.currentContent = '';
      this.state = 'file';
      this.h.onFileStart?.(this.currentPath);
      return true;
    }

    // Etiket disindaki metin atilir; olasi yarim etiketi elde tut.
    const lastLt = this.buf.lastIndexOf('<');
    this.buf = lastLt === -1 ? '' : this.buf.slice(lastLt);
    return false;
  }

  #stepInside() {
    const closeTag = this.state === 'plan' ? PLAN_CLOSE : FILE_CLOSE;
    const at = this.buf.indexOf(closeTag);
    if (at !== -1) {
      const content = this.buf.slice(0, at);
      this.buf = this.buf.slice(at + closeTag.length);
      this.#close(content, false);
      return true;
    }

    // Kapanis etiketi iki parcaya bolunmus olabilir; kuyrugu elde tut.
    const keep = closeTag.length - 1;
    if (this.buf.length > keep) {
      const emit = this.buf.slice(0, this.buf.length - keep);
      this.buf = this.buf.slice(this.buf.length - keep);
      this.#emit(emit);
    }
    return false;
  }

  #emit(text) {
    if (!text) return;
    if (this.state === 'plan') {
      this.plan += text;
      this.h.onPlan?.(text);
    } else {
      this.currentContent += text;
      this.h.onFileDelta?.(this.currentPath, text);
    }
  }

  #close(tail, truncated) {
    this.#emit(tail);
    if (this.state === 'file' && this.currentPath) {
      const file = {
        path: this.currentPath,
        content: stripCodeFence(this.currentContent),
        truncated,
      };
      if (isSafePath(file.path)) {
        this.files.push(file);
        this.h.onFileEnd?.(file);
      }
    }
    this.state = 'outside';
    this.currentPath = null;
    this.currentContent = '';
  }
}

/** Akis olmayan durumlar icin tek seferlik ayristirma. */
export function parseFileBlocks(text) {
  const parser = new FileBlockParser();
  parser.feed(text);
  return parser.end();
}
