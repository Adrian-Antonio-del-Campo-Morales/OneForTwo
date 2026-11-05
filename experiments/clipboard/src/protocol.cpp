// Responsabilidad: codificar y decodificar tramas con límites explícitos.
// No hay estructuras nativas en la red: todo es big-endian y longitud prefijada.

#include "protocol.hpp"

#include <cstring>
#include <utility>

namespace oit {
namespace {

void put_u8(Bytes& out, std::uint8_t value) { out.push_back(value); }

void put_u16(Bytes& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void put_u32(Bytes& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void put_u64(Bytes& out, std::uint64_t value) {
  for (int shift = 56; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
  }
}

void put_string(Bytes& out, std::string_view value) {
  put_u32(out, static_cast<std::uint32_t>(value.size()));
  out.insert(out.end(), value.begin(), value.end());
}

std::uint32_t read_u32(const std::uint8_t* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24) |
         (static_cast<std::uint32_t>(data[1]) << 16) |
         (static_cast<std::uint32_t>(data[2]) << 8) |
         static_cast<std::uint32_t>(data[3]);
}

class Reader {
 public:
  Reader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size) {}

  bool u8(std::uint8_t& out) {
    if (remaining() < 1) {
      return fail("missing byte");
    }
    out = data_[position_++];
    return true;
  }

  bool u16(std::uint16_t& out) {
    if (remaining() < 2) {
      return fail("missing 16-bit field");
    }
    out = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data_[position_]) << 8) |
        static_cast<std::uint16_t>(data_[position_ + 1]));
    position_ += 2;
    return true;
  }

  bool u64(std::uint64_t& out) {
    if (remaining() < 8) {
      return fail("missing 64-bit field");
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
      out = (out << 8) | data_[position_ + static_cast<std::size_t>(i)];
    }
    position_ += 8;
    return true;
  }

  bool str(std::string& out, std::size_t max_length) {
    std::uint8_t raw[4];
    if (remaining() < 4) {
      return fail("missing string length");
    }
    std::memcpy(raw, data_ + position_, 4);
    const std::uint32_t length = read_u32(raw);
    position_ += 4;
    if (length > max_length) {
      return fail("string exceeds its limit");
    }
    if (remaining() < length) {
      return fail("truncated string");
    }
    out.assign(reinterpret_cast<const char*>(data_ + position_), length);
    position_ += length;
    return true;
  }

  bool done() const { return position_ == size_; }
  std::size_t remaining() const { return size_ - position_; }
  const std::string& error() const { return error_; }

 private:
  bool fail(const char* reason) {
    if (error_.empty()) {
      error_ = reason;
    }
    return false;
  }

  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t position_ = 0;
  std::string error_;
};

Bytes wrap(MessageType type, const Bytes& body) {
  Bytes frame;
  put_u32(frame, static_cast<std::uint32_t>(body.size() + 1));
  frame.push_back(static_cast<std::uint8_t>(type));
  frame.insert(frame.end(), body.begin(), body.end());
  return frame;
}

}  // namespace

const char* message_type_name(MessageType type) noexcept {
  switch (type) {
    case MessageType::Hello:
      return "hello";
    case MessageType::Clipboard:
      return "clipboard";
    case MessageType::Applied:
      return "applied";
    case MessageType::Error:
      return "error";
    case MessageType::Bye:
      return "bye";
  }
  return "unknown";
}

Bytes encode_hello(const HelloMessage& message) {
  Bytes body;
  put_u16(body, message.version);
  put_u64(body, message.generation);
  put_string(body, message.endpoint);
  std::uint8_t flags = 0;
  if (message.can_read) {
    flags |= 1u;
  }
  if (message.can_write) {
    flags |= 2u;
  }
  if (message.can_watch) {
    flags |= 4u;
  }
  put_u8(body, flags);
  return wrap(MessageType::Hello, body);
}

Bytes encode_clipboard(const ClipboardMessage& message) {
  Bytes body;
  put_u64(body, message.generation);
  put_u64(body, message.logical);
  put_string(body, message.origin);
  put_string(body, message.text);
  return wrap(MessageType::Clipboard, body);
}

Bytes encode_applied(const AppliedMessage& message) {
  Bytes body;
  put_u64(body, message.generation);
  put_u64(body, message.logical);
  put_string(body, message.origin);
  put_u8(body, static_cast<std::uint8_t>(message.code));
  put_string(body, message.detail);
  return wrap(MessageType::Applied, body);
}

Bytes encode_error(const ErrorMessage& message) {
  Bytes body;
  put_u8(body, static_cast<std::uint8_t>(message.code));
  put_string(body, message.detail);
  return wrap(MessageType::Error, body);
}

Bytes encode_bye(const ByeMessage& message) {
  Bytes body;
  put_string(body, message.reason);
  return wrap(MessageType::Bye, body);
}

Status validate_hello(const HelloMessage& message) {
  if (message.version != kProtocolVersion) {
    return {StatusCode::WrongProtocol,
            "unsupported protocol version " +
                std::to_string(message.version)};
  }
  if (message.generation == 0) {
    return {StatusCode::Rejected, "generation must be non-zero"};
  }
  if (!valid_endpoint_id(message.endpoint)) {
    return {StatusCode::InvalidText, "invalid endpoint identifier"};
  }
  return {StatusCode::Ok, ""};
}

