#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace oneintwo::notification_support {

// Rejects truncated sequences, loose continuations, overlong forms,
// UTF-16 substitutes and code points outside the Unicode range.
bool is_valid_utf8(std::string_view text);

// A title composed of only ASCII space does not satisfy the obligation.
bool is_blank(std::string_view text);

// Keeps validated text and replaces C0 controls (except tab and jump)
// line) and DEL for a space. Used when presenting on the desktop, never
// to decide the validity of the message.
std::string sanitize_plain_text(std::string_view text);

} // namespace oneintwo::notification_support
