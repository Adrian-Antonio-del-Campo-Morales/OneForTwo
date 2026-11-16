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

// Certificates and keys in PEM. The print must be SHA-256 of the certificate
// The transport does not rely on the AC:
// The only accepted identity is the fixed footprint.
struct TlsTransportConfig {
    TlsTransportMode mode = TlsTransportMode::connect;
    std::string certificate_pem;
    std::string private_key_pem;
    std::string peer_fingerprint;
    // Only for temporary matching channel: allows handshake and
    // exposes the track observed, but does not authenticate until confirming SAS.
    bool allow_unverified = false;
    std::string bind_address = "127.0.0.1";
    std::uint16_t listen_port = 0;
    std::string peer_host = "127.0.0.1";
    std::uint16_t peer_port = 0;
};

// Transport TLS asynchronous single pair. start() starts the listener or the
// connection; ready is invoked only after checking the fingerprint of the
// Remote certificate. An identity failure never exposes the print.
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
