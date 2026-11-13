#pragma once

#include "../../../integration/feature_contracts.hpp"

#include <memory>

namespace oneintwo {
class DesktopVisualCapture;
}

namespace oneintwo::windows {

std::unique_ptr<DesktopVisualCapture> makeWindowsCapture(PlatformContext context);

} // namespace oneintwo::windows
