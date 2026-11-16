#pragma once

#include "../core/session.hpp"
#include "../platform/desktop_result.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <set>
#include <vector>

namespace oneintwo {

inline constexpr std::size_t max_notification_title_bytes = 256;
inline constexpr std::size_t max_notification_body_bytes = 2048;
inline constexpr std::size_t max_notification_app_name_bytes = 128;
inline constexpr auto notification_queue_lifetime = std::chrono::seconds{10};

enum class NotificationMode {
    manual,
    forwarded,
};

struct NotificationContent {
    std::string title;
    std::string body;
};

// El identificador de la aplicación se usa para el filtro local.
struct ObservedNotification {
    std::string app_id;
    std::string app_name;
    NotificationContent content;
};

// Aplicación que puede ofrecerse en la lista autorizada. app_id es la
// identidad local fiable; app_name solo sirve para mostrarla.
struct NotificationApplication {
    std::string app_id;
    std::string app_name;
};

struct NotificationRef {
    SessionGeneration generation;
    std::uint64_t notice_id;
};

struct RemoteNotification {
    NotificationRef ref;
    NotificationMode mode;
    NotificationContent content;
    std::string app_name; // Vacío en los avisos manuales.
};

// El coordinador suministra la generación vigente al enviar.
// La sesión valida el sentido antes de entregar un aviso recibido.
struct NotificationDependencies;

class SharedNotifications {
public:
    explicit SharedNotifications(NotificationDependencies dependencies);
    ~SharedNotifications();
    SharedNotifications(const SharedNotifications&) = delete;
    SharedNotifications& operator=(const SharedNotifications&) = delete;
    void tick();
    // Enumera las aplicaciones con avisos actuales. Puede solicitar el
    // consentimiento del sistema y no inicia ni modifica la observación.
    // El callback se ejecuta una sola vez y puede llegar después.
    void discoverApplications(
        std::function<void(DesktopResult, std::vector<NotificationApplication>)> complete);
    void setAllowedApplications(std::set<std::string> app_ids);
    void setAuthorization(SessionGeneration generation, FeatureSet send, FeatureSet receive);
    void sendManual(SessionGeneration generation, const NotificationContent& content);
    void forwardObserved(SessionGeneration generation, const ObservedNotification& notice);
    void receiveNotice(const RemoteNotification& notice);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
