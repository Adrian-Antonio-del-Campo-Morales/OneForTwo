#pragma once

#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace oneintwo::tests {

// Reloj monotónico y ejecutor post controlados. Los callbacks de los dobles se
// invocan explícitamente para que cada caso decida el orden observable.
class ManualClock {
public:
    MonotonicTime now() const { return time_; }

    void advance(std::chrono::milliseconds delta) { time_ += delta; }
    void advance(std::chrono::seconds delta) { time_ += delta; }

    ModuleRuntime runtime() {
        return ModuleRuntime{
            [this] { return time_; },
            [this](Task task) { posted_.push_back(std::move(task)); }};
    }

    std::size_t posted_count() const { return posted_.size(); }

    void run_posted() {
        while (!posted_.empty()) {
            Task task = std::move(posted_.front());
            posted_.pop_front();
            task();
        }
    }

private:
    MonotonicTime time_{};
    std::deque<Task> posted_;
};

struct RecordingSend {
    std::deque<SendResult> script;
    SendResult default_result = SendResult::queued;
    std::vector<RemoteNotification> attempts;
    std::vector<RemoteNotification> accepted;

    SendFeatureMessage function() {
        return [this](FeatureMessage message) {
            const auto* notice = std::get_if<RemoteNotification>(&message);
            if (notice == nullptr) {
                return SendResult::denied;
            }
            attempts.push_back(*notice);
            SendResult result = default_result;
            if (!script.empty()) {
                result = script.front();
                script.pop_front();
            }
            if (result == SendResult::queued) {
                accepted.push_back(*notice);
            }
            return result;
        };
    }
};

struct PresentationRecord {
    std::string peer_name;
    RemoteNotification notice;
    std::function<void(DesktopResult)> complete;
};

struct ObservationStart {
    std::function<void(ObservedNotification)> observed;
    std::function<void(DesktopResult)> state_changed;
};

class FakeDesktopNotifications final : public DesktopNotifications {
public:
    NotificationCapabilities caps{true, true};
    DesktopResult observation_permission = DesktopResult::ready;
    int start_count = 0;
    int stop_count = 0;
    int withdraw_count = 0;
    int permission_requests = 0;
    std::vector<std::function<void(DesktopResult)>> permission_callbacks;
    std::vector<ObservationStart> starts;
    std::vector<PresentationRecord> presentations;

    NotificationCapabilities notificationCapabilities() const override { return caps; }

    void requestObservationPermission(std::function<void(DesktopResult)> complete) override {
        ++permission_requests;
        permission_callbacks.push_back(std::move(complete));
    }

    void startObserving(std::function<void(ObservedNotification)> observed,
                        std::function<void(DesktopResult)> state_changed) override {
        ++start_count;
        starts.push_back(ObservationStart{std::move(observed), std::move(state_changed)});
    }

    void stopObserving() override { ++stop_count; }

    void presentNotice(const std::string& peer_name, const RemoteNotification& notice,
                       std::function<void(DesktopResult)> complete) override {
        presentations.push_back(PresentationRecord{peer_name, notice, std::move(complete)});
    }

    void withdrawNotices() override { ++withdraw_count; }

    void emit_observed(std::size_t start_index, ObservedNotification notice) const {
        if (start_index < starts.size()) {
            starts[start_index].observed(std::move(notice));
        }
    }

    void emit_state(std::size_t start_index, DesktopResult result) const {
        if (start_index < starts.size()) {
            starts[start_index].state_changed(result);
        }
    }
};

struct Fixture {
    ManualClock clock;
    FakeDesktopNotifications desktop;
    RecordingSend sender;
    std::vector<std::string> errors;
    std::string peer_name = "PORTATIL-A";
    std::unique_ptr<SharedNotifications> module;
    SessionGeneration generation = 7;
    FeatureSet send_features{};
    FeatureSet receive_features{};

    Fixture() {
        send_features.manual_notifications = true;
        send_features.forwarded_notifications = true;
        receive_features = send_features;
        NotificationDependencies dependencies{
            clock.runtime(), desktop, sender.function(),
            [this] { return peer_name; },
            [this](std::string message) { errors.push_back(std::move(message)); }};
        module = std::make_unique<SharedNotifications>(std::move(dependencies));
        module->setAuthorization(generation, send_features, receive_features);
        desktop.start_count = 0;
        desktop.stop_count = 0;
        desktop.withdraw_count = 0;
    }

    ~Fixture() { module.reset(); }

    void tick() { module->tick(); }

    void send_manual(std::string title, std::string body = {}) {
        module->sendManual(generation, NotificationContent{std::move(title), std::move(body)});
    }

    RemoteNotification notice(std::uint64_t identifier, NotificationMode mode,
                              std::string title, std::string body = {},
                              std::string app_name = {}) const {
        return RemoteNotification{NotificationRef{generation, identifier}, mode,
                                  NotificationContent{std::move(title), std::move(body)},
                                  std::move(app_name)};
    }

    void receive(const RemoteNotification& value) { module->receiveNotice(value); }

    void authorize() { module->setAuthorization(generation, send_features, receive_features); }
};

} // namespace oneintwo::tests
