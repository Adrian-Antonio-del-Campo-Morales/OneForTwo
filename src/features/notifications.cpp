#include "notifications.hpp"

#include "notification_support/identity.hpp"
#include "notification_support/policy.hpp"
#include "notification_support/text_validation.hpp"
#include "../integration/feature_contracts.hpp"
#include "../platform/desktop.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

/*
 * Manual notices and forwarding of new notifications.
 *
 * All public entries are executed in the main thread of the coordinator. The adapters deliver
 * their callbacks by ModuleRuntime.post and the result of presentNotice also returns by that
 * executor. The module retains an emission counter per generation and shared sense, the latest
 * identifier received, the queues taped with its input instant and the current observation
 * subscription.
 */

namespace oneintwo {
namespace {

using notification_support::is_blank;
using notification_support::is_product_application;
using notification_support::is_valid_utf8;

constexpr std::size_t queue_capacity = notification_policy::queue_capacity;
constexpr auto queue_lifetime = notification_policy::queue_lifetime;

constexpr const char* manual_rejected = "Discarded manual warning: ";
constexpr const char* forwarded_rejected = "Discarded forward notice: ";

enum class ContentProblem {
    none,
    missing_title,
    malformed,
    too_long,
};

ContentProblem checkContent(const NotificationContent& content) {
    if (!is_valid_utf8(content.title) || !is_valid_utf8(content.body)) {
        return ContentProblem::malformed;
    }
    if (is_blank(content.title)) {
        return ContentProblem::missing_title;
    }
    if (content.title.size() > max_notification_title_bytes ||
        content.body.size() > max_notification_body_bytes) {
        return ContentProblem::too_long;
    }
    return ContentProblem::none;
}

const char* contentProblemDetail(ContentProblem problem) {
    switch (problem) {
    case ContentProblem::missing_title:
        return "the title is compulsory";
    case ContentProblem::malformed:
        return "misformed content";
    case ContentProblem::too_long:
        return "off-limit content";
    case ContentProblem::none:
        break;
    }
    return "invalid content";
}

std::string contentProblemText(NotificationMode mode, ContentProblem problem) {
    const char* prefix =
        mode == NotificationMode::forwarded ? forwarded_rejected : manual_rejected;
    return std::string(prefix) + contentProblemDetail(problem);
}

const char* presentationFailureText(DesktopResult result) {
    switch (result) {
    case DesktopResult::cancelled:
        return "Unsubmitted notice: cancelled by desktop";
    case DesktopResult::denied:
        return "Unsubmitted notice: permission of desk denied";
    case DesktopResult::unavailable:
        return "not submitted: not available notification service";
    case DesktopResult::error:
        return "not submitted notice: desktop error";
    case DesktopResult::ready:
        break;
    }
    return "not submitted";
}

bool modePermitted(FeatureSet features, NotificationMode mode) {
    return mode == NotificationMode::manual ? features.manual_notifications
                                            : features.forwarded_notifications;
}

bool validMode(NotificationMode mode) {
    const auto raw = static_cast<unsigned>(mode);
    return raw == static_cast<unsigned>(NotificationMode::manual) ||
           raw == static_cast<unsigned>(NotificationMode::forwarded);
}

} // namespace

struct SharedNotifications::Impl {
    struct Lifetime {
        std::atomic<bool> alive{true};
        Impl* owner = nullptr;
    };

    struct Outbound {
        RemoteNotification notice;
        std::string source_app_id; // Empty in manual warnings.
        MonotonicTime queued_at;
    };

    struct Inbound {
        RemoteNotification notice;
        MonotonicTime queued_at;
    };

    struct ObservedFingerprint {
        std::string app_id;
        std::string title;
        std::string body;
        MonotonicTime seen_at;
    };

    explicit Impl(NotificationDependencies dependencies)
        : deps(std::move(dependencies)), lifetime(std::make_shared<Lifetime>()) {
        lifetime->owner = this;
    }

    NotificationDependencies deps;
    std::shared_ptr<Lifetime> lifetime;

    SessionGeneration generation = 0;
    FeatureSet send_features{};
    FeatureSet receive_features{};

    std::set<std::string> allowed_applications;

    std::uint64_t next_send_id = 1;
    std::uint64_t last_received_id = 0;
    bool send_counter_exhausted = false;

    std::deque<Outbound> outbound;
    std::deque<Inbound> inbound;
    std::deque<MonotonicTime> send_stamps;
    std::deque<MonotonicTime> receive_stamps;
    std::deque<ObservedFingerprint> observed_fingerprints;

