// Responsibility: choose the Linux adapter according to the graphical session and what you
// Wayland is selected only in Wayland sessions; X11 in Xorg.
#include "platform/backend.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace oit {
std::unique_ptr<Backend> makeX11Backend();
#ifdef OIT_WITH_WAYLAND
std::unique_ptr<Backend> makeWaylandBackend();
#endif

namespace {

bool envIsWayland() {
  const char* type = std::getenv("XDG_SESSION_TYPE");
  if (type != nullptr && std::strcmp(type, "wayland") == 0) return true;
  const char* display = std::getenv("WAYLAND_DISPLAY");
  return display != nullptr && *display != '\0';
}

}  // namespace

std::unique_ptr<Backend> makePlatformBackend(std::string& error) {
  const bool wayland = envIsWayland();
#ifdef OIT_WITH_WAYLAND
  if (wayland) return makeWaylandBackend();
#endif
#ifdef OIT_WITH_X11
  if (!wayland) return makeX11Backend();
#endif
  if (wayland) {
    error =
        "Wayland session detected, but binary does not include Wayland backend. "
        "Compile with gio-2.0 and libei-1.0 (ONEINTWO_WAYLAND=AUTO) or use an Xorg session.";
  } else {
    error =
        "session X11 detected, but binary does not include backend X11. "
        "Install libx11-dev, libxi-dev and libxtst-dev or use a Wayland session.";
  }
  return nullptr;
}

}  // namespace oit
