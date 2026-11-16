#include "session_link.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace oneintwo {
namespace {

constexpr std::array<std::uint8_t, 4> handshake_magic{'O', '2', 'S', '1'};
constexpr std::uint16_t handshake_version = 1;
constexpr std::size_t max_device_id_bytes = 128;
constexpr std::size_t max_handshake_bytes = 4 + 2 + 1 + 4 + max_device_id_bytes + 8;
constexpr std::size_t interactive_budget_bytes = 64 * 1024;
constexpr auto interactive_budget_window = std::chrono::seconds{1};

enum class HandshakeType : std::uint8_t {
    hello = 1,
    accept = 2,
};

struct Handshake {
    HandshakeType type = HandshakeType::hello;
    std::string device_id;
    SessionGeneration generation = 0;
};

void putU8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void putU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

void putU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

std::vector<std::uint8_t> encodeHandshake(const Handshake& handshake) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(4 + 2 + 1 + 4 + handshake.device_id.size() + 8);
    bytes.insert(bytes.end(), handshake_magic.begin(), handshake_magic.end());
    putU16(bytes, handshake_version);
    putU8(bytes, static_cast<std::uint8_t>(handshake.type));
    putU32(bytes, static_cast<std::uint32_t>(handshake.device_id.size()));
    bytes.insert(bytes.end(), handshake.device_id.begin(), handshake.device_id.end());
    putU64(bytes, handshake.generation);
    return bytes;
}

bool isHandshakeFrame(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= handshake_magic.size() &&
           std::equal(handshake_magic.begin(), handshake_magic.end(), bytes.begin());
}

bool decodeHandshake(const std::vector<std::uint8_t>& bytes, Handshake& out) {
    if (!isHandshakeFrame(bytes) || bytes.size() < 4 + 2 + 1 + 4 + 8) {
        return false;
    }
    std::size_t index = 4;
    const auto read16 = [&](std::uint16_t& value) {
        if (index + 2 > bytes.size()) {
            return false;
        }
        value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[index]) << 8) |
                                           bytes[index + 1]);
        index += 2;
        return true;
    };
    const auto read32 = [&](std::uint32_t& value) {
        if (index + 4 > bytes.size()) {
            return false;
        }
        value = (static_cast<std::uint32_t>(bytes[index]) << 24) |
                (static_cast<std::uint32_t>(bytes[index + 1]) << 16) |
                (static_cast<std::uint32_t>(bytes[index + 2]) << 8) |
                bytes[index + 3];
        index += 4;
        return true;
    };
    const auto read64 = [&](std::uint64_t& value) {
        if (index + 8 > bytes.size()) {
            return false;
        }
        value = 0;
        for (int offset = 0; offset < 8; ++offset) {
            value = (value << 8) | bytes[index + static_cast<std::size_t>(offset)];
        }
        index += 8;
        return true;
    };

    std::uint16_t version = 0;
    std::uint8_t raw_type = 0;
    std::uint32_t device_length = 0;
    if (!read16(version) || version != handshake_version || index >= bytes.size()) {
        return false;
    }
    raw_type = bytes[index++];
    if (raw_type != static_cast<std::uint8_t>(HandshakeType::hello) &&
        raw_type != static_cast<std::uint8_t>(HandshakeType::accept)) {
        return false;
    }
    if (!read32(device_length) || device_length == 0 || device_length > max_device_id_bytes ||
        index + device_length > bytes.size()) {
        return false;
    }
    out.type = static_cast<HandshakeType>(raw_type);
    out.device_id.assign(reinterpret_cast<const char*>(bytes.data() + index), device_length);
    index += device_length;
    if (!read64(out.generation) || index != bytes.size() || out.generation == 0) {
        return false;
    }
    return true;
}

