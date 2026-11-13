// C++/WinRT incluye <experimental/coroutine> al compilar en C++17. El SDK
// 26100 con MSVC 14.51 convierte esa inclusión en un error salvo que se
// silencie explícitamente; el paquete no usa corrutinas propias.
#if defined(_MSC_VER) && !defined(_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS)
#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
#endif

#include "features/notification_support/identity.hpp"
#include "integration/feature_contracts.hpp"
#include "platform/desktop.hpp"
#include "platform/windows/notification_support/observation_baseline.hpp"
#include "platform/windows/notification_support/toast_text.hpp"

#include <windows.h>

#include <appmodel.h>
#include <shellapi.h>
#include <shobjidl_core.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/base.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/*
 * Presentación y observación nativas en Windows.
 *
 * La presentación usa ToastNotificationManager. En una aplicación empaquetada
 * (MSIX) la identidad procede del paquete; en una compilación sin empaquetar se
 * usa el AppUserModelID del producto, que debe estar registrado por un acceso
 * directo del menú Inicio (ver notification_support/README.md).
 *
 * La observación usa UserNotificationListener y solo está disponible con
 * identidad de paquete y la capacidad userNotificationListener. RequestAccessAsync
 * se invoca desde el hilo de interfaz del coordinador; la escucha, la lectura
 * inicial y la presentación se ejecutan en un hilo propio con apartamento MTA.
 * El consentimiento se vuelve a comprobar periódicamente con GetAccessStatus.
 */

namespace oneintwo {
namespace {

namespace wun = winrt::Windows::UI::Notifications;
namespace wunm = winrt::Windows::UI::Notifications::Management;

using windows_notifications::ObservationBaseline;
using windows_notifications::ToastText;

constexpr auto access_poll_interval = std::chrono::seconds{2};
constexpr std::size_t maximum_tracked_toasts = 32;

DesktopResult map_hresult(winrt::hresult code) {
    const HRESULT value = static_cast<HRESULT>(code.value);
    if (value == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) ||
        value == HRESULT_FROM_WIN32(APPMODEL_ERROR_NO_PACKAGE)) {
        return DesktopResult::unavailable;
    }
    if (value == E_ACCESSDENIED) {
        return DesktopResult::denied;
    }
    return DesktopResult::error;
}

DesktopResult map_access(wunm::UserNotificationListenerAccessStatus status) {
    switch (status) {
    case wunm::UserNotificationListenerAccessStatus::Allowed:
        return DesktopResult::ready;
    case wunm::UserNotificationListenerAccessStatus::Denied:
        return DesktopResult::denied;
    case wunm::UserNotificationListenerAccessStatus::Unspecified:
    default:
        return DesktopResult::denied;
    }
}

bool has_package_identity() {
    UINT32 length = 0;
    const LONG status = GetCurrentPackageFullName(&length, nullptr);
    return status != APPMODEL_ERROR_NO_PACKAGE;
}

struct Shared {
    ModuleRuntime runtime;
    bool packaged = false;

    std::atomic<bool> alive{true};
    std::atomic<std::uint64_t> subscription{0};

    std::mutex mutex;
    std::function<void(ObservedNotification)> observed_sink;
    std::function<void(DesktopResult)> state_sink;
    std::function<void()> capabilities_changed;
    ObservationBaseline baseline;

    std::mutex worker_mutex;
    std::condition_variable worker_wake;
    std::deque<Task> tasks;
    bool stopping = false;

    // Propiedad exclusiva del hilo de trabajo.
    wun::ToastNotifier notifier{nullptr};
    bool notifier_ready = false;
    std::map<std::uint64_t, wun::ToastNotification> tracked_toasts;
    std::uint64_t next_toast = 1;
    wunm::UserNotificationListener listener{nullptr};
    winrt::event_token listener_token{};
    bool listening = false;

    bool is_current(std::uint64_t identifier) const {
        return alive.load(std::memory_order_acquire) &&
               subscription.load(std::memory_order_acquire) == identifier;
    }

