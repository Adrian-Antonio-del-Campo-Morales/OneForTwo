// Responsabilidad: informar que esta compilación no incluye OpenSSL. No
// simula una conexión ni presenta éxito: el modo entre equipos no está
// disponible y se indica cómo habilitarlo.

#include "net/peer.hpp"

#include <iostream>

namespace oit {

int run_peer(const PeerOptions& options, ClipboardPort* clipboard,
             const ClipboardCapabilities& capabilities) {
  (void)options;
  (void)clipboard;
  (void)capabilities;
  std::cerr
      << "This build has no TLS peer transport because OpenSSL was not "
         "found at configure time.\n"
      << "Install the OpenSSL development package and rebuild "
         "(see MANUAL.md). No connection was attempted.\n";
  return 3;
}

}  // namespace oit