std::string normalizeFingerprint(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        if (character == ':' || character == ' ' || character == '-') {
            continue;
        }
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

bool sameFingerprint(const std::string& left, const std::string& right) {
    const std::string normalized_left = normalizeFingerprint(left);
    const std::string normalized_right = normalizeFingerprint(right);
    return !normalized_left.empty() && normalized_left == normalized_right;
}

bool sendAllows(const FeatureSet& features, VisualSourceKind kind) {
    return kind == VisualSourceKind::monitor ? features.share_monitor : features.share_window;
}

bool receiveAllows(const FeatureSet& features, VisualSourceKind kind) {
    return kind == VisualSourceKind::monitor ? features.share_monitor : features.share_window;
}

struct OpKey {
    SessionGeneration generation = 0;
    VisualInitiator initiator = VisualInitiator::local;
    std::uint64_t offer_id = 0;

    bool operator<(const OpKey& other) const {
        if (generation != other.generation) {
            return generation < other.generation;
        }
        if (initiator != other.initiator) {
            return initiator < other.initiator;
        }
        return offer_id < other.offer_id;
    }
};

struct OpState {
    VisualSourceKind kind = VisualSourceKind::monitor;
    bool local_origin = false;
    bool accepted = false;
};

OpKey keyFor(const VisualShareRef& ref) {
    return OpKey{ref.generation, ref.initiator, ref.offer_id};
}

SessionGeneration defaultGeneration() {
    static std::atomic<SessionGeneration> next{0};
    const SessionGeneration value = next.fetch_add(1) + 1;
    return value == 0 ? next.fetch_add(1) + 1 : value;
}

} // namespace

struct AuthenticatedSession::Impl {
    struct Life {
        bool alive = true;
    };

    struct BudgetEntry {
        MonotonicTime at;
        std::size_t bytes = 0;
    };

    Impl(SessionLinkOptions options_in, SessionTransport& transport_in, ModuleRuntime runtime_in,
         SessionLinkHandlers handlers_in)
        : options(std::move(options_in)),
          transport(transport_in),
          runtime(std::move(runtime_in)),
          handlers(std::move(handlers_in)),
          life(std::make_shared<Life>()) {
        if (!options.has_profile_limits) {
            permissions.applyProfileLimits(
                FeatureSet{true, true, true, true, true, true, true, true, true});
        } else {
            permissions.applyProfileLimits(options.profile_limits);
        }
        permissions.setGrantedFeatures(options.granted_send, options.granted_receive);
    }

    ~Impl() {
        life->alive = false;
        transport.setReceiver({});
        transport.setClosedHandler({});
    }

    MonotonicTime now() const {
        return runtime.now ? runtime.now() : std::chrono::steady_clock::now();
    }

    void post(Task task) {
        if (!runtime.post) {
            task();
            return;
        }
        const std::shared_ptr<Life> token = life;
        runtime.post([token, task = std::move(task)] {
            if (token->alive) {
                task();
            }
        });
    }

    void start() {
        if (phase != SessionLinkPhase::idle) {
            return;
        }
        transport.setReceiver([this](SessionChannelKind channel, std::vector<std::uint8_t> bytes) {
            const std::shared_ptr<Life> token = life;
            post([token, this, channel, bytes = std::move(bytes)]() mutable {
                if (!token->alive) {
                    return;
                }
                handleFrame(channel, std::move(bytes));
            });
        });
        transport.setClosedHandler([this] {
            const std::shared_ptr<Life> token = life;
            post([token, this] {
                if (token->alive) {
                    transportClosed();
                }
            });
        });

        if (!sameFingerprint(transport.authenticated_peer_fingerprint(),
                             options.peer_fingerprint)) {
            fail(SessionLinkError::identity,
                 "la huella autenticada del par no coincide con la fijada");
            return;
        }
        local_generation = options.allocate_generation ? options.allocate_generation()
                                                       : defaultGeneration();
        if (local_generation == 0) {
            fail(SessionLinkError::generation, "generación local inválida");
            return;
        }
        if (options.local_device_id.empty() || options.local_device_id.size() > max_device_id_bytes ||
            options.peer_device_id.empty() || options.peer_device_id.size() > max_device_id_bytes) {
            fail(SessionLinkError::identity, "identidad local o remota inválida");
            return;
        }
        phase = SessionLinkPhase::negotiating;
        Handshake hello;
        hello.type = HandshakeType::hello;
        hello.device_id = options.local_device_id;
        hello.generation = local_generation;
        if (transport.send(SessionChannelKind::interactive, encodeHandshake(hello)) !=
            TransportSendStatus::sent) {
            fail(SessionLinkError::transport, "no se pudo enviar el saludo de sesión");
        }
    }

