#pragma once

#include "../platform/desktop.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <variant>

class QWidget;

namespace oneintwo {

using MonotonicTime = std::chrono::steady_clock::time_point;
using Task = std::function<void()>;

struct ModuleRuntime {
    std::function<MonotonicTime()> now;
    std::function<void(Task)> post;
};

struct VisualDecisionMessage {
    VisualShareRef ref;
    VisualDecision decision;
};
struct VisualStateMessage {
    VisualShareRef ref;
    VisualSourceRevision revision;
    VisualSourceState state;
};
struct VisualActivityMessage {
    VisualShareRef ref;
    VisualSourceRevision revision;
};
struct VisualStopMessage {
    VisualShareRef ref;
};

using FeatureMessage = std::variant<VisualShareOffer, VisualDecisionMessage,
    VisualStateMessage, VisualActivityMessage, VisualStopMessage,
    VisualEncodedFrame, RemoteNotification>;

enum class SendResult {
    queued,
    busy,
    disconnected,
    denied,
};

// queued transfers the property to the session without confirming delivery.
using SendFeatureMessage = std::function<SendResult(FeatureMessage)>;

struct VisualDependencies {
    ModuleRuntime runtime;
    DesktopVisualCapture& capture;
    VisualViewer& viewer;
    SendFeatureMessage send;
    std::function<void(VisualShareOffer)> show_offer;
    std::function<void(VisualShareRef)> dismiss_offer;
    std::function<void(std::string)> report_error;
};

struct NotificationDependencies {
    ModuleRuntime runtime;
    DesktopNotifications& desktop;
    SendFeatureMessage send;
    std::function<std::string()> peer_name;
    std::function<void(std::string)> report_error;
};

struct DesktopEvents {
    std::function<void()> capabilities_changed;
    std::function<void(bool)> locked;
};

struct PlatformContext {
    ModuleRuntime runtime;
    QWidget* parent = nullptr;
    DesktopEvents events;
};

std::unique_ptr<DesktopVisualCapture> makeWindowsVisualCapture(PlatformContext context);
std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCapture(PlatformContext context);
std::unique_ptr<DesktopNotifications> makeWindowsNotifications(PlatformContext context);
std::unique_ptr<DesktopNotifications> makeLinuxNotifications(PlatformContext context);
std::unique_ptr<VisualViewer> makeVisualViewer(
    ModuleRuntime runtime, std::function<void(VisualShareRef)> user_closed);

inline constexpr auto feature_tick_interval = std::chrono::milliseconds{100};
inline constexpr auto visual_activity_interval = std::chrono::seconds{1};
inline constexpr auto visual_activity_timeout = std::chrono::seconds{5};
inline constexpr std::size_t visual_fragment_bytes = 64 * 1024;
inline constexpr std::size_t notification_queue_capacity = 16;
// Limit of interactive channel and local politics: three per second.
inline constexpr unsigned notification_rate_per_second = 3;

} // namespace oneintwo