    void post(Task task) const {
        if (!runtime.post) {
            task();
            return;
        }
        try {
            runtime.post(std::move(task));
        } catch (...) {
        }
    }

    void enqueue(Task task) {
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            if (stopping) {
                return;
            }
            tasks.push_back(std::move(task));
        }
        worker_wake.notify_one();
    }
};

using SharedPtr = std::shared_ptr<Shared>;

std::function<void(DesktopResult)> state_sink(const SharedPtr& shared) {
    std::lock_guard<std::mutex> lock(shared->mutex);
    return shared->state_sink;
}

std::function<void(ObservedNotification)> observed_sink(const SharedPtr& shared) {
    std::lock_guard<std::mutex> lock(shared->mutex);
    return shared->observed_sink;
}

void post_state(const SharedPtr& shared, DesktopResult result) {
    const auto sink = state_sink(shared);
    if (!sink) {
        return;
    }
    shared->post([shared, sink, result] {
        if (shared->alive.load(std::memory_order_acquire)) {
            sink(result);
        }
    });
}

// El consentimiento forma parte de la disponibilidad efectiva: el coordinador
// vuelve a consultar capacidades y recalcula la autorización.
void notify_capabilities_changed(const SharedPtr& shared) {
    const auto callback = shared->capabilities_changed;
    if (!callback) {
        return;
    }
    shared->post([shared, callback] {
        if (!shared->alive.load(std::memory_order_acquire)) {
            return;
        }
        try {
            callback();
        } catch (...) {
        }
    });
}

void unsubscribe_worker(const SharedPtr& shared) {
    if (shared->listening && shared->listener) {
        try {
            shared->listener.NotificationChanged(shared->listener_token);
        } catch (...) {
        }
    }
    shared->listening = false;
    shared->listener = nullptr;
    shared->listener_token = {};
    std::lock_guard<std::mutex> lock(shared->mutex);
    shared->baseline.clear();
}

void ensure_notifier(const SharedPtr& shared) {
    if (shared->notifier_ready) {
        return;
    }
    if (shared->packaged) {
        shared->notifier = wun::ToastNotificationManager::CreateToastNotifier();
    } else {
        shared->notifier = wun::ToastNotificationManager::CreateToastNotifier(
            winrt::to_hstring(notification_support::product_application_id));
    }
    shared->notifier_ready = true;
}

void present_worker(const SharedPtr& shared, const std::string& peer_name,
                    const RemoteNotification& notice,
                    std::function<void(DesktopResult)> complete, std::uint64_t local_id) {
    if (!shared->alive.load(std::memory_order_acquire)) {
        return;
    }

    DesktopResult result = DesktopResult::error;
    try {
        ensure_notifier(shared);

        ToastText text;
        text.title = notice.content.title;
        text.app_name = notice.mode == NotificationMode::forwarded ? notice.app_name : std::string{};
        text.body = notice.content.body;
        text.peer_name = peer_name;

        const std::string xml = windows_notifications::build_toast_xml(text);
        winrt::Windows::Data::Xml::Dom::XmlDocument document;
        document.LoadXml(winrt::to_hstring(xml));
        wun::ToastNotification toast(document);
        shared->notifier.Show(toast);

        shared->tracked_toasts.emplace(local_id, toast);
        while (shared->tracked_toasts.size() > maximum_tracked_toasts) {
            shared->tracked_toasts.erase(shared->tracked_toasts.begin());
        }
        result = DesktopResult::ready;
    } catch (const winrt::hresult_error& error) {
        result = map_hresult(error.code());
    } catch (...) {
        result = DesktopResult::error;
    }

    if (complete) {
        shared->post([shared, complete = std::move(complete), result] {
            if (shared->alive.load(std::memory_order_acquire)) {
                complete(result);
            }
        });
    }
}

