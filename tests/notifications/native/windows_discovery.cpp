// Mismo ajuste que el backend: C++/WinRT incluye <experimental/coroutine>
// cuando se compila en C++17 con el SDK 26100.
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
 * Comprobación nativa sin empaquetar. El proceso no tiene identidad MSIX, así
 * que el adaptador debe rechazar el descubrimiento antes de tocar el listener:
 * discoverApplications completa con unavailable y nunca inicia la observación.
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
        std::fprintf(stderr, "fallo: makeWindowsNotifications devolvió null\n");
        return 1;
    }

    const NotificationCapabilities capabilities = desktop->notificationCapabilities();
    if (capabilities.observe) {
        std::printf(
            "omitido: el proceso tiene identidad de paquete; esta prueba exige "
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
                // Una segunda llamada no cambia el resultado esperado.
            }
        });

    if (future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
        std::fprintf(stderr, "fallo: discoverApplications no completó\n");
        return 1;
    }

    auto [result, applications] = future.get();
    if (result != DesktopResult::unavailable) {
        std::fprintf(stderr, "fallo: descubrimiento sin paquete devolvió %d\n",
                     static_cast<int>(result));
        return 1;
    }
    if (!applications.empty()) {
        std::fprintf(stderr, "fallo: descubrimiento sin paquete devolvió %zu aplicaciones\n",
                     applications.size());
        return 1;
    }

    std::printf("descubrimiento sin identidad de paquete: unavailable (0 aplicaciones)\n");
    return 0;
}
