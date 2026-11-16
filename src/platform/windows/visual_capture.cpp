#include "../desktop.hpp"
#include "../../integration/feature_contracts.hpp"
#include "visual/windows_capture_factory.hpp"

/*
 * Local selection and capture of monitors and windows on Windows.
 *
 * The adapter retains the native capture object of the chosen identity (HWND or HMONITOR) and
 * never searches the source by title again. Changes in availability, size and closure come from
 * the retained object. If Windows Graphics Capture is not supported, unavailable is reported.
 */

namespace oneintwo {

std::unique_ptr<DesktopVisualCapture> makeWindowsVisualCapture(PlatformContext context)
{
    return windows::makeWindowsCapture(std::move(context));
}

} // namespace oneintwo
