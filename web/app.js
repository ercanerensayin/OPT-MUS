const $ = (id) => document.getElementById(id);

const el = {
  sidebar: $('sidebar'),
  menuBtn: $('menuBtn'),
  providerLabel: $('providerLabel'),
  projectList: $('projectList'),
  newProject: $('newProject'),
  projectTitle: $('projectTitle'),
  downloadBtn: $('downloadBtn'),
  openBtn: $('openBtn'),
  promptInput: $('promptInput'),
  generateBtn: $('generateBtn'),
  examples: $('examples'),
  fileTree: $('fileTree'),
  codeView: $('codeView'),
  statusChip: $('statusChip'),
  preview: $('preview'),
  previewEmpty: $('previewEmpty'),
  previewFrame: $('previewFrame'),
  viewportSwitch: $('viewportSwitch'),
  planBar: $('planBar'),
};

const state = {
  projectId: null,
  mode: 'site',
  files: new Map(),
  active: null,
  busy: false,
};

/* ------------------------------------------------------------------ ag */

async function api(path, options) {
  const res = await fetch(path, {
    headers: { 'content-type': 'application/json' },
    ...options,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `Istek basarisiz (${res.status})`);
  return data;
}

/** SSE bicimindeki akis govdesini olay olay okur. */
async function readEventStream(response, onEvent) {
  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buf = '';

  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    buf += decoder.decode(value, { stream: true });

    let split;
    while ((split = buf.indexOf('\n\n')) !== -1) {
      const block = buf.slice(0, split);
      buf = buf.slice(split + 2);

      let event = 'message';
      let data = '';
      for (const line of block.split('\n')) {
        if (line.startsWith('event:')) event = line.slice(6).trim();
        else if (line.startsWith('data:')) data += line.slice(5).trim();
      }
      if (!data) continue;
      try {
        onEvent(event, JSON.parse(data));
      } catch {
        /* bozuk olay atlanir */
      }
    }
  }
}

/* --------------------------------------------------------------- durum */

function setStatus(text, kind = '') {
  el.statusChip.textContent = text;
  el.statusChip.className = `status-chip ${kind}`;
}

function setBusy(busy) {
  state.busy = busy;
  el.generateBtn.disabled = busy;
  el.generateBtn.textContent = busy ? 'Uretiliyor…' : state.projectId ? 'Guncelle' : 'Uret';
  el.promptInput.disabled = busy;
}

function setPlan(text, { warnings = [], error = '' } = {}) {
  const parts = [];
  if (text) parts.push(text.trim());
  for (const w of warnings) parts.push(`<span class="warn">! ${escapeHtml(w)}</span>`);
  if (error) parts.push(`<span class="err">Hata: ${escapeHtml(error)}</span>`);
  el.planBar.innerHTML = parts.map((p) => (p.startsWith('<span') ? p : escapeHtml(p))).join('\n');
  el.planBar.hidden = parts.length === 0;
}

function escapeHtml(text) {
  return String(text).replace(/[&<>"']/g, (c) =>
    ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c],
  );
}

/* -------------------------------------------------------------- dosyalar */

function renderTree(writingPath = null) {
  el.fileTree.innerHTML = '';
  for (const path of state.files.keys()) {
    const button = document.createElement('button');
    button.textContent = path;
    button.title = path;
    if (path === state.active) button.classList.add('active');
    if (path === writingPath) button.classList.add('writing');
    button.addEventListener('click', () => showFile(path));
    el.fileTree.appendChild(button);
  }
}

function showFile(path) {
  state.active = path;
  el.codeView.textContent = state.files.get(path) ?? '';
  renderTree();
}

function refreshPreview() {
  if (!state.projectId || !state.files.has('index.html')) return;
  const url = `/p/${state.projectId}/index.html?t=${Date.now()}`;
  el.preview.src = url;
  el.openBtn.href = `/p/${state.projectId}/`;
  el.openBtn.hidden = false;
  el.previewEmpty.style.display = 'none';
}

function resetWorkspace() {
  state.files.clear();
  state.active = null;
  el.fileTree.innerHTML = '';
  el.codeView.textContent = 'Uretim baslayinca kod burada akmaya baslar.';
  el.preview.removeAttribute('src');
  el.previewEmpty.style.display = '';
  el.openBtn.hidden = true;
  el.downloadBtn.disabled = true;
  setPlan('');
}

/* -------------------------------------------------------------- projeler */

async function loadProjects() {
  const { projects } = await api('/api/projects');
  el.projectList.innerHTML = '';

  for (const project of projects) {
    const row = document.createElement('div');
    row.className = `project-item${project.id === state.projectId ? ' active' : ''}`;

    const open = document.createElement('span');
    open.textContent = project.title || 'Isimsiz proje';
    open.title = project.title;

    const del = document.createElement('button');
    del.className = 'del';
    del.textContent = '×';
    del.title = 'Projeyi sil';
    del.addEventListener('click', async (event) => {
      event.stopPropagation();
      if (!confirm('Bu proje silinsin mi?')) return;
      await api(`/api/projects/${project.id}`, { method: 'DELETE' });
      if (state.projectId === project.id) newProject();
      loadProjects();
    });

    row.addEventListener('click', () => openProject(project.id));
    row.append(open, del);
    el.projectList.appendChild(row);
  }
}

