#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace oneintwo::notification_support {

// Rechaza secuencias truncadas, continuaciones sueltas, formas sobrelargas,
// sustitutos UTF-16 y puntos de código fuera del rango Unicode.
bool is_valid_utf8(std::string_view text);

// Un título compuesto solo por espacio ASCII no satisface la obligación.
bool is_blank(std::string_view text);

// Conserva el texto validado y sustituye controles C0 (salvo tabulador y salto
// de línea) y DEL por un espacio. Se usa al presentar en el escritorio, nunca
// para decidir la validez del mensaje.
std::string sanitize_plain_text(std::string_view text);

} // namespace oneintwo::notification_support
