#pragma once

#include "../features/notifications.hpp"
#include "../features/visual_share.hpp"
#include "desktop_result.hpp"

#include <functional>
#include <string>
#include <vector>

/*
 * Contract between common logic and desktop. The other native operations are described in their
 * adapters and in ARCHITECTURE.md.
 *
 * The desktop maintains the geometry, graphical session status and system permissions. Your
 * clipboard adapter coordinates text and files. Input capture and injection are different
 * capabilities and should normalize events and declare your yarn affinity. URL opening is
 * delegated to the local browser after validating and confirming the offer.
 */

namespace oneintwo {

struct VisualCaptureCapabilities {
    bool monitor = false;
    bool window = false;
};

struct VisualCaptureCallbacks {
    std::function<void(VisualShareRef, VisualFrame)> frame;
    std::function<void(VisualShareRef, VisualSourceState)> state_changed;
};

class DesktopVisualCapture {
public:
    virtual ~DesktopVisualCapture() = default;
    virtual VisualCaptureCapabilities visualCapabilities() const = 0;

    // The adapter retains a single local source by reference.
    virtual void selectSource(VisualShareRef offer, VisualSourceKind kind,
                              std::function<void(VisualShareRef, DesktopResult)> selected) = 0;
    // It's called after acceptance.
    virtual void startCapture(VisualShareRef offer, VisualCaptureCallbacks callbacks) = 0;
    // Cancel the pending selector or release the active source.
    virtual void stopCapture(VisualShareRef offer) = 0;
};

struct NotificationCapabilities {
    bool present = false;
    bool observe = false;
};

class DesktopNotifications {
public:
    virtual ~DesktopNotifications() = default;
    virtual NotificationCapabilities notificationCapabilities() const = 0;
    virtual void requestObservationPermission(std::function<void(DesktopResult)> complete) = 0;
    // Returns a snapshot of apps with existing ads. You can
    // request consent. It does not initiate observation or alter its baseline.
    virtual void discoverApplications(
        std::function<void(DesktopResult, std::vector<NotificationApplication>)> complete) = 0;
    virtual void startObserving(std::function<void(ObservedNotification)> observed,
                                std::function<void(DesktopResult)> state_changed) = 0;
    virtual void stopObserving() = 0;

    // peer_name comes from the session. The adapter presents the contents as text.
    virtual void presentNotice(const std::string& peer_name,
                               const RemoteNotification& notice,
                               std::function<void(DesktopResult)> complete) = 0;
    virtual void withdrawNotices() = 0;
};

} // namespace oneintwo
