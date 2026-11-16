#pragma once

#include <string>
#include <string_view>

namespace oneintwo::windows_notifications {

// Content already validated by the common module. Name of the application only
// appears in forwarded notices and the name of the pair is local attribution.
struct ToastText {
    std::string title;
    std::string app_name;
    std::string body;
    std::string peer_name;
};

// Escape marked XML and replaces characters that XML 1.0 does not support.
// valid Unicode text and does not enter active elements.
std::string escape_xml_text(std::string_view text);

// Build a generic toast without actions, without launch and without images.
std::string build_toast_xml(const ToastText& text);

} // namespace oneintwo::windows_notifications
