#include "parent_window.hpp"

#include <QGuiApplication>
#include <QLatin1String>
#include <QWidget>

#include <cstdio>

namespace oneintwo::wayland {

std::string portalParentWindow(const PlatformContext& context)
{
    if (context.parent == nullptr) {
        return {};
    }
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
        // En Wayland nativo el padre requeriría exportar una superficie con
        // xdg-foreign; el portal admite la cadena vacía.
        return {};
    }
    const WId id = context.parent->winId();
    if (id == 0) {
        return {};
    }
    char token[32] = {};
    std::snprintf(token, sizeof(token), "x11:0x%lx", static_cast<unsigned long>(id));
    return token;
}

} // namespace oneintwo::wayland
