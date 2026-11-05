#pragma once

// Responsabilidad: ejecutar una sesión entre dos instancias por dirección
// explícita. No incluye descubrimiento, vinculación persistente ni interfaz.

#include <cstdint>
#include <string>

#include "net/tls_channel.hpp"
#include "platform/clipboard.hpp"

namespace oit {

struct PeerOptions {
  bool server = false;
  std::string bind_address = "0.0.0.0";
  std::string host;
  std::uint16_t port = 0;
  TlsFiles tls;
  bool show_content = false;
};

// Devuelve 0 si la sesión de cliente terminó limpiamente, 3 si falló.
// En modo servidor acepta conexiones sucesivas hasta que el usuario lo detiene.
int run_peer(const PeerOptions& options, ClipboardPort* clipboard,
             const ClipboardCapabilities& capabilities);

}  // namespace oit
