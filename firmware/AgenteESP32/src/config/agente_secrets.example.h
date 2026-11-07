/**
 * @file agente_secrets.example.h
 * @brief PLANTILLA de credenciales. NO ponga claves reales en este archivo.
 *
 * Pasos:
 *   1. Copie este archivo como `agente_secrets.h` en la misma carpeta.
 *   2. Complete los valores.
 *   3. `agente_secrets.h` está en .gitignore: nunca se sube al repositorio.
 *
 * Si `agente_secrets.h` no existe, el firmware compila con esta plantilla (valores
 * vacíos): arrancará en modo punto de acceso y el LLM quedará deshabilitado,
 * pero el control manual y los planes JSON directos seguirán funcionando.
 *
 * Las claves nunca se envían a la interfaz web ni aparecen en /api/status:
 * la API solo informa si están configuradas (true/false).
 */
#pragma once

// ------------------------------------------------------------------ Wi-Fi
#define SECRET_WIFI_SSID     ""
#define SECRET_WIFI_PASSWORD ""

/** Clave del punto de acceso de respaldo (mínimo 8 caracteres). */
#define SECRET_AP_PASSWORD   "agente1234"

// ------------------------------------------------------------------- LLM
/**
 * Proveedor del modelo de lenguaje:
 *   LLM_PROVIDER_ANTHROPIC     -> API de Claude (https://api.anthropic.com/v1/messages)
 *   LLM_PROVIDER_OPENAI_COMPAT -> cualquier endpoint compatible con /v1/chat/completions
 *                                 (OpenAI, Groq, OpenRouter, Gemini, Ollama, LM Studio...)
 */
#define SECRET_LLM_PROVIDER  LLM_PROVIDER_ANTHROPIC

/** Clave de la API del proveedor elegido. */
#define SECRET_LLM_API_KEY   ""

/** Modelo. Para Anthropic: "claude-opus-5-5". Para otros proveedores, su identificador. */
#define SECRET_LLM_MODEL     "claude-opus-5-5"

/**
 * URL completa del endpoint. Déjela vacía para usar la del proveedor por defecto:
 *   Anthropic:     https://api.anthropic.com/v1/messages
 *   OpenAI-compat: https://api.openai.com/v1/chat/completions
 * Ejemplo Ollama local: "http://192.168.1.50:11434/v1/chat/completions"
 */
#define SECRET_LLM_ENDPOINT  ""

/**
 * Certificado raíz (PEM) del servidor del LLM para verificar TLS.
 * Si se deja vacío, la conexión HTTPS se cifra pero NO se verifica la identidad
 * del servidor (aceptable para un prototipo académico; ver docs/SECURITY_NOTES.md
 * para obtener el certificado con openssl).
 */
#define SECRET_LLM_ROOT_CA   ""

// ------------------------------------------------- API HTTP del dispositivo
/**
 * Token opcional para proteger la API REST del ESP32-S3.
 * Si no está vacío, todas las rutas /api/... (excepto /api/health) exigen la
 * cabecera:  Authorization: Bearer <token>
 */
#define SECRET_DEVICE_API_TOKEN ""

// ------------------------------------------------ Telegram (opcional)
/** Si ambos valores están completos, las alertas también se envían por Telegram. */
#define SECRET_TELEGRAM_BOT_TOKEN ""
#define SECRET_TELEGRAM_CHAT_ID   ""
