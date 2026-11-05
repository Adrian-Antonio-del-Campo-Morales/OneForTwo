#pragma once
// Responsabilidad: codificación y validación del mensaje de control de tamaño
// fijo. No incluye sockets, TLS ni estado de sesión.

#include "types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace oit {

constexpr size_t kFrameSize = 28;
using Frame = std::array<uint8_t, kFrameSize>;

Frame encodeMessage(const Message& message);

// Devuelve false ante tamaño, marca, versión o campos inválidos.
bool decodeMessage(const uint8_t* data, size_t size, Message& out, std::string& error);

}  // namespace oit
