#include "LlmClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../config/AppConfig.h"
#include "../config/Secrets.h"

namespace agent {

namespace {

const char* kAnthropicUrl = "https://api.anthropic.com/v1/messages";
const char* kAnthropicVersion = "2026-10-21";
const char* kAnthropicFallbackBeta = "server-side-fallback-2026-07-01";
const char* kOpenAiUrl = "https://api.openai.com/v1/chat/completions";

bool isAnthropic() { return SECRET_LLM_PROVIDER == LLM_PROVIDER_ANTHROPIC; }

const char* endpoint() {
  if (strlen(SECRET_LLM_ENDPOINT) > 0) return SECRET_LLM_ENDPOINT;
  return isAnthropic() ? kAnthropicUrl : kOpenAiUrl;
}

/**
 * Los fallbacks del servidor ("fallbacks":"default") solo existen en ciertos
 * modelos de Claude; si el usuario configura otro modelo no se envían.
 */
bool supportsServerFallback(const char* model) {
  return strncmp(model, "claude-opus-5", 13) == 0 || strncmp(model, "claude-fable-5", 14) == 0 ||
         strcmp(model, "claude-sonnet-5-5") == 0;
}

bool isTransient(int status) { return status < 0 || status == 408 || status == 429 || status >= 500; }

}  // namespace

bool LlmClient::configured() const {
  // Ollama/LM Studio locales no requieren clave, pero sí un endpoint propio.
  return strlen(SECRET_LLM_API_KEY) > 0 || (!isAnthropic() && strlen(SECRET_LLM_ENDPOINT) > 0);
}

const char* LlmClient::providerName() const { return isAnthropic() ? "anthropic" : "openai_compat"; }

const char* LlmClient::model() const { return SECRET_LLM_MODEL; }

LlmResult LlmClient::complete(const std::string& system, const std::string& user) {
  LlmResult r;
  if (!configured()) {
    r.errorCode = "llm_not_configured";
    r.errorMessage = "No hay clave de API del LLM configurada (config/agente_secrets.h)";
    return r;
  }
  if (WiFi.status() != WL_CONNECTED) {
    r.errorCode = "no_network";
    r.errorMessage = "Sin conexión Wi-Fi: no es posible consultar el LLM";
    return r;
  }
  const uint32_t start = millis();
  for (uint8_t attempt = 1; attempt <= AGENT_LLM_MAX_RETRIES + 1; ++attempt) {
    r = requestOnce(system, user);
    r.attempts = attempt;
    if (r.ok || !isTransient(r.httpStatus) || r.errorCode == "llm_refusal") break;
    Serial.printf("[llm] Error transitorio (%d), reintentando...\n", r.httpStatus);
    vTaskDelay(pdMS_TO_TICKS(1500 * attempt));
  }
  r.latencyMs = millis() - start;
  return r;
}

LlmResult LlmClient::requestOnce(const std::string& system, const std::string& user) {
  LlmResult r;
  const char* url = endpoint();
  const bool https = strncmp(url, "https://", 8) == 0;

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  if (https) {
    if (strlen(SECRET_LLM_ROOT_CA) > 0) secureClient.setCACert(SECRET_LLM_ROOT_CA);
    else secureClient.setInsecure();  // cifrado sin verificación de identidad (ver docs/SECURITY_NOTES.md)
  }

  HTTPClient http;
  bool begun = https ? http.begin(secureClient, url) : http.begin(plainClient, url);
  if (!begun) {
    r.httpStatus = -1;
    r.errorCode = "llm_network";
    r.errorMessage = "URL del LLM inválida";
    return r;
  }
  http.setConnectTimeout(10000);
  http.setTimeout(AGENT_LLM_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  // ------------------------------------------------------- Cuerpo de la solicitud
  JsonDocument req;
  req["model"] = SECRET_LLM_MODEL;
  if (isAnthropic()) {
    http.addHeader("x-api-key", SECRET_LLM_API_KEY);
    http.addHeader("anthropic-version", kAnthropicVersion);
    req["max_tokens"] = AGENT_LLM_MAX_TOKENS;
    req["system"] = system;
    JsonObject msg = req["messages"].to<JsonArray>().add<JsonObject>();
    msg["role"] = "user";
    msg["content"] = user;
    if (strlen(AGENT_LLM_EFFORT) > 0) req["output_config"]["effort"] = AGENT_LLM_EFFORT;
    if (supportsServerFallback(SECRET_LLM_MODEL)) {
      // Si el modelo rechaza la solicitud por sus clasificadores, el servidor la
      // reintenta automáticamente con el modelo de respaldo recomendado.
      http.addHeader("anthropic-beta", kAnthropicFallbackBeta);
      req["fallbacks"] = "default";
    }
  } else {
    if (strlen(SECRET_LLM_API_KEY) > 0) http.addHeader("Authorization", String("Bearer ") + SECRET_LLM_API_KEY);
    JsonArray msgs = req["messages"].to<JsonArray>();
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = system;
    JsonObject usr = msgs.add<JsonObject>();
    usr["role"] = "user";
    usr["content"] = user;
#if AGENT_OPENAI_JSON_MODE
    req["response_format"]["type"] = "json_object";
#endif
  }
  String body;
  serializeJson(req, body);
  req.clear();

  // ------------------------------------------------------------------ Envío
  Serial.printf("[llm] POST %s (%u bytes)\n", url, body.length());
  int status = http.POST(body);
  body = String();  // liberar memoria antes de leer la respuesta
  r.httpStatus = status;
  if (status <= 0) {
    r.errorCode = "llm_network";
    r.errorMessage = std::string("Fallo de conexión con el LLM: ") + HTTPClient::errorToString(status).c_str();
    http.end();
    return r;
  }
  String payload = http.getString();
  http.end();

  // --------------------------------------------------------------- Respuesta
  JsonDocument filter;
  if (isAnthropic()) {
    filter["content"][0]["type"] = true;
    filter["content"][0]["text"] = true;
    filter["stop_reason"] = true;
    filter["model"] = true;
  } else {
    filter["choices"][0]["message"]["content"] = true;
    filter["choices"][0]["finish_reason"] = true;
    filter["model"] = true;
  }
  filter["error"]["message"] = true;
  filter["error"]["type"] = true;

  JsonDocument res;
  DeserializationError err = deserializeJson(res, payload, DeserializationOption::Filter(filter));
  if (err) {
    r.errorCode = "llm_bad_response";
    r.errorMessage = std::string("Respuesta del proveedor ilegible (HTTP ") + std::to_string(status) + ")";
    return r;
  }
  const char* model = res["model"] | SECRET_LLM_MODEL;
  r.model = model;

  if (status != 200) {
    r.errorCode = "llm_http";
    const char* m = res["error"]["message"] | "error desconocido";
    r.errorMessage = "El proveedor del LLM respondió HTTP " + std::to_string(status) + ": " + m;
    return r;
  }

  if (isAnthropic()) {
    const char* stop = res["stop_reason"] | "";
    // Revisar stop_reason ANTES de leer el contenido.
    if (strcmp(stop, "refusal") == 0) {
      r.errorCode = "llm_refusal";
      r.errorMessage = "El modelo declinó la solicitud";
      return r;
    }
    if (strcmp(stop, "max_tokens") == 0) {
      r.errorCode = "llm_truncated";
      r.errorMessage = "La respuesta del modelo quedó truncada (max_tokens)";
      return r;
    }
    for (JsonObject block : res["content"].as<JsonArray>()) {
      if (block["type"] == "text") r.text += block["text"].as<const char*>() ? block["text"].as<const char*>() : "";
    }
  } else {
    const char* finish = res["choices"][0]["finish_reason"] | "";
    if (strcmp(finish, "length") == 0) {
      r.errorCode = "llm_truncated";
      r.errorMessage = "La respuesta del modelo quedó truncada";
      return r;
    }
    const char* content = res["choices"][0]["message"]["content"] | "";
    r.text = content;
  }

  if (r.text.empty()) {
    r.errorCode = "llm_bad_response";
    r.errorMessage = "El modelo no devolvió texto";
    return r;
  }
  r.ok = true;
  return r;
}

}  // namespace agent
