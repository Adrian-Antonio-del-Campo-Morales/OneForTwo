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
    // Huella verificada del certificado del par. Nunca se acepta un par sin
    // identidad fijada.
    std::string peer_fingerprint;

    // Permisos concedidos por el par; los límites del perfil se aplican aparte.
    FeatureSet granted_send;
    FeatureSet granted_receive;
    FeatureSet profile_limits;
    bool has_profile_limits = false;

    // Debe devolver una generación nueva y creciente por conexión. Si se omite,
    // se usa un contador interno del proceso.
    std::function<SessionGeneration()> allocate_generation;
    // Última generación conocida del par, para rechazar repeticiones tras
    // reconectar. La persistencia entre reinicios pertenece al almacenamiento.
    std::function<SessionGeneration(const std::string&)> last_peer_generation;
    std::function<void(const std::string&, SessionGeneration)> remember_peer_generation;
};

struct SessionLinkHandlers {
    std::function<void(SessionGeneration local, SessionGeneration peer)> established;
    std::function<void(SessionLinkError error, std::string detail)> failed;
    std::function<void()> closed;
    std::function<void(FeatureMessage)> message;
};

// Sesión autenticada sobre un transporte ya cifrado. Realiza el intercambio de
// identidad y generaciones, traduce ambas generaciones y autoriza cada mensaje
// por sentido y modalidad. El canal de datos solo se acepta dentro de esta
// sesión establecida y de una operación aceptada.
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
