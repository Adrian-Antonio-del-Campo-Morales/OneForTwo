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
#include <thread>

/*
 * Recorrido nativo de Windows. Debe ejecutarse sobre una compilación empaquetada
 * (MSIX) con la capacidad userNotificationListener, o sobre una compilación sin
 * empaquetar cuyo AppUserModelID esté registrado por el acceso directo del menú
 * Inicio (notification_support/register-unpackaged-toast.ps1).
 *
 * Devuelve 77 cuando la presentación no está disponible: sin identidad
 * registrada Windows puede descartar el aviso sin lanzar una excepción, por eso
 * el recorrido debe confirmarse visualmente además de por su código de salida.
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
        std::fprintf(stderr, "fallo: makeWindowsNotifications devolvió null\n");
        return 1;
    }

    const NotificationCapabilities capabilities = desktop->notificationCapabilities();
    std::printf("capacidades: present=%d observe=%d\n", capabilities.present ? 1 : 0,
                capabilities.observe ? 1 : 0);
    if (!capabilities.observe) {
        std::printf(
            "observación: no disponible (sin identidad de paquete; ver "
            "notification_support/README.md)\n");
    }

    RemoteNotification notice;
    notice.ref = NotificationRef{1, 1};
    notice.mode = NotificationMode::manual;
    notice.content.title = "OneInTwo · aviso de prueba";
    notice.content.body = "Texto plano con Unicode: ñandú, 日本語, 😀";

    std::promise<DesktopResult> presented;
    auto presented_future = presented.get_future();
    desktop->presentNotice("EQUIPO-DE-PRUEBA", notice,
                           [&presented](DesktopResult result) { presented.set_value(result); });
    if (presented_future.wait_for(std::chrono::seconds(15)) != std::future_status::ready) {
        std::fprintf(stderr, "fallo: presentNotice no completó\n");
        return 1;
    }
    const DesktopResult result = presented_future.get();
    if (result == DesktopResult::unavailable) {
        std::printf("omitido: sin identidad de notificaciones registrada\n");
        return 77;
    }
    if (result != DesktopResult::ready) {
        std::fprintf(stderr, "fallo: presentación rechazada (%d)\n",
                     static_cast<int>(result));
        return 1;
    }
    std::printf("presentación: solicitud aceptada; confirmar visualmente la atribución\n");

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
            std::printf("observación: %s\n",
                        state == DesktopResult::ready ? "iniciada" : "no autorizada");
            if (state == DesktopResult::ready) {
                std::printf("esperando un aviso nuevo de una aplicación no excluida...\n");
                if (notice_future.wait_for(std::chrono::seconds(20)) ==
                    std::future_status::ready) {
                    const ObservedNotification observed = notice_future.get();
                    std::printf("observado: app=%s nombre=%s título_bytes=%zu cuerpo_bytes=%zu\n",
                                observed.app_id.c_str(), observed.app_name.c_str(),
                                observed.content.title.size(), observed.content.body.size());
                } else {
                    std::printf("observado: sin eventos nuevos durante la espera\n");
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
