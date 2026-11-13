#pragma once

#include <string>
#include <string_view>

namespace oneintwo::windows_notifications {

// Contenido ya validado por el módulo común. El nombre de la aplicación solo
// aparece en los avisos reenviados y el nombre del par es la atribución local.
struct ToastText {
    std::string title;
    std::string app_name;
    std::string body;
    std::string peer_name;
};

// Escapa marcado XML y sustituye caracteres que XML 1.0 no admite. Mantiene el
// texto Unicode válido y no introduce elementos activos.
std::string escape_xml_text(std::string_view text);

// Construye un toast genérico sin acciones, sin launch y sin imágenes.
std::string build_toast_xml(const ToastText& text);

} // namespace oneintwo::windows_notifications
