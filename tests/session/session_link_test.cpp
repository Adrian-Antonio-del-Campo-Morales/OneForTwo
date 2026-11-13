#include "net/session_link.hpp"

#include "net/protocol.hpp"
#include "test_support.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;

namespace {

FeatureSet allFeatures() {
    return FeatureSet{true, true, true, true, true, true, true, true, true};
}

struct FakeRuntime {
    std::shared_ptr<MonotonicTime> clock = std::make_shared<MonotonicTime>(MonotonicTime{});
    std::deque<Task> tasks;

    ModuleRuntime moduleRuntime() {
        ModuleRuntime runtime;
        runtime.now = [this] { return *clock; };
        runtime.post = [this](Task task) { tasks.push_back(std::move(task)); };
        return runtime;
    }

    void drain() {
        std::size_t guard = 0;
        while (!tasks.empty() && guard++ < 10000) {
            Task task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
    }
};

struct FakeTransport : SessionTransport {
    struct Frame {
        SessionChannelKind channel;
        std::vector<std::uint8_t> bytes;
    };

    FakeTransport* peer = nullptr;
    std::string fingerprint;
    std::deque<Frame> inbox;
    std::vector<Frame> sent;
    TransportReceiver receiver;
    TransportClosed closed;
    bool open = true;

    std::string authenticated_peer_fingerprint() const override {
        return fingerprint;
    }

    TransportSendStatus send(SessionChannelKind channel,
                             const std::vector<std::uint8_t>& frame) override {
        if (!open) {
            return TransportSendStatus::closed;
        }
        sent.push_back(Frame{channel, frame});
        if (peer == nullptr || !peer->open) {
            return TransportSendStatus::closed;
        }
        peer->inbox.push_back(Frame{channel, frame});
        return TransportSendStatus::sent;
    }

    void setReceiver(TransportReceiver value) override {
        receiver = std::move(value);
    }

    void setClosedHandler(TransportClosed handler) override {
        closed = std::move(handler);
    }

    void close() override {
        if (!open) {
            return;
        }
        open = false;
        if (peer != nullptr && peer->open) {
            peer->open = false;
            if (peer->closed) {
                peer->closed();
            }
        }
    }

    void inject(SessionChannelKind channel, std::vector<std::uint8_t> bytes) {
        inbox.push_back(Frame{channel, std::move(bytes)});
    }

    void pump(FakeRuntime& runtime) {
        runtime.drain();
        std::size_t guard = 0;
        while (!inbox.empty() && guard++ < 10000) {
            Frame frame = std::move(inbox.front());
            inbox.pop_front();
            if (!open) {
                continue;
            }
            if (receiver) {
                receiver(frame.channel, std::move(frame.bytes));
            }
            runtime.drain();
        }
    }
};

SessionLinkOptions optionsFor(const std::string& local, const std::string& peer,
                              const std::string& peer_fingerprint,
                              SessionGeneration generation) {
    SessionLinkOptions options;
    options.local_device_id = local;
    options.peer_device_id = peer;
    options.peer_fingerprint = peer_fingerprint;
    options.granted_send = allFeatures();
    options.granted_receive = allFeatures();
    options.allocate_generation = [generation] { return generation; };
    return options;
}

struct Link {
    FakeRuntime runtime_a;
    FakeRuntime runtime_b;
    FakeTransport transport_a;
    FakeTransport transport_b;
    std::vector<FeatureMessage> messages_a;
    std::vector<FeatureMessage> messages_b;
    std::vector<SessionLinkError> errors_a;
    std::vector<SessionLinkError> errors_b;
    std::vector<std::string> details_a;
    std::vector<std::string> details_b;
    int established_a = 0;
    int established_b = 0;
    int closed_a = 0;
    int closed_b = 0;
    std::unique_ptr<AuthenticatedSession> a;
    std::unique_ptr<AuthenticatedSession> b;

