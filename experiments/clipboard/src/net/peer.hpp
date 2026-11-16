#pragma once

// Responsibility: run a session between two instances by address
// Explicit. It does not include discovery, persistent linkage or interface.

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

// Returns 0 if the client session ended cleanly, 3 if failed.
// In server mode it accepts successive connections until the user stops it.
int run_peer(const PeerOptions& options, ClipboardPort* clipboard,
             const ClipboardCapabilities& capabilities);

}  // namespace oit