    void close() {
        if (phase == SessionLinkPhase::closed) {
            return;
        }
        phase = SessionLinkPhase::closed;
        transport.close();
        cleanup();
        notifyClosed();
    }

    void transportClosed() {
        if (phase == SessionLinkPhase::closed) {
            return;
        }
        phase = SessionLinkPhase::closed;
        cleanup();
        notifyClosed();
    }

    void cleanup() {
        ops.clear();
        decoder.reset();
        interactive_budget.clear();
    }

    void notifyClosed() {
        if (closed_notified) {
            return;
        }
        closed_notified = true;
        if (handlers.closed) {
            handlers.closed();
        }
    }

    void fail(SessionLinkError error, std::string detail) {
        if (phase == SessionLinkPhase::closed) {
            return;
        }
        phase = SessionLinkPhase::closed;
        cleanup();
        if (!failure_notified) {
            failure_notified = true;
            if (handlers.failed) {
                handlers.failed(error, std::move(detail));
            }
        }
        transport.close();
    }

    void handleFrame(SessionChannelKind channel, std::vector<std::uint8_t> bytes) {
        if (phase == SessionLinkPhase::closed) {
            return;
        }
        if (isHandshakeFrame(bytes)) {
            if (phase != SessionLinkPhase::negotiating &&
                phase != SessionLinkPhase::established) {
                fail(SessionLinkError::protocol, "negociación fuera de estado");
                return;
            }
            Handshake handshake;
            if (!decodeHandshake(bytes, handshake)) {
                fail(SessionLinkError::protocol, "saludo de sesión malformado");
                return;
            }
            handleHandshake(handshake);
            return;
        }
        if (phase != SessionLinkPhase::established) {
            fail(SessionLinkError::protocol, "mensaje de funciones antes de establecer sesión");
            return;
        }

        DecodeResult decoded = decoder.feed(bytes.data(), bytes.size());
        switch (decoded.status) {
        case ProtocolStatus::needs_more:
            return;
        case ProtocolStatus::ok:
            break;
        default:
            fail(SessionLinkError::protocol, "trama de funciones inválida");
            return;
        }
        const ProtocolChannel expected =
            channel == SessionChannelKind::data ? ProtocolChannel::data
                                                : ProtocolChannel::interactive;
        if (decoded.channel != expected) {
            fail(SessionLinkError::protocol, "canal de datos no asociado a la trama");
            return;
        }
        if (!decoded.message.has_value()) {
            return;
        }
        FeatureMessage message = std::move(*decoded.message);
        if (!acceptInbound(message)) {
            return;
        }
        if (handlers.message) {
            handlers.message(std::move(message));
        }
    }

