#include "notifications.hpp"

#include "notification_support/identity.hpp"
#include "notification_support/policy.hpp"
#include "notification_support/text_validation.hpp"
#include "../integration/feature_contracts.hpp"
#include "../platform/desktop.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

/*
 * Avisos manuales y reenvío de notificaciones nuevas.
 *
 * Todas las entradas públicas se ejecutan en el hilo principal del coordinador.
 * Los adaptadores entregan sus callbacks por ModuleRuntime.post y el resultado
 * de presentNotice también vuelve por ese ejecutor. El módulo conserva un
 * contador de emisión por generación y sentido compartido, el último
 * identificador recibido, las colas acotadas con su instante de entrada y la
 * suscripción de observación vigente.
 */

namespace oneintwo {
namespace {

using notification_support::is_blank;
using notification_support::is_product_application;
using notification_support::is_valid_utf8;

constexpr std::size_t queue_capacity = notification_policy::queue_capacity;
constexpr auto queue_lifetime = notification_policy::queue_lifetime;

constexpr const char* manual_rejected = "aviso manual descartado: ";
constexpr const char* forwarded_rejected = "aviso reenviado descartado: ";

enum class ContentProblem {
    none,
    missing_title,
    malformed,
    too_long,
};

ContentProblem checkContent(const NotificationContent& content) {
    if (!is_valid_utf8(content.title) || !is_valid_utf8(content.body)) {
        return ContentProblem::malformed;
    }
    if (is_blank(content.title)) {
        return ContentProblem::missing_title;
    }
    if (content.title.size() > max_notification_title_bytes ||
        content.body.size() > max_notification_body_bytes) {
        return ContentProblem::too_long;
    }
    return ContentProblem::none;
}

const char* contentProblemDetail(ContentProblem problem) {
    switch (problem) {
    case ContentProblem::missing_title:
        return "el título es obligatorio";
    case ContentProblem::malformed:
        return "contenido mal formado";
    case ContentProblem::too_long:
        return "contenido fuera de límite";
    case ContentProblem::none:
        break;
    }
    return "contenido inválido";
}

std::string contentProblemText(NotificationMode mode, ContentProblem problem) {
    const char* prefix =
        mode == NotificationMode::forwarded ? forwarded_rejected : manual_rejected;
    return std::string(prefix) + contentProblemDetail(problem);
}

const char* presentationFailureText(DesktopResult result) {
    switch (result) {
    case DesktopResult::cancelled:
        return "aviso no presentado: cancelado por el escritorio";
    case DesktopResult::denied:
        return "aviso no presentado: permiso del escritorio denegado";
    case DesktopResult::unavailable:
        return "aviso no presentado: servicio de notificaciones no disponible";
    case DesktopResult::error:
        return "aviso no presentado: error del escritorio";
    case DesktopResult::ready:
        break;
    }
    return "aviso no presentado";
}

bool modePermitted(FeatureSet features, NotificationMode mode) {
    return mode == NotificationMode::manual ? features.manual_notifications
                                            : features.forwarded_notifications;
}

bool validMode(NotificationMode mode) {
    const auto raw = static_cast<unsigned>(mode);
    return raw == static_cast<unsigned>(NotificationMode::manual) ||
           raw == static_cast<unsigned>(NotificationMode::forwarded);
}

} // namespace

struct SharedNotifications::Impl {
    struct Lifetime {
        std::atomic<bool> alive{true};
        Impl* owner = nullptr;
    };

    struct Outbound {
        RemoteNotification notice;
        std::string source_app_id; // Vacío en los avisos manuales.
        MonotonicTime queued_at;
    };

    struct Inbound {
        RemoteNotification notice;
        MonotonicTime queued_at;
    };

    struct ObservedFingerprint {
        std::string app_id;
        std::string title;
        std::string body;
        MonotonicTime seen_at;
    };

    explicit Impl(NotificationDependencies dependencies)
        : deps(std::move(dependencies)), lifetime(std::make_shared<Lifetime>()) {
        lifetime->owner = this;
    }

    NotificationDependencies deps;
    std::shared_ptr<Lifetime> lifetime;

