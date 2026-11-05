// Responsabilidad: implementar validación UTF-8, límites, normalización de
// finales de línea, huellas y orden lógico sin tocar el escritorio.

#include "model.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <random>
#include <string>

namespace oit {
namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

bool scan_utf8(std::string_view text, std::size_t& codepoints,
               std::string& detail) {
  std::size_t i = 0;
  codepoints = 0;
  while (i < text.size()) {
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    if (lead == 0) {
      detail = "text contains U+0000";
      return false;
    }
    std::size_t length = 0;
    std::uint32_t codepoint = 0;
    if (lead < 0x80) {
      length = 1;
      codepoint = lead;
    } else if ((lead & 0xE0) == 0xC0) {
      length = 2;
      codepoint = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
      length = 3;
      codepoint = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
      length = 4;
      codepoint = lead & 0x07u;
    } else {
      detail = "invalid UTF-8 leading byte";
      return false;
    }
    if (i + length > text.size()) {
      detail = "truncated UTF-8 sequence";
      return false;
    }
    for (std::size_t k = 1; k < length; ++k) {
      const unsigned char continuation =
          static_cast<unsigned char>(text[i + k]);
      if ((continuation & 0xC0) != 0x80) {
        detail = "invalid UTF-8 continuation byte";
        return false;
      }
      codepoint = (codepoint << 6) | (continuation & 0x3Fu);
    }
    if ((length == 2 && codepoint < 0x80) ||
        (length == 3 && codepoint < 0x800) ||
        (length == 4 && codepoint < 0x10000)) {
      detail = "overlong UTF-8 sequence";
      return false;
    }
    if (codepoint > 0x10FFFF) {
      detail = "UTF-8 code point above U+10FFFF";
      return false;
    }
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
      detail = "UTF-8 surrogate code point";
      return false;
    }
    ++codepoints;
    i += length;
  }
  return true;
}

std::uint64_t random_u64() {
  std::random_device device;
  std::uint64_t value = (static_cast<std::uint64_t>(device()) << 32) ^
                        static_cast<std::uint64_t>(device());
  if (value == 0) {
    value = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    if (value == 0) {
      value = 1;
    }
  }
  return value;
}

}  // namespace

const char* status_name(StatusCode code) noexcept {
  switch (code) {
    case StatusCode::Ok:
      return "ok";
    case StatusCode::Unavailable:
      return "unavailable";
    case StatusCode::Denied:
      return "denied";
    case StatusCode::TooLarge:
      return "too_large";
    case StatusCode::InvalidText:
      return "invalid_text";
    case StatusCode::Busy:
      return "busy";
    case StatusCode::IoError:
      return "io_error";
    case StatusCode::NotSupported:
      return "not_supported";
    case StatusCode::WrongGeneration:
      return "wrong_generation";
    case StatusCode::WrongProtocol:
      return "wrong_protocol";
    case StatusCode::Stale:
      return "stale";
    case StatusCode::Duplicate:
      return "duplicate";
    case StatusCode::Rejected:
      return "rejected";
  }
  return "unknown";
}

Validation validate_text(std::string_view text, std::size_t max_bytes) {
  Validation result;
  if (text.empty()) {
    result.status = {StatusCode::InvalidText, "empty text is not transferred"};
    return result;
  }
  if (text.size() > max_bytes) {
    result.status = {StatusCode::TooLarge,
                     "text has " + std::to_string(text.size()) +
                         " bytes; limit is " + std::to_string(max_bytes)};
    return result;
  }
  std::size_t codepoints = 0;
  std::string detail;
  if (!scan_utf8(text, codepoints, detail)) {
    result.status = {StatusCode::InvalidText, detail};
    return result;
  }

  std::string normalized = normalize_line_endings(text);
  std::size_t normalized_codepoints = 0;
  std::string ignored;
  // scan_utf8 cannot fail here: normalization only removes U+000D.
  (void)scan_utf8(normalized, normalized_codepoints, ignored);

  result.valid = true;
  result.status = {StatusCode::Ok, ""};
  result.metrics.bytes = normalized.size();
  result.metrics.codepoints = normalized_codepoints;
  result.metrics.lines =
      1 + static_cast<std::size_t>(std::count(normalized.begin(),
                                              normalized.end(), '\n'));
  result.normalized = std::move(normalized);
  return result;
}

std::string normalize_line_endings(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      out.push_back('\n');
      ++i;
    } else {
      out.push_back(text[i]);
    }
  }
  return out;
}

std::string to_windows_line_endings(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 16);
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n' && (i == 0 || text[i - 1] != '\r')) {
      out.append("\r\n");
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::uint64_t fingerprint(std::string_view text) noexcept {
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex_u64(std::uint64_t value) {
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = kHexDigits[value & 0xFu];
    value >>= 4;
  }
  return out;
}

std::string fingerprint_hex(std::uint64_t value) { return hex_u64(value); }

int compare_updates(const ClipboardUpdate& a,
                    const ClipboardUpdate& b) noexcept {
  if (a.logical != b.logical) {
    return a.logical < b.logical ? -1 : 1;
  }
  const int origin_compare = a.origin.compare(b.origin);
  if (origin_compare < 0) {
    return -1;
  }
  if (origin_compare > 0) {
    return 1;
  }
  return 0;
}

bool update_is_newer(const ClipboardUpdate& candidate,
                     const ClipboardUpdate& current) noexcept {
  return compare_updates(candidate, current) > 0;
}

bool valid_endpoint_id(std::string_view id) noexcept {
  if (id.empty() || id.size() > kMaxEndpointChars) {
    return false;
  }
  for (const char c : id) {
    const unsigned char byte = static_cast<unsigned char>(c);
    if (!std::isalnum(byte) && c != '-' && c != '_') {
      return false;
    }
  }
  return true;
}

std::string make_endpoint_id() {
  const std::uint64_t value = random_u64();
  return hex_u64(value).substr(0, 16);
}

std::uint64_t make_generation() { return random_u64(); }

}  // namespace oit
