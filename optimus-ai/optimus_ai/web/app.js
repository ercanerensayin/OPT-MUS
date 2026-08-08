const $ = (id) => document.getElementById(id);
let caps = null;

// ---- sekmeler ---------------------------------------------------------------

document.querySelectorAll(".tabs button").forEach((button) => {
  button.addEventListener("click", () => {
    document.querySelectorAll(".tabs button").forEach((b) => b.classList.remove("active"));
    document.querySelectorAll(".tab").forEach((t) => t.classList.remove("active"));
    button.classList.add("active");
    $(`tab-${button.dataset.tab}`).classList.add("active");
  });
});

// ---- yetenekler -------------------------------------------------------------

async function loadCapabilities() {
  try {
    caps = await (await fetch("/api/capabilities")).json();
  } catch {
    $("caps").textContent = "sunucuya ulaşılamadı";
    return;
  }
  const parts = [
    `model: ${caps.model} (${caps.effort})`,
    `görsel: ${caps.image_provider}`,
    `video: ${caps.video_provider}`,
    `ffmpeg: ${caps.ffmpeg ? "var" : "YOK"}`,
  ];
  $("caps").innerHTML = parts.join(" · ") +
    (caps.warnings.length ? ` · <span class="warn">${caps.warnings.length} uyarı</span>` : "");
  if (caps.warnings.length) $("caps").title = caps.warnings.join("\n\n");

  const preset = $("video-preset");
  preset.innerHTML = caps.presets
    .map((p) => `<option value="${p}"${p === "youtube" ? " selected" : ""}>${p}</option>`)
    .join("");
}

// ---- sohbet -----------------------------------------------------------------

const history = [];

function addMessage(who, text) {
  const div = document.createElement("div");
  div.className = `msg ${who}`;
  div.innerHTML = `<div class="who">${who === "user" ? "sen" : "optimus"}</div>`;
  const body = document.createElement("div");
  body.textContent = text;
  div.appendChild(body);
  $("transcript").appendChild(div);
  div.scrollIntoView({ block: "end" });
  return body;
}

$("chat-input").addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    $("chat-form").requestSubmit();
  }
});

$("chat-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const message = $("chat-input").value.trim();
  if (!message) return;
  $("chat-input").value = "";
  addMessage("user", message);

  fetch("/api/route", { ...jsonPost({ message }) })
    .then((r) => r.json())
    .then((r) => {
      $("route-hint").textContent =
        r.intent === "chat" ? "" : `İpucu: bu istek için "${r.intent}" sekmesi daha uygun olabilir.`;
    })
    .catch(() => {});

  const target = addMessage("assistant", "");
  let answer = "";
  try {
    const response = await fetch("/api/chat", jsonPost({ message, history }));
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      answer += decoder.decode(value, { stream: true });
      target.textContent = answer;
    }
  } catch (error) {
    target.textContent = `[bağlantı hatası] ${error}`;
    return;
  }
  history.push({ role: "user", content: message });
  history.push({ role: "assistant", content: answer });
});

// ---- yardımcılar ------------------------------------------------------------

function jsonPost(body) {
  return {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  };
}

async function call(url, body) {
  const response = await fetch(url, jsonPost(body));
  const data = await response.json().catch(() => ({ detail: "geçersiz cevap" }));
  if (!response.ok) throw new Error(data.detail || response.statusText);
  return data;
}

async function withBusy(button, work) {
  button.disabled = true;
  try {
    return await work();
  } finally {
    button.disabled = false;
  }
}

// ---- uygulama / oyun --------------------------------------------------------

$("build-go").addEventListener("click", () =>
  withBusy($("build-go"), async () => {
    $("build-out").textContent = "üretiliyor… (büyük projelerde birkaç dakika sürebilir)";
    try {
      const result = await call("/api/build", {
        request: $("build-input").value,
        kind: $("build-kind").value,
      });
      const lines = [
        result.name,
        result.summary,
        "",
        `klasör: ${result.directory}`,
        result.run_command ? `çalıştır: ${result.run_command}` : "çalıştır: dosyayı tarayıcıda aç",
        "",
        "dosyalar:",
        ...result.files.map((f) => `  ${f}`),
      ];
      $("build-out").textContent = lines.join("\n");
      if (result.entrypoint && result.entrypoint.endsWith(".html")) {
        $("build-out").textContent += `\n\naç: /files?path=${result.entrypoint}`;
      }
    } catch (error) {
      $("build-out").textContent = `Hata: ${error.message}`;
    }
  })
);