    void handleHandshake(const Handshake& handshake) {
        if (handshake.type == HandshakeType::accept) {
            if (phase != SessionLinkPhase::established) {
                return;
            }
            if (handshake.device_id != options.peer_device_id ||
                handshake.generation != peer_generation) {
                fail(SessionLinkError::identity, "confirmación de identidad incoherente");
            }
            return;
        }

        if (handshake.device_id != options.peer_device_id) {
            fail(SessionLinkError::identity, "identidad remota no autorizada");
            return;
        }
        SessionGeneration minimum = last_peer_generation;
        if (options.last_peer_generation) {
            minimum = std::max(minimum, options.last_peer_generation(handshake.device_id));
        }
        if (handshake.generation <= minimum) {
            fail(SessionLinkError::generation, "generación remota repetida o antigua");
            return;
        }
        if (hello_seen && (peer_generation != handshake.generation ||
                           handshake.device_id != options.peer_device_id)) {
            fail(SessionLinkError::identity, "segundo saludo con identidad distinta");
            return;
        }

        Handshake accept;
        accept.type = HandshakeType::accept;
        accept.device_id = options.local_device_id;
        accept.generation = local_generation;
        if (transport.send(SessionChannelKind::interactive, encodeHandshake(accept)) !=
            TransportSendStatus::sent) {
            fail(SessionLinkError::transport, "no se pudo confirmar la sesión");
            return;
        }

        if (!hello_seen) {
            hello_seen = true;
            peer_generation = handshake.generation;
            last_peer_generation = peer_generation;
            if (options.remember_peer_generation) {
                options.remember_peer_generation(handshake.device_id, peer_generation);
            }
            decoder.setGenerations(GenerationPair{local_generation, peer_generation});
            phase = SessionLinkPhase::established;
            if (handlers.established) {
                handlers.established(local_generation, peer_generation);
            }
        }
    }

    bool acceptInbound(const FeatureMessage& message) {
        const FeatureSet send_features = permissions.effectiveSendFeatures();
        const FeatureSet receive_features = permissions.effectiveReceiveFeatures();

        if (const auto* offer = std::get_if<VisualShareOffer>(&message)) {
            if (offer->ref.initiator != VisualInitiator::peer) {
                return false;
            }
            if (offer->ref.offer_id <= last_peer_offer_id) {
                return false;
            }
            last_peer_offer_id = offer->ref.offer_id;
            if (!receiveAllows(receive_features, offer->source)) {
                return false;
            }
            OpState op;
            op.kind = offer->source;
            op.local_origin = false;
            op.accepted = false;
            ops[keyFor(offer->ref)] = op;
            return true;
        }
        if (const auto* decision = std::get_if<VisualDecisionMessage>(&message)) {
            const auto found = ops.find(keyFor(decision->ref));
            if (found == ops.end() || !found->second.local_origin || found->second.accepted) {
                return false;
            }
            if (decision->decision == VisualDecision::accepted) {
                if (!sendAllows(send_features, found->second.kind)) {
                    return false;
                }
                found->second.accepted = true;
            } else {
                ops.erase(found);
            }
            return true;
        }
        if (const auto* state = std::get_if<VisualStateMessage>(&message)) {
            const auto found = ops.find(keyFor(state->ref));
            if (found == ops.end() || found->second.local_origin || !found->second.accepted ||
                !receiveAllows(receive_features, found->second.kind)) {
                return false;
            }
            return true;
        }
        if (const auto* activity = std::get_if<VisualActivityMessage>(&message)) {
            const auto found = ops.find(keyFor(activity->ref));
            if (found == ops.end() || found->second.local_origin || !found->second.accepted ||
                !receiveAllows(receive_features, found->second.kind)) {
                return false;
            }
            return true;
        }
        if (const auto* frame = std::get_if<VisualEncodedFrame>(&message)) {
            const auto found = ops.find(keyFor(frame->ref));
            if (found == ops.end() || found->second.local_origin || !found->second.accepted ||
                !receiveAllows(receive_features, found->second.kind)) {
                return false;
            }
            return true;
        }
        if (const auto* stop = std::get_if<VisualStopMessage>(&message)) {
            const auto found = ops.find(keyFor(stop->ref));
            if (found == ops.end()) {
                return false;
            }
            ops.erase(found);
            return true;
        }
        if (const auto* notice = std::get_if<RemoteNotification>(&message)) {
            const bool allowed = notice->mode == NotificationMode::manual
                                     ? receive_features.manual_notifications
                                     : receive_features.forwarded_notifications;
            return allowed;
        }
        return false;
    }

