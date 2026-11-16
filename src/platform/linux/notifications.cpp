#include "integration/feature_contracts.hpp"
#include "platform/linux/notification_support/linux_notifications.hpp"

#include <memory>
#include <utility>

/*
 * Linux backend factory. Select the session bus service and leave automatic observation as
 * unavailable as long as there is no authorized provider for reading other people's notices.
 */

namespace oneintwo {

std::unique_ptr<DesktopNotifications> makeLinuxNotifications(PlatformContext context) {
    return linux_notifications::makeAdapter(std::move(context),
                                            linux_notifications::makeSessionService());
}

} // namespace oneintwo
