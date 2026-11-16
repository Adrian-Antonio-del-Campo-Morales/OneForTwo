// Force the selector link and WGC: a static library by itself
// does not check for d3d11, dwmapi, windowsapp and the rest of import libs.
#include "platform/windows/visual/win32_port.hpp"

int main()
{
    oneintwo::PlatformContext context{};
    const auto port = oneintwo::windows::makeWin32CapturePort(context);
    const auto capture
        = oneintwo::windows::createWin32Capture(0, oneintwo::VisualSourceKind::monitor);
    (void)capture;
    return port && oneintwo::windows::windowsGraphicsCaptureSupported() ? 0 : 1;
}
