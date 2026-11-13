#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oneintwo {

enum class SessionChannelKind {
    interactive,
    data,
};

enum class TransportSendStatus {
    sent,
    busy,
    closed,
};

using TransportReceiver = std::function<void(SessionChannelKind, std::vector<std::uint8_t>)>;
using TransportClosed = std::function<void()>;

// Transporte ya conectado y autenticado. La implementación TLS (u otro canal
// seguro mantenido) verifica el certificado del par antes de exponerlo aquí;
// authenticated_peer_fingerprint devuelve vacío mientras no exista esa prueba.
class SessionTransport {
public:
    virtual ~SessionTransport() = default;

    virtual std::string authenticated_peer_fingerprint() const = 0;
    virtual TransportSendStatus send(SessionChannelKind channel,
                                     const std::vector<std::uint8_t>& frame) = 0;
    virtual void setReceiver(TransportReceiver receiver) = 0;
    virtual void setClosedHandler(TransportClosed handler) = 0;
    virtual void close() = 0;
};

} // namespace oneintwo
