// Same fit as the backend: C++/WinRT includes <experimental/coroutine>
// when compiled in C++17 with SDK 26100.
#if defined(_MSC_VER) && !defined(_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS)
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#endif

#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"

#include <winrt/base.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

/*
 * A native Windows route. It must be run on a packaged compilation (MSIX) with the
 * userNotificationListener capability, or on an unpackaged compilation whose AppUserModelID is
 * registered by the direct access of the Start menu
 * (notification_support/register-unpackaged-toast.ps1).
 *
 * Also consult DiscoverApplications: No package identity must be completed with unavailable; with
 * MSIX requests real consent. Returns 77 when submission is not available: without Windows
 * registered identity you can discard the notice without releasing an exception, so the route must
 * be confirmed visually in addition to its output code.
 */

int main() {
    using namespace oneintwo;

    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
    } catch (...) {
    }

    ModuleRuntime runtime{
        [] { return std::chrono::steady_clock::now(); },
        [](Task task) { task(); }};
    PlatformContext context{runtime, nullptr, DesktopEvents{}};

    auto desktop = makeWindowsNotifications(context);
    if (!desktop) {
        std::fprintf(stderr, "failure: makeWindowsNotifications returned null\n");
        return 1;
    }

    const NotificationCapabilities capabilities = desktop->notificationCapabilities();
    std::printf("capacidades: present=%d observe=%d\n", capabilities.present ? 1 : 0,
                capabilities.observe ? 1 : 0);
    if (!capabilities.observe) {
        std::printf(
            "observation: not available (without package identity; see "
            "notification_support/README.md)\n");
    }

    // No complete package unavailable without opening dialog. With MSIX request the
    // the actual consent of the listener and, if granted, lists the
    // applications with current notices.
    {
        struct DiscoveryState {
            std::promise<std::pair<DesktopResult, std::vector<NotificationApplication>>> value;
        };
        const auto discovery = std::make_shared<DiscoveryState>();
        auto discovery_future = discovery->value.get_future();
        desktop->discoverApplications(
            [discovery](DesktopResult result,
                        std::vector<NotificationApplication> applications) {
                try {
                    discovery->value.set_value(
                        std::make_pair(result, std::move(applications)));
                } catch (...) {
                }
            });
        if (discovery_future.wait_for(std::chrono::seconds(30)) !=
            std::future_status::ready) {
            std::fprintf(stderr, "failure: discoverApplications did not complete\n");
            return 1;
        }
        auto [discovery_result, applications] = discovery_future.get();
        std::printf("discovery: result=%d applications=%zu\n",
                    static_cast<int>(discovery_result), applications.size());
        for (const auto& application : applications) {
            std::printf("  application: id=%s name=%s\n", application.app_id.c_str(),
                        application.app_name.c_str());
        }
    }

    RemoteNotification notice;
    notice.ref = NotificationRef{1, 1};
    notice.mode = NotificationMode::manual;
    notice.content.title = "OneInTwo · test notice";
    notice.content.body = "Plain Unicode text: café, 日本語, 😀";

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    desktop->presentNotice("TEST-DEVICE", notice,
                           [&presented](DesktopResult result) { presented.set_value(result); });
    if (presented_future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
        std::fprintf(stderr, "failure: presentNotice did not complete\n");
        return 1;
    }
    const DesktopResult result = presented_future.get();
    if (result == DesktopResult::unavailable) {
        std::printf("omitted: no identity of registered notifications\n");
        return 77;
    }
    if (result != DesktopResult::ready) {
        std::fprintf(stderr, "Judgment: rejected submission (%d)\n",
                     static_cast<int>(result));
        return 1;
    }
    std::printf("submission: request accepted; visually confirm attribution\n");

    if (capabilities.observe) {
        std::promise<DesktopResult> first_state;
        auto state_future = first_state.get_future();
        std::promise<ObservedNotification> first_notice;
        auto notice_future = first_notice.get_future();
        desktop->startObserving(
            [&first_notice](ObservedNotification observed) {
                try {
                    first_notice.set_value(std::move(observed));
                } catch (...) {
                }
            },
            [&first_state](DesktopResult state) {
                try {
                    first_state.set_value(state);
                } catch (...) {
                }
            });
        if (state_future.wait_for(std::chrono::seconds(15)) == std::future_status::ready) {
            const DesktopResult state = state_future.get();
            std::printf("observation: %s\n",
                        state == DesktopResult::ready ? "iniciada" : "no autorizada");
            if (state == DesktopResult::ready) {
                std::printf("waiting for a new notice of an application not excluded...\n");
                if (notice_future.wait_for(std::chrono::seconds(20)) ==
                    std::future_status::ready) {
                    const ObservedNotification observed = notice_future.get();
                    std::printf("observed: app=%s name=%s title_bytes=%zu body_bytes=%zu\n",
                                observed.app_id.c_str(), observed.app_name.c_str(),
                                observed.content.title.size(), observed.content.body.size());
                } else {
                    std::printf("observed: no new events during waiting\n");
                }
            }
        }
        desktop->stopObserving();
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    desktop->withdrawNotices();
    std::printf("retirada solicitada\n");
    return 0;
}
