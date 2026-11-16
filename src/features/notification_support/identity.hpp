#pragma once

#include <string_view>

namespace oneintwo::notification_support {

// Identidad de aplicación del producto. Windows la usa como AppUserModelID y el
// material de empaquetado de src/platform/windows/notification_support/ la
// replica. El identificador nativo de otras aplicaciones nunca sale del equipo.
inline constexpr const char* product_application_id = "OneInTwo.Desktop";
inline constexpr const char* product_display_name = "OneInTwo";

// Las aplicaciones empaquetadas exponen su AppUserModelID con el sufijo de la
// familia de paquete. Se comparan prefijos de identidad, no nombres visibles.
bool is_product_application(std::string_view application_id);

} // namespace oneintwo::notification_support
