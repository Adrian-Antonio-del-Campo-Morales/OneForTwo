#pragma once

// Responsabilidad: adaptador Linux/Wayland basado en las interfaces oficiales
// xdg-desktop-portal RemoteDesktop + Clipboard. Solicita una sesión con cero
// dispositivos de entrada y nunca llama a métodos Notify* ni ConnectToEIS.
// No hay fallback a X11: el llamador debe elegir esta vía sólo en Wayland.

#include <string>

#include "platform/clipboard.hpp"

namespace oit {

// Capacidades declaradas por el adaptador. La concesión efectiva depende del
// diálogo del portal; crear el puerto es lo que lo comprueba.
ClipboardCapabilities portal_clipboard_capabilities();

// Crea la sesión de portal y solicita clipboard_access. Puede bloquearse
// mientras el usuario decide; nunca se llama desde --inventory ni --self-test.
PortCreation create_portal_clipboard_port();

// Comprueba codificación/decodificación de mensajes D-Bus y selección de MIME
// sin abrir una conexión, un bus, una sesión de permisos ni el portapapeles.
bool run_portal_isolated_checks(std::string& detail);

// Pipes sintéticos: llegada diferida, EOF y timeout, sin portapapeles ni bus.
bool run_portal_pipe_isolated_checks(std::string& detail);

}  // namespace oit
