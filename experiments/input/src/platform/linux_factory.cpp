// Responsabilidad: elegir el adaptador Linux según la sesión gráfica y lo que se
// haya compilado. Wayland se selecciona solo en sesiones Wayland; X11, en Xorg.
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
        "sesión Wayland detectada, pero el binario no incluye el backend Wayland. "
        "Compile con gio-2.0 y libei-1.0 (ONEINTWO_WAYLAND=AUTO) o use una sesión Xorg.";
  } else {
    error =
        "sesión X11 detectada, pero el binario no incluye el backend X11. "
        "Instale libx11-dev, libxi-dev y libxtst-dev, o use una sesión Wayland.";
  }
  return nullptr;
}

}  // namespace oit
