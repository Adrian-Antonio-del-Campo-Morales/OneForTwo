#pragma once

// Responsibility: Native clipboard adapter contract. The engine
// Just use this interface; the platform code is behind the factory.
// No function of this header accesses the clipboard alone.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "model.hpp"

namespace oit {

struct ClipboardCapabilities {
  bool read = false;
  bool write = false;
  bool watch = false;  // notifications while the process is still alive
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

// Creates the actual adapter of the current platform. If no track exists
// supported returns port==nullptr and an explicit state (never a double).
PortCreation create_clipboard_port();

// Capabilities and inventory do not read or write the clipboard.
ClipboardCapabilities clipboard_capabilities();
std::string platform_inventory_text();
std::string platform_name();

}  // namespace oit
