/**
 * @file LlmClient.h
 * @brief Cliente HTTP/HTTPS mínimo para consultar el modelo de lenguaje.
 *
 * No usa ningún framework de agentes ni "tool calling": envía un prompt de
 * texto y recibe texto (que debe contener un objeto JSON). Toda la lógica de
 * interpretación de ese JSON está en el núcleo (PlanValidator / Agent).
 *
 * Proveedores soportados (config/agente_secrets.h -> SECRET_LLM_PROVIDER):
 *  - LLM_PROVIDER_ANTHROPIC: API Messages de Claude (POST /v1/messages).
 *  - LLM_PROVIDER_OPENAI_COMPAT: cualquier endpoint /v1/chat/completions
 *    (OpenAI, Groq, OpenRouter, Gemini, Ollama, LM Studio...).
 *
 * La llamada es BLOQUEANTE (varios segundos), por eso solo se invoca desde la
 * tarea de red (núcleo 0). La tarea de E/S y el planificador siguen corriendo.
 */
#pragma once

#include <Arduino.h>

#include <string>

namespace agent {

struct LlmResult {
  bool ok = false;
  int httpStatus = 0;        ///< Código HTTP (negativo = error de conexión)
  std::string text;          ///< Texto devuelto por el modelo
  std::string errorCode;     ///< llm_not_configured | llm_network | llm_http | llm_refusal | llm_truncated | llm_bad_response
  std::string errorMessage;
  std::string model;         ///< Modelo que respondió
  uint32_t latencyMs = 0;
  uint8_t attempts = 0;
};

class LlmClient {
 public:
  /** ¿Hay clave de API (o endpoint propio) configurada? */
  bool configured() const;
  /** "anthropic" u "openai_compat". */
  const char* providerName() const;
  /** Modelo configurado. */
  const char* model() const;
  /** Envía el prompt y devuelve el texto de la respuesta. */
  LlmResult complete(const std::string& system, const std::string& user);

 private:
  LlmResult requestOnce(const std::string& system, const std::string& user);
};

}  // namespace agent
