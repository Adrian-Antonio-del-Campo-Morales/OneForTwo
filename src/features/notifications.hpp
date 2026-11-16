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

// The application identifier is used for the local filter.
struct ObservedNotification {
    std::string app_id;
    std::string app_name;
    NotificationContent content;
};

// Application available on the approved list. app_id is the
// reliable local identity; app_name only serves to show it.
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
    std::string app_name; // Empty in manual warnings.
};

// The coordinator provides the current generation when sending.
// The session validates the meaning before delivering a notice received.
struct NotificationDependencies;

class SharedNotifications {
public:
    explicit SharedNotifications(NotificationDependencies dependencies);
    ~SharedNotifications();
    SharedNotifications(const SharedNotifications&) = delete;
    SharedNotifications& operator=(const SharedNotifications&) = delete;
    void tick();
    // List applications with current notices. You can request the
    // consent of the system and does not initiate or modify observation.
    // The callback runs only once and can arrive later.
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
