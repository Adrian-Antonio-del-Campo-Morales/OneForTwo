#pragma once

#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"
#include "platform/linux/notification_support/dbus_notifications.hpp"

#include <memory>

namespace oneintwo::linux_notifications {

// The product uses the service of the session bus. The injection allows to check
// the adapter with a controlled service without relying on a desk.
std::unique_ptr<DesktopNotifications> makeAdapter(PlatformContext context,
                                                  std::unique_ptr<Service> service);

} // namespace oneintwo::linux_notifications
