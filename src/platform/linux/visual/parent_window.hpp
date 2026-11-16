#pragma once

#include "../../../integration/feature_contracts.hpp"

#include <string>

namespace oneintwo::wayland {

/*
 * Portal selector parent identifier. With Qt over XWayland the X11 window is exported; in Native
 * Wayland the composer controls the dialogue and is left empty, which the portal supports.
 */
std::string portalParentWindow(const PlatformContext& context);

} // namespace oneintwo::wayland
