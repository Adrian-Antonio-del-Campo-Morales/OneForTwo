#pragma once

#include <string_view>

namespace oneintwo::notification_support {

// Product application identity. Windows uses it as AppUserModelID and the
// packaging material of src/platform/windows/notification_support/ the
// replica. The native identifier of other applications never leaves the computer.
inline constexpr const char* product_application_id = "OneInTwo.Desktop";
inline constexpr const char* product_display_name = "OneInTwo";

// The packaged applications expose your AppUserModelID with the suffix of the
// package family. Compared identity prefixes, not visible names.
bool is_product_application(std::string_view application_id);

} // namespace oneintwo::notification_support