    SessionGeneration generation = 0;
    FeatureSet send_features{};
    FeatureSet receive_features{};

    std::set<std::string> allowed_applications;

    std::uint64_t next_send_id = 1;
    std::uint64_t last_received_id = 0;
    bool send_counter_exhausted = false;

    std::deque<Outbound> outbound;
    std::deque<Inbound> inbound;
    std::deque<MonotonicTime> send_stamps;
    std::deque<MonotonicTime> receive_stamps;
    std::deque<ObservedFingerprint> observed_fingerprints;

    bool observing = false;
    std::uint64_t observation_subscription = 0;
    std::uint64_t next_observation_subscription = 0;

    MonotonicTime now() const {
        return deps.runtime.now ? deps.runtime.now() : MonotonicTime{};
    }

    void report(std::string message) const {
        if (!deps.report_error) {
            return;
        }
        try {
            deps.report_error(std::move(message));
        } catch (...) {
            // El registro local nunca puede tumbar el hilo principal.
        }
    }

    bool canSend(NotificationMode mode) const {
        return modePermitted(send_features, mode);
    }

    bool canReceive(NotificationMode mode) const {
        return modePermitted(receive_features, mode);
    }

    static bool allow(std::deque<MonotonicTime>& stamps, MonotonicTime current) {
        while (!stamps.empty() && current - stamps.front() >= notification_policy::rate_window) {
            stamps.pop_front();
        }
        if (stamps.size() >= notification_policy::rate_allowance) {
            return false;
        }
        stamps.push_back(current);
        return true;
    }

    SendResult deliver(const RemoteNotification& notice) const {
        if (!deps.send) {
            return SendResult::disconnected;
        }
        try {
            return deps.send(FeatureMessage{notice});
        } catch (...) {
            return SendResult::disconnected;
        }
    }

    void withdrawActive() const {
        try {
            deps.desktop.withdrawNotices();
        } catch (...) {
        }
    }

    void enqueueOrSend(RemoteNotification notice, std::string source_app_id) {
        const SendResult result = deliver(notice);
        if (result == SendResult::queued) {
            return;
        }
        if (result == SendResult::busy) {
            if (outbound.size() >= queue_capacity) {
                report("aviso descartado: cola local de envío llena");
                return;
            }
            outbound.push_back(Outbound{std::move(notice), std::move(source_app_id), now()});
            return;
        }
        outbound.clear();
        inbound.clear();
        report(result == SendResult::denied ? "aviso descartado: la sesión denegó el envío"
                                           : "aviso descartado: sesión desconectada");
    }

    // Asigna identificador y entrega. El permiso y la forma ya se comprobaron.
    void emit(SessionGeneration requested_generation, NotificationMode mode,
              NotificationContent content, std::string app_name,
              std::string source_app_id) {
        if (requested_generation != generation) {
            return;
        }
        if (!canSend(mode)) {
            return;
        }
        if (!allow(send_stamps, now())) {
            report(std::string(mode == NotificationMode::forwarded ? forwarded_rejected
                                                                   : manual_rejected) +
                   "límite de frecuencia");
            return;
        }
        if (send_counter_exhausted || next_send_id == 0) {
            report("aviso descartado: contador de identificadores agotado");
            return;
        }

        const std::uint64_t identifier = next_send_id;
        if (identifier == std::numeric_limits<std::uint64_t>::max()) {
            send_counter_exhausted = true;
            next_send_id = 0;
        } else {
            ++next_send_id;
        }

        RemoteNotification notice{NotificationRef{generation, identifier}, mode,
                                  std::move(content), std::move(app_name)};
        enqueueOrSend(std::move(notice), std::move(source_app_id));
    }

    bool observedAllowed(const ObservedNotification& notice) const {
        if (notice.app_id.empty()) {
            return false;
        }
        if (is_product_application(notice.app_id)) {
            return false; // Eco de nuestros propios avisos.
        }
        return allowed_applications.find(notice.app_id) != allowed_applications.end();
    }

    bool seenRecently(const ObservedNotification& notice, MonotonicTime current) const {
        for (const auto& entry : observed_fingerprints) {
            if (current - entry.seen_at >= notification_policy::observed_dedup_lifetime) {
                continue;
            }
            if (entry.app_id == notice.app_id && entry.title == notice.content.title &&
                entry.body == notice.content.body) {
                return true;
            }
        }
        return false;
    }