// ---- görsel -----------------------------------------------------------------

$("image-go").addEventListener("click", () =>
  withBusy($("image-go"), async () => {
    $("image-out").textContent = "üretiliyor…";
    try {
      const result = await call("/api/image", {
        prompt: $("image-input").value,
        shape: $("image-shape").value,
      });
      $("image-out").innerHTML = `<img src="${result.url}" alt="" /><div>${result.path}</div>`;
    } catch (error) {
      $("image-out").textContent = `Hata: ${error.message}`;
    }
  })
);

// ---- video ------------------------------------------------------------------

$("video-go").addEventListener("click", () =>
  withBusy($("video-go"), async () => {
    $("video-out").textContent = "";
    $("video-progress").innerHTML = "iş başlatılıyor…";
    let job;
    try {
      job = await call("/api/video", {
        prompt: $("video-input").value,
        total_seconds: Number($("video-seconds").value),
        preset: $("video-preset").value,
        transition: $("video-transition").value,
        captions: $("video-captions").checked,
      });
    } catch (error) {
      $("video-progress").textContent = `Hata: ${error.message}`;
      return;
    }
    await pollJob(job.job_id);
  })
);

async function pollJob(jobId) {
  for (;;) {
    await new Promise((resolve) => setTimeout(resolve, 2000));
    let state;
    try {
      state = await (await fetch(`/api/jobs/${jobId}`)).json();
    } catch {
      continue;
    }
    const percent = state.total ? Math.round((state.done / state.total) * 100) : 0;
    $("video-progress").innerHTML =
      `${state.stage} — ${state.message}` +
      `<div class="bar"><div style="width:${percent}%"></div></div>`;

    if (state.status === "done") {
      $("video-out").innerHTML =
        `<video controls src="/files?path=${state.output}"></video><div>${state.output}</div>`;
      return;
    }
    if (state.status === "failed") {
      $("video-out").textContent = `Hata: ${state.error}`;
      return;
    }
  }
}

// ---- kurgu ------------------------------------------------------------------

$("edit-input").value = JSON.stringify(
  {
    output: { path: "kurgu.mp4", preset: "youtube", fps: 30 },
    clips: [
      {
        src: "media/klip1.mp4",
        start: 0,
        end: 5,
        filters: { saturation: 1.15, contrast: 1.05, fade_in: 0.5 },
        text: [{ content: "Merhaba", start: 0.5, end: 3 }],
      },
      {
        src: "media/klip2.mp4",
        start: 2,
        end: 8,
        transition: { type: "fade", duration: 0.75 },
        filters: { speed: 1.5 },
      },
    ],
    audio: { music: "media/muzik.mp3", music_volume: 0.3, ducking: true, fade_out: 2 },
  },
  null,
  2
);

async function runEdit(dryRun) {
  const button = dryRun ? $("edit-preview") : $("edit-render");
  await withBusy(button, async () => {
    $("edit-out").textContent = dryRun ? "derleniyor…" : "render ediliyor…";
    let timeline;
    try {
      timeline = JSON.parse($("edit-input").value);
    } catch (error) {
      $("edit-out").textContent = `JSON hatası: ${error.message}`;
      return;
    }
    try {
      const result = await call("/api/edit", { timeline, dry_run: dryRun });
      if (dryRun) {
        $("edit-out").textContent =
          `süre: ${result.duration ?? "bilinmiyor"} sn\n\n${result.command}`;
      } else {
        $("edit-out").innerHTML =
          `<video controls src="${result.url}"></video><div>${result.path}</div>`;
      }
    } catch (error) {
      $("edit-out").textContent = `Hata: ${error.message}`;
    }
  });
}

$("edit-preview").addEventListener("click", () => runEdit(true));
$("edit-render").addEventListener("click", () => runEdit(false));

loadCapabilities();
