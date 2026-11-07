#include "TextUtils.h"

#include <ctype.h>
#include <string.h>

namespace agent {
namespace text {

namespace {

/** Mapa de letras acentuadas UTF-8 (2 bytes, prefijo 0xC3) a ASCII en minúscula. */
char foldLatin1(unsigned char second) {
  switch (second) {
    case 0xA1: case 0x81: return 'a';  // á Á
    case 0xA9: case 0x89: return 'e';  // é É
    case 0xAD: case 0x8D: return 'i';  // í Í
    case 0xB3: case 0x93: return 'o';  // ó Ó
    case 0xBA: case 0x9A: return 'u';  // ú Ú
    case 0xBC: case 0x9C: return 'u';  // ü Ü
    case 0xB1: case 0x91: return 'n';  // ñ Ñ
    default: return 0;
  }
}

bool containsWord(const std::string& hay, const char* word) {
  // Coincidencia de palabra completa (bordes = inicio/fin o no-alfanumérico).
  size_t wl = strlen(word);
  size_t pos = hay.find(word);
  while (pos != std::string::npos) {
    bool leftOk = pos == 0 || !isalnum(static_cast<unsigned char>(hay[pos - 1]));
    bool rightOk = pos + wl >= hay.size() || !isalnum(static_cast<unsigned char>(hay[pos + wl]));
    if (leftOk && rightOk) return true;
    pos = hay.find(word, pos + 1);
  }
  return false;
}

/** Expresiones temporales vagas (texto ya normalizado). */
const char* const kAmbiguous[] = {
    "mas tarde", "mas tardecito", "en un rato", "en un ratico", "al rato", "un rato",
    "pronto", "en algun momento", "algun dia", "cuando puedas", "cuando sea",
    "mas adelante", "despuesito", "temprano", "esta noche", "esta tarde",
    "por la noche", "por la tarde", "por la manana", "en la noche", "en la tarde",
    "en la manana", "otro dia", "luego",
};

/** Indicadores de que SÍ hay una referencia temporal concreta. */
const char* const kExplicit[] = {
    "ahora", "ya mismo", "inmediatamente", "de inmediato", "enseguida", "mediodia", "medianoche",
    "la una", "un minuto", "una hora", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho",
    "nueve", "diez", "once", "doce", "trece", "catorce", "quince", "veinte", "veinticinco",
    "treinta", "cuarenta", "cincuenta", "sesenta", "media", "cuarto",
};

}  // namespace

std::string normalize(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  bool lastSpace = false;
  for (size_t i = 0; i < in.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(in[i]);
    char mapped = 0;
    if (c == 0xC3 && i + 1 < in.size()) {
      mapped = foldLatin1(static_cast<unsigned char>(in[i + 1]));
      if (mapped) {
        ++i;
      } else {
        out.push_back(static_cast<char>(c));  // conserva otros caracteres UTF-8
        lastSpace = false;
        continue;
      }
    } else if (c < 0x80) {
      mapped = static_cast<char>(tolower(c));
    } else {
      out.push_back(static_cast<char>(c));
      lastSpace = false;
      continue;
    }
    bool space = mapped == ' ' || mapped == '\t' || mapped == '\n' || mapped == '\r';
    if (space) {
      if (!lastSpace && !out.empty()) out.push_back(' ');
      lastSpace = true;
    } else {
      out.push_back(mapped);
      lastSpace = false;
    }
  }
  while (!out.empty() && out[out.size() - 1] == ' ') out.erase(out.size() - 1);
  return out;
}

bool extractJsonObject(const std::string& in, std::string& out) {
  size_t start = in.find('{');
  while (start != std::string::npos) {
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = start; i < in.size(); ++i) {
      char c = in[i];
      if (inString) {
        if (escape) escape = false;
        else if (c == '\\') escape = true;
        else if (c == '"') inString = false;
        continue;
      }
      if (c == '"') inString = true;
      else if (c == '{') ++depth;
      else if (c == '}') {
        --depth;
        if (depth == 0) {
          out = in.substr(start, i - start + 1);
          return true;
        }
      }
    }
    // Objeto sin cerrar: probar con la siguiente llave (texto previo con '{' suelto).
    start = in.find('{', start + 1);
  }
  return false;
}

bool findAmbiguousTime(const std::string& in, std::string& phrase) {
  std::string t = normalize(in);
  if (t.find('?') != std::string::npos) return false;  // es una consulta
  const char* found = nullptr;
  for (size_t i = 0; i < sizeof(kAmbiguous) / sizeof(kAmbiguous[0]); ++i) {
    if (containsWord(t, kAmbiguous[i])) {
      found = kAmbiguous[i];
      break;
    }
  }
  if (!found) return false;
  for (size_t i = 0; i < t.size(); ++i) {
    if (t[i] >= '0' && t[i] <= '9') return false;
  }
  for (size_t i = 0; i < sizeof(kExplicit) / sizeof(kExplicit[0]); ++i) {
    if (containsWord(t, kExplicit[i])) return false;
  }
  phrase = found;
  return true;
}

std::string truncateUtf8(const std::string& in, size_t max) {
  if (in.size() <= max) return in;
  size_t cut = max;
  // Retroceder mientras estemos en un byte de continuación UTF-8 (10xxxxxx).
  while (cut > 0 && (static_cast<unsigned char>(in[cut]) & 0xC0) == 0x80) --cut;
  return in.substr(0, cut);
}

}  // namespace text
}  // namespace agent