    Link(SessionLinkOptions options_a, SessionLinkOptions options_b) {
        transport_a.peer = &transport_b;
        transport_b.peer = &transport_a;
        transport_a.fingerprint = options_a.peer_fingerprint;
        transport_b.fingerprint = options_b.peer_fingerprint;

        SessionLinkHandlers handlers_a;
        handlers_a.established = [this](SessionGeneration, SessionGeneration) { ++established_a; };
        handlers_a.failed = [this](SessionLinkError error, std::string detail) {
            errors_a.push_back(error);
            details_a.push_back(std::move(detail));
        };
        handlers_a.closed = [this] { ++closed_a; };
        handlers_a.message = [this](FeatureMessage message) {
            messages_a.push_back(std::move(message));
        };
        SessionLinkHandlers handlers_b = handlers_a;
        handlers_b.established = [this](SessionGeneration, SessionGeneration) { ++established_b; };
        handlers_b.failed = [this](SessionLinkError error, std::string detail) {
            errors_b.push_back(error);
            details_b.push_back(std::move(detail));
        };
        handlers_b.closed = [this] { ++closed_b; };
        handlers_b.message = [this](FeatureMessage message) {
            messages_b.push_back(std::move(message));
        };

        a = std::make_unique<AuthenticatedSession>(std::move(options_a), transport_a,
                                                   runtime_a.moduleRuntime(),
                                                   std::move(handlers_a));
        b = std::make_unique<AuthenticatedSession>(std::move(options_b), transport_b,
                                                   runtime_b.moduleRuntime(),
                                                   std::move(handlers_b));
    }

    void startBoth() {
        a->start();
        b->start();
    }

    void settle() {
        for (int round = 0; round < 8; ++round) {
            transport_b.pump(runtime_b);
            transport_a.pump(runtime_a);
        }
    }
};

VisualShareRef refFor(const Link& link, std::uint64_t id) {
    return VisualShareRef{link.a->generation(), VisualInitiator::local, id};
}

VisualEncodedFrame smallFrame(const VisualShareRef& ref, std::uint64_t revision = 1,
                              std::uint64_t sequence = 1) {
    VisualEncodedFrame frame;
    frame.ref = ref;
    frame.revision = revision;
    frame.sequence = sequence;
    frame.size = VisualSize{8, 8};
    frame.key_frame = true;
    frame.bytes.assign(128, 0x42);
    return frame;
}

} // namespace

TEST_CASE(establishes_session_with_exchanged_generations) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    CHECK_EQ(link.a->phase(), SessionLinkPhase::established);
    CHECK_EQ(link.b->phase(), SessionLinkPhase::established);
    CHECK_EQ(link.a->generation(), static_cast<SessionGeneration>(10));
    CHECK_EQ(link.a->peerGeneration(), static_cast<SessionGeneration>(20));
    CHECK_EQ(link.b->generation(), static_cast<SessionGeneration>(20));
    CHECK_EQ(link.b->peerGeneration(), static_cast<SessionGeneration>(10));
    CHECK_EQ(link.established_a, 1);
    CHECK_EQ(link.established_b, 1);
    CHECK(link.errors_a.empty());
    CHECK(link.errors_b.empty());
}

TEST_CASE(rejects_unpinned_or_mismatched_peer_identity) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.transport_a.fingerprint = "ff"; // el transporte autenticó otro certificado
    link.startBoth();
    link.settle();

    CHECK_EQ(link.a->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_a.size(), static_cast<std::size_t>(1));
    CHECK_EQ(link.errors_a.front(), SessionLinkError::identity);
    CHECK(link.transport_a.sent.empty()); // no se envió ningún saludo
    CHECK(link.established_a == 0);
}

TEST_CASE(rejects_unexpected_remote_device_id) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("C", "A", "aa", 20));
    link.startBoth();
    link.settle();

    CHECK_EQ(link.a->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_a.size(), static_cast<std::size_t>(1));
    CHECK_EQ(link.errors_a.front(), SessionLinkError::identity);
    CHECK(link.established_a == 0);
}

