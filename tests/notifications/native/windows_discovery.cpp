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
#include <utility>
#include <vector>

/*
 * Native check without packaging. The process has no MSIX identity, so the adapter must refuse
 * discovery before touching the listener: discoverApplications complete with unavailable and never
 * starts observation.
 */

namespace {

struct DiscoveryState {
    std::promise<std::pair<oneintwo::DesktopResult,
                             std::vector<oneintwo::NotificationApplication>>>
        result;
};

} // namespace

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
    if (capabilities.observe) {
        std::printf(
            "omitted: the process has package identity; this test requires "
            "ejecutarse sin MSIX\n");
        return 77;
    }

    const auto state = std::make_shared<DiscoveryState>();
    auto future = state->result.get_future();
    desktop->discoverApplications(
        [state](DesktopResult result, std::vector<NotificationApplication> applications) {
            try {
                state->result.set_value(
                    std::make_pair(result, std::move(applications)));
            } catch (...) {
                // A second call does not change the expected result.
            }
        });

    if (future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
        std::fprintf(stderr, "failure: discoverApplications did not complete\n");
        return 1;
    }

    auto [result, applications] = future.get();
    if (result != DesktopResult::unavailable) {
        std::fprintf(stderr, "failure: discovery without package returned %d\n",
                     static_cast<int>(result));
        return 1;
    }
    if (!applications.empty()) {
        std::fprintf(stderr, "failure: discovery without package returned %zu applications\n",
                     applications.size());
        return 1;
    }

    std::printf("discovery without package identity: unavailable (0 applications)\n");
    return 0;
}
