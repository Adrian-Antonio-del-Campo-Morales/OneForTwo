#include "windows_capture_factory.hpp"
#include "capture_model.hpp"
#include "win32_capture_port.hpp"

#if defined(_WIN32) && defined(__has_include)
#if __has_include(<winrt/Windows.Graphics.Capture.h>)
#define ONEINTWO_HAS_WGC 1
#include "win32_port.hpp"
#endif
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