    bool checkOutbound(const FeatureMessage& message, bool& commit_accepted,
                       std::optional<OpKey>& erase_key, std::optional<OpKey>& insert_key,
                       OpState& insert_state) {
        const FeatureSet send_features = permissions.effectiveSendFeatures();
        const FeatureSet receive_features = permissions.effectiveReceiveFeatures();

        if (const auto* offer = std::get_if<VisualShareOffer>(&message)) {
            if (offer->ref.initiator != VisualInitiator::local ||
                offer->ref.generation != local_generation || offer->ref.offer_id == 0 ||
                !sendAllows(send_features, offer->source)) {
                return false;
            }
            if (ops.find(keyFor(offer->ref)) != ops.end()) {
                return false;
            }
            insert_key = keyFor(offer->ref);
            insert_state.kind = offer->source;
            insert_state.local_origin = true;
            insert_state.accepted = false;
            return true;
        }
        if (const auto* decision = std::get_if<VisualDecisionMessage>(&message)) {
            const auto found = ops.find(keyFor(decision->ref));
            if (found == ops.end() || found->second.local_origin || found->second.accepted) {
                return false;
            }
            if (decision->decision == VisualDecision::accepted &&
                !receiveAllows(receive_features, found->second.kind)) {
                return false;
            }
            if (decision->decision == VisualDecision::accepted) {
                commit_accepted = true;
            } else {
                erase_key = keyFor(decision->ref);
            }
            return true;
        }
        if (const auto* state = std::get_if<VisualStateMessage>(&message)) {
            const auto found = ops.find(keyFor(state->ref));
            return found != ops.end() && found->second.local_origin &&
                   found->second.accepted && sendAllows(send_features, found->second.kind);
        }
        if (const auto* activity = std::get_if<VisualActivityMessage>(&message)) {
            const auto found = ops.find(keyFor(activity->ref));
            return found != ops.end() && found->second.local_origin &&
                   found->second.accepted && sendAllows(send_features, found->second.kind);
        }
        if (const auto* frame = std::get_if<VisualEncodedFrame>(&message)) {
            const auto found = ops.find(keyFor(frame->ref));
            return found != ops.end() && found->second.local_origin &&
                   found->second.accepted && sendAllows(send_features, found->second.kind);
        }
        if (const auto* stop = std::get_if<VisualStopMessage>(&message)) {
            const auto found = ops.find(keyFor(stop->ref));
            if (found == ops.end()) {
                return false;
            }
            erase_key = keyFor(stop->ref);
            return true;
        }
        if (const auto* notice = std::get_if<RemoteNotification>(&message)) {
            return notice->mode == NotificationMode::manual
                       ? send_features.manual_notifications
                       : send_features.forwarded_notifications;
        }
        return false;
    }

    bool withinInteractiveBudget(std::size_t bytes) {
        const MonotonicTime current = now();
        while (!interactive_budget.empty() &&
               current - interactive_budget.front().at >= interactive_budget_window) {
            interactive_budget.pop_front();
        }
        std::size_t used = 0;
        for (const BudgetEntry& entry : interactive_budget) {
            used += entry.bytes;
        }
        if (used + bytes > interactive_budget_bytes) {
            return false;
        }
        interactive_budget.push_back(BudgetEntry{current, bytes});
        return true;
    }

    SendResult send(FeatureMessage message) {
        if (phase != SessionLinkPhase::established) {
            return SendResult::disconnected;
        }
        bool commit_accepted = false;
        std::optional<OpKey> erase_key;
        std::optional<OpKey> insert_key;
        OpState insert_state;
        if (!checkOutbound(message, commit_accepted, erase_key, insert_key, insert_state)) {
            return SendResult::denied;
        }

        std::vector<WireFrame> frames;
        const ProtocolStatus encoded = encodeFeatureMessage(
            message, GenerationPair{local_generation, peer_generation}, frames);
        if (encoded != ProtocolStatus::ok || frames.empty()) {
            return SendResult::denied;
        }

        std::size_t interactive_bytes = 0;
        for (const WireFrame& frame : frames) {
            if (frame.channel == ProtocolChannel::interactive) {
                interactive_bytes += frame.bytes.size();
            }
        }
        if (interactive_bytes != 0 && !withinInteractiveBudget(interactive_bytes)) {
            return SendResult::busy;
        }

        for (const WireFrame& frame : frames) {
            const TransportSendStatus status =
                transport.send(frame.channel == ProtocolChannel::data
                                   ? SessionChannelKind::data
                                   : SessionChannelKind::interactive,
                               frame.bytes);
            if (status != TransportSendStatus::sent) {
                return status == TransportSendStatus::busy ? SendResult::busy
                                                           : SendResult::disconnected;
            }
        }

        if (erase_key.has_value()) {
            ops.erase(*erase_key);
        }
        if (insert_key.has_value()) {
            ops.emplace(*insert_key, insert_state);
        }
        if (commit_accepted) {
            if (const auto* decision = std::get_if<VisualDecisionMessage>(&message)) {
                const auto found = ops.find(keyFor(decision->ref));
                if (found != ops.end()) {
                    found->second.accepted = true;
                }
            }
        }
        return SendResult::queued;
    }