    bool observing = false;
    std::uint64_t observation_subscription = 0;
    std::uint64_t next_observation_subscription = 0;

    MonotonicTime now() const {
        return deps.runtime.now ? deps.runtime.now() : MonotonicTime{};
    }

    void report(std::string message) const {
        if (!deps.report_error) {
            return;
        }
        try {
            deps.report_error(std::move(message));
        } catch (...) {
            // The local record can never knock down the main thread.
        }
    }

    bool canSend(NotificationMode mode) const {
        return modePermitted(send_features, mode);
    }

    bool canReceive(NotificationMode mode) const {
        return modePermitted(receive_features, mode);
    }

    static bool allow(std::deque<MonotonicTime>& stamps, MonotonicTime current) {
        while (!stamps.empty() && current - stamps.front() >= notification_policy::rate_window) {
            stamps.pop_front();
        }
        if (stamps.size() >= notification_policy::rate_allowance) {
            return false;
        }
        stamps.push_back(current);
        return true;
    }

    SendResult deliver(const RemoteNotification& notice) const {
        if (!deps.send) {
            return SendResult::disconnected;
        }
        try {
            return deps.send(FeatureMessage{notice});
        } catch (...) {
            return SendResult::disconnected;
        }
    }

    void withdrawActive() const {
        try {
            deps.desktop.withdrawNotices();
        } catch (...) {
        }
    }

    void enqueueOrSend(RemoteNotification notice, std::string source_app_id) {
        const SendResult result = deliver(notice);
        if (result == SendResult::queued) {
            return;
        }
        if (result == SendResult::busy) {
            if (outbound.size() >= queue_capacity) {
                report("Discarded notice: local queue full shipping");
                return;
            }
            outbound.push_back(Outbound{std::move(notice), std::move(source_app_id), now()});
            return;
        }
        outbound.clear();
        inbound.clear();
        report(result == SendResult::denied ? "Discarded notice: session denied shipment"
                                           : "Discarded notice: offline session");
    }

    // Assign identifier and delivery. Permission and form have already been checked.
    void emit(SessionGeneration requested_generation, NotificationMode mode,
              NotificationContent content, std::string app_name,
              std::string source_app_id) {
        if (requested_generation != generation) {
            return;
        }
        if (!canSend(mode)) {
            return;
        }
        if (!allow(send_stamps, now())) {
            report(std::string(mode == NotificationMode::forwarded ? forwarded_rejected
                                                                   : manual_rejected) +
                   "frequency limit");
            return;
        }
        if (send_counter_exhausted || next_send_id == 0) {
            report("Discarded warning: Identifier counter sold out");
            return;
        }

        const std::uint64_t identifier = next_send_id;
        if (identifier == std::numeric_limits<std::uint64_t>::max()) {
            send_counter_exhausted = true;
            next_send_id = 0;
        } else {
            ++next_send_id;
        }

        RemoteNotification notice{NotificationRef{generation, identifier}, mode,
                                  std::move(content), std::move(app_name)};
        enqueueOrSend(std::move(notice), std::move(source_app_id));
    }

    bool observedAllowed(const ObservedNotification& notice) const {
        if (notice.app_id.empty()) {
            return false;
        }
        if (is_product_application(notice.app_id)) {
            return false; // Echoing our own announcements.
        }
        return allowed_applications.find(notice.app_id) != allowed_applications.end();
    }

    bool seenRecently(const ObservedNotification& notice, MonotonicTime current) const {
        for (const auto& entry : observed_fingerprints) {
            if (current - entry.seen_at >= notification_policy::observed_dedup_lifetime) {
                continue;
            }
            if (entry.app_id == notice.app_id && entry.title == notice.content.title &&
                entry.body == notice.content.body) {
                return true;
            }
        }
        return false;
    }

    void rememberObserved(const ObservedNotification& notice, MonotonicTime current) {
        while (!observed_fingerprints.empty() &&
               (observed_fingerprints.size() >= notification_policy::observed_dedup_capacity ||
                current - observed_fingerprints.front().seen_at >=
                    notification_policy::observed_dedup_lifetime)) {
            observed_fingerprints.pop_front();
        }
        observed_fingerprints.push_back(ObservedFingerprint{
            notice.app_id, notice.content.title, notice.content.body, current});
    }

