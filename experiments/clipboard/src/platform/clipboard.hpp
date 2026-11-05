#pragma once

// Responsabilidad: contrato del adaptador nativo del portapapeles. El motor
// sólo usa esta interfaz; el código de plataforma queda detrás de la fábrica.
// Ninguna función de este encabezado accede al portapapeles por sí sola.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "model.hpp"

namespace oit {

struct ClipboardCapabilities {
  bool read = false;
  bool write = false;
  bool watch = false;  // notificaciones mientras el proceso sigue vivo
  bool independent_of_input = false;
  std::string backend;
  std::string session;
  std::string limitation;
  std::vector<std::string> notes;
};

enum class WaitResult {
  Change,
  Timeout,
  Error,
  Stopped,
};

class ClipboardPort {
 public:
  virtual ~ClipboardPort() = default;

  virtual ClipboardCapabilities capabilities() const = 0;
  virtual Status read(ClipboardContent& out) = 0;
  virtual Status write(std::string_view text, std::string_view marker) = 0;
  virtual WaitResult wait_for_change(ClipboardContent& out, int timeout_ms,
                                     std::string& detail) = 0;
  virtual void request_stop() = 0;
};

struct PortCreation {
  std::unique_ptr<ClipboardPort> port;
  Status status;
};

// Crea el adaptador real de la plataforma actual. Si no existe una vía
// soportada devuelve port == nullptr y un estado explícito (nunca un doble).
PortCreation create_clipboard_port();

// Capacidades e inventario no leen ni escriben el portapapeles.
ClipboardCapabilities clipboard_capabilities();
std::string platform_inventory_text();
std::string platform_name();

}  // namespace oit
