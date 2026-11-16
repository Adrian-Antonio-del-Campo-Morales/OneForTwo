#include "coordinator.hpp"

#include "../features/visual_share.hpp"

#include <utility>

namespace oneintwo {
namespace {

bool sameRef(const VisualShareRef& left, const VisualShareRef& right) {
    return left.generation == right.generation && left.initiator == right.initiator &&
           left.offer_id == right.offer_id;
}

class NullViewer final : public VisualViewer {
public:
    void open(const VisualShareOffer&) override {}
    void present(VisualShareRef, const VisualFrame&) override {}
    void clear(VisualShareRef, VisualSourceState) override {}
    void close(VisualShareRef) override {}
};

} // namespace

struct Coordinator::Impl {
    explicit Impl(CoordinatorDependencies dependencies_in)
        : dependencies(std::move(dependencies_in)) {
        viewer = dependencies.make_viewer
                     ? dependencies.make_viewer(dependencies.runtime,
                                                [this](VisualShareRef ref) {
                                                    onViewerClosed(ref);
                                                })
                     : nullptr;
        static NullViewer no_viewer;
        VisualDependencies visual_dependencies{
            dependencies.runtime,
            dependencies.capture,
            viewer ? *viewer : no_viewer,
            [this](FeatureMessage message) { return sendFromVisual(std::move(message)); },
            [this](VisualShareOffer offer) { presentOffer(offer); },
            [this](VisualShareRef ref) { withdrawOffer(ref); },
            [this](std::string error) { dependencies.view.setError(error); },
        };
        visual = std::make_unique<VisualShare>(std::move(visual_dependencies));

        UiActions actions;
        actions.profile_changed = [this](ProfileId profile) { setProfile(profile); };
        actions.permissions_changed = [this](FeatureSet send, FeatureSet receive) {
            setUserPermissions(send, receive);
        };
        actions.share_requested = [this](VisualSourceKind source) { shareSource(source); };
        actions.stop_requested = [this] { stopActiveVisual(); };
        actions.offer_decision = [this](VisualShareRef ref, VisualDecision decision) {
            decideOffer(ref, decision);
        };
        actions.notice_requested = [this](NotificationContent content) {
            sendManualNotice(std::move(content));
        };
        actions.allowed_applications_changed = [this](std::set<std::string> apps) {
            setAllowedApplications(std::move(apps));
        };
        dependencies.view.bindActions(std::move(actions));

        dependencies.session.applyProfileLimits(requestedFeatures(profile));
        dependencies.session.setGrantedFeatures(grants_send, grants_receive);
        refreshAuthorization();
        dependencies.view.setSessionAvailable(false);
        dependencies.view.setStatus("Sin sesión");
    }

    ~Impl() {
        visual.reset();
        viewer.reset();
    }

    void refreshAuthorization() {
        const FeatureSet send = locked ? FeatureSet{} : dependencies.session.effectiveSendFeatures();
        const FeatureSet receive =
            locked ? FeatureSet{} : dependencies.session.effectiveReceiveFeatures();
        if (visual) {
            visual->setAuthorization(generation, send, receive);
        }
        dependencies.notifications.setAuthorization(generation, send, receive);
        dependencies.view.setEffectiveFeatures(send, receive);
    }

    SendResult sendFromVisual(FeatureMessage message) {
        const auto* offer = std::get_if<VisualShareOffer>(&message);
        const auto* stop = std::get_if<VisualStopMessage>(&message);
        const std::optional<VisualShareRef> offered = offer ? std::optional<VisualShareRef>(offer->ref)
                                                            : std::nullopt;
        const std::optional<VisualShareRef> stopped = stop ? std::optional<VisualShareRef>(stop->ref)
                                                           : std::nullopt;
        const SendResult result = dependencies.session.send(std::move(message));
        if (offered.has_value() && result == SendResult::queued) {
            active = *offered;
            dependencies.view.setVisualActive(true, "Compartiendo");
        }
        if (stopped.has_value() && result == SendResult::queued) {
            if (active.has_value() && sameRef(*active, *stopped)) {
                active.reset();
                dependencies.view.setVisualActive(false, "");
            }
        }
        return result;
    }

