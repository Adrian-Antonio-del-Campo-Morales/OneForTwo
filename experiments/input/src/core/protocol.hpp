#pragma once
// Responsibility: Encoding and Validating the Size Control Message
// Fixed. Does not include sockets, TLS or session status.

#include "types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace oit {

constexpr size_t kFrameSize = 28;
using Frame = std::array<uint8_t, kFrameSize>;

Frame encodeMessage(const Message& message);

// Returns false to size, mark, version or invalid fields.
bool decodeMessage(const uint8_t* data, size_t size, Message& out, std::string& error);

}  // namespace oit
