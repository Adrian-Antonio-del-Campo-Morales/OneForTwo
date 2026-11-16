#pragma once

// Responsibility: common text model, boundaries, footprints and logical order.
// It does not depend on the desktop, network or any external library.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace oit {

// Test limit for text UTF-8. It is not a product value.
inline constexpr std::size_t kMaxTextBytes = 256u * 1024u;
inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint16_t kDefaultPort = 47831;
inline constexpr int kEchoWindowMs = 1500;
inline constexpr std::size_t kMaxEndpointChars = 64;

enum class StatusCode : std::uint8_t {
  Ok = 0,
  Unavailable = 1,
  Denied = 2,
  TooLarge = 3,
  InvalidText = 4,
  Busy = 5,
  IoError = 6,
  NotSupported = 7,
  WrongGeneration = 8,
  WrongProtocol = 9,
  Stale = 10,
  Duplicate = 11,
  Rejected = 12,
};

struct Status {
  StatusCode code = StatusCode::Ok;
  std::string detail;
  bool ok() const noexcept { return code == StatusCode::Ok; }
};

const char* status_name(StatusCode code) noexcept;

struct TextMetrics {
  std::size_t bytes = 0;
  std::size_t codepoints = 0;
  std::size_t lines = 0;
};

struct Validation {
  bool valid = false;
  Status status;
  TextMetrics metrics;
  std::string normalized;  // UTF-8 with LF line ends.
};

// Validation UTF-8 strict, rejects NUL and empty text, applies limit and
// normalizes CRLF to LF. It does not crop or replace content.
Validation validate_text(std::string_view text,
                         std::size_t max_bytes = kMaxTextBytes);

std::string normalize_line_endings(std::string_view text);
std::string to_windows_line_endings(std::string_view text);

// Non-cryptographic fingerprint (FNV-1a of 64 bits) for diagnosis and
// Local echo heuristics. Not used as authentication.
std::uint64_t fingerprint(std::string_view text) noexcept;
std::string fingerprint_hex(std::uint64_t value);
std::string hex_u64(std::uint64_t value);

struct ClipboardContent {
  std::string text;    // UTF-8
  std::string marker;  // Self-write internal marker; empty if foreign.
};

struct ClipboardUpdate {
  std::uint64_t generation = 0;
  std::uint64_t logical = 0;
  std::string origin;
  std::string text;
};

// Independent deterministic order of wall clocks: first counter
// logical and, in a tie, the identifier of major origin.
int compare_updates(const ClipboardUpdate& a,
                    const ClipboardUpdate& b) noexcept;
bool update_is_newer(const ClipboardUpdate& candidate,
                     const ClipboardUpdate& current) noexcept;

bool valid_endpoint_id(std::string_view id) noexcept;
std::string make_endpoint_id();
std::uint64_t make_generation();

}  // namespace oit
