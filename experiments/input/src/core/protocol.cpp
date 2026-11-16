// Responsibility: small-endian serialization of the message of control and rejection
// of poorly formed or unexplained plots.
#include "protocol.hpp"

namespace oit {
namespace {

void putU16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

void putU32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint16_t getU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t getU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool knownType(uint8_t value) {
  return value >= static_cast<uint8_t>(MsgType::Hello) &&
         value <= static_cast<uint8_t>(MsgType::Pong);
}

bool validReason(uint8_t value) {
  return value <= static_cast<uint8_t>(LeaveReason::InjectionFailed);
}

}  // namespace

Frame encodeMessage(const Message& message) {
  Frame frame{};
  frame[0] = 'O';
  frame[1] = 'I';
  frame[2] = 'T';
  frame[3] = '2';
  frame[4] = kProtocolVersion;
  frame[5] = static_cast<uint8_t>(message.type);
  putU16(frame.data() + 6, 16);  // length of the body
  putU32(frame.data() + 8, message.generation);
  putU32(frame.data() + 12, message.sequence);
  putU16(frame.data() + 16, static_cast<uint16_t>(message.x));
  putU16(frame.data() + 18, static_cast<uint16_t>(message.y));
  putU16(frame.data() + 20, message.code);
  frame[22] = message.pressed ? 1 : 0;
  frame[23] = message.reason;
  putU16(frame.data() + 24, message.ratio);
  putU16(frame.data() + 26, 0);  // reservado
  return frame;
}

bool decodeMessage(const uint8_t* data, size_t size, Message& out, std::string& error) {
  if (data == nullptr || size != kFrameSize) {
    error = "wrong size plot";
    return false;
  }
  if (data[0] != 'O' || data[1] != 'I' || data[2] != 'T' || data[3] != '2') {
    error = "wrong plotting mark";
    return false;
  }
  if (data[4] != kProtocolVersion) {
    error = "non-compatible protocol version";
    return false;
  }
  if (getU16(data + 6) != 16 || getU16(data + 26) != 0) {
    error = "invalid length or reserved";
    return false;
  }
  if (!knownType(data[5])) {
    error = "type of message unknown";
    return false;
  }
  if (data[22] > 1) {
    error = "Invalid pulsation indicator";
    return false;
  }

  Message message;
  message.type = static_cast<MsgType>(data[5]);
  message.generation = getU32(data + 8);
  message.sequence = getU32(data + 12);
  message.x = static_cast<int16_t>(getU16(data + 16));
  message.y = static_cast<int16_t>(getU16(data + 18));
  message.code = getU16(data + 20);
  message.pressed = data[22] != 0;
  message.reason = data[23];
  message.ratio = getU16(data + 24);

  if (message.type == MsgType::EnterRequest) {
    if (message.code > 1) {
      error = "invalid border in EnterRequest";
      return false;
    }
  }
  if (message.type == MsgType::EnterReject || message.type == MsgType::Leave ||
      message.type == MsgType::ReleaseAll) {
    if (!validReason(message.reason)) {
      error = "invalid ground";
      return false;
    }
  }
  out = message;
  return true;
}

}  // namespace oit
