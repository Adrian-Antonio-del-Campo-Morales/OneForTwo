#include "platform/linux/notification_support/linux_notifications.hpp"

#include "features/notification_support/identity.hpp"
#include "features/notification_support/text_validation.hpp"
#include "integration/feature_contracts.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

/*
 * Presentación nativa en Ubuntu mediante org.freedesktop.Notifications.
 *
 * La especificación estándar permite publicar y retirar avisos propios, pero no
 * leer los de otras aplicaciones. Sin un proveedor de observación autorizado,
 * notificationCapabilities().observe es false y startObserving informa
 * unavailable; el envío manual y la recepción autorizada siguen disponibles.
 * Las llamadas al bus se ejecutan en un hilo propio y los resultados vuelven
 * por ModuleRuntime.post.
 */

namespace oneintwo::linux_notifications {
namespace {

using notification_support::sanitize_plain_text;

constexpr std::size_t maximum_tracked_notices = 32;

class LinuxNotificationAdapter final : public DesktopNotifications {
public:
    LinuxNotificationAdapter(PlatformContext context, std::unique_ptr<Service> service)
        : context_(std::move(context)),
          service_(std::move(service)),
          lifetime_(std::make_shared<Lifetime>()) {
        if (service_) {
            worker_ = std::thread([this] { run(); });
        }
    }

    ~LinuxNotificationAdapter() override {
        lifetime_->alive.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        // La retirada final no puede depender de una tarea ya descartada por la
        // parada: se ejecuta aquí, sin concurrencia, antes de liberar el servicio.
        if (service_ != nullptr) {
            for (const std::uint32_t identifier : tracked_) {
                service_->withdraw(identifier);
            }
        }
        tracked_.clear();
    }

    NotificationCapabilities notificationCapabilities() const override {
        NotificationCapabilities capabilities;
        capabilities.present = service_ != nullptr && service_->available();
        capabilities.observe = false;
        return capabilities;
    }

    void requestObservationPermission(std::function<void(DesktopResult)> complete) override {
        deliver(std::move(complete), DesktopResult::unavailable);
    }

    void startObserving(std::function<void(ObservedNotification)> observed,
                        std::function<void(DesktopResult)> state_changed) override {
        (void)observed;
        // No hay proveedor autorizado de lectura; se anuncia una sola vez.
        deliver(std::move(state_changed), DesktopResult::unavailable);
    }

    void stopObserving() override {}

    void presentNotice(const std::string& peer_name, const RemoteNotification& notice,
                       std::function<void(DesktopResult)> complete) override {
        if (service_ == nullptr || !service_->available()) {
            deliver(std::move(complete), DesktopResult::unavailable);
            return;
        }

        const std::shared_ptr<Lifetime> token = lifetime_;
        enqueue([this, token, peer_name, notice, complete = std::move(complete)]() mutable {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            const auto presentation = buildPresentation(peer_name, notice);
            const auto result = service_->present(presentation);
            if (result.accepted && result.notification_id != 0) {
                remember(result.notification_id);
            }
            const DesktopResult mapped = result.accepted
                                             ? DesktopResult::ready
                                             : (result.unavailable ? DesktopResult::unavailable
                                                                   : DesktopResult::error);
            deliver(std::move(complete), mapped);
        });
    }

    void withdrawNotices() override {
        const std::shared_ptr<Lifetime> token = lifetime_;
        enqueue([this, token] {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            for (const std::uint32_t identifier : tracked_) {
                service_->withdraw(identifier);
            }
            tracked_.clear();
        });
    }

private:
    struct Lifetime {
        std::atomic<bool> alive{true};
    };

    void run() {
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
    }

    void enqueue(Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            tasks_.push_back(std::move(task));
        }
        wake_.notify_one();
    }

    void post(Task task) const {
        if (!context_.runtime.post) {
            return;
        }
        try {
            context_.runtime.post(std::move(task));
        } catch (...) {
        }
    }

    void deliver(std::function<void(DesktopResult)> complete, DesktopResult result) const {
        if (!complete) {
            return;
        }
        const std::shared_ptr<Lifetime> token = lifetime_;
        post([token, complete = std::move(complete), result] {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            complete(result);
        });
    }

    // Solo se ejecuta en el hilo de trabajo.
    void remember(std::uint32_t identifier) {
        tracked_.push_back(identifier);
        while (tracked_.size() > maximum_tracked_notices) {
            tracked_.pop_front();
        }
    }

    static Presentation buildPresentation(const std::string& peer_name,
                                          const RemoteNotification& notice) {
        Presentation presentation;
        presentation.application_name = notification_support::product_display_name;
        presentation.title = sanitize_plain_text(notice.content.title);
        presentation.replaces_id = 0;
        presentation.timeout_ms = -1;

        std::string body;
        if (notice.mode == NotificationMode::forwarded && !notice.app_name.empty()) {
            // Atribución de la aplicación de origen como texto plano.
            body.append(sanitize_plain_text(notice.app_name));
            body.push_back('\n');
        }
        body.append(sanitize_plain_text(notice.content.body));
        if (!peer_name.empty()) {
            if (!body.empty() && body.back() != '\n') {
                body.push_back('\n');
            }
            body.append("De ");
            body.append(sanitize_plain_text(peer_name));
        }
        presentation.body = std::move(body);
        return presentation;
    }

    PlatformContext context_;
    std::unique_ptr<Service> service_;
    std::shared_ptr<Lifetime> lifetime_;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Task> tasks_;
    bool stopping_ = false;
    std::thread worker_;

    // Propiedad exclusiva del hilo de trabajo.
    std::deque<std::uint32_t> tracked_;
};

} // namespace

std::unique_ptr<DesktopNotifications> makeAdapter(PlatformContext context,
                                                  std::unique_ptr<Service> service) {
    return std::make_unique<LinuxNotificationAdapter>(std::move(context), std::move(service));
}

} // namespace oneintwo::linux_notifications