    void setGrantedFeatures(FeatureSet send_features, FeatureSet receive_features) {
        permissions.setGrantedFeatures(send_features, receive_features);
        purgeUnauthorizedOperations();
    }

    void applyProfileLimits(FeatureSet limits) {
        permissions.applyProfileLimits(limits);
        purgeUnauthorizedOperations();
    }

    void purgeUnauthorizedOperations() {
        const FeatureSet send_features = permissions.effectiveSendFeatures();
        const FeatureSet receive_features = permissions.effectiveReceiveFeatures();
        for (auto entry = ops.begin(); entry != ops.end();) {
            const bool allowed =
                entry->second.local_origin
                    ? sendAllows(send_features, entry->second.kind)
                    : receiveAllows(receive_features, entry->second.kind);
            if (!allowed) {
                entry = ops.erase(entry);
            } else {
                ++entry;
            }
        }
    }

    SessionLinkOptions options;
    SessionTransport& transport;
    ModuleRuntime runtime;
    SessionLinkHandlers handlers;
    std::shared_ptr<Life> life;
    Session permissions;
    SessionLinkPhase phase = SessionLinkPhase::idle;
    SessionGeneration local_generation = 0;
    SessionGeneration peer_generation = 0;
    SessionGeneration last_peer_generation = 0;
    bool hello_seen = false;
    bool closed_notified = false;
    bool failure_notified = false;
    std::uint64_t last_peer_offer_id = 0;
    FeatureFrameDecoder decoder{{0, 0}};
    std::map<OpKey, OpState> ops;
    std::deque<BudgetEntry> interactive_budget;
};

AuthenticatedSession::AuthenticatedSession(SessionLinkOptions options,
                                           SessionTransport& transport, ModuleRuntime runtime,
                                           SessionLinkHandlers handlers)
    : impl_(std::make_unique<Impl>(std::move(options), transport, std::move(runtime),
                                   std::move(handlers))) {}

AuthenticatedSession::~AuthenticatedSession() = default;

void AuthenticatedSession::start() {
    impl_->start();
}

void AuthenticatedSession::close() {
    impl_->close();
}

SendResult AuthenticatedSession::send(FeatureMessage message) {
    return impl_->send(std::move(message));
}

void AuthenticatedSession::setGrantedFeatures(FeatureSet send, FeatureSet receive) {
    impl_->setGrantedFeatures(send, receive);
}

void AuthenticatedSession::applyProfileLimits(FeatureSet limits) {
    impl_->applyProfileLimits(limits);
}

SessionLinkPhase AuthenticatedSession::phase() const {
    return impl_->phase;
}

SessionGeneration AuthenticatedSession::generation() const {
    return impl_->local_generation;
}

SessionGeneration AuthenticatedSession::peerGeneration() const {
    return impl_->peer_generation;
}

FeatureSet AuthenticatedSession::effectiveSendFeatures() const {
    return impl_->permissions.effectiveSendFeatures();
}

FeatureSet AuthenticatedSession::effectiveReceiveFeatures() const {
    return impl_->permissions.effectiveReceiveFeatures();
}

} // namespace oneintwo
