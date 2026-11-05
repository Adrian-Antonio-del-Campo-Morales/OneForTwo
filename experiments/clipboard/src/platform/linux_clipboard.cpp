// Responsabilidad: selección del adaptador Linux. En Wayland usa el portal
// oficial RemoteDesktop + Clipboard (si libdbus está disponible al compilar).
// En X11 no hay backend y no se sustituye Wayland por X11 en silencio.
// El inventario sólo lee variables de entorno y /etc/os-release; no abre el
// bus de sesión ni el portapapeles.

#include "platform/clipboard.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/utsname.h>

#ifdef OIT_HAVE_PORTAL
#include "platform/portal_clipboard.hpp"
#endif

namespace oit {
namespace {

std::string env_value(const char* name) {
  const char* value = std::getenv(name);
  return value == nullptr ? std::string() : std::string(value);
}

bool is_wayland_session() {
  return env_value("XDG_SESSION_TYPE") == "wayland" ||
         !env_value("WAYLAND_DISPLAY").empty();
}

bool is_x11_session() {
  return env_value("XDG_SESSION_TYPE") == "x11" ||
         (!env_value("DISPLAY").empty() &&
          env_value("WAYLAND_DISPLAY").empty());
}

std::string os_release_pretty_name() {
  std::ifstream file("/etc/os-release");
  if (!file) {
    return "unknown";
  }
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("PRETTY_NAME=", 0) == 0) {
      std::string value = line.substr(12);
      if (value.size() >= 2 &&
          (value.front() == '"' || value.front() == '\'') &&
          value.back() == value.front()) {
        value = value.substr(1, value.size() - 2);
      }
      return value;
    }
  }
  return "unknown";
}

ClipboardCapabilities unavailable_capabilities(const std::string& session,
                                               const std::string& limitation,
                                               const std::string& backend) {
  ClipboardCapabilities capabilities;
  capabilities.backend = backend;
  capabilities.session = session;
  capabilities.read = false;
  capabilities.write = false;
  capabilities.watch = false;
  capabilities.independent_of_input = false;
  capabilities.limitation = limitation;
  capabilities.notes = {
      "Peer mode is still usable for transport diagnostics; inbound text is "
      "rejected with an explicit Unavailable status.",
      "--inventory only inspects environment variables and /etc/os-release.",
  };
  return capabilities;
}

}  // namespace

std::string platform_name() { return "linux"; }

ClipboardCapabilities clipboard_capabilities() {
  const std::string session_type = env_value("XDG_SESSION_TYPE");
  const std::string wayland_display = env_value("WAYLAND_DISPLAY");
  const std::string display = env_value("DISPLAY");

  if (is_wayland_session()) {
    const std::string session =
        "Wayland session (XDG_SESSION_TYPE=" +
        (session_type.empty() ? std::string("unset") : session_type) +
        ", WAYLAND_DISPLAY=" +
        (wayland_display.empty() ? std::string("unset") : wayland_display) +
        ")";
#ifdef OIT_HAVE_PORTAL
    ClipboardCapabilities capabilities = portal_clipboard_capabilities();
    capabilities.session = session;
    capabilities.notes.push_back(
        "--inventory declares the adapter, but does not open a portal "
        "session; the permission dialog and Start result are only evaluated "
        "by --read-text, --publish-*, --watch or a peer session.");
    return capabilities;
#else
    return unavailable_capabilities(
        session,
        "This build has no xdg-desktop-portal client because libdbus "
        "development files were not found at configure time. The Wayland "
        "clipboard backend is not compiled; no X11 fallback is used. Install "
        "libdbus-1-dev and rebuild.",
        "linux-wayland-portal-uncompiled");
#endif
  }

  if (is_x11_session()) {
    return unavailable_capabilities(
        "X11 session (DISPLAY=" +
            (display.empty() ? std::string("unset") : display) + ")",
        "No X11 selection backend is implemented. The Wayland portal backend "
        "is not used on X11 and this prototype does not silently substitute "
        "X11 for Wayland. Reading/writing would require libX11 and selection "
        "ownership; it is not included or verified.",
        "linux-x11-unimplemented");
  }

  return unavailable_capabilities(
      "no graphical session detected",
      "No supported graphical clipboard path is available in this "
      "environment; the prototype reports Unavailable instead of faking a "
      "backend.",
      "linux-no-graphical-session");
}

PortCreation create_clipboard_port() {
  if (is_wayland_session()) {
#ifdef OIT_HAVE_PORTAL
    return create_portal_clipboard_port();
#else
    const ClipboardCapabilities capabilities = clipboard_capabilities();
    return {nullptr, {StatusCode::Unavailable, capabilities.limitation}};
#endif
  }
  const ClipboardCapabilities capabilities = clipboard_capabilities();
  return {nullptr, {StatusCode::Unavailable, capabilities.limitation}};
}

std::string platform_inventory_text() {
  std::ostringstream out;
  out << "os_release=" << os_release_pretty_name() << "\n";

  struct utsname info {};
  if (uname(&info) == 0) {
    out << "uname=" << info.sysname << " " << info.release << " "
        << info.machine << "\n";
  }

  const auto env = [&out](const char* name) {
    const std::string value = env_value(name);
    out << name << "=" << (value.empty() ? "(unset)" : value) << "\n";
  };
  env("XDG_SESSION_TYPE");
  env("XDG_CURRENT_DESKTOP");
  env("WAYLAND_DISPLAY");
  env("DISPLAY");
  env("XDG_SESSION_ID");
#ifdef OIT_HAVE_PORTAL
  out << "portal_client=compiled\n";
#else
  out << "portal_client=unavailable\n";
#endif
  return out.str();
}

}  // namespace oit