std::vector<std::string> notification_text_lines(const wun::UserNotification& notification) {
    std::vector<std::string> lines;
    try {
        const auto visual = notification.Notification().Visual();
        wun::NotificationBinding binding{nullptr};
        try {
            binding = visual.GetBinding(wun::KnownNotificationBindings::ToastGeneric());
        } catch (const winrt::hresult_error&) {
            // Plantillas antiguas sin enlace ToastGeneric.
        }
        if (!binding) {
            const auto bindings = visual.Bindings();
            if (bindings.Size() > 0) {
                binding = bindings.GetAt(0);
            }
        }
        if (!binding) {
            return lines;
        }
        for (const auto& element : binding.GetTextElements()) {
            std::string text = winrt::to_string(element.Text());
            if (!text.empty()) {
                lines.push_back(std::move(text));
            }
        }
    } catch (...) {
        lines.clear();
    }
    return lines;
}

void handle_changed(const SharedPtr& shared, std::uint64_t identifier,
                    const wun::UserNotificationChangedEventArgs& args) {
    if (!shared->is_current(identifier)) {
        return;
    }
    if (args.ChangeKind() != wun::UserNotificationChangedKind::Added) {
        return;
    }

    try {
        const std::uint32_t native_id = args.UserNotificationId();
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            if (!shared->baseline.accept(native_id)) {
                return; // Existía antes de iniciar la observación o ya se entregó.
            }
        }

        // El evento solo identifica el aviso; puede haberse retirado ya.
        const auto notification =
            wunm::UserNotificationListener::Current().GetNotification(native_id);
        if (!notification) {
            return;
        }

        const std::string app_id = winrt::to_string(notification.AppInfo().AppUserModelId());
        if (app_id.empty() || notification_support::is_product_application(app_id)) {
            return; // Sin identidad fiable o eco de OneInTwo.
        }
        std::string app_name;
        try {
            app_name = winrt::to_string(notification.AppInfo().DisplayInfo().DisplayName());
        } catch (...) {
            app_name.clear();
        }
        if (app_name.empty()) {
            return; // Sin nombre visible no hay atribución utilizable.
        }

        const auto lines = notification_text_lines(notification);
        if (lines.empty()) {
            return;
        }

        ObservedNotification observed;
        observed.app_id = app_id;
        observed.app_name = std::move(app_name);
        observed.content.title = lines.front();
        for (std::size_t index = 1; index < lines.size(); ++index) {
            if (!observed.content.body.empty()) {
                observed.content.body.push_back('\n');
            }
            observed.content.body.append(lines[index]);
        }

        const auto sink = observed_sink(shared);
        if (!sink) {
            return;
        }
        shared->post([shared, identifier, sink, observed = std::move(observed)] {
            if (shared->is_current(identifier)) {
                sink(observed);
            }
        });
    } catch (...) {
        // Una notificación retirada entre el evento y la lectura se descarta.
    }
}

void begin_listening(const SharedPtr& shared, std::uint64_t identifier) {
    if (!shared->is_current(identifier)) {
        return;
    }
    try {
        auto listener = wunm::UserNotificationListener::Current();
        if (listener.GetAccessStatus() !=
            wunm::UserNotificationListenerAccessStatus::Allowed) {
            shared->subscription.store(0, std::memory_order_release);
            post_state(shared, DesktopResult::denied);
            notify_capabilities_changed(shared);
            return;
        }

        // La carga inicial forma la línea base: no se reenvía el historial.
        const auto existing = listener.GetNotificationsAsync(wun::NotificationKinds::Toast).get();
        std::set<std::uint32_t> identifiers;
        for (const auto& notification : existing) {
            identifiers.insert(notification.Id());
        }
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->baseline.clear();
            shared->baseline.seed(identifiers);
        }

        shared->listener = listener;
        shared->listening = true;
        shared->listener_token = listener.NotificationChanged(
            [shared, identifier](const wunm::UserNotificationListener&,
                                 const wun::UserNotificationChangedEventArgs& args) {
                handle_changed(shared, identifier, args);
            });

        if (!shared->is_current(identifier)) {
            unsubscribe_worker(shared);
            return;
        }
        post_state(shared, DesktopResult::ready);
    } catch (const winrt::hresult_error& error) {
        shared->subscription.store(0, std::memory_order_release);
        unsubscribe_worker(shared);
        post_state(shared, map_hresult(error.code()));
    } catch (...) {
        shared->subscription.store(0, std::memory_order_release);
        unsubscribe_worker(shared);
        post_state(shared, DesktopResult::error);
    }
}

