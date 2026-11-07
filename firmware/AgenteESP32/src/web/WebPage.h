/**
 * @file WebPage.h
 * @brief Interfaz web incrustada en flash.
 *
 * ARCHIVO GENERADO AUTOMÁTICAMENTE: NO LO EDITE A MANO.
 * Fuente: web/index.html  ->  python tools/build_web.py
 */
#pragma once

#include <Arduino.h>

// 17542 bytes
static const char kIndexHtml[] PROGMEM = R"AGENTHTML(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Agente ESP32-S3</title>
<style>
:root{--bg:#f4f6f8;--card:#fff;--fg:#1c2530;--mut:#66727f;--line:#dfe4ea;--acc:#0f766e;--acc2:#e6f4f2;--ok:#15803d;--warn:#b45309;--bad:#b91c1c;--on:#f59e0b;--chip:#eef1f4}
@media (prefers-color-scheme:dark){:root{--bg:#0f1418;--card:#182027;--fg:#e6edf3;--mut:#93a1ad;--line:#2a343d;--acc:#2dd4bf;--acc2:#12312d;--ok:#4ade80;--warn:#fbbf24;--bad:#f87171;--on:#fbbf24;--chip:#222c35}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:var(--card);border-bottom:1px solid var(--line);padding:10px 16px;display:flex;flex-wrap:wrap;gap:8px 16px;align-items:center}
header h1{font-size:17px;margin:0;margin-right:auto}
.chip{display:inline-flex;gap:6px;align-items:center;background:var(--chip);border-radius:999px;padding:3px 10px;font-size:13px;color:var(--mut)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mut)}
.dot.ok{background:var(--ok)}.dot.warn{background:var(--warn)}.dot.bad{background:var(--bad)}
main{display:grid;grid-template-columns:minmax(0,1.25fr) minmax(0,1fr);gap:16px;padding:16px;max-width:1300px;margin:0 auto}
@media (max-width:900px){main{grid-template-columns:1fr}}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;margin-bottom:16px}
h2{font-size:15px;margin:0 0 10px;display:flex;align-items:center;gap:8px}
h2 .sp{margin-left:auto}
textarea,input{width:100%;font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:9px 10px}
textarea{resize:vertical;min-height:64px}
button{font:inherit;cursor:pointer;border:1px solid var(--line);background:var(--chip);color:var(--fg);border-radius:8px;padding:6px 12px}
button.pri{background:var(--acc);border-color:var(--acc);color:#fff}
@media (prefers-color-scheme:dark){button.pri{color:#06201d}}
button:disabled{opacity:.55;cursor:wait}
button.sm{padding:3px 9px;font-size:13px}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.ex{font-size:13px;padding:4px 10px;border-radius:999px}
#chat{display:flex;flex-direction:column;gap:10px;max-height:62vh;overflow:auto;margin-bottom:10px}
.msg{border-radius:10px;padding:9px 12px;border:1px solid var(--line)}
.msg.user{align-self:flex-end;background:var(--acc2);border-color:transparent;max-width:85%}
.msg.sys{background:var(--bg)}
.badge{display:inline-block;font-size:12px;font-weight:600;border-radius:6px;padding:1px 7px;margin-right:6px;text-transform:uppercase;letter-spacing:.02em}
.b-executed,.b-valid{background:#dcfce7;color:#166534}.b-scheduled,.b-mixed{background:#dbeafe;color:#1e40af}
.b-clarification{background:#fef3c7;color:#92400e}.b-rejected,.b-error,.b-unsupported{background:#fee2e2;color:#991b1b}
.b-pending{background:var(--chip);color:var(--mut)}
ul.res{margin:6px 0 0;padding-left:18px}ul.res li{margin:2px 0}
.st{font-size:12px;color:var(--mut)}
details{margin-top:6px}summary{cursor:pointer;color:var(--mut);font-size:13px}
pre{background:var(--chip);border-radius:8px;padding:8px;overflow:auto;font-size:12px;max-height:260px;margin:6px 0 0}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
.dev{border:1px solid var(--line);border-radius:10px;padding:8px 10px;display:flex;flex-direction:column;gap:6px}
.dev .n{font-size:13.5px;font-weight:600}.dev .k{font-size:12px;color:var(--mut)}
.lamp{width:14px;height:14px;border-radius:50%;background:var(--line);display:inline-block;vertical-align:middle;margin-right:6px}
.lamp.on{background:var(--on);box-shadow:0 0 10px var(--on)}
.acc{display:flex;justify-content:space-between;align-items:center;border-bottom:1px dashed var(--line);padding:6px 0}
.acc:last-child{border-bottom:0}
.pill{font-size:12px;font-weight:600;border-radius:999px;padding:2px 10px}
.pill.open{background:#fee2e2;color:#991b1b}.pill.closed{background:#dcfce7;color:#166534}
.task{display:flex;gap:10px;align-items:flex-start;border-bottom:1px dashed var(--line);padding:7px 0}
.task:last-child{border-bottom:0}.task .d{flex:1}
.ev{font-size:13px;padding:5px 0;border-bottom:1px dashed var(--line);display:flex;gap:8px}
.ev:last-child{border-bottom:0}.ev .t{color:var(--mut);white-space:nowrap;font-variant-numeric:tabular-nums}
.ev.warning .m{color:var(--warn)}.ev.alert .m{color:var(--bad);font-weight:600}
#events{max-height:320px;overflow:auto}
.empty{color:var(--mut);font-size:13px}
#toasts{position:fixed;right:16px;bottom:16px;display:flex;flex-direction:column;gap:8px;z-index:9;max-width:min(380px,calc(100vw - 32px))}
.toast{background:var(--bad);color:#fff;border-radius:10px;padding:10px 14px;box-shadow:0 6px 20px rgba(0,0,0,.25);font-size:14px}
.toast.warning{background:var(--warn)}
.ctx{font-size:13px;color:var(--warn);margin-bottom:6px}
</style>
</head>
<body>
<header>
  <h1>🏢 Agente ESP32-S3</h1>
  <span class="chip"><span id="dNet" class="dot"></span><span id="tNet">Red…</span></span>
  <span class="chip"><span id="dTime" class="dot"></span><span id="tTime">Hora…</span></span>
  <span class="chip"><span id="dLlm" class="dot"></span><span id="tLlm">LLM…</span></span>
  <span class="chip"><span id="dAl" class="dot"></span><span id="tAl">Alertas 0</span></span>
  <button class="sm" id="btnCfg" title="Token de la API">⚙</button>
</header>

<main>
<div>
  <section>
    <h2>💬 Instrucción en lenguaje natural</h2>
    <div id="chat"><div class="empty">Escriba una orden, por ejemplo: «Enciende la luz de la oficina a las 6:00 p. m.».</div></div>
    <div id="ctx" class="ctx" hidden></div>
    <form id="fCmd">
      <textarea id="txt" maxlength="400" placeholder="¿Qué quiere que haga el sistema?" required></textarea>
      <div class="row" style="margin-top:8px">
        <button class="pri" id="btnSend" type="submit">Enviar</button>
        <span class="st" id="stCmd"></span>
      </div>
    </form>
    <div class="row" style="margin-top:10px" id="examples"></div>
  </section>

  <section>
    <details>
      <summary>🧪 Plan JSON directo (sin LLM) — para pruebas de validación</summary>
      <textarea id="planTxt" style="min-height:150px;margin-top:8px;font-family:ui-monospace,monospace;font-size:12.5px"></textarea>
      <div class="row" style="margin-top:8px">
        <button id="btnVal" type="button">Validar</button>
        <button id="btnRun" type="button" class="pri">Ejecutar</button>
      </div>
      <pre id="planOut" hidden></pre>
    </details>
  </section>
</div>

<div>
  <section>
    <h2>💡 Luces y aspersores <span class="sp"></span></h2>
    <div class="grid" id="outputs"></div>
  </section>
  <section>
    <h2>🚪 Puertas y ventanas</h2>
    <div id="accesses"></div>
  </section>
  <section>
    <h2>⏰ Tareas programadas <span class="sp st" id="taskCount"></span></h2>
    <div id="tasks"></div>
  </section>
  <section>
    <h2>🔔 Eventos y alertas <button class="sm sp" id="btnAck">Reconocer alertas</button></h2>
    <div id="events"></div>
  </section>
</div>
</main>
<div id="toasts"></div>

<script>
"use strict";
const $ = s => document.querySelector(s);
const esc = s => String(s ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"}[c]));
let token = ""; try { token = localStorage.getItem("agentToken") || ""; } catch (e) {}
let lastEvent = 0, firstEvents = true, pendingCtx = null;

async function api(path, opts = {}) {
  const headers = Object.assign({}, opts.body ? {"Content-Type": "application/json"} : {}, token ? {Authorization: "Bearer " + token} : {});
  const r = await fetch(path, Object.assign({}, opts, {headers}));
  let data = null; try { data = await r.json(); } catch (e) {}
  if (r.status === 401) setChip("Al", "bad", "Token requerido (⚙)");
  return {ok: r.ok, status: r.status, data};
}
function setChip(id, cls, text) { $("#d" + id).className = "dot " + cls; $("#t" + id).textContent = text; }
function toast(msg, level) {
  const t = document.createElement("div"); t.className = "toast " + level; t.textContent = msg;
  $("#toasts").appendChild(t); setTimeout(() => t.remove(), 7000);
}
const hhmm = iso => iso ? iso.slice(11, 16) : "";
const dShort = iso => iso ? iso.slice(8, 10) + "/" + iso.slice(5, 7) + " " + iso.slice(11, 16) : "";

// ---------------------------------------------------------------- Ejemplos
const EXAMPLES = [
  "A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta.",
  "Revisa las puertas y, si todas están cerradas, apaga todas las luces.",
  "Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero.",
  "¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?",
  "Enciende los aspersores más tarde.",
  "Enciende la luz de la bodega ahora."
];
EXAMPLES.forEach(e => {
  const b = document.createElement("button"); b.type = "button"; b.className = "ex"; b.textContent = e.length > 46 ? e.slice(0, 44) + "…" : e; b.title = e;
  b.onclick = () => { $("#txt").value = e; $("#txt").focus(); }; $("#examples").appendChild(b);
});
$("#planTxt").value = JSON.stringify({version: 1, type: "plan", summary: "Prueba directa",
  steps: [{action: "set_device", params: {device: "luz_entrada", state: "on"}},
          {action: "set_device", params: {device: "luz_entrada", state: "off"}, when: {type: "after_step", step: 0, seconds: 30}}]}, null, 2);

// ---------------------------------------------------------- Conversación
function addMsg(cls, html) {
  const chat = $("#chat"); if (chat.querySelector(".empty")) chat.innerHTML = "";
  const d = document.createElement("div"); d.className = "msg " + cls; d.innerHTML = html; chat.appendChild(d);
  chat.scrollTop = chat.scrollHeight; return d;
}
function renderResult(r) {
  if (!r) return "<span class='badge b-error'>error</span>Sin respuesta";
  let h = `<span class="badge b-${esc(r.outcome)}">${esc(r.outcome)}</span>${esc(r.message)}`;
  if (r.results && r.results.length) {
    h += "<ul class='res'>" + r.results.map(x => `<li><b>${esc(x.description)}</b> — <span class="st">${esc(x.status)}</span>${x.message ? ": " + esc(x.message) : ""}${x.due_local && x.status !== "scheduled" ? " (" + dShort(x.due_local) + ")" : ""}</li>`).join("") + "</ul>";
  }
  if (r.errors && r.errors.length) {
    h += "<ul class='res'>" + r.errors.map(e => `<li style="color:var(--bad)"><code>${esc(e.code)}</code> ${esc(e.field)} — ${esc(e.message)}</li>`).join("") + "</ul>";
  }
  const extra = {};
  if (r.plan) extra.plan = r.plan;
  if (r.llm) extra.llm = r.llm;
  if (r.llm_output) extra.llm_output = r.llm_output;
  if (Object.keys(extra).length) h += `<details><summary>Ver JSON (plan validado y respuesta del LLM)</summary><pre>${esc(JSON.stringify(extra, null, 2))}</pre></details>`;
  return h;
}
$("#fCmd").onsubmit = async ev => {
  ev.preventDefault();
  const text = $("#txt").value.trim(); if (!text) return;
  addMsg("user", esc(text));
  const body = {text}; if (pendingCtx) body.context = pendingCtx;
  $("#btnSend").disabled = true; $("#stCmd").textContent = "Enviando…"; $("#txt").value = "";
  const box = addMsg("sys", "<span class='badge b-pending'>procesando</span>Consultando al modelo de lenguaje…");
  try {
    const res = await api("/api/command", {method: "POST", body: JSON.stringify(body)});
    if (!res.ok) { box.innerHTML = renderResult({outcome: "error", message: res.data?.error?.message || ("HTTP " + res.status)}); return; }
    const id = res.data.job_id; const t0 = Date.now();
    while (Date.now() - t0 < 120000) {
      await new Promise(r => setTimeout(r, 800));
      const j = await api("/api/jobs?id=" + id);
      if (j.ok && j.data.status === "done") {
        const r = j.data.result; box.innerHTML = renderResult(r);
        pendingCtx = r && r.outcome === "clarification" ? {request: (pendingCtx ? pendingCtx.request + " " : "") + text, question: r.question} : null;
        $("#ctx").hidden = !pendingCtx; if (pendingCtx) $("#ctx").textContent = "↳ Su próxima respuesta se enviará como aclaración de: «" + pendingCtx.request + "»";
        refreshAll(); return;
      }
      $("#stCmd").textContent = "Procesando (" + Math.round((Date.now() - t0) / 1000) + " s)…";
    }
    box.innerHTML = renderResult({outcome: "error", message: "Tiempo de espera agotado"});
  } catch (e) { box.innerHTML = renderResult({outcome: "error", message: "Sin conexión con el dispositivo"}); }
  finally { $("#btnSend").disabled = false; $("#stCmd").textContent = ""; }
};
$("#txt").addEventListener("keydown", e => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); $("#fCmd").requestSubmit(); } });

async function sendPlan(validateOnly) {
  let plan; try { plan = JSON.parse($("#planTxt").value); } catch (e) { $("#planOut").hidden = false; $("#planOut").textContent = "JSON mal formado (navegador): " + e.message; }
  const res = await api(validateOnly ? "/api/plan/validate" : "/api/plan", {method: "POST", body: plan ? JSON.stringify(plan) : $("#planTxt").value});
  $("#planOut").hidden = false; $("#planOut").textContent = "HTTP " + res.status + "\n" + JSON.stringify(res.data, null, 2);
  refreshAll();
}
$("#btnVal").onclick = () => sendPlan(true);
$("#btnRun").onclick = () => sendPlan(false);

// ------------------------------------------------------------- Paneles
async function refreshStatus() {
  const s = await api("/api/status"); if (!s.ok) { if (s.status !== 401) setChip("Net", "bad", "Sin conexión"); return; }
  const d = s.data, n = d.network;
  setChip("Net", n.connected ? "ok" : "warn", n.connected ? `Wi-Fi ${n.ssid} (${n.rssi} dBm)` : (n.ap_active ? "Modo AP " + n.ap_ip : "Sin red"));
  setChip("Time", d.time.synced ? "ok" : "warn", d.time.synced ? dShort(d.time.local) : "Hora sin sincronizar");
  setChip("Llm", d.llm.configured ? (n.connected ? "ok" : "warn") : "bad", d.llm.configured ? d.llm.model : "LLM sin configurar");
  const al = d.agent.alerts_unacknowledged; setChip("Al", al ? "bad" : "ok", "Alertas " + al);
}
async function refreshDevices() {
  const r = await api("/api/devices"); if (!r.ok) return;
  const outs = r.data.devices.filter(x => x.io === "output"), ins = r.data.devices.filter(x => x.io === "input");
  $("#outputs").innerHTML = outs.map(d => `<div class="dev"><div class="n"><span class="lamp ${d.on ? "on" : ""}"></span>${esc(d.name)}</div><div class="k">${esc(d.id)} · GPIO${d.pin}</div><button class="sm" data-dev="${esc(d.id)}" data-on="${d.on ? 0 : 1}">${d.on ? "Apagar" : "Encender"}</button></div>`).join("");
  $("#accesses").innerHTML = ins.map(d => `<div class="acc"><span>${d.kind === "door" ? "🚪" : "🪟"} ${esc(d.name)} <span class="st">GPIO${d.pin}</span></span><span class="pill ${d.open ? "open" : "closed"}">${d.open ? "ABIERTA" : "CERRADA"}</span></div>`).join("");
}
$("#outputs").onclick = async e => {
  const b = e.target.closest("button[data-dev]"); if (!b) return; b.disabled = true;
  const r = await api("/api/devices/set", {method: "POST", body: JSON.stringify({device: b.dataset.dev, state: b.dataset.on === "1" ? "on" : "off"})});
  if (!r.ok) toast(r.data?.message || r.data?.error?.message || "Error", "warning");
  refreshDevices();
};
async function refreshTasks() {
  const r = await api("/api/tasks"); if (!r.ok) return;
  $("#taskCount").textContent = r.data.count + " / " + r.data.capacity;
  $("#tasks").innerHTML = r.data.tasks.length ? r.data.tasks.map(t => `<div class="task"><div class="d"><b>#${t.id}</b> · ${dShort(t.due_local)}${t.repeat === "daily" ? " · diaria" : ""}<br>${esc(t.description)}<div class="st">«${esc(t.origin)}»</div></div><button class="sm" data-cancel="${t.id}">Cancelar</button></div>`).join("") : "<div class='empty'>No hay tareas programadas.</div>";
}
$("#tasks").onclick = async e => {
  const b = e.target.closest("button[data-cancel]"); if (!b) return;
  await api("/api/tasks?id=" + b.dataset.cancel, {method: "DELETE"}); refreshTasks();
};
async function refreshEvents() {
  const r = await api("/api/events?since=" + lastEvent + "&limit=40"); if (!r.ok) return;
  const list = r.data.events; if (!list.length) return;
  const box = $("#events"); if (box.querySelector(".empty")) box.innerHTML = "";
  list.forEach(e => {
    lastEvent = Math.max(lastEvent, e.id);
    const d = document.createElement("div"); d.className = "ev " + e.level;
    d.innerHTML = `<span class="t">${e.time ? hhmm(e.time) : "+" + e.uptime_s + "s"}</span><span class="m">${esc(e.message)}</span>`;
    box.prepend(d);
    if (!firstEvents && e.level !== "info") toast(e.message, e.level);
  });
  while (box.children.length > 80) box.lastChild.remove();
  firstEvents = false;
}
$("#btnAck").onclick = async () => { await api("/api/events/ack", {method: "POST"}); refreshStatus(); };
$("#btnCfg").onclick = () => {
  const t = prompt("Token de la API del dispositivo (vacío si no se usa):", token);
  if (t === null) return; token = t.trim(); try { localStorage.setItem("agentToken", token); } catch (e) {} refreshAll();
};

function refreshAll() { refreshStatus(); refreshDevices(); refreshTasks(); refreshEvents(); }
$("#events").innerHTML = "<div class='empty'>Sin eventos todavía.</div>";
refreshAll();
setInterval(() => { refreshStatus(); refreshDevices(); refreshEvents(); }, 2000);
setInterval(refreshTasks, 5000);
</script>
</body>
</html>
)AGENTHTML";
