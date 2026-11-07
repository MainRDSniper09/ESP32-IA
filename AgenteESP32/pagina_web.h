// =====================================================================
//  pagina_web.h  —  INTERFAZ WEB (HTML + CSS + JavaScript)
// ---------------------------------------------------------------------
//  Página sencilla que guarda el ESP32 en su memoria. Cada 2 segundos
//  pide /api/estado para mostrar luces, puertas, tareas y avisos.
// =====================================================================
#pragma once

const char PAGINA_WEB[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Agente ESP32</title>
<style>
  body { font-family: Arial, sans-serif; background: #f2f4f7; margin: 0; padding: 16px; color: #222; }
  h1 { font-size: 22px; margin: 0 0 4px; }
  .info { color: #666; font-size: 14px; margin-bottom: 16px; }
  .caja { background: white; border-radius: 10px; padding: 14px; margin-bottom: 14px; box-shadow: 0 1px 3px #0002; }
  h2 { font-size: 17px; margin: 0 0 10px; }
  textarea { width: 100%; box-sizing: border-box; height: 60px; font-size: 15px; padding: 8px; }
  button { padding: 7px 12px; border: none; border-radius: 6px; background: #0b7a75; color: white; cursor: pointer; font-size: 14px; }
  button.gris { background: #8a939c; }
  .ejemplo { background: #e3eeed; color: #0b5a56; margin: 4px 4px 0 0; font-size: 13px; }
  pre { background: #f6f6f6; padding: 10px; white-space: pre-wrap; font-size: 13px; border-radius: 6px; }
  .fila { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid #eee; }
  .on { color: #c77700; font-weight: bold; } .off { color: #888; }
  .abierta { color: #c62828; font-weight: bold; } .cerrada { color: #2e7d32; }
  .alerta { color: #c62828; font-weight: bold; }
  .columnas { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
  @media (max-width: 800px) { .columnas { grid-template-columns: 1fr; } }
</style>
</head>
<body>
  <h1>🏢 Agente de automatización ESP32-S3</h1>
  <div class="info">Hora: <span id="hora">...</span> · Wi-Fi: <span id="wifi">...</span></div>

  <div class="caja">
    <h2>1. Escribe una orden</h2>
    <textarea id="texto" placeholder="Ejemplo: Enciende la luz de la oficina a las 6:00 p. m."></textarea>
    <p id="pendiente" class="alerta"></p>
    <button onclick="enviarOrden()">Enviar orden</button>
    <div id="ejemplos"></div>
    <h2 style="margin-top:14px">Respuesta</h2>
    <pre id="respuesta">Aquí aparecerá lo que hizo el sistema.</pre>
  </div>

  <div class="columnas">
    <div class="caja"><h2>💡 Luces y aspersores</h2><div id="salidas"></div></div>
    <div class="caja"><h2>🚪 Puertas y ventanas</h2><div id="entradas"></div></div>
    <div class="caja"><h2>⏰ Tareas programadas</h2><div id="tareas"></div></div>
    <div class="caja">
      <h2>🔔 Avisos <button class="gris" onclick="alertasVistas()">Marcar alertas como vistas</button></h2>
      <div id="avisos"></div>
    </div>
  </div>

<script>
// Órdenes de ejemplo (los escenarios del taller)
const EJEMPLOS = [
  "A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta",
  "Revisa las puertas y, si todas están cerradas, apaga todas las luces",
  "Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero",
  "¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?",
  "Enciende los aspersores más tarde"
];
for (const e of EJEMPLOS) {
  const b = document.createElement("button");
  b.className = "ejemplo"; b.textContent = e;
  b.onclick = () => document.getElementById("texto").value = e;
  document.getElementById("ejemplos").appendChild(b);
}

// Si el sistema hizo una pregunta, guardamos la orden original
let ordenPendiente = null;

async function enviarOrden() {
  let texto = document.getElementById("texto").value.trim();
  if (!texto) return;
  if (ordenPendiente) {   // la respuesta completa la orden anterior
    texto = ordenPendiente + ". Aclaración: " + texto;
    ordenPendiente = null;
  }
  document.getElementById("respuesta").textContent = "Pensando... (consultando al LLM)";
  document.getElementById("pendiente").textContent = "";
  const r = await fetch("/api/orden", { method: "POST", body: JSON.stringify({ texto }) });
  const datos = await r.json();
  mostrarRespuesta(datos);
  if (datos.tipo === "aclaracion") {
    ordenPendiente = texto;
    document.getElementById("pendiente").textContent = "❓ " + datos.pregunta + " (escribe tu respuesta y envía)";
  }
  document.getElementById("texto").value = "";
  actualizar();
}

function mostrarRespuesta(d) {
  let t = (d.ok ? "✅ " : "❌ ") + (d.tipo || "") + "\n";
  if (d.mensaje) t += d.mensaje + "\n";
  if (d.pregunta) t += "Pregunta: " + d.pregunta + "\n";
  if (d.resultados) t += "\nResultados:\n - " + d.resultados.join("\n - ") + "\n";
  if (d.errores && d.errores.length) t += "\nErrores:\n - " + d.errores.join("\n - ") + "\n";
  if (d.json_del_llm) t += "\nJSON que devolvió el LLM:\n" + d.json_del_llm;
  document.getElementById("respuesta").textContent = t;
}

// Botones de encender/apagar: envían un plan JSON directo (sin LLM)
async function cambiar(id, accion) {
  const plan = { tipo: "plan", acciones: [{ accion: accion, dispositivo: id }] };
  await fetch("/api/plan", { method: "POST", body: JSON.stringify(plan) });
  actualizar();
}

async function cancelar(id) {
  await fetch("/api/cancelar", { method: "POST", body: JSON.stringify({ id }) });
  actualizar();
}

async function alertasVistas() {
  await fetch("/api/alertas/vistas", { method: "POST" });
  actualizar();
}

// Evita que un texto se interprete como HTML
function seguro(s) { const d = document.createElement("div"); d.textContent = s; return d.innerHTML; }

async function actualizar() {
  const e = await (await fetch("/api/estado")).json();
  document.getElementById("hora").textContent = e.hora;
  document.getElementById("wifi").textContent = e.wifi;

  let salidas = "", entradas = "";
  for (const d of e.dispositivos) {
    if (d.tipo === "luz" || d.tipo === "aspersor") {
      salidas += `<div class="fila"><span>${d.nombre} <small>(GPIO ${d.pin})</small></span>
        <span class="${d.encendido ? "on" : "off"}">${d.encendido ? "ENCENDIDO" : "apagado"}</span>
        <button onclick="cambiar('${d.id}','${d.encendido ? "apagar" : "encender"}')">${d.encendido ? "Apagar" : "Encender"}</button></div>`;
    } else {
      entradas += `<div class="fila"><span>${d.nombre} <small>(GPIO ${d.pin})</small></span>
        <span class="${d.abierto ? "abierta" : "cerrada"}">${d.abierto ? "ABIERTA" : "cerrada"}</span></div>`;
    }
  }
  document.getElementById("salidas").innerHTML = salidas;
  document.getElementById("entradas").innerHTML = entradas;

  document.getElementById("tareas").innerHTML = e.tareas.length === 0 ? "No hay tareas." :
    e.tareas.map(t => `<div class="fila"><span><b>#${t.id}</b> ${t.cuando} — ${t.accion} ${seguro(t.objetivo || t.mensaje)}
      ${t.condicion ? "<small>(si " + t.condicion + ")</small>" : ""}</span>
      <button class="gris" onclick="cancelar(${t.id})">Cancelar</button></div>`).join("");

  document.getElementById("avisos").innerHTML = e.avisos.length === 0 ? "Sin avisos." :
    e.avisos.map(a => `<div class="fila ${a.alerta ? "alerta" : ""}"><span>${a.hora} — ${seguro(a.texto)}</span></div>`).join("");
}

actualizar();
setInterval(actualizar, 2000);
</script>
</body>
</html>
)HTML";