void poll_access(const SharedPtr& shared) {
    if (!shared->listening || !shared->listener) {
        return;
    }
    const std::uint64_t identifier = shared->subscription.load(std::memory_order_acquire);
    if (identifier == 0) {
        return;
    }
    try {
        if (shared->listener.GetAccessStatus() ==
            wunm::UserNotificationListenerAccessStatus::Allowed) {
            return;
        }
    } catch (...) {
    }
    // El usuario retiró el consentimiento en Configuración: se detiene y se
    // informa al coordinador aunque no llegue ningún aviso nuevo.
    shared->subscription.store(0, std::memory_order_release);
    unsubscribe_worker(shared);
    post_state(shared, DesktopResult::denied);
    notify_capabilities_changed(shared);
}

// RequestAccessAsync debe invocarse desde el hilo de interfaz. La continuación
// se entrega ya en el hilo del coordinador.
void request_access(const SharedPtr& shared, std::function<void(DesktopResult)> continuation) {
    if (!shared->alive.load(std::memory_order_acquire)) {
        return;
    }
    if (!shared->packaged) {
        shared->post([continuation = std::move(continuation)] {
            if (continuation) {
                continuation(DesktopResult::unavailable);
            }
        });
        return;
    }
    try {
        auto listener = wunm::UserNotificationListener::Current();
        auto operation = listener.RequestAccessAsync();
        operation.Completed([shared, continuation = std::move(continuation)](
                                const auto& async,
                                winrt::Windows::Foundation::AsyncStatus status) mutable {
            DesktopResult result = DesktopResult::error;
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                try {
                    result = map_access(async.GetResults());
                } catch (const winrt::hresult_error& error) {
                    result = map_hresult(error.code());
                } catch (...) {
                    result = DesktopResult::error;
                }
            } else if (status == winrt::Windows::Foundation::AsyncStatus::Canceled) {
                result = DesktopResult::cancelled;
            }
            shared->post([continuation = std::move(continuation), result] {
                if (continuation) {
                    continuation(result);
                }
            });
        });
    } catch (const winrt::hresult_error& error) {
        const DesktopResult result = map_hresult(error.code());
        shared->post([continuation = std::move(continuation), result] {
            if (continuation) {
                continuation(result);
            }
        });
    } catch (...) {
        shared->post([continuation = std::move(continuation)] {
            if (continuation) {
                continuation(DesktopResult::error);
            }
        });
    }
}

class WindowsNotifications final : public DesktopNotifications {
public:
    explicit WindowsNotifications(PlatformContext context) : shared_(std::make_shared<Shared>()) {
        shared_->runtime = context.runtime;
        shared_->capabilities_changed = context.events.capabilities_changed;
        shared_->packaged = has_package_identity();

        if (!shared_->packaged) {
            // El mismo AUMID que el acceso directo del menú Inicio. No sustituye
            // al registro del acceso, pero mantiene coherente la identidad.
            try {
                const auto identifier = winrt::to_hstring(
                    notification_support::product_application_id);
                SetCurrentProcessExplicitAppUserModelID(identifier.c_str());
            } catch (...) {
            }
        }
        worker_ = std::thread([shared = shared_] { run(shared); });
    }

    ~WindowsNotifications() override {
        shared_->alive.store(false, std::memory_order_release);
        shared_->subscription.store(0, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(shared_->worker_mutex);
            shared_->stopping = true;
        }
        shared_->worker_wake.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        // La parada descarta tareas pendientes; la retirada final se hace aquí.
        if (shared_->notifier_ready) {
            for (const auto& entry : shared_->tracked_toasts) {
                try {
                    shared_->notifier.Hide(entry.second);
                } catch (...) {
                }
            }
        }
        shared_->tracked_toasts.clear();
    }