    void receiveNotice(const RemoteNotification& notice) {
        if (!validMode(notice.mode) || notice.ref.generation != generation) {
            return; // Old generation or unknown mode.
        }
        if (!canReceive(notice.mode)) {
            return; // Unauthorized: deduplication does not progress.
        }
        if (notice.ref.notice_id == 0) {
            report("notification received discarded: invalid identifier");
            return;
        }
        const ContentProblem problem = checkContent(notice.content);
        if (problem != ContentProblem::none) {
            report(std::string("Notice received discarded: ") +
                   contentProblemDetail(problem));
            return;
        }
        if (notice.mode == NotificationMode::manual) {
            if (!notice.app_name.empty()) {
                report("Discarded notice received: manual notice with application");
                return;
            }
        } else {
            if (notice.app_name.empty() || !is_valid_utf8(notice.app_name) ||
                notice.app_name.size() > max_notification_app_name_bytes) {
                report("notification received discarded: application of invalid origin");
                return;
            }
        }

        if (notice.ref.notice_id <= last_received_id) {
            return; // Repeat or identifier already consumed.
        }
        last_received_id = notice.ref.notice_id;

        const MonotonicTime current = now();
        if (!allow(receive_stamps, current)) {
            // The deduplication has already advanced: repeating it does not provoke another presentation.
            report("warning received discarded: frequency limit");
            return;
        }
        if (inbound.size() >= queue_capacity) {
            report("Notice received discarded: local queue full presentation");
            return;
        }
        inbound.push_back(Inbound{notice, current});
    }

    void present(RemoteNotification notice) {
        std::string peer;
        try {
            if (deps.peer_name) {
                peer = deps.peer_name();
            }
        } catch (...) {
            peer.clear();
        }
        if (peer.empty()) {
            report("not submitted: identity of equipment not available");
            return;
        }

        const std::shared_ptr<Lifetime> token = lifetime;
        const SessionGeneration presented_generation = generation;
        const NotificationRef reference = notice.ref;
        const NotificationMode mode = notice.mode;
        auto complete = [token, presented_generation, reference, mode](DesktopResult result) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onPresented(presented_generation, reference, mode, result);
        };

        try {
            deps.desktop.presentNotice(peer, notice, std::move(complete));
        } catch (...) {
            report("not submitted notice: desktop error");
        }
    }

    void onPresented(SessionGeneration presented_generation, NotificationRef reference,
                     NotificationMode mode, DesktopResult result) {
        (void)reference;
        if (presented_generation != generation) {
            return; // Withdrawal or new generation: the result no longer applies.
        }
        if (!canReceive(mode)) {
            return;
        }
        if (result == DesktopResult::ready) {
            return;
        }
        report(presentationFailureText(result));
    }

    void flushInbound(MonotonicTime current) {
        for (auto iterator = inbound.begin(); iterator != inbound.end();) {
            const bool expired = current - iterator->queued_at >= queue_lifetime;
            if (expired) {
                report("notice received discarded: tail expired");
                iterator = inbound.erase(iterator);
            } else if (!canReceive(iterator->notice.mode)) {
                iterator = inbound.erase(iterator);
            } else {
                ++iterator;
            }
        }

        while (!inbound.empty()) {
            Inbound next = std::move(inbound.front());
            inbound.pop_front();
            if (current - next.queued_at >= queue_lifetime) {
                report("notice received discarded: tail expired");
                continue;
            }
            if (!canReceive(next.notice.mode)) {
                continue;
            }
            present(std::move(next.notice));
        }
    }

    void retryOutbound(MonotonicTime current) {
        while (!outbound.empty()) {
            Outbound next = std::move(outbound.front());
            outbound.pop_front();

            if (current - next.queued_at >= queue_lifetime) {
                report("Dismissed notice: local tail expired");
                continue;
            }
            if (!canSend(next.notice.mode)) {
                continue;
            }
            if (next.notice.mode == NotificationMode::forwarded) {
                if (next.source_app_id.empty() ||
                    is_product_application(next.source_app_id) ||
                    allowed_applications.find(next.source_app_id) ==
                        allowed_applications.end()) {
                    continue;
                }
            }

            const SendResult result = deliver(next.notice);
            if (result == SendResult::queued) {
                continue;
            }
            if (result == SendResult::busy) {
                outbound.push_front(std::move(next));
                break;
            }
            outbound.clear();
            report(result == SendResult::denied ? "Discarded notice: session denied shipment"
                                               : "Discarded notice: offline session");
            break;
        }
    }

