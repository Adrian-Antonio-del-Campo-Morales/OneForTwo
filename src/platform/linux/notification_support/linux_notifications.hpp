#pragma once

#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"
#include "platform/linux/notification_support/dbus_notifications.hpp"

#include <memory>

namespace oneintwo::linux_notifications {

// El producto usa el servicio del bus de sesión. La inyección permite comprobar
// el adaptador con un servicio controlado sin depender de un escritorio.
std::unique_ptr<DesktopNotifications> makeAdapter(PlatformContext context,
                                                  std::unique_ptr<Service> service);

} // namespace oneintwo::linux_notifications