    void rememberObserved(const ObservedNotification& notice, MonotonicTime current) {
        while (!observed_fingerprints.empty() &&
               (observed_fingerprints.size() >= notification_policy::observed_dedup_capacity ||
                current - observed_fingerprints.front().seen_at >=
                    notification_policy::observed_dedup_lifetime)) {
            observed_fingerprints.pop_front();
        }
        observed_fingerprints.push_back(ObservedFingerprint{
            notice.app_id, notice.content.title, notice.content.body, current});
    }

    void receiveNotice(const RemoteNotification& notice) {
        if (!validMode(notice.mode) || notice.ref.generation != generation) {
            return; // Generación antigua o modalidad desconocida.
        }
        if (!canReceive(notice.mode)) {
            return; // No autorizado: la deduplicación no avanza.
        }
        if (notice.ref.notice_id == 0) {
            report("aviso recibido descartado: identificador inválido");
            return;
        }
        const ContentProblem problem = checkContent(notice.content);
        if (problem != ContentProblem::none) {
            report(std::string("aviso recibido descartado: ") +
                   contentProblemDetail(problem));
            return;
        }
        if (notice.mode == NotificationMode::manual) {
            if (!notice.app_name.empty()) {
                report("aviso recibido descartado: aviso manual con aplicación");
                return;
            }
        } else {
            if (notice.app_name.empty() || !is_valid_utf8(notice.app_name) ||
                notice.app_name.size() > max_notification_app_name_bytes) {
                report("aviso recibido descartado: aplicación de origen inválida");
                return;
            }
        }

        if (notice.ref.notice_id <= last_received_id) {
            return; // Repetición o identificador ya consumido.
        }
        last_received_id = notice.ref.notice_id;

        const MonotonicTime current = now();
        if (!allow(receive_stamps, current)) {
            // La deduplicación ya avanzó: repetirlo no provoca otra presentación.
            report("aviso recibido descartado: límite de frecuencia");
            return;
        }
        if (inbound.size() >= queue_capacity) {
            report("aviso recibido descartado: cola local de presentación llena");
            return;
        }
        inbound.push_back(Inbound{notice, current});
    }

    void present(RemoteNotification notice) {
        std::string peer;
        try {
            if (deps.peer_name) {
                peer = deps.peer_name();
            }
        } catch (...) {
            peer.clear();
        }
        if (peer.empty()) {
            report("aviso no presentado: identidad del equipo no disponible");
            return;
        }

        const std::shared_ptr<Lifetime> token = lifetime;
        const SessionGeneration presented_generation = generation;
        const NotificationRef reference = notice.ref;
        const NotificationMode mode = notice.mode;
        auto complete = [token, presented_generation, reference, mode](DesktopResult result) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onPresented(presented_generation, reference, mode, result);
        };

        try {
            deps.desktop.presentNotice(peer, notice, std::move(complete));
        } catch (...) {
            report("aviso no presentado: error del escritorio");
        }
    }

    void onPresented(SessionGeneration presented_generation, NotificationRef reference,
                     NotificationMode mode, DesktopResult result) {
        (void)reference;
        if (presented_generation != generation) {
            return; // Retirada o generación nueva: el resultado ya no aplica.
        }
        if (!canReceive(mode)) {
            return;
        }
        if (result == DesktopResult::ready) {
            return;
        }
        report(presentationFailureText(result));
    }

    void flushInbound(MonotonicTime current) {
        for (auto iterator = inbound.begin(); iterator != inbound.end();) {
            const bool expired = current - iterator->queued_at >= queue_lifetime;
            if (expired) {
                report("aviso recibido descartado: cola caducada");
                iterator = inbound.erase(iterator);
            } else if (!canReceive(iterator->notice.mode)) {
                iterator = inbound.erase(iterator);
            } else {
                ++iterator;
            }
        }

        while (!inbound.empty()) {
            Inbound next = std::move(inbound.front());
            inbound.pop_front();
            if (current - next.queued_at >= queue_lifetime) {
                report("aviso recibido descartado: cola caducada");
                continue;
            }
            if (!canReceive(next.notice.mode)) {
                continue;
            }
            present(std::move(next.notice));
        }
    }