    void startObservation() {
        if (observing) {
            return;
        }
        observing = true;
        observation_subscription = ++next_observation_subscription;
        const std::uint64_t subscription = observation_subscription;
        const std::shared_ptr<Lifetime> token = lifetime;

        auto observed = [token, subscription](ObservedNotification notice) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onObserved(subscription, std::move(notice));
        };
        auto state_changed = [token, subscription](DesktopResult result) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onObservationState(subscription, result);
        };

        try {
            deps.desktop.startObserving(std::move(observed), std::move(state_changed));
        } catch (...) {
            observing = false;
            observation_subscription = 0;
            report("observation of notifications not available");
        }
    }

    void stopObservation() {
        if (!observing) {
            return;
        }
        // Subscription is disabled before disconnecting the backend: any
        // already glued event of this subscription is discarded.
        observing = false;
        observation_subscription = 0;
        try {
            deps.desktop.stopObserving();
        } catch (...) {
        }
    }

    bool observationWanted() const {
        if (!send_features.forwarded_notifications || allowed_applications.empty()) {
            return false;
        }
        try {
            return deps.desktop.notificationCapabilities().observe;
        } catch (...) {
            return false;
        }
    }

    void syncObservation() {
        if (observationWanted()) {
            startObservation();
        } else {
            stopObservation();
        }
    }

    void onObserved(std::uint64_t subscription, ObservedNotification notice) {
        if (!observing || subscription != observation_subscription) {
            return; // Previous subscription already invalidated.
        }
        forwardObserved(generation, notice);
    }

    void onObservationState(std::uint64_t subscription, DesktopResult result) {
        if (!observing || subscription != observation_subscription) {
            return;
        }
        if (result == DesktopResult::ready) {
            return;
        }
        observing = false;
        observation_subscription = 0;
        try {
            deps.desktop.stopObserving();
        } catch (...) {
        }
        switch (result) {
        case DesktopResult::denied:
            report("observation of notifications denied by desktop");
            break;
        case DesktopResult::unavailable:
            report("observation of notifications not available");
            break;
        case DesktopResult::cancelled:
            report("observation of notifications cancelled");
            break;
        default:
            report("observation of notifications interrupted");
            break;
        }
    }

    void forwardObserved(SessionGeneration requested_generation,
                         const ObservedNotification& notice) {
        if (requested_generation != generation) {
            return;
        }
        if (!send_features.forwarded_notifications) {
            return; // The permit is withdrawn; it is not registered as seen.
        }
        if (!observedAllowed(notice)) {
            return; // Without reliable identity, excluded application or the product itself.
        }
        if (notice.app_name.empty() || !is_valid_utf8(notice.app_name) ||
            notice.app_name.size() > max_notification_app_name_bytes) {
            return; // Without a visible name there is no usable attribution.
        }
        const ContentProblem problem = checkContent(notice.content);
        if (problem != ContentProblem::none) {
            report(contentProblemText(NotificationMode::forwarded, problem));
            return;
        }

        const MonotonicTime current = now();
        if (seenRecently(notice, current)) {
            return;
        }
        rememberObserved(notice, current);
        emit(generation, NotificationMode::forwarded, notice.content, notice.app_name,
             notice.app_id);
    }

    void sendManual(SessionGeneration requested_generation, const NotificationContent& content) {
        if (requested_generation != generation) {
            return;
        }
        if (!send_features.manual_notifications) {
            report("Discarded manual notice: shipping permit not granted");
            return;
        }
        const ContentProblem problem = checkContent(content);
        if (problem != ContentProblem::none) {
            report(contentProblemText(NotificationMode::manual, problem));
            return;
        }
        emit(generation, NotificationMode::manual, content, {}, {});
    }

    void setAllowedApplications(std::set<std::string> app_ids) {
        allowed_applications = std::move(app_ids);
        // An application removed from the list should not consume the shipping queue.
        for (auto iterator = outbound.begin(); iterator != outbound.end();) {
            const bool forwarded = iterator->notice.mode == NotificationMode::forwarded;
            if (forwarded &&
                (iterator->source_app_id.empty() ||
                 allowed_applications.find(iterator->source_app_id) ==
                     allowed_applications.end())) {
                iterator = outbound.erase(iterator);
            } else {
                ++iterator;
            }
        }
        syncObservation();
    }

    void setAuthorization(SessionGeneration requested_generation, FeatureSet send,
                          FeatureSet receive) {
        const bool generation_changed = requested_generation != generation;
        if (generation_changed) {
            stopObservation();
            withdrawActive();
            outbound.clear();
            inbound.clear();
            send_stamps.clear();
            receive_stamps.clear();
            observed_fingerprints.clear();
            generation = requested_generation;
            next_send_id = 1;
            last_received_id = 0;
            send_counter_exhausted = false;
        } else {
            const FeatureSet previous_receive = receive_features;
            const bool receive_revoked =
                (previous_receive.manual_notifications && !receive.manual_notifications) ||
                (previous_receive.forwarded_notifications && !receive.forwarded_notifications);
            if (receive_revoked) {
                withdrawActive();
            }
            for (auto iterator = inbound.begin(); iterator != inbound.end();) {
                if (!modePermitted(receive, iterator->notice.mode)) {
                    iterator = inbound.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            for (auto iterator = outbound.begin(); iterator != outbound.end();) {
                if (!modePermitted(send, iterator->notice.mode)) {
                    iterator = outbound.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }

        send_features = send;
        receive_features = receive;
        syncObservation();
    }

    void tick() {
        const MonotonicTime current = now();
        retryOutbound(current);
        flushInbound(current);
    }

    void discoverApplications(
        std::function<void(DesktopResult, std::vector<NotificationApplication>)> complete) {
        if (!complete) {
            return;
        }
        bool supported = false;
        try {
            supported = deps.desktop.notificationCapabilities().observe;
        } catch (...) {
            supported = false;
        }
        if (!supported) {
            complete(DesktopResult::unavailable, {});
            return;
        }

        const std::shared_ptr<Lifetime> token = lifetime;
        auto finish = [token, complete = std::move(complete)](
                          DesktopResult result,
                          std::vector<NotificationApplication> applications) mutable {
            if (!token->alive.load(std::memory_order_acquire)) {
                return; // The module was destroyed while being consulted.
            }
            if (result != DesktopResult::ready) {
                complete(result, {});
                return;
            }

            // The list is a configuration surface: they are discarded
            // Unusable and duplicated identities before offering.
            std::set<std::string> seen;
            std::vector<NotificationApplication> accepted;
            for (auto& application : applications) {
                if (application.app_id.empty() ||
                    is_product_application(application.app_id) ||
                    !seen.insert(application.app_id).second) {
                    continue;
                }
                if (application.app_name.empty() ||
                    !is_valid_utf8(application.app_name) ||
                    application.app_name.size() > max_notification_app_name_bytes) {
                    continue;
                }
                accepted.push_back(std::move(application));
            }
            std::sort(accepted.begin(), accepted.end(),
                      [](const NotificationApplication& left,
                         const NotificationApplication& right) {
                          if (left.app_name != right.app_name) {
                              return left.app_name < right.app_name;
                          }
                          return left.app_id < right.app_id;
                      });
            complete(result, std::move(accepted));
        };

        try {
            deps.desktop.discoverApplications(finish);
        } catch (...) {
            finish(DesktopResult::error, {});
        }
    }

    void shutdown() {
        lifetime->alive.store(false, std::memory_order_release);
        stopObservation();
        withdrawActive();
        outbound.clear();
        inbound.clear();
    }
};

SharedNotifications::SharedNotifications(NotificationDependencies dependencies)
    : impl_(std::make_unique<Impl>(std::move(dependencies))) {}

SharedNotifications::~SharedNotifications() {
    impl_->shutdown();
}

void SharedNotifications::tick() {
    impl_->tick();
}

void SharedNotifications::discoverApplications(
    std::function<void(DesktopResult, std::vector<NotificationApplication>)> complete) {
    impl_->discoverApplications(std::move(complete));
}

void SharedNotifications::setAllowedApplications(std::set<std::string> app_ids) {
    impl_->setAllowedApplications(std::move(app_ids));
}

void SharedNotifications::setAuthorization(SessionGeneration generation, FeatureSet send,
                                           FeatureSet receive) {
    impl_->setAuthorization(generation, send, receive);
}

void SharedNotifications::sendManual(SessionGeneration generation,
                                     const NotificationContent& content) {
    impl_->sendManual(generation, content);
}

void SharedNotifications::forwardObserved(SessionGeneration generation,
                                          const ObservedNotification& notice) {
    impl_->forwardObserved(generation, notice);
}

void SharedNotifications::receiveNotice(const RemoteNotification& notice) {
    impl_->receiveNotice(notice);
}

} // namespace oneintwo
