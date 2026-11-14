// Fuerza el enlace del selector y de WGC: una biblioteca estática por sí sola
// no comprueba que existan d3d11, dwmapi, windowsapp y el resto de import libs.
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
