#pragma once

#include "../../../integration/feature_contracts.hpp"

#include <cstdint>

namespace oneintwo::windows {

// Returns the HWND of the selector parent, or 0 if it is not available.
std::uintptr_t nativeParentWindow(const PlatformContext& context);

} // namespace oneintwo::windows