TEST_CASE(rejects_replayed_peer_generation_after_reconnect) {
    auto last_peer = std::make_shared<std::map<std::string, SessionGeneration>>();
    const auto with_store = [last_peer](SessionLinkOptions options) {
        options.last_peer_generation = [last_peer](const std::string& device) {
            const auto found = last_peer->find(device);
            return found == last_peer->end() ? SessionGeneration{0} : found->second;
        };
        options.remember_peer_generation = [last_peer](const std::string& device,
                                                       SessionGeneration generation) {
            (*last_peer)[device] = generation;
        };
        return options;
    };

    Link first(with_store(optionsFor("A", "B", "bb", 10)),
               with_store(optionsFor("B", "A", "aa", 20)));
    first.startBoth();
    first.settle();
    CHECK_EQ(first.a->phase(), SessionLinkPhase::established);

    Link second(with_store(optionsFor("A", "B", "bb", 11)),
                with_store(optionsFor("B", "A", "aa", 20)));
    second.startBoth();
    second.settle();
    CHECK_EQ(second.a->phase(), SessionLinkPhase::closed);
    CHECK_EQ(second.errors_a.size(), static_cast<std::size_t>(1));
    CHECK_EQ(second.errors_a.front(), SessionLinkError::generation);
}

TEST_CASE(local_generation_zero_is_rejected) {
    Link link(optionsFor("A", "B", "bb", 0), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();
    CHECK_EQ(link.a->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_a.front(), SessionLinkError::generation);
}

TEST_CASE(feature_message_before_establishment_terminates_session) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    // Se descarta el saludo encolado y se inyecta una trama antes de negociar.
    link.transport_b.inbox.clear();
    std::vector<std::uint8_t> bogus{0, 1, 0, 5, 0, 0, 0, 0};
    link.transport_b.inject(SessionChannelKind::interactive, bogus);
    link.settle();
    CHECK_EQ(link.b->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_b.front(), SessionLinkError::protocol);
}

TEST_CASE(all_visual_messages_flow_with_translated_identity) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();
    CHECK_EQ(link.a->phase(), SessionLinkPhase::established);

    const VisualShareRef local = refFor(link, 1);
    CHECK_EQ(link.a->send(VisualShareOffer{local, VisualSourceKind::monitor}),
             SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_b.size(), static_cast<std::size_t>(1));
    const VisualShareOffer offer_b = std::get<VisualShareOffer>(link.messages_b.back());
    CHECK_EQ(offer_b.source, VisualSourceKind::monitor);
    CHECK_EQ(offer_b.ref.generation, static_cast<SessionGeneration>(20));
    CHECK_EQ(offer_b.ref.initiator, VisualInitiator::peer);
    CHECK_EQ(offer_b.ref.offer_id, static_cast<std::uint64_t>(1));

    // B responde sobre la identidad de operación original.
    CHECK_EQ(link.b->send(VisualDecisionMessage{offer_b.ref, VisualDecision::accepted}),
             SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_a.size(), static_cast<std::size_t>(1));
    const VisualDecisionMessage decision_a = std::get<VisualDecisionMessage>(link.messages_a.back());
    CHECK_EQ(decision_a.ref.generation, static_cast<SessionGeneration>(10));
    CHECK_EQ(decision_a.ref.initiator, VisualInitiator::local);
    CHECK_EQ(decision_a.ref.offer_id, static_cast<std::uint64_t>(1));
    CHECK_EQ(decision_a.decision, VisualDecision::accepted);

    CHECK_EQ(link.a->send(VisualStateMessage{local, 1, VisualSourceState::available}),
             SendResult::queued);
    CHECK_EQ(link.a->send(VisualActivityMessage{local, 1}), SendResult::queued);
    CHECK_EQ(link.a->send(smallFrame(local)), SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_b.size(), static_cast<std::size_t>(4));
    CHECK(std::holds_alternative<VisualEncodedFrame>(link.messages_b.back()));
    const VisualEncodedFrame frame_b = std::get<VisualEncodedFrame>(link.messages_b.back());
    CHECK_EQ(frame_b.ref.generation, static_cast<SessionGeneration>(20));
    CHECK_EQ(frame_b.ref.initiator, VisualInitiator::peer);
    CHECK_EQ(frame_b.sequence, static_cast<std::uint64_t>(1));

    CHECK_EQ(link.b->send(VisualStopMessage{offer_b.ref}), SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_a.size(), static_cast<std::size_t>(2));
    CHECK(std::holds_alternative<VisualStopMessage>(link.messages_a.back()));
    // La operación cerrada ya no admite más datos.
    CHECK_EQ(link.a->send(VisualActivityMessage{local, 1}), SendResult::denied);
}

