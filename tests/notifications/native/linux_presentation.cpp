#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"

#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

/*
 * Recorrido nativo de presentación en Ubuntu. Publica un aviso manual por
 * org.freedesktop.Notifications, espera el resultado del escritorio y comprueba
 * que la observación automática se anuncia como no disponible. Devuelve 77
 * cuando no hay bus de sesión o servicio, para que CTest lo registre como
 * omitido en lugar de aprobado.
 */

int main() {
    using namespace oneintwo;

    ModuleRuntime runtime{
        [] { return std::chrono::steady_clock::now(); },
        [](Task task) { task(); }};
    PlatformContext context{runtime, nullptr, DesktopEvents{}};

    auto desktop = makeLinuxNotifications(context);
    if (!desktop) {
        std::fprintf(stderr, "fallo: makeLinuxNotifications devolvió null\n");
        return 1;
    }

    const NotificationCapabilities capabilities = desktop->notificationCapabilities();
    std::printf("capacidades: present=%d observe=%d\n", capabilities.present ? 1 : 0,
                capabilities.observe ? 1 : 0);
    if (!capabilities.present) {
        std::printf("omitido: no hay bus de sesión configurado\n");
        return 77;
    }
    if (capabilities.observe) {
        std::fprintf(stderr, "fallo: Linux no debe anunciar observación automática\n");
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
        std::fprintf(stderr, "fallo: startObserving no informó estado\n");
        return 1;
    }
    const DesktopResult observation_result = observation_future.get();
    std::printf("observación: %s\n",
                observation_result == DesktopResult::unavailable ? "no disponible" : "inesperada");
    if (observation_result != DesktopResult::unavailable) {
        return 1;
    }

    RemoteNotification notice;
    notice.ref = NotificationRef{1, 1};
    notice.mode = NotificationMode::manual;
    notice.content.title = "OneInTwo · aviso de prueba";
    notice.content.body = "Texto plano con Unicode: ñandú, 日本語, 😀";

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    desktop->presentNotice("EQUIPO-DE-PRUEBA", notice,
                           [&presented](DesktopResult result) {
                               presented.set_value(result);
                           });
    if (presented_future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        std::fprintf(stderr, "fallo: presentNotice no completó\n");
        return 1;
    }
    const DesktopResult result = presented_future.get();
    switch (result) {
    case DesktopResult::ready:
        std::printf("presentación: aceptada por el servicio\n");
        // El aviso permanece visible el tiempo suficiente para la inspección.
        std::this_thread::sleep_for(std::chrono::seconds(3));
        desktop->withdrawNotices();
        return 0;
    case DesktopResult::unavailable:
        std::printf("omitido: el servicio de notificaciones no está disponible\n");
        return 77;
    default:
        std::fprintf(stderr, "fallo: presentación rechazada (%d)\n",
                     static_cast<int>(result));
        return 1;
    }
}
