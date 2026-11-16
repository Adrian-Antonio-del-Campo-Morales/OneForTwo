#pragma once

// Responsibility: Linux/Wayland adapter based on official interfaces
// xdg-desktop-portal RemoteDesktop + Clipboard. Request a session with zero
// Input devices and never calls Notify* or ConnectToEIS methods.
// There is no failback to X11: the caller must choose this route only in Wayland.

#include <string>

#include "platform/clipboard.hpp"

namespace oit {

// Capacities declared by the adapter. The effective concession depends on the
// portal dialog; creating the port is what checks it.
ClipboardCapabilities portal_clipboard_capabilities();

// Create the portal session and request clipboard_access. It can be blocked
// while the user decides; it is never called from --inventory or --self-test.
PortCreation create_portal_clipboard_port();

// Check D-Bus message encoding and decoding and MIME type selection
// without opening a connection, a bus, a permission session or the clipboard.
bool run_portal_isolated_checks(std::string& detail);

// Synthetic pipes: delayed arrival, EOF and timeout, without clipboard or bus.
bool run_portal_pipe_isolated_checks(std::string& detail);

}  // namespace oit
