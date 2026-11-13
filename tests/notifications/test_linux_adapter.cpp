#include "platform/linux/notification_support/linux_notifications.hpp"
#include "support/test_harness.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace oneintwo;
using namespace oneintwo::linux_notifications;
using namespace oneintwo::tests;

namespace {

class FakeNotificationService final : public Service {
public:
    bool available() const override { return available_value; }

    PresentationResult present(const Presentation& presentation) override {
        std::lock_guard<std::mutex> lock(mutex_);
        presentations_.push_back(presentation);
        PresentationResult result = next_result_;
        if (result.accepted && result.notification_id == 0) {
            result.notification_id = next_identifier_++;
        }
        wake_.notify_all();
        return result;
    }

    void withdraw(std::uint32_t notification_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        withdrawn_.push_back(notification_id);
        wake_.notify_all();
    }

    bool wait_for_presentations(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return wake_.wait_for(lock, std::chrono::seconds(5),
                              [&] { return presentations_.size() >= count; });
    }

    bool wait_for_withdrawals(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        return wake_.wait_for(lock, std::chrono::seconds(5),
                              [&] { return withdrawn_.size() >= count; });
    }

    std::vector<Presentation> presentations() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return presentations_;
    }

    std::vector<std::uint32_t> withdrawals() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return withdrawn_;
    }

    void set_next_result(PresentationResult result) {
        std::lock_guard<std::mutex> lock(mutex_);
        next_result_ = result;
    }

    bool available_value = true;
    PresentationResult next_result_{true, false, 0, {}};

private:
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<Presentation> presentations_;
    std::vector<std::uint32_t> withdrawn_;
    std::uint32_t next_identifier_ = 40;
};

ModuleRuntime direct_runtime() {
    return ModuleRuntime{
        [] { return std::chrono::steady_clock::now(); },
        [](Task task) { task(); }};
}

RemoteNotification remote(NotificationMode mode, std::string title,
                          std::string body = {}, std::string app_name = {}) {
    return RemoteNotification{NotificationRef{1, 1}, mode,
                              NotificationContent{std::move(title), std::move(body)},
                              std::move(app_name)};
}

} // namespace