async function openProject(id) {
  if (state.busy) return;
  const { project, files } = await api(`/api/projects/${id}`);

  state.projectId = project.id;
  state.mode = project.mode;
  state.files = new Map(files.map((f) => [f.path, f.content]));
  el.projectTitle.textContent = project.title || 'Proje';
  el.downloadBtn.disabled = files.length === 0;

  document.querySelectorAll('.mode').forEach((b) => {
    const on = b.dataset.mode === project.mode;
    b.classList.toggle('active', on);
    b.setAttribute('aria-selected', String(on));
  });

  renderTree();
  if (files.length) showFile(files[0].path);
  refreshPreview();
  setPlan('');
  setStatus('yuklendi');
  setBusy(false);
  el.sidebar.classList.remove('open');
  loadProjects();
}

function newProject() {
  state.projectId = null;
  el.projectTitle.textContent = 'Yeni proje';
  el.promptInput.value = '';
  resetWorkspace();
  setStatus('hazir');
  setBusy(false);
  loadProjects();
}

/* --------------------------------------------------------------- uretim */

async function generate() {
  if (state.busy) return;
  const prompt = el.promptInput.value.trim();
  if (!prompt) {
    el.promptInput.focus();
    return;
  }

  setBusy(true);
  setStatus('baglaniyor…', 'busy');
  setPlan('');

  try {
    if (!state.projectId) {
      const { project } = await api('/api/projects', {
        method: 'POST',
        body: JSON.stringify({ prompt, mode: state.mode }),
      });
      state.projectId = project.id;
      el.projectTitle.textContent = project.title;
      resetWorkspace();
    }

    const response = await fetch(`/api/projects/${state.projectId}/generate`, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ prompt }),
    });

    if (!response.ok || !response.body) {
      const data = await response.json().catch(() => ({}));
      throw new Error(data.error || `Uretim baslatilamadi (${response.status})`);
    }

    let plan = '';
    let failed = '';

    await readEventStream(response, (event, data) => {
      switch (event) {
        case 'start':
          setStatus(data.edit ? 'guncelleniyor…' : 'uretiliyor…', 'busy');
          break;

        case 'plan':
          plan += data.delta;
          setPlan(plan);
          break;

        case 'file_start':
          state.files.set(data.path, '');
          state.active = data.path;
          el.codeView.textContent = '';
          renderTree(data.path);
          setStatus(`yaziliyor: ${data.path}`, 'busy');
          break;

        case 'file_delta':
          state.files.set(data.path, (state.files.get(data.path) || '') + data.delta);
          if (state.active === data.path) {
            el.codeView.textContent += data.delta;
            el.codeView.parentElement.scrollTop = el.codeView.parentElement.scrollHeight;
          }
          break;

        case 'file_end':
          state.files.set(data.path, data.content);
          if (state.active === data.path) el.codeView.textContent = data.content;
          renderTree();
          break;

        case 'done':
          setPlan(plan, { warnings: data.warnings });
          setStatus(`bitti · ${data.files.length} dosya`);
          break;

        case 'error':
          failed = data.message;
          break;
      }
    });

    if (failed) throw new Error(failed);

    el.promptInput.value = '';
    el.downloadBtn.disabled = state.files.size === 0;
    refreshPreview();
    await openProjectFilesSilently();
    loadProjects();
  } catch (error) {
    setStatus('hata', 'error');
    setPlan('', { error: error.message });
  } finally {
    setBusy(false);
  }
}

/** Duzenleme turlarindan sonra dosya listesini sunucudaki gercek haliyle esitler. */
async function openProjectFilesSilently() {
  if (!state.projectId) return;
  const { project, files } = await api(`/api/projects/${state.projectId}`);
  state.files = new Map(files.map((f) => [f.path, f.content]));
  el.projectTitle.textContent = project.title || 'Proje';
  renderTree();
  if (state.active && state.files.has(state.active)) showFile(state.active);
  else if (files.length) showFile(files[0].path);
}

/* ----------------------------------------------------------------- olaylar */

el.generateBtn.addEventListener('click', generate);
el.newProject.addEventListener('click', newProject);
el.menuBtn.addEventListener('click', () => el.sidebar.classList.toggle('open'));

el.promptInput.addEventListener('keydown', (event) => {
  if ((event.metaKey || event.ctrlKey) && event.key === 'Enter') generate();
});

el.downloadBtn.addEventListener('click', () => {
  if (state.projectId) window.location.href = `/api/projects/${state.projectId}/zip`;
});

el.examples.addEventListener('click', (event) => {
  if (event.target.tagName !== 'BUTTON') return;
  el.promptInput.value = event.target.textContent;
  el.promptInput.focus();
});

document.querySelectorAll('.mode').forEach((button) => {
  button.addEventListener('click', () => {
    if (state.busy || state.projectId) return; // tur, proje acildiktan sonra sabit
    state.mode = button.dataset.mode;
    document.querySelectorAll('.mode').forEach((b) => {
      const on = b === button;
      b.classList.toggle('active', on);
      b.setAttribute('aria-selected', String(on));
    });
  });
});

el.viewportSwitch.addEventListener('click', (event) => {
  if (event.target.tagName !== 'BUTTON') return;
  el.previewFrame.style.width = event.target.dataset.w;
  el.viewportSwitch.querySelectorAll('button').forEach((b) => b.classList.toggle('active', b === event.target));
});

/* ------------------------------------------------------------------ acilis */

(async function init() {
  try {
    const status = await api('/api/status');
    el.providerLabel.textContent = `${status.provider} · ${status.model}`;
    if (!status.ready) {
      setStatus('saglayici hazir degil', 'error');
      setPlan('', { error: status.hint });
    }
  } catch {
    el.providerLabel.textContent = 'sunucuya ulasilamadi';
  }
  await loadProjects();
})();
