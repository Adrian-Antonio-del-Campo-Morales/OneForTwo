#include "../desktop.hpp"
#include "../../integration/feature_contracts.hpp"
#include "visual/windows_capture_factory.hpp"

/*
 * Selección y captura local de monitores y ventanas en Windows.
 *
 * El adaptador retiene el objeto de captura nativo de la identidad elegida
 * (HWND o HMONITOR) y nunca vuelve a buscar la fuente por título. Los cambios
 * de disponibilidad, tamaño y cierre proceden del objeto retenido. Si Windows
 * Graphics Capture no está soportado, se informa unavailable.
 */

namespace oneintwo {

std::unique_ptr<DesktopVisualCapture> makeWindowsVisualCapture(PlatformContext context)
{
    return windows::makeWindowsCapture(std::move(context));
}

} // namespace oneintwo
