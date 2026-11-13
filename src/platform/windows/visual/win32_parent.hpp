#pragma once

#include "../../../integration/feature_contracts.hpp"

#include <cstdint>

namespace oneintwo::windows {

// Devuelve el HWND del padre del selector, o 0 si no está disponible.
std::uintptr_t nativeParentWindow(const PlatformContext& context);

} // namespace oneintwo::windows
