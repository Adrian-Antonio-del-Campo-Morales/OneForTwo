#include "integration/feature_contracts.hpp"
#include "platform/linux/notification_support/linux_notifications.hpp"

#include <memory>
#include <utility>

/*
 * Fábrica del backend Linux. Selecciona el servicio del bus de sesión y deja la
 * observación automática como no disponible mientras no exista un proveedor
 * autorizado de lectura de avisos ajenos.
 */

namespace oneintwo {

std::unique_ptr<DesktopNotifications> makeLinuxNotifications(PlatformContext context) {
    return linux_notifications::makeAdapter(std::move(context),
                                            linux_notifications::makeSessionService());
}

} // namespace oneintwo