TEST_CASE(notices_are_authorized_per_modality) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    RemoteNotification manual;
    manual.ref = NotificationRef{link.a->generation(), 1};
    manual.mode = NotificationMode::manual;
    manual.content = NotificationContent{"Hola", "Mensaje"};
    CHECK_EQ(link.a->send(manual), SendResult::queued);

    RemoteNotification forwarded;
    forwarded.ref = NotificationRef{link.b->generation(), 2};
    forwarded.mode = NotificationMode::forwarded;
    forwarded.content = NotificationContent{"Correo", "Nuevo"};
    forwarded.app_name = "mail";
    CHECK_EQ(link.b->send(forwarded), SendResult::queued);
    link.settle();

    CHECK_EQ(link.messages_b.size(), static_cast<std::size_t>(1));
    CHECK_EQ(link.messages_a.size(), static_cast<std::size_t>(1));
    CHECK_EQ(std::get<RemoteNotification>(link.messages_a.front()).ref.generation,
             static_cast<SessionGeneration>(10));
    CHECK_EQ(std::get<RemoteNotification>(link.messages_a.front()).ref.notice_id,
             static_cast<std::uint64_t>(2));
}

TEST_CASE(send_and_receive_permissions_are_independent) {
    SessionLinkOptions options_a = optionsFor("A", "B", "bb", 10);
    options_a.granted_send.share_monitor = false;
    Link link(options_a, optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    CHECK_EQ(link.a->send(VisualShareOffer{refFor(link, 1), VisualSourceKind::monitor}),
             SendResult::denied);
    // La modalidad de ventana sigue habilitada.
    CHECK_EQ(link.a->send(VisualShareOffer{refFor(link, 2), VisualSourceKind::window}),
             SendResult::queued);

    SessionLinkOptions options_b = optionsFor("B", "A", "aa", 20);
    options_b.granted_receive.share_monitor = false;
    Link receive_link(optionsFor("A", "B", "bb", 10), options_b);
    receive_link.startBoth();
    receive_link.settle();
    CHECK_EQ(receive_link.a->send(
                 VisualShareOffer{refFor(receive_link, 1), VisualSourceKind::monitor}),
             SendResult::queued);
    receive_link.settle();
    CHECK(receive_link.messages_b.empty());
}

TEST_CASE(video_requires_an_accepted_operation) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    const VisualShareRef local = refFor(link, 1);
    CHECK_EQ(link.a->send(VisualShareOffer{local, VisualSourceKind::monitor}),
             SendResult::queued);
    // Antes de aceptar no se admite el canal de datos.
    CHECK_EQ(link.a->send(smallFrame(local)), SendResult::denied);

    link.settle();
    const VisualShareOffer offer_b = std::get<VisualShareOffer>(link.messages_b.front());
    CHECK_EQ(link.b->send(VisualDecisionMessage{offer_b.ref, VisualDecision::accepted}),
             SendResult::queued);
    link.settle();
    CHECK_EQ(link.a->send(smallFrame(local)), SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_b.size(), static_cast<std::size_t>(2));
}

