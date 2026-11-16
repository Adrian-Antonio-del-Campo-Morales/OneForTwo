#include "visual_share.hpp"

#include "../integration/feature_contracts.hpp"
#include "visual/visual_codec.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace oneintwo {
namespace {

// docs/COMPARTIR_PANTALLA.md fija tres segundos sin señales válidas. La
// cabecera común declara visual_activity_timeout = 5 s; prevalece el ciclo
// funcional de esta entrega (ver entrega al integrador).
constexpr auto viewer_signal_timeout = std::chrono::seconds{3};
constexpr auto frame_interval = std::chrono::microseconds{1000000 / max_visual_frame_rate};

bool sameRef(const VisualShareRef& left, const VisualShareRef& right) {
    return left.generation == right.generation && left.initiator == right.initiator &&
           left.offer_id == right.offer_id;
}

bool sendFeatureAllows(const FeatureSet& features, VisualSourceKind kind) {
    return kind == VisualSourceKind::monitor ? features.share_monitor : features.share_window;
}

bool receiveFeatureAllows(const FeatureSet& features, VisualSourceKind kind) {
    return kind == VisualSourceKind::monitor ? features.share_monitor : features.share_window;
}

} // namespace

struct VisualShare::Impl {
    enum class Phase {
        idle,
        selecting,
        offer_pending,
        live,
    };

    enum class Role {
        none,
        origin,
        viewer,
    };

    struct Life {
        bool alive = true;
    };

    struct PendingCapture {
        VisualFrame frame;
        VisualSourceRevision revision = 0;
        std::uint64_t sequence = 0;
    };

    explicit Impl(VisualDependencies dependencies_in)
        : dependencies(std::move(dependencies_in)), life(std::make_shared<Life>()) {}

    ~Impl() {
        life->alive = false;
        if (phase != Phase::idle) {
            const VisualShareRef old_ref = ref;
            const bool close_viewer = viewer_open;
            const bool dismiss = offer_shown;
            phase = Phase::idle;
            role = Role::none;
            viewer_open = false;
            offer_shown = false;
            dependencies.capture.stopCapture(old_ref);
            if (dismiss && dependencies.dismiss_offer) {
                dependencies.dismiss_offer(old_ref);
            }
            if (close_viewer) {
                dependencies.viewer.close(old_ref);
            }
        }
    }

    MonotonicTime now() const {
        return dependencies.runtime.now ? dependencies.runtime.now()
                                        : std::chrono::steady_clock::now();
    }

    void post(Task task) {
        if (!dependencies.runtime.post) {
            task();
            return;
        }
        const std::shared_ptr<Life> token = life;
        dependencies.runtime.post([token, task = std::move(task)] {
            if (token->alive) {
                task();
            }
        });
    }

    void report(const std::string& message) const {
        if (dependencies.report_error) {
            dependencies.report_error(message);
        }
    }

    SendResult emit(FeatureMessage message) {
        if (!dependencies.send) {
            return SendResult::disconnected;
        }
        return dependencies.send(std::move(message));
    }

    bool sendAllowed(VisualSourceKind source) const {
        return sendFeatureAllows(send_features, source);
    }

    bool receiveAllowed(VisualSourceKind source) const {
        return receiveFeatureAllows(receive_features, source);
    }

    bool isCurrent(const VisualShareRef& candidate, Role expected) const {
        return phase != Phase::idle && role == expected && sameRef(ref, candidate);
    }

    void clearQueues() {
        encoded_queue.clear();
        pending_capture.reset();
        codec_busy = false;
        has_last_encode = false;
    }

    // Invalida primero la referencia y después libera recursos. `send_stop`
    // sólo se usa para un cierre iniciado localmente.
    void terminate(bool send_stop) {
        if (phase == Phase::idle) {
            return;
        }
        const VisualShareRef old_ref = ref;
        const Role old_role = role;
        const bool old_viewer = viewer_open;
        const bool old_offer = offer_shown;
        const bool was_live = phase == Phase::live;
        const bool was_origin_pending = phase == Phase::offer_pending && role == Role::origin;

        phase = Phase::idle;
        role = Role::none;
        viewer_open = false;
        offer_shown = false;
        revision = 0;
        source_state = VisualSourceState::suspended;
        last_presented_sequence = 0;
        next_sequence = 1;
        deadline = MonotonicTime{};
        clearQueues();
        ref = VisualShareRef{};
        kind = VisualSourceKind::monitor;
        operation_generation = 0;

        if (old_role == Role::origin) {
            dependencies.capture.stopCapture(old_ref);
        }
        if (old_offer && dependencies.dismiss_offer) {
            dependencies.dismiss_offer(old_ref);
        }
        if (old_viewer) {
            dependencies.viewer.close(old_ref);
        }
        if (send_stop && (was_live || was_origin_pending) && dependencies.send) {
            dependencies.send(VisualStopMessage{old_ref});
        }
    }