    void presentOffer(const VisualShareOffer& offer) {
        active = offer.ref;
        dependencies.view.showVisualOffer(offer, dependencies.peer_name);
    }

    void withdrawOffer(VisualShareRef ref) {
        if (active.has_value() && sameRef(*active, ref)) {
            active.reset();
        }
        dependencies.view.dismissVisualOffer();
    }

    void onViewerClosed(VisualShareRef ref) {
        if (visual) {
            visual->stop(ref);
        }
        if (active.has_value() && sameRef(*active, ref)) {
            active.reset();
            dependencies.view.setVisualActive(false, "");
        }
    }

    void setProfile(ProfileId new_profile) {
        profile = new_profile;
        dependencies.session.applyProfileLimits(requestedFeatures(profile));
        refreshAuthorization();
        if (dependencies.persist_profile) {
            dependencies.persist_profile(profile);
        }
    }

    void setUserPermissions(FeatureSet send, FeatureSet receive) {
        grants_send = send;
        grants_receive = receive;
        dependencies.session.setGrantedFeatures(grants_send, grants_receive);
        refreshAuthorization();
    }

    void shareSource(VisualSourceKind source) {
        if (!dependencies.session.established()) {
            dependencies.view.setError("No hay una sesión establecida");
            return;
        }
        if (locked) {
            dependencies.view.setError("La sesión está bloqueada");
            return;
        }
        if (visual) {
            visual->offerSource(generation, source);
        }
    }

    void stopActiveVisual() {
        if (active.has_value() && visual) {
            visual->stop(*active);
        }
        active.reset();
        dependencies.view.setVisualActive(false, "");
    }

    void decideOffer(VisualShareRef offer, VisualDecision decision) {
        if (visual) {
            visual->decideOffer(offer, decision);
        }
        if (decision != VisualDecision::accepted) {
            if (active.has_value() && sameRef(*active, offer)) {
                active.reset();
                dependencies.view.setVisualActive(false, "");
            }
        } else {
            active = offer;
            dependencies.view.setVisualActive(true, "Recibiendo");
        }
    }

    void sendManualNotice(NotificationContent content) {
        if (!dependencies.session.established()) {
            dependencies.view.setError("No hay una sesión establecida");
            return;
        }
        if (locked) {
            dependencies.view.setError("La sesión está bloqueada");
            return;
        }
        if (!dependencies.session.effectiveSendFeatures().manual_notifications) {
            dependencies.view.setError("El envío manual de avisos está desactivado");
            return;
        }
        dependencies.notifications.sendManual(generation, content);
    }

    void setAllowedApplications(std::set<std::string> app_ids) {
        dependencies.notifications.setAllowedApplications(app_ids);
        if (dependencies.persist_applications) {
            dependencies.persist_applications(std::move(app_ids));
        }
    }

    void setLocked(bool value) {
        locked = value;
        if (locked) {
            if (active.has_value() && visual) {
                visual->stop(*active);
            }
            active.reset();
            dependencies.view.setVisualActive(false, "");
        }
        refreshAuthorization();
        dependencies.view.setStatus(locked ? "Sesión bloqueada" : "Sesión reanudada");
    }

