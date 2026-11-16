#include "windows_capture_factory.hpp"
#include "capture_model.hpp"
#include "win32_capture_port.hpp"

#if defined(_WIN32) && defined(__has_include)
#if __has_include(<winrt/Windows.Graphics.Capture.h>) \
    && __has_include(<windows.graphics.capture.interop.h>) \
    && __has_include(<windows.graphics.directx.direct3d11.interop.h>)
#define ONEINTWO_HAS_WGC 1
#include "win32_port.hpp"
#endif
#endif

#if defined(_WIN32) && defined(ONEINTWO_REQUIRE_WGC) && !defined(ONEINTWO_HAS_WGC)
#error "ONEINTWO_REQUIRE_WGC=ON requiere winrt/Windows.Graphics.Capture.h, windows.graphics.capture.interop.h y windows.graphics.directx.direct3d11.interop.h"
#endif

#include <utility>

namespace oneintwo::windows {

std::unique_ptr<DesktopVisualCapture> makeWindowsCapture(PlatformContext context)
{
#if defined(ONEINTWO_HAS_WGC)
    auto port = makeWin32CapturePort(context);
    return std::make_unique<CaptureModel>(std::move(context), std::move(port));
#else
    return std::make_unique<CaptureModel>(std::move(context),
                                          makeUnavailableWindowsCapturePort());
#endif
}

} // namespace oneintwo::windows