    void retryOutbound(MonotonicTime current) {
        while (!outbound.empty()) {
            Outbound next = std::move(outbound.front());
            outbound.pop_front();

            if (current - next.queued_at >= queue_lifetime) {
                report("aviso descartado: cola local caducada");
                continue;
            }
            if (!canSend(next.notice.mode)) {
                continue;
            }
            if (next.notice.mode == NotificationMode::forwarded) {
                if (next.source_app_id.empty() ||
                    is_product_application(next.source_app_id) ||
                    allowed_applications.find(next.source_app_id) ==
                        allowed_applications.end()) {
                    continue;
                }
            }

            const SendResult result = deliver(next.notice);
            if (result == SendResult::queued) {
                continue;
            }
            if (result == SendResult::busy) {
                outbound.push_front(std::move(next));
                break;
            }
            outbound.clear();
            report(result == SendResult::denied ? "aviso descartado: la sesión denegó el envío"
                                               : "aviso descartado: sesión desconectada");
            break;
        }
    }

    void startObservation() {
        if (observing) {
            return;
        }
        observing = true;
        observation_subscription = ++next_observation_subscription;
        const std::uint64_t subscription = observation_subscription;
        const std::shared_ptr<Lifetime> token = lifetime;

        auto observed = [token, subscription](ObservedNotification notice) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onObserved(subscription, std::move(notice));
        };
        auto state_changed = [token, subscription](DesktopResult result) {
            if (!token->alive.load(std::memory_order_acquire)) {
                return;
            }
            token->owner->onObservationState(subscription, result);
        };