    NotificationCapabilities notificationCapabilities() const override {
        NotificationCapabilities capabilities;
        capabilities.present = true;
        capabilities.observe = shared_->packaged;
        return capabilities;
    }

    void requestObservationPermission(std::function<void(DesktopResult)> complete) override {
        const SharedPtr shared = shared_;
        shared->post([shared, complete = std::move(complete)]() mutable {
            if (!shared->alive.load(std::memory_order_acquire)) {
                return;
            }
            request_access(shared, [shared, complete = std::move(complete)](
                                       DesktopResult result) mutable {
                if (complete) {
                    complete(result);
                }
            });
        });
    }

    void startObserving(std::function<void(ObservedNotification)> observed,
                        std::function<void(DesktopResult)> state_changed) override {
        const SharedPtr shared = shared_;
        const std::uint64_t identifier = ++next_subscription_;
        shared->subscription.store(identifier, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->observed_sink = std::move(observed);
            shared->state_sink = std::move(state_changed);
        }

        shared->post([shared, identifier] {
            if (!shared->alive.load(std::memory_order_acquire)) {
                return;
            }
            request_access(shared, [shared, identifier](DesktopResult access) {
                if (!shared->is_current(identifier)) {
                    return;
                }
                if (access != DesktopResult::ready) {
                    shared->subscription.store(0, std::memory_order_release);
                    post_state(shared, access);
                    notify_capabilities_changed(shared);
                    return;
                }
                shared->enqueue([shared, identifier] {
                    begin_listening(shared, identifier);
                });
            });
        });
    }

    void stopObserving() override {
        shared_->subscription.store(0, std::memory_order_release);
        const SharedPtr shared = shared_;
        shared->enqueue([shared] { unsubscribe_worker(shared); });
    }

    void presentNotice(const std::string& peer_name, const RemoteNotification& notice,
                       std::function<void(DesktopResult)> complete) override {
        const SharedPtr shared = shared_;
        const std::uint64_t local_id = next_toast_++;
        shared->enqueue([shared, peer_name, notice, complete = std::move(complete), local_id] {
            present_worker(shared, peer_name, notice, complete, local_id);
        });
    }

    void withdrawNotices() override {
        const SharedPtr shared = shared_;
        shared->enqueue([shared] {
            if (!shared->notifier_ready) {
                shared->tracked_toasts.clear();
                return;
            }
            for (const auto& entry : shared->tracked_toasts) {
                try {
                    shared->notifier.Hide(entry.second);
                } catch (...) {
                }
            }
            shared->tracked_toasts.clear();
        });
    }

private:
    static void run(const SharedPtr& shared) {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        } catch (...) {
        }

        for (;;) {
            Task task;
            bool has_task = false;
            {
                std::unique_lock<std::mutex> lock(shared->worker_mutex);
                shared->worker_wake.wait_for(
                    lock, access_poll_interval,
                    [&shared] { return shared->stopping || !shared->tasks.empty(); });
                if (shared->stopping) {
                    break;
                }
                if (!shared->tasks.empty()) {
                    task = std::move(shared->tasks.front());
                    shared->tasks.pop_front();
                    has_task = true;
                }
            }
            if (has_task) {
                task();
            } else {
                poll_access(shared);
            }
        }

        unsubscribe_worker(shared);
        shared->tracked_toasts.clear();
        shared->notifier = nullptr;
        shared->notifier_ready = false;
        try {
            winrt::uninit_apartment();
        } catch (...) {
        }
    }

    SharedPtr shared_;
    std::thread worker_;
    std::uint64_t next_subscription_ = 0;
    std::uint64_t next_toast_ = 1;
};

} // namespace

std::unique_ptr<DesktopNotifications> makeWindowsNotifications(PlatformContext context) {
    return std::make_unique<WindowsNotifications>(std::move(context));
}

} // namespace oneintwo
