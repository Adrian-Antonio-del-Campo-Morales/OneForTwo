#pragma once

#include "transport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneintwo {

enum class TlsTransportMode {
    listen,
    connect,
};

// Certificados y claves en PEM. La huella debe ser el SHA-256 del certificado
// del par; se acepta con o sin separadores. El transporte no confía en la CA:
// la única identidad aceptada es la huella fijada.
struct TlsTransportConfig {
    TlsTransportMode mode = TlsTransportMode::connect;
    std::string certificate_pem;
    std::string private_key_pem;
    std::string peer_fingerprint;
    std::string bind_address = "127.0.0.1";
    std::uint16_t listen_port = 0;
    std::string peer_host = "127.0.0.1";
    std::uint16_t peer_port = 0;
};

// Transporte TLS asíncrono de un solo par. start() inicia el listener o la
// conexión; ready se invoca únicamente después de verificar la huella del
// certificado remoto. Un fallo de identidad nunca expone la huella.
class QtTlsTransport final : public SessionTransport {
public:
    explicit QtTlsTransport(TlsTransportConfig config);
    ~QtTlsTransport() override;
    QtTlsTransport(const QtTlsTransport&) = delete;
    QtTlsTransport& operator=(const QtTlsTransport&) = delete;

    bool start(std::string* error = nullptr);
    void setReadyHandler(std::function<void()> handler);
    void setFailureHandler(std::function<void(std::string)> handler);

    std::uint16_t listeningPort() const;
    bool encrypted() const;

    std::string authenticated_peer_fingerprint() const override;
    TransportSendStatus send(SessionChannelKind channel,
                             const std::vector<std::uint8_t>& frame) override;
    void setReceiver(TransportReceiver receiver) override;
    void setClosedHandler(TransportClosed handler) override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