    void receive(FeatureMessage message) {
        if (auto* offer = std::get_if<VisualShareOffer>(&message)) {
            if (visual) {
                visual->receiveOffer(*offer);
            }
            return;
        }
        if (auto* decision = std::get_if<VisualDecisionMessage>(&message)) {
            const bool current = active.has_value() && sameRef(*active, decision->ref);
            if (visual) {
                visual->receiveDecision(decision->ref, decision->decision);
            }
            if (current && decision->decision != VisualDecision::accepted) {
                active.reset();
                dependencies.view.setVisualActive(false, "");
            }
            return;
        }
        if (auto* state = std::get_if<VisualStateMessage>(&message)) {
            const bool current = active.has_value() && sameRef(*active, state->ref);
            if (visual) {
                visual->receiveSourceState(state->ref, state->revision, state->state);
            }
            if (current && state->state == VisualSourceState::closed) {
                active.reset();
                dependencies.view.setVisualActive(false, "");
            }
            return;
        }
        if (auto* activity = std::get_if<VisualActivityMessage>(&message)) {
            if (visual) {
                visual->receiveActivity(activity->ref, activity->revision);
            }
            return;
        }
        if (auto* stop = std::get_if<VisualStopMessage>(&message)) {
            const bool current = active.has_value() && sameRef(*active, stop->ref);
            if (visual) {
                visual->receiveStop(stop->ref);
            }
            if (current) {
                active.reset();
                dependencies.view.setVisualActive(false, "");
            }
            return;
        }
        if (auto* frame = std::get_if<VisualEncodedFrame>(&message)) {
            if (visual) {
                visual->receiveFrame(std::move(*frame));
            }
            return;
        }
        if (auto* notice = std::get_if<RemoteNotification>(&message)) {
            dependencies.notifications.receiveNotice(*notice);
        }
    }

    CoordinatorDependencies dependencies;
    std::unique_ptr<VisualViewer> viewer;
    std::unique_ptr<VisualShare> visual;
    ProfileId profile = ProfileId::usual;
    FeatureSet grants_send{};
    FeatureSet grants_receive{};
    SessionGeneration generation = 0;
    std::optional<VisualShareRef> active;
    bool locked = false;
};

Coordinator::Coordinator(CoordinatorDependencies dependencies)
    : impl_(std::make_unique<Impl>(std::move(dependencies))) {}

Coordinator::~Coordinator() = default;

void Coordinator::tick() {
    if (impl_->visual) {
        impl_->visual->tick();
    }
    impl_->dependencies.notifications.tick();
}

void Coordinator::sessionEstablished(SessionGeneration generation) {
    impl_->generation = generation;
    impl_->active.reset();
    impl_->dependencies.view.setVisualActive(false, "");
    impl_->dependencies.view.setSessionAvailable(true);
    impl_->dependencies.view.setStatus("Sesión establecida");
    impl_->refreshAuthorization();
}

void Coordinator::sessionClosed() {
    impl_->generation = 0;
    impl_->active.reset();
    impl_->dependencies.view.setVisualActive(false, "");
    impl_->dependencies.view.setSessionAvailable(false);
    impl_->dependencies.view.setStatus("Sin sesión");
    if (impl_->visual) {
        impl_->visual->setAuthorization(0, FeatureSet{}, FeatureSet{});
    }
    impl_->dependencies.notifications.setAuthorization(0, FeatureSet{}, FeatureSet{});
}

void Coordinator::receive(FeatureMessage message) {
    impl_->receive(std::move(message));
}

void Coordinator::setProfile(ProfileId profile) {
    impl_->setProfile(profile);
}

void Coordinator::setUserPermissions(FeatureSet send, FeatureSet receive) {
    impl_->setUserPermissions(send, receive);
}

void Coordinator::shareSource(VisualSourceKind source) {
    impl_->shareSource(source);
}

void Coordinator::stopActiveVisual() {
    impl_->stopActiveVisual();
}

void Coordinator::decideOffer(VisualShareRef offer, VisualDecision decision) {
    impl_->decideOffer(offer, decision);
}

void Coordinator::setAllowedApplications(std::set<std::string> app_ids) {
    impl_->setAllowedApplications(std::move(app_ids));
}

void Coordinator::sendManualNotice(NotificationContent content) {
    impl_->sendManualNotice(std::move(content));
}

void Coordinator::setLocked(bool locked) {
    impl_->setLocked(locked);
}

ProfileId Coordinator::profile() const {
    return impl_->profile;
}

FeatureSet Coordinator::effectiveSendFeatures() const {
    return impl_->dependencies.session.effectiveSendFeatures();
}

FeatureSet Coordinator::effectiveReceiveFeatures() const {
    return impl_->dependencies.session.effectiveReceiveFeatures();
}

SessionGeneration Coordinator::generation() const {
    return impl_->generation;
}

bool Coordinator::hasActiveVisual() const {
    return impl_->active.has_value();
}

} // namespace oneintwo
