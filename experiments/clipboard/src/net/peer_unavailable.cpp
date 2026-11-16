// Responsibility: report that this compilation does not include OpenSSL. No
// simulates a connection or succeeds: the mode between computers is not
// available and how to enable it.

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
