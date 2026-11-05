#pragma once

// Responsabilidad: representación y validación estructural de los mensajes
// entre dos instancias. No interpreta permisos ni modifica el portapapeles.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "model.hpp"

namespace oit {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::size_t kMaxFrameBytes = kMaxTextBytes + 1024;
inline constexpr std::size_t kMaxDetailBytes = 1024;

enum class MessageType : std::uint8_t {
  Hello = 1,
  Clipboard = 2,
  Applied = 3,
  Error = 4,
  Bye = 5,
};

struct HelloMessage {
  std::uint16_t version = kProtocolVersion;
  std::uint64_t generation = 0;
  std::string endpoint;
  bool can_read = false;
  bool can_write = false;
  bool can_watch = false;
};

struct ClipboardMessage {
  std::uint64_t generation = 0;
  std::uint64_t logical = 0;
  std::string origin;
  std::string text;
};

struct AppliedMessage {
  std::uint64_t generation = 0;
  std::uint64_t logical = 0;
  std::string origin;
  StatusCode code = StatusCode::Ok;
  std::string detail;
};

struct ErrorMessage {
  StatusCode code = StatusCode::Rejected;
  std::string detail;
};

struct ByeMessage {
  std::string reason;
};

struct DecodedMessage {
  MessageType type = MessageType::Error;
  HelloMessage hello;
  ClipboardMessage clipboard;
  AppliedMessage applied;
  ErrorMessage error;
  ByeMessage bye;
};

Bytes encode_hello(const HelloMessage& message);
Bytes encode_clipboard(const ClipboardMessage& message);
Bytes encode_applied(const AppliedMessage& message);
Bytes encode_error(const ErrorMessage& message);
Bytes encode_bye(const ByeMessage& message);

// Espera una trama completa (prefijo de longitud incluido) y la decodifica.
bool decode_message(const Bytes& frame, DecodedMessage& out, Status& error);

Status validate_hello(const HelloMessage& message);
Status validate_applied(const AppliedMessage& message);

const char* message_type_name(MessageType type) noexcept;

}  // namespace oit