TEST_CASE(data_frame_on_interactive_channel_is_rejected) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    VisualEncodedFrame frame = smallFrame(VisualShareRef{10, VisualInitiator::local, 1});
    std::vector<WireFrame> wire = encodeFeatureMessage(
        frame, GenerationPair{10, 20});
    CHECK_EQ(wire.size(), static_cast<std::size_t>(1));
    link.transport_b.inject(SessionChannelKind::interactive, wire.front().bytes);
    link.settle();

    CHECK_EQ(link.b->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_b.front(), SessionLinkError::protocol);
}

TEST_CASE(wrong_wire_generation_terminates_session) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    VisualStopMessage stop{VisualShareRef{999, VisualInitiator::local, 1}};
    std::vector<WireFrame> wire = encodeFeatureMessage(stop, GenerationPair{999, 20});
    CHECK_EQ(wire.size(), static_cast<std::size_t>(1));
    link.transport_b.inject(SessionChannelKind::interactive, wire.front().bytes);
    link.settle();

    CHECK_EQ(link.b->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.errors_b.front(), SessionLinkError::protocol);
}

TEST_CASE(profile_limits_deny_new_and_active_operations) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    CHECK_EQ(link.a->send(VisualShareOffer{refFor(link, 1), VisualSourceKind::monitor}),
             SendResult::queued);
    link.settle();
    const VisualShareOffer offer_b = std::get<VisualShareOffer>(link.messages_b.front());
    CHECK_EQ(link.b->send(VisualDecisionMessage{offer_b.ref, VisualDecision::accepted}),
             SendResult::queued);
    link.settle();

    link.b->applyProfileLimits(FeatureSet{});
    // B ya no acepta datos de la operación anterior aunque lleguen de A.
    CHECK_EQ(link.a->send(smallFrame(refFor(link, 1))), SendResult::queued);
    link.settle();
    CHECK_EQ(link.messages_b.size(), static_cast<std::size_t>(1));

    link.a->applyProfileLimits(FeatureSet{});
    CHECK_EQ(link.a->send(VisualShareOffer{refFor(link, 2), VisualSourceKind::monitor}),
             SendResult::denied);
}

TEST_CASE(interactive_budget_returns_busy_and_recovers) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    int queued = 0;
    int busy = 0;
    for (int index = 0; index < 40; ++index) {
        RemoteNotification notice;
        notice.ref = NotificationRef{link.a->generation(), static_cast<std::uint64_t>(index + 1)};
        notice.mode = NotificationMode::manual;
        notice.content = NotificationContent{"Aviso", std::string(2000, 'x')};
        const SendResult result = link.a->send(notice);
        if (result == SendResult::queued) {
            ++queued;
        } else if (result == SendResult::busy) {
            ++busy;
        }
    }
    CHECK(busy > 0);
    CHECK(queued > 0);
    CHECK(queued < 40);

    *link.runtime_a.clock += std::chrono::seconds(2);
    RemoteNotification after;
    after.ref = NotificationRef{link.a->generation(), 100};
    after.mode = NotificationMode::manual;
    after.content = NotificationContent{"Aviso", "corto"};
    CHECK_EQ(link.a->send(after), SendResult::queued);
}

TEST_CASE(close_is_idempotent_and_disconnects) {
    Link link(optionsFor("A", "B", "bb", 10), optionsFor("B", "A", "aa", 20));
    link.startBoth();
    link.settle();

    link.a->close();
    CHECK_EQ(link.closed_a, 1);
    link.a->close();
    CHECK_EQ(link.closed_a, 1);
    CHECK_EQ(link.a->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.a->send(VisualStopMessage{VisualShareRef{10, VisualInitiator::local, 1}}),
             SendResult::disconnected);
    link.settle();
    CHECK_EQ(link.b->phase(), SessionLinkPhase::closed);
    CHECK_EQ(link.closed_b, 1);
}

int main() {
    return oneintwo_test::run();
}