        try {
            deps.desktop.startObserving(std::move(observed), std::move(state_changed));
        } catch (...) {
            observing = false;
            observation_subscription = 0;
            report("observación de notificaciones no disponible");
        }
    }

    void stopObservation() {
        if (!observing) {
            return;
        }
        // Se invalida la suscripción antes de desconectar el backend: cualquier
        // evento ya encolado de esta suscripción se descarta.
        observing = false;
        observation_subscription = 0;
        try {
            deps.desktop.stopObserving();
        } catch (...) {
        }
    }

    bool observationWanted() const {
        if (!send_features.forwarded_notifications || allowed_applications.empty()) {
            return false;
        }
        try {
            return deps.desktop.notificationCapabilities().observe;
        } catch (...) {
            return false;
        }
    }

    void syncObservation() {
        if (observationWanted()) {
            startObservation();
        } else {
            stopObservation();
        }
    }

    void onObserved(std::uint64_t subscription, ObservedNotification notice) {
        if (!observing || subscription != observation_subscription) {
            return; // Suscripción anterior ya invalidada.
        }
        forwardObserved(generation, notice);
    }

    void onObservationState(std::uint64_t subscription, DesktopResult result) {
        if (!observing || subscription != observation_subscription) {
            return;
        }
        if (result == DesktopResult::ready) {
            return;
        }
        observing = false;
        observation_subscription = 0;
        try {
            deps.desktop.stopObserving();
        } catch (...) {
        }
        switch (result) {
        case DesktopResult::denied:
            report("observación de notificaciones denegada por el escritorio");
            break;
        case DesktopResult::unavailable:
            report("observación de notificaciones no disponible");
            break;
        case DesktopResult::cancelled:
            report("observación de notificaciones cancelada");
            break;
        default:
            report("observación de notificaciones interrumpida");
            break;
        }
    }

    void forwardObserved(SessionGeneration requested_generation,
                         const ObservedNotification& notice) {
        if (requested_generation != generation) {
            return;
        }
        if (!send_features.forwarded_notifications) {
            return; // El permiso está retirado; no se registra como visto.
        }
        if (!observedAllowed(notice)) {
            return; // Sin identidad fiable, aplicación excluida o el propio producto.
        }
        if (notice.app_name.empty() || !is_valid_utf8(notice.app_name) ||
            notice.app_name.size() > max_notification_app_name_bytes) {
            return; // Sin nombre visible no hay atribución utilizable.
        }
        const ContentProblem problem = checkContent(notice.content);
        if (problem != ContentProblem::none) {
            report(contentProblemText(NotificationMode::forwarded, problem));
            return;
        }

        const MonotonicTime current = now();
        if (seenRecently(notice, current)) {
            return;
        }
        rememberObserved(notice, current);
        emit(generation, NotificationMode::forwarded, notice.content, notice.app_name,
             notice.app_id);
    }

    void sendManual(SessionGeneration requested_generation, const NotificationContent& content) {
        if (requested_generation != generation) {
            return;
        }
        if (!send_features.manual_notifications) {
            report("aviso manual descartado: permiso de envío no concedido");
            return;
        }
        const ContentProblem problem = checkContent(content);
        if (problem != ContentProblem::none) {
            report(contentProblemText(NotificationMode::manual, problem));
            return;
        }
        emit(generation, NotificationMode::manual, content, {}, {});
    }

    void setAllowedApplications(std::set<std::string> app_ids) {
        allowed_applications = std::move(app_ids);
        // Una aplicación retirada de la lista no debe consumir la cola de envío.
        for (auto iterator = outbound.begin(); iterator != outbound.end();) {
            const bool forwarded = iterator->notice.mode == NotificationMode::forwarded;
            if (forwarded &&
                (iterator->source_app_id.empty() ||
                 allowed_applications.find(iterator->source_app_id) ==
                     allowed_applications.end())) {
                iterator = outbound.erase(iterator);
            } else {
                ++iterator;
            }
        }
        syncObservation();
    }

    void setAuthorization(SessionGeneration requested_generation, FeatureSet send,
                          FeatureSet receive) {
        const bool generation_changed = requested_generation != generation;
        if (generation_changed) {
            stopObservation();
            withdrawActive();
            outbound.clear();
            inbound.clear();
            send_stamps.clear();
            receive_stamps.clear();
            observed_fingerprints.clear();
            generation = requested_generation;
            next_send_id = 1;
            last_received_id = 0;
            send_counter_exhausted = false;
        } else {
            const FeatureSet previous_receive = receive_features;
            const bool receive_revoked =
                (previous_receive.manual_notifications && !receive.manual_notifications) ||
                (previous_receive.forwarded_notifications && !receive.forwarded_notifications);
            if (receive_revoked) {
                withdrawActive();
            }
            for (auto iterator = inbound.begin(); iterator != inbound.end();) {
                if (!modePermitted(receive, iterator->notice.mode)) {
                    iterator = inbound.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            for (auto iterator = outbound.begin(); iterator != outbound.end();) {
                if (!modePermitted(send, iterator->notice.mode)) {
                    iterator = outbound.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }

        send_features = send;
        receive_features = receive;
        syncObservation();
    }

    void tick() {
        const MonotonicTime current = now();
        retryOutbound(current);
        flushInbound(current);
    }

    void shutdown() {
        lifetime->alive.store(false, std::memory_order_release);
        stopObservation();
        withdrawActive();
        outbound.clear();
        inbound.clear();
    }
};

SharedNotifications::SharedNotifications(NotificationDependencies dependencies)
    : impl_(std::make_unique<Impl>(std::move(dependencies))) {}

SharedNotifications::~SharedNotifications() {
    impl_->shutdown();
}

void SharedNotifications::tick() {
    impl_->tick();
}

void SharedNotifications::setAllowedApplications(std::set<std::string> app_ids) {
    impl_->setAllowedApplications(std::move(app_ids));
}

void SharedNotifications::setAuthorization(SessionGeneration generation, FeatureSet send,
                                           FeatureSet receive) {
    impl_->setAuthorization(generation, send, receive);
}

void SharedNotifications::sendManual(SessionGeneration generation,
                                     const NotificationContent& content) {
    impl_->sendManual(generation, content);
}

void SharedNotifications::forwardObserved(SessionGeneration generation,
                                          const ObservedNotification& notice) {
    impl_->forwardObserved(generation, notice);
}

void SharedNotifications::receiveNotice(const RemoteNotification& notice) {
    impl_->receiveNotice(notice);
}

} // namespace oneintwo
