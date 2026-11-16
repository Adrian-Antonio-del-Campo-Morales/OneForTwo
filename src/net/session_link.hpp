#pragma once

#include "../core/session.hpp"
#include "../integration/feature_contracts.hpp"
#include "protocol.hpp"
#include "transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace oneintwo {

enum class SessionLinkPhase {
    idle,
    negotiating,
    established,
    closed,
};

enum class SessionLinkError {
    none,
    version,
    identity,
    generation,
    protocol,
    transport,
    permission,
};

struct SessionLinkOptions {
    std::string local_device_id;
    std::string peer_device_id;
    // Verified print of the pair certificate. A pair is never accepted without
    // identidad fijada.
    std::string peer_fingerprint;

    // Permissions granted by the pair; profile limits apply separately.
    FeatureSet granted_send;
    FeatureSet granted_receive;
    FeatureSet profile_limits;
    bool has_profile_limits = false;

    // You must return a new and growing generation by connection. If omitted,
    // an internal process counter is used.
    std::function<SessionGeneration()> allocate_generation;
    // Last known generation of the pair, to reject repetitions after
    // reconnect. Persistence between restarts belongs to storage.
    std::function<SessionGeneration(const std::string&)> last_peer_generation;
    std::function<void(const std::string&, SessionGeneration)> remember_peer_generation;
};

struct SessionLinkHandlers {
    std::function<void(SessionGeneration local, SessionGeneration peer)> established;
    std::function<void(SessionLinkError error, std::string detail)> failed;
    std::function<void()> closed;
    std::function<void(FeatureMessage)> message;
};

// Authenticated session on an already encrypted transport.
// identity and generations, translates both generations and authorizes each message
// by meaning and modality. The data channel is only accepted within this
// established session and an accepted operation.
class AuthenticatedSession {
public:
    AuthenticatedSession(SessionLinkOptions options, SessionTransport& transport,
                         ModuleRuntime runtime, SessionLinkHandlers handlers);
    ~AuthenticatedSession();
    AuthenticatedSession(const AuthenticatedSession&) = delete;
    AuthenticatedSession& operator=(const AuthenticatedSession&) = delete;

    void start();
    void close();

    SendResult send(FeatureMessage message);

    void setGrantedFeatures(FeatureSet send, FeatureSet receive);
    void applyProfileLimits(FeatureSet limits);

    SessionLinkPhase phase() const;
    SessionGeneration generation() const;
    SessionGeneration peerGeneration() const;
    FeatureSet effectiveSendFeatures() const;
    FeatureSet effectiveReceiveFeatures() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