    void discardStaleSelection(const VisualShareRef& selected) {
        dependencies.capture.stopCapture(selected);
    }

    void offerSource(SessionGeneration requested_generation, VisualSourceKind source) {
        if (requested_generation == 0 || requested_generation != generation) {
            report("la generación de sesión no está vigente");
            return;
        }
        if (!sendAllowed(source)) {
            report("el permiso de compartir está desactivado para esta modalidad");
            return;
        }
        if (phase != Phase::idle) {
            report("ya hay una operación visual en curso");
            return;
        }
        const VisualCaptureCapabilities capabilities =
            dependencies.capture.visualCapabilities();
        const bool supported = source == VisualSourceKind::monitor ? capabilities.monitor
                                                                   : capabilities.window;
        if (!supported) {
            report("el escritorio no admite esta modalidad de captura");
            return;
        }
        if (next_offer_id == std::numeric_limits<std::uint64_t>::max()) {
            report("identificador de operación agotado");
            return;
        }

        ref = VisualShareRef{generation, VisualInitiator::local, next_offer_id++};
        kind = source;
        operation_generation = generation;
        role = Role::origin;
        phase = Phase::selecting;
        revision = 0;
        source_state = VisualSourceState::suspended;
        viewer_open = false;
        offer_shown = false;
        clearQueues();

        const VisualShareRef expected = ref;
        dependencies.capture.selectSource(
            expected, source, [this, expected](VisualShareRef selected, DesktopResult result) {
                post([this, expected, selected, result] {
                    handleSelection(expected, selected, result);
                });
            });
    }

    void handleSelection(const VisualShareRef& expected, const VisualShareRef& selected,
                         DesktopResult result) {
        if (phase != Phase::selecting || role != Role::origin || !sameRef(ref, expected)) {
            if (result == DesktopResult::ready) {
                discardStaleSelection(selected);
            }
            return;
        }
        if (result == DesktopResult::ready) {
            if (!sameRef(selected, ref)) {
                discardStaleSelection(selected);
                terminate(false);
                report("el selector devolvió una fuente distinta de la solicitada");
                return;
            }
            if (ref.generation != generation || !sendAllowed(kind)) {
                terminate(false);
                report("el permiso se retiró durante la selección");
                return;
            }
            phase = Phase::offer_pending;
            deadline = now() + visual_offer_lifetime;
            const SendResult sent = emit(VisualShareOffer{ref, kind});
            if (sent != SendResult::queued) {
                terminate(false);
                report("no se pudo enviar la invitación visual");
            }
            return;
        }
        terminate(false);
        if (result == DesktopResult::denied) {
            report("el sistema denegó la captura de la fuente");
        } else if (result == DesktopResult::unavailable) {
            report("la fuente seleccionada no está disponible");
        } else if (result == DesktopResult::error) {
            report("falló la selección local de la fuente");
        }
    }

    void receiveOffer(const VisualShareOffer& offer) {
        if (generation == 0 || offer.ref.generation != generation ||
            offer.ref.initiator != VisualInitiator::peer || offer.ref.offer_id == 0) {
            return;
        }
        if (offer.ref.offer_id <= last_peer_offer_id) {
            return;
        }
        last_peer_offer_id = offer.ref.offer_id;
        if (!receiveAllowed(offer.source)) {
            return;
        }
        if (phase != Phase::idle) {
            // Una invitación simultánea se rechaza por ocupación, incluso si la
            // operación activa pertenece a la otra modalidad o al otro sentido.
            emit(VisualDecisionMessage{offer.ref, VisualDecision::busy});
            return;
        }

        ref = offer.ref;
        kind = offer.source;
        operation_generation = generation;
        role = Role::viewer;
        phase = Phase::offer_pending;
        revision = 0;
        source_state = VisualSourceState::suspended;
        last_presented_sequence = 0;
        next_sequence = 1;
        last_signal = now();
        deadline = last_signal + visual_offer_lifetime;
        viewer_open = false;
        offer_shown = false;
        clearQueues();
        if (dependencies.show_offer) {
            dependencies.show_offer(offer);
            offer_shown = true;
        }
    }

