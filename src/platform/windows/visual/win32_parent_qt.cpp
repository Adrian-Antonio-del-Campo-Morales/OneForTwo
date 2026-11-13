#include "win32_parent.hpp"

#include <QWidget>

#include <cstdint>

namespace oneintwo::windows {

std::uintptr_t nativeParentWindow(const PlatformContext& context)
{
    if (context.parent == nullptr) {
        return 0;
    }
    return static_cast<std::uintptr_t>(context.parent->winId());
}

} // namespace oneintwo::windows