Status validate_applied(const AppliedMessage& message) {
  if (message.generation == 0 || message.logical == 0) {
    return {StatusCode::InvalidText, "invalid applied operation id"};
  }
  if (!valid_endpoint_id(message.origin)) {
    return {StatusCode::InvalidText, "invalid applied origin"};
  }
  if (message.detail.size() > kMaxDetailBytes) {
    return {StatusCode::TooLarge, "applied detail exceeds limit"};
  }
  return {StatusCode::Ok, ""};
}

namespace {

bool finish(Reader& reader, Status& error) {
  if (!reader.done()) {
    error = {StatusCode::InvalidText,
             "trailing bytes inside message: " + reader.error()};
    return false;
  }
  return true;
}

}  // namespace

bool decode_message(const Bytes& frame, DecodedMessage& out, Status& error) {
  out = DecodedMessage{};
  error = {StatusCode::Ok, ""};
  if (frame.size() < 5) {
    error = {StatusCode::InvalidText,
             "frame shorter than length prefix and type"};
    return false;
  }
  const std::uint32_t length = read_u32(frame.data());
  if (length == 0) {
    error = {StatusCode::InvalidText, "zero-length frame"};
    return false;
  }
  if (length > kMaxFrameBytes) {
    error = {StatusCode::TooLarge, "frame exceeds protocol limit"};
    return false;
  }
  if (frame.size() != 4 + static_cast<std::size_t>(length)) {
    error = {StatusCode::InvalidText,
             "frame length does not match available bytes"};
    return false;
  }

  const std::uint8_t raw_type = frame[4];
  Reader reader(frame.data() + 5, static_cast<std::size_t>(length) - 1);

  switch (raw_type) {
    case static_cast<std::uint8_t>(MessageType::Hello): {
      HelloMessage message;
      std::uint8_t flags = 0;
      if (!reader.u16(message.version) || !reader.u64(message.generation) ||
          !reader.str(message.endpoint, kMaxEndpointChars) ||
          !reader.u8(flags)) {
        error = {StatusCode::InvalidText,
                 "malformed hello: " + reader.error()};
        return false;
      }
      if ((flags & ~0x07u) != 0) {
        error = {StatusCode::InvalidText, "unknown hello capability flags"};
        return false;
      }
      message.can_read = (flags & 0x01u) != 0;
      message.can_write = (flags & 0x02u) != 0;
      message.can_watch = (flags & 0x04u) != 0;
      if (!finish(reader, error)) {
        return false;
      }
      out.type = MessageType::Hello;
      out.hello = std::move(message);
      return true;
    }
    case static_cast<std::uint8_t>(MessageType::Clipboard): {
      ClipboardMessage message;
      if (!reader.u64(message.generation) || !reader.u64(message.logical) ||
          !reader.str(message.origin, kMaxEndpointChars) ||
          !reader.str(message.text, kMaxTextBytes)) {
        error = {StatusCode::InvalidText,
                 "malformed clipboard message: " + reader.error()};
        return false;
      }
      if (!finish(reader, error)) {
        return false;
      }
      out.type = MessageType::Clipboard;
      out.clipboard = std::move(message);
      return true;
    }
    case static_cast<std::uint8_t>(MessageType::Applied): {
      AppliedMessage message;
      std::uint8_t code = 0;
      if (!reader.u64(message.generation) || !reader.u64(message.logical) ||
          !reader.str(message.origin, kMaxEndpointChars) ||
          !reader.u8(code) || !reader.str(message.detail, kMaxDetailBytes)) {
        error = {StatusCode::InvalidText,
                 "malformed applied message: " + reader.error()};
        return false;
      }
      message.code = static_cast<StatusCode>(code);
      if (!finish(reader, error)) {
        return false;
      }
      out.type = MessageType::Applied;
      out.applied = std::move(message);
      return true;
    }
    case static_cast<std::uint8_t>(MessageType::Error): {
      ErrorMessage message;
      std::uint8_t code = 0;
      if (!reader.u8(code) ||
          !reader.str(message.detail, kMaxDetailBytes)) {
        error = {StatusCode::InvalidText,
                 "malformed error message: " + reader.error()};
        return false;
      }
      message.code = static_cast<StatusCode>(code);
      if (!finish(reader, error)) {
        return false;
      }
      out.type = MessageType::Error;
      out.error = std::move(message);
      return true;
    }
    case static_cast<std::uint8_t>(MessageType::Bye): {
      ByeMessage message;
      if (!reader.str(message.reason, kMaxDetailBytes)) {
        error = {StatusCode::InvalidText,
                 "malformed bye message: " + reader.error()};
        return false;
      }
      if (!finish(reader, error)) {
        return false;
      }
      out.type = MessageType::Bye;
      out.bye = std::move(message);
      return true;
    }
    default:
      error = {StatusCode::NotSupported,
               "unknown message type " + std::to_string(raw_type)};
      return false;
  }
}

}  // namespace oit
