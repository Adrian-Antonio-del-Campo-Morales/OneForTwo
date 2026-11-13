#include "text_validation.hpp"

#include <cstdint>

namespace oneintwo::notification_support {

bool is_valid_utf8(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80) {
            ++index;
            continue;
        }

        std::size_t continuation = 0;
        char32_t code_point = 0;
        if ((lead & 0xE0) == 0xC0) {
            continuation = 1;
            code_point = lead & 0x1Fu;
        } else if ((lead & 0xF0) == 0xE0) {
            continuation = 2;
            code_point = lead & 0x0Fu;
        } else if ((lead & 0xF8) == 0xF0) {
            continuation = 3;
            code_point = lead & 0x07u;
        } else {
            return false;
        }

        if (index + continuation >= text.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation; ++offset) {
            const auto byte = static_cast<unsigned char>(text[index + offset]);
            if ((byte & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (byte & 0x3Fu);
        }

        const char32_t minimum =
            continuation == 1 ? 0x80 : (continuation == 2 ? 0x800 : 0x10000);
        if (code_point < minimum || code_point > 0x10FFFF) {
            return false;
        }
        if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            return false;
        }
        index += continuation + 1;
    }
    return true;
}

bool is_blank(std::string_view text) {
    for (const char character : text) {
        switch (character) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case '\v':
            break;
        default:
            return false;
        }
    }
    return true;
}

std::string sanitize_plain_text(std::string_view text) {
    std::string sanitized;
    sanitized.reserve(text.size());
    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80) {
            const char character = static_cast<char>(lead);
            if ((lead < 0x20 && character != '\t' && character != '\n') || lead == 0x7F) {
                sanitized.push_back(' ');
            } else {
                sanitized.push_back(character);
            }
            ++index;
            continue;
        }

        std::size_t length = 1;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
        }
        if (index + length > text.size()) {
            // El llamador valida antes; una secuencia truncada no se propaga.
            break;
        }
        sanitized.append(text.substr(index, length));
        index += length;
    }
    return sanitized;
}

} // namespace oneintwo::notification_support
