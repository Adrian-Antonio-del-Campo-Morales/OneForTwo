#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"

#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

/*
 * Native presentation tour at Ubuntu. Post a manual notice by org.freedesktop.Notifications, wait
 * for the desktop result and check that automatic observation is advertised as unavailable.
 * Returns 77 when there is no session bus or service, so that CTest records it as omitted rather
 * than approved.
 */

int main() {
    using namespace oneintwo;

    ModuleRuntime runtime{
        [] { return std::chrono::steady_clock::now(); },
        [](Task task) { task(); }};
    PlatformContext context{runtime, nullptr, DesktopEvents{}};

    auto desktop = makeLinuxNotifications(context);
    if (!desktop) {
        std::fprintf(stderr, "failure: makeLinuxNotifications returned null\n");
        return 1;
    }

    const NotificationCapabilities capabilities = desktop->notificationCapabilities();
    std::printf("capacidades: present=%d observe=%d\n", capabilities.present ? 1 : 0,
                capabilities.observe ? 1 : 0);
    if (!capabilities.present) {
        std::printf("omitted: no session bus configured\n");
        return 77;
    }
    if (capabilities.observe) {
        std::fprintf(stderr, "failure: Linux must not announce automatic observation\n");
        return 1;
    }

    std::promise<DesktopResult> observation_state;
    auto observation_future = observation_state.get_future();
    desktop->startObserving(
        [](ObservedNotification) {},
        [&observation_state](DesktopResult result) {
            observation_state.set_value(result);
        });
    if (observation_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        std::fprintf(stderr, "failure: startObserving did not report status\n");
        return 1;
    }
    const DesktopResult observation_result = observation_future.get();
    std::printf("observation: %s\n",
                observation_result == DesktopResult::unavailable ? "unavailable" : "unexpected");
    if (observation_result != DesktopResult::unavailable) {
        return 1;
    }

    RemoteNotification notice;
    notice.ref = NotificationRef{1, 1};
    notice.mode = NotificationMode::manual;
    notice.content.title = "OneInTwo · test notice";
    notice.content.body = "Plain Unicode text: café, 日本語, 😀";

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    desktop->presentNotice("TEST-DEVICE", notice,
                           [&presented](DesktopResult result) {
                               presented.set_value(result);
                           });
    if (presented_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        std::fprintf(stderr, "failure: presentNotice did not complete\n");
        return 1;
    }
    const DesktopResult result = presented_future.get();
    switch (result) {
    case DesktopResult::ready:
        std::printf("presentation: accepted by the service\n");
        // The warning remains visible long enough for inspection.
        std::this_thread::sleep_for(std::chrono::seconds(3));
        desktop->withdrawNotices();
        return 0;
    case DesktopResult::unavailable:
        std::printf("omitted: notification service is not available\n");
        return 77;
    default:
        std::fprintf(stderr, "Judgment: rejected submission (%d)\n",
                     static_cast<int>(result));
        return 1;
    }
}