    void decideOffer(VisualShareRef offer, VisualDecision decision) {
        if (phase != Phase::offer_pending || role != Role::viewer ||
            !sameRef(ref, offer) || offer.generation != generation) {
            return;
        }
        VisualDecision effective = decision;
        if (effective == VisualDecision::accepted &&
            (!receiveAllowed(kind) || offer.generation != generation)) {
            effective = VisualDecision::rejected;
        }
        if (emit(VisualDecisionMessage{ref, effective}) != SendResult::queued) {
            terminate(false);
            report("no se pudo enviar la decisión visual");
            return;
        }
        if (effective != VisualDecision::accepted) {
            if (effective == VisualDecision::busy) {
                report("la operación visual está ocupada");
            }
            terminate(false);
            return;
        }

        phase = Phase::live;
        revision = 0;
        source_state = VisualSourceState::suspended;
        last_presented_sequence = 0;
        next_sequence = 1;
        last_signal = now();
        deadline = MonotonicTime{};
        offer_shown = false;
        if (dependencies.dismiss_offer) {
            dependencies.dismiss_offer(ref);
        }
        dependencies.viewer.open(VisualShareOffer{ref, kind});
        viewer_open = true;
    }

    void receiveDecision(VisualShareRef offer, VisualDecision decision) {
        if (phase != Phase::offer_pending || role != Role::origin ||
            !sameRef(ref, offer) || offer.generation != generation) {
            return;
        }
        if (now() >= deadline) {
            terminate(false);
            report("la aceptación llegó después del plazo de la invitación");
            return;
        }
        if (decision != VisualDecision::accepted) {
            terminate(false);
            return;
        }
        if (!sendAllowed(kind) || offer.generation != generation) {
            terminate(false);
            report("el permiso de compartir se retiró antes de la aceptación");
            return;
        }

        phase = Phase::live;
        revision = 1;
        source_state = VisualSourceState::available;
        last_presented_sequence = 0;
        next_sequence = 1;
        last_signal = now();
        has_last_activity = false;
        deadline = MonotonicTime{};
        if (emit(VisualStateMessage{ref, revision, VisualSourceState::available}) !=
            SendResult::queued) {
            terminate(false);
            report("no se pudo anunciar el estado de la fuente");
            return;
        }
        VisualCaptureCallbacks callbacks;
        callbacks.frame = [this](VisualShareRef source, VisualFrame frame) {
            post([this, source, frame = std::move(frame)]() mutable {
                handleCaptureFrame(source, std::move(frame));
            });
        };
        callbacks.state_changed = [this](VisualShareRef source, VisualSourceState state) {
            post([this, source, state] { handleCaptureState(source, state); });
        };
        dependencies.capture.startCapture(ref, std::move(callbacks));
    }

    void announceSourceState(VisualSourceState state) {
        if (revision == std::numeric_limits<VisualSourceRevision>::max()) {
            terminate(false);
            report("revisión de fuente agotada");
            return;
        }
        ++revision;
        last_presented_sequence = 0;
        encoded_queue.clear();
        pending_capture.reset();
        source_state = state;
        if (emit(VisualStateMessage{ref, revision, state}) != SendResult::queued) {
            terminate(false);
            report("no se pudo anunciar el cambio de estado de la fuente");
            return;
        }
        if (state == VisualSourceState::closed) {
            terminate(false);
        }
    }

    void handleCaptureState(const VisualShareRef& source, VisualSourceState state) {
        if (!isCurrent(source, Role::origin) || phase != Phase::live) {
            return;
        }
        if (state == source_state) {
            return;
        }
        announceSourceState(state);
    }

    void handleCaptureFrame(const VisualShareRef& source, VisualFrame frame) {
        if (!isCurrent(source, Role::origin) || phase != Phase::live ||
            source_state != VisualSourceState::available) {
            return;
        }
        std::string error;
        if (!validateVisualFrameGeometry(frame, &error)) {
            report(error);
            return;
        }
        const MonotonicTime current = now();
        if (has_last_encode && current - last_encode < frame_interval) {
            return;
        }
        if (codec_busy) {
            if (pending_capture.has_value() || encoded_queue.size() >= 2) {
                return;
            }
            pending_capture = PendingCapture{std::move(frame), revision, 0};
            return;
        }
        if (encoded_queue.size() >= 2) {
            return;
        }
        startEncode(std::move(frame), revision);
    }

    void allocateAndStart(const PendingCapture& pending) {
        if (next_sequence == std::numeric_limits<std::uint64_t>::max()) {
            terminate(false);
            report("secuencia de imagen agotada");
            return;
        }
        PendingCapture copy = pending;
        copy.sequence = next_sequence++;
        startEncode(std::move(copy.frame), copy.revision, copy.sequence);
    }