TEST_CASE(linux_adapter_announces_presentation_and_no_observation) {
    auto service = std::make_unique<FakeNotificationService>();
    auto* fake = service.get();
    auto adapter = makeAdapter(PlatformContext{direct_runtime(), nullptr, DesktopEvents{}},
                               std::move(service));
    REQUIRE(adapter != nullptr);

    const NotificationCapabilities capabilities = adapter->notificationCapabilities();
    CHECK(capabilities.present);
    CHECK(!capabilities.observe);

    std::promise<DesktopResult> state;
    auto state_future = state.get_future();
    adapter->startObserving([](ObservedNotification) {},
                            [&state](DesktopResult result) { state.set_value(result); });
    REQUIRE(state_future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(state_future.get() == DesktopResult::unavailable);

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    adapter->presentNotice("PORTATIL-A", remote(NotificationMode::manual, "Reunión ☕", "Cuerpo"),
                           [&presented](DesktopResult result) { presented.set_value(result); });
    REQUIRE(presented_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(presented_future.get() == DesktopResult::ready);

    REQUIRE(fake->wait_for_presentations(1));
    const auto presentations = fake->presentations();
    REQUIRE(presentations.size() == 1);
    CHECK_EQUAL(presentations.front().application_name, std::string("OneInTwo"));
    CHECK_EQUAL(presentations.front().title, std::string("Reunión ☕"));
    CHECK(presentations.front().body.find("Cuerpo") != std::string::npos);
    CHECK(presentations.front().body.find("De PORTATIL-A") != std::string::npos);
    CHECK(presentations.front().replaces_id == 0);
    CHECK(presentations.front().timeout_ms == -1);

    adapter->withdrawNotices();
    CHECK(fake->wait_for_withdrawals(1));
    const auto withdrawals = fake->withdrawals();
    REQUIRE(withdrawals.size() == 1);
    CHECK(withdrawals.front() == 40);
}

TEST_CASE(linux_adapter_attributes_forwarded_application_as_plain_text) {
    auto service = std::make_unique<FakeNotificationService>();
    auto* fake = service.get();
    auto adapter = makeAdapter(PlatformContext{direct_runtime(), nullptr, DesktopEvents{}},
                               std::move(service));

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    adapter->presentNotice(
        "PORTATIL-A",
        remote(NotificationMode::forwarded, "Mensaje", "Cuerpo", "Visor de Documentos"),
        [&presented](DesktopResult result) { presented.set_value(result); });
    REQUIRE(presented_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(presented_future.get() == DesktopResult::ready);

    REQUIRE(fake->wait_for_presentations(1));
    const auto presentations = fake->presentations();
    REQUIRE(presentations.size() == 1);
    CHECK(presentations.front().body.find("Visor de Documentos\n") == 0);
    CHECK(presentations.front().body.find("Cuerpo") != std::string::npos);
    CHECK(presentations.front().body.find("De PORTATIL-A") != std::string::npos);
}

TEST_CASE(linux_adapter_reports_service_availability) {
    auto service = std::make_unique<FakeNotificationService>();
    service->available_value = false;
    auto* fake = service.get();
    auto adapter = makeAdapter(PlatformContext{direct_runtime(), nullptr, DesktopEvents{}},
                               std::move(service));
    CHECK(!adapter->notificationCapabilities().present);

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    adapter->presentNotice("PORTATIL-A", remote(NotificationMode::manual, "Título", "Cuerpo"),
                           [&presented](DesktopResult result) { presented.set_value(result); });
    REQUIRE(presented_future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(presented_future.get() == DesktopResult::unavailable);
    CHECK(fake->presentations().empty());

    std::promise<DesktopResult> permission;
    auto permission_future = permission.get_future();
    adapter->requestObservationPermission(
        [&permission](DesktopResult result) { permission.set_value(result); });
    REQUIRE(permission_future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(permission_future.get() == DesktopResult::unavailable);
}

TEST_CASE(linux_adapter_maps_service_failures) {
    auto service = std::make_unique<FakeNotificationService>();
    service->set_next_result(PresentationResult{false, true, 0, "sin servicio"});
    auto* fake = service.get();
    auto adapter = makeAdapter(PlatformContext{direct_runtime(), nullptr, DesktopEvents{}},
                               std::move(service));

    std::promise<DesktopResult> unavailable;
    auto unavailable_future = unavailable.get_future();
    adapter->presentNotice("PORTATIL-A", remote(NotificationMode::manual, "Título"),
                           [&unavailable](DesktopResult result) {
                               unavailable.set_value(result);
                           });
    REQUIRE(unavailable_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(unavailable_future.get() == DesktopResult::unavailable);

    fake->set_next_result(PresentationResult{false, false, 0, "error del servicio"});
    std::promise<DesktopResult> failed;
    auto failed_future = failed.get_future();
    adapter->presentNotice("PORTATIL-A", remote(NotificationMode::manual, "Título"),
                           [&failed](DesktopResult result) { failed.set_value(result); });
    REQUIRE(failed_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(failed_future.get() == DesktopResult::error);
}

TEST_CASE(linux_adapter_sanitises_presentation_controls) {
    auto service = std::make_unique<FakeNotificationService>();
    auto* fake = service.get();
    auto adapter = makeAdapter(PlatformContext{direct_runtime(), nullptr, DesktopEvents{}},
                               std::move(service));

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    adapter->presentNotice(
        "PORTATIL-A",
        remote(NotificationMode::manual, std::string("Ti\x01tulo"), "Cuerpo"),
        [&presented](DesktopResult result) { presented.set_value(result); });
    REQUIRE(presented_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    CHECK(presented_future.get() == DesktopResult::ready);

    REQUIRE(fake->wait_for_presentations(1));
    CHECK_EQUAL(fake->presentations().front().title, std::string("Ti tulo"));
}
