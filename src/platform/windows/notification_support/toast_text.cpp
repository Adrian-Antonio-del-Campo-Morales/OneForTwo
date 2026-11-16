#include "toast_text.hpp"

#include <cstdint>

namespace oneintwo::windows_notifications {
namespace {

void append_escaped(std::string& output, std::string_view text) {
    output.reserve(output.size() + text.size());
    for (const char character : text) {
        switch (character) {
        case '&':
            output.append("&amp;");
            break;
        case '<':
            output.append("&lt;");
            break;
        case '>':
            output.append("&gt;");
            break;
        case '"':
            output.append("&quot;");
            break;
        case '\'':
            output.append("&apos;");
            break;
        default:
            output.push_back(character);
            break;
        }
    }
}

// Recorre puntos de código para descartar controles y no caracteres. El módulo
// ya valida UTF-8; una secuencia truncada se corta en lugar de propagarse.
std::string sanitize_for_xml(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80) {
            const bool allowed =
                lead >= 0x20 || lead == 0x09 || lead == 0x0A || lead == 0x0D;
            if (allowed && lead != 0x7F) {
                result.push_back(static_cast<char>(lead));
            } else if (lead == 0x09 || lead == 0x0A || lead == 0x0D) {
                result.push_back(static_cast<char>(lead));
            } else {
                result.push_back(' ');
            }
            ++index;
            continue;
        }

        std::size_t length = 1;
        std::uint32_t code_point = 0;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
            code_point = lead & 0x1Fu;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            code_point = lead & 0x0Fu;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            code_point = lead & 0x07u;
        }
        if (index + length > text.size()) {
            break;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            code_point = (code_point << 6) |
                         (static_cast<unsigned char>(text[index + offset]) & 0x3Fu);
        }
        if (code_point == 0xFFFE || code_point == 0xFFFF) {
            result.push_back(' ');
        } else {
            result.append(text.substr(index, length));
        }
        index += length;
    }
    return result;
}

void append_text_line(std::string& xml, std::string_view value) {
    if (value.empty()) {
        return;
    }
    xml.append("<text>");
    append_escaped(xml, sanitize_for_xml(value));
    xml.append("</text>");
}

} // namespace

std::string escape_xml_text(std::string_view text) {
    std::string escaped;
    append_escaped(escaped, sanitize_for_xml(text));
    return escaped;
}

std::string build_toast_xml(const ToastText& text) {
    std::string xml;
    xml.reserve(text.title.size() + text.app_name.size() + text.body.size() +
                text.peer_name.size() + 320);
    xml.append("<toast activationType=\"foreground\"><visual><binding template=\"ToastGeneric\">");
    append_text_line(xml, text.title);
    // El nombre visible de la aplicación origen solo existe en los reenviados.
    append_text_line(xml, text.app_name);
    append_text_line(xml, text.body);
    if (!text.peer_name.empty()) {
        // Atribución explícita al equipo vinculado, en texto plano.
        append_text_line(xml, "De " + sanitize_for_xml(text.peer_name));
    }
    xml.append("</binding></visual></toast>");
    return xml;
}

} // namespace oneintwo::windows_notifications
