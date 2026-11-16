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
        // In native Wayland the father would need to export a surface with
        // xdg-foreign; portal supports empty string.
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