    void startEncode(VisualFrame frame, VisualSourceRevision frame_revision,
                     std::uint64_t sequence = 0) {
        if (sequence == 0) {
            if (next_sequence == std::numeric_limits<std::uint64_t>::max()) {
                terminate(false);
                report("secuencia de imagen agotada");
                return;
            }
            sequence = next_sequence++;
        }
        codec_busy = true;
        has_last_encode = true;
        last_encode = now();
        const VisualShareRef current_ref = ref;
        post([this, current_ref, frame = std::move(frame), frame_revision, sequence]() mutable {
            runEncode(current_ref, std::move(frame), frame_revision, sequence);
        });
    }

    void runEncode(const VisualShareRef& encoded_ref, VisualFrame frame,
                   VisualSourceRevision frame_revision, std::uint64_t sequence) {
        std::string error;
        const std::optional<VisualEncodedFrame> encoded =
            encodeVisualFrame(encoded_ref, frame_revision, sequence, frame, &error);
        codec_busy = false;

        const bool still_current = isCurrent(encoded_ref, Role::origin) && phase == Phase::live &&
                                   frame_revision == revision;
        if (still_current) {
            if (encoded.has_value()) {
                if (encoded_queue.size() < 2) {
                    encoded_queue.push_back(*encoded);
                    flushEncoded();
                }
            } else {
                report(error);
            }
        }

        if (phase == Phase::live && role == Role::origin && pending_capture.has_value() &&
            pending_capture->revision == revision) {
            PendingCapture pending = std::move(*pending_capture);
            pending_capture.reset();
            allocateAndStart(pending);
        } else if (pending_capture.has_value() && pending_capture->revision != revision) {
            pending_capture.reset();
        }
    }

    void flushEncoded() {
        while (!encoded_queue.empty() && phase == Phase::live && role == Role::origin) {
            const SendResult sent = emit(FeatureMessage{encoded_queue.front()});
            if (sent == SendResult::queued) {
                encoded_queue.pop_front();
                continue;
            }
            if (sent == SendResult::busy) {
                return;
            }
            terminate(false);
            report("el canal de datos no está disponible");
            return;
        }
    }

    void receiveSourceState(VisualShareRef offer, VisualSourceRevision new_revision,
                            VisualSourceState state) {
        if (!isCurrent(offer, Role::viewer) || phase != Phase::live) {
            return;
        }
        if (!receiveAllowed(kind)) {
            terminate(false);
            report("el permiso de recepción se retiró");
            return;
        }
        last_signal = now();
        if (new_revision < revision) {
            return;
        }
        if (new_revision > revision) {
            revision = new_revision;
            last_presented_sequence = 0;
            source_state = state;
            dependencies.viewer.clear(ref, state);
        } else if (state != source_state) {
            source_state = state;
            dependencies.viewer.clear(ref, state);
        }
        if (state == VisualSourceState::closed) {
            terminate(false);
        }
    }

    void receiveActivity(VisualShareRef offer, VisualSourceRevision new_revision) {
        if (!isCurrent(offer, Role::viewer) || phase != Phase::live) {
            return;
        }
        if (!receiveAllowed(kind)) {
            terminate(false);
            report("el permiso de recepción se retiró");
            return;
        }
        last_signal = now();
        if (new_revision < revision) {
            return;
        }
        if (new_revision > revision) {
            revision = new_revision;
            source_state = VisualSourceState::available;
            last_presented_sequence = 0;
            dependencies.viewer.clear(ref, VisualSourceState::available);
        }
    }

    void receiveFrame(VisualEncodedFrame frame) {
        if (!isCurrent(frame.ref, Role::viewer) || phase != Phase::live) {
            return;
        }
        if (!receiveAllowed(kind)) {
            terminate(false);
            report("el permiso de recepción se retiró");
            return;
        }
        if (source_state != VisualSourceState::available || frame.revision != revision ||
            frame.sequence <= last_presented_sequence || !frame.key_frame) {
            // Imagen adelantada a su anuncio, de una revisión anterior o repetida.
            return;
        }
        std::string error;
        const std::optional<VisualFrame> decoded = decodeVisualFrame(frame, &error);
        if (!decoded.has_value()) {
            report(error);
            return;
        }
        last_presented_sequence = frame.sequence;
        last_signal = now();
        dependencies.viewer.present(ref, *decoded);
    }

    void stop(VisualShareRef offer) {
        if (phase == Phase::idle || !sameRef(ref, offer)) {
            return;
        }
        if (role == Role::viewer && phase == Phase::offer_pending) {
            emit(VisualDecisionMessage{ref, VisualDecision::rejected});
            terminate(false);
            return;
        }
        const bool send_stop = phase == Phase::live || (role == Role::origin &&
                                                        phase == Phase::offer_pending);
        terminate(send_stop);
    }

