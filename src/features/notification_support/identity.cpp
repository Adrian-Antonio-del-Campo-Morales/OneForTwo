#include "identity.hpp"

#include <cctype>
#include <string>

namespace oneintwo::notification_support {
namespace {

char ascii_lower(char character) {
    const auto value = static_cast<unsigned char>(character);
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return character;
}

std::string lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(ascii_lower(character));
    }
    return result;
}

} // namespace

bool is_product_application(std::string_view application_id) {
    if (application_id.empty()) {
        return false;
    }
    const std::string identifier = lower(application_id);
    constexpr std::string_view family = "oneintwo";
    if (identifier.size() < family.size()) {
        return false;
    }
    if (identifier.compare(0, family.size(), family) != 0) {
        return false;
    }
    if (identifier.size() == family.size()) {
        return true;
    }
    const char separator = identifier[family.size()];
    return separator == '.' || separator == '_' || separator == '-';
}

} // namespace oneintwo::notification_support
