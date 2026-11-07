/**
 * @file Secrets.h
 * @brief Punto único de acceso a las credenciales.
 *
 * Incluye `agente_secrets.h` (privado, ignorado por git) si existe; si no, usa la
 * plantilla `agente_secrets.example.h` para que el proyecto compile igualmente.
 */
#pragma once

#define LLM_PROVIDER_ANTHROPIC     1
#define LLM_PROVIDER_OPENAI_COMPAT 2

#if defined(__has_include)
#  if __has_include("agente_secrets.h")
#    include "agente_secrets.h"
#    define AGENT_USING_SECRET_TEMPLATE 0
#  else
#    include "agente_secrets.example.h"
#    define AGENT_USING_SECRET_TEMPLATE 1
#  endif
#else
#  include "agente_secrets.h"
#  define AGENT_USING_SECRET_TEMPLATE 0
#endif
