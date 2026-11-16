#pragma once

#include "../../../integration/feature_contracts.hpp"

#include <string>

namespace oneintwo::wayland {

/*
 * Identificador del padre del selector para el portal.
 * Con Qt sobre XWayland se exporta la ventana X11; en Wayland nativo el
 * compositor controla el diálogo y se deja vacío, que el portal admite.
 */
std::string portalParentWindow(const PlatformContext& context);

} // namespace oneintwo::wayland