    void receiveStop(VisualShareRef offer) {
        if (phase == Phase::idle || !sameRef(ref, offer)) {
            return;
        }
        // Un cierre remoto no reenvía otro cierre.
        terminate(false);
    }

    void setAuthorization(SessionGeneration new_generation, FeatureSet new_send,
                          FeatureSet new_receive) {
        if (new_generation != generation) {
            terminate(false);
            generation = new_generation;
            last_peer_offer_id = 0;
            next_offer_id = 1;
        } else if (phase != Phase::idle) {
            const bool still_allowed =
                role == Role::origin ? sendFeatureAllows(new_send, kind)
                                     : receiveFeatureAllows(new_receive, kind);
            if (!still_allowed) {
                if (role == Role::viewer && phase == Phase::offer_pending) {
                    emit(VisualDecisionMessage{ref, VisualDecision::rejected});
                }
                terminate(phase == Phase::live);
            }
        }
        send_features = new_send;
        receive_features = new_receive;
    }

    void tick() {
        const MonotonicTime current = now();
        if (phase == Phase::offer_pending && current >= deadline) {
            terminate(false);
            report("la invitación visual caducó");
            return;
        }
        if (phase != Phase::live) {
            return;
        }
        if (role == Role::origin) {
            if (!has_last_activity || current - last_activity >= visual_activity_interval) {
                has_last_activity = true;
                last_activity = current;
                if (emit(VisualActivityMessage{ref, revision}) != SendResult::queued) {
                    terminate(false);
                    report("no se pudo mantener la actividad de la operación visual");
                    return;
                }
            }
            flushEncoded();
            return;
        }
        if (current - last_signal >= viewer_signal_timeout) {
            terminate(true);
            report("se perdió la señal del origen visual");
        }
    }

    VisualDependencies dependencies;
    std::shared_ptr<Life> life;
    SessionGeneration generation = 0;
    FeatureSet send_features{};
    FeatureSet receive_features{};
    Phase phase = Phase::idle;
    Role role = Role::none;
    VisualShareRef ref{};
    VisualSourceKind kind = VisualSourceKind::monitor;
    SessionGeneration operation_generation = 0;
    std::uint64_t next_offer_id = 1;
    std::uint64_t last_peer_offer_id = 0;
    MonotonicTime deadline{};
    MonotonicTime last_signal{};
    MonotonicTime last_activity{};
    MonotonicTime last_encode{};
    bool has_last_activity = false;
    bool has_last_encode = false;
    VisualSourceRevision revision = 0;
    VisualSourceState source_state = VisualSourceState::suspended;
    std::uint64_t next_sequence = 1;
    std::uint64_t last_presented_sequence = 0;
    bool viewer_open = false;
    bool offer_shown = false;
    bool codec_busy = false;
    std::deque<VisualEncodedFrame> encoded_queue;
    std::optional<PendingCapture> pending_capture;
};

VisualShare::VisualShare(VisualDependencies dependencies)
    : impl_(std::make_unique<Impl>(std::move(dependencies))) {}

VisualShare::~VisualShare() = default;

void VisualShare::tick() {
    impl_->tick();
}

void VisualShare::setAuthorization(SessionGeneration generation, FeatureSet send,
                                   FeatureSet receive) {
    impl_->setAuthorization(generation, send, receive);
}

void VisualShare::offerSource(SessionGeneration generation, VisualSourceKind source) {
    impl_->offerSource(generation, source);
}

void VisualShare::receiveOffer(const VisualShareOffer& offer) {
    impl_->receiveOffer(offer);
}

void VisualShare::decideOffer(VisualShareRef offer, VisualDecision decision) {
    impl_->decideOffer(offer, decision);
}

void VisualShare::receiveDecision(VisualShareRef offer, VisualDecision decision) {
    impl_->receiveDecision(offer, decision);
}

void VisualShare::receiveSourceState(VisualShareRef offer, VisualSourceRevision revision,
                                     VisualSourceState state) {
    impl_->receiveSourceState(offer, revision, state);
}

void VisualShare::receiveActivity(VisualShareRef offer, VisualSourceRevision revision) {
    impl_->receiveActivity(offer, revision);
}

void VisualShare::receiveFrame(VisualEncodedFrame frame) {
    impl_->receiveFrame(std::move(frame));
}

void VisualShare::stop(VisualShareRef offer) {
    impl_->stop(offer);
}

void VisualShare::receiveStop(VisualShareRef offer) {
    impl_->receiveStop(offer);
}

} // namespace oneintwo
