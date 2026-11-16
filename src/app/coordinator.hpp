#pragma once

#include "../features/profiles.hpp"
#include "../integration/feature_contracts.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace oneintwo {

// Session port. The product adapts it to AuthenticatedSession; the tests
// Composition use a double that records permissions and messages.
class SessionPort {
public:
    virtual ~SessionPort() = default;
    virtual SendResult send(FeatureMessage message) = 0;
    virtual void applyProfileLimits(FeatureSet limits) = 0;
    virtual void setGrantedFeatures(FeatureSet send, FeatureSet receive) = 0;
    virtual FeatureSet effectiveSendFeatures() const = 0;
    virtual FeatureSet effectiveReceiveFeatures() const = 0;
    virtual SessionGeneration generation() const = 0;
    virtual bool established() const = 0;
};

// Port of the notice module. The product adapts it to SharedNotifications.
class NotificationPort {
public:
    virtual ~NotificationPort() = default;
    virtual void setAuthorization(SessionGeneration generation, FeatureSet send,
                                  FeatureSet receive) = 0;
    virtual void setAllowedApplications(const std::set<std::string>& app_ids) = 0;
    virtual void tick() = 0;
    virtual void sendManual(SessionGeneration generation, const NotificationContent& content) = 0;
    virtual void receiveNotice(const RemoteNotification& notice) = 0;
};

struct UiActions {
    std::function<void(ProfileId)> profile_changed;
    std::function<void(FeatureSet, FeatureSet)> permissions_changed;
    std::function<void(VisualSourceKind)> share_requested;
    std::function<void()> stop_requested;
    std::function<void(VisualShareRef, VisualDecision)> offer_decision;
    std::function<void(NotificationContent)> notice_requested;
    std::function<void(std::set<std::string>)> allowed_applications_changed;
};

// Main window contract with the coordinator.
class CoordinatorView {
public:
    virtual ~CoordinatorView() = default;
    virtual void bindActions(UiActions actions) = 0;
    virtual void showVisualOffer(const VisualShareOffer& offer, const std::string& peer_name) = 0;
    virtual void dismissVisualOffer() = 0;
    virtual void setVisualActive(bool active, const std::string& description) = 0;
    virtual void setSessionAvailable(bool available) = 0;
    virtual void setEffectiveFeatures(FeatureSet send, FeatureSet receive) = 0;
    virtual void setStatus(const std::string& status) = 0;
    virtual void setError(const std::string& error) = 0;
};

struct CoordinatorDependencies {
    ModuleRuntime runtime;
    SessionPort& session;
    DesktopVisualCapture& capture;
    NotificationPort& notifications;
    CoordinatorView& view;
    std::function<std::unique_ptr<VisualViewer>(ModuleRuntime, std::function<void(VisualShareRef)>)>
        make_viewer;
    std::string peer_name;

    // Optional local preferences persistence; the coordinator does not know
    // the specific warehouse.
    std::function<void(ProfileId)> persist_profile;
    std::function<void(std::set<std::string>)> persist_applications;
};

class Coordinator {
public:
    explicit Coordinator(CoordinatorDependencies dependencies);
    ~Coordinator();
    Coordinator(const Coordinator&) = delete;
    Coordinator& operator=(const Coordinator&) = delete;

    void tick();
    void sessionEstablished(SessionGeneration generation);
    void sessionClosed();
    void receive(FeatureMessage message);

    void setProfile(ProfileId profile);
    void setUserPermissions(FeatureSet send, FeatureSet receive);
    void shareSource(VisualSourceKind source);
    void stopActiveVisual();
    void decideOffer(VisualShareRef offer, VisualDecision decision);
    void setAllowedApplications(std::set<std::string> app_ids);
    void sendManualNotice(NotificationContent content);
    void setLocked(bool locked);

    ProfileId profile() const;
    FeatureSet effectiveSendFeatures() const;
    FeatureSet effectiveReceiveFeatures() const;
    SessionGeneration generation() const;
    bool hasActiveVisual() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
