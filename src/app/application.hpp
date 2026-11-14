#pragma once

#include "../integration/feature_contracts.hpp"
#include "../net/tls_transport.hpp"
#include "../net/transport.hpp"

#include <functional>
#include <memory>
#include <string>

namespace oneintwo {

// Configuración de producto que aporta el integrador. create_transport debe
// devolver un transporte ya conectado y autenticado; si se omite y hay
// certificado/clave TLS, la aplicación inicia QtTlsTransport y espera su
// handshake con la huella fijada antes de crear AuthenticatedSession. Sin
// transporte la UI arranca en modo sin sesión.
struct ApplicationOptions {
    std::string local_device_id;
    std::string peer_device_id;
    std::string peer_name = "Par";
    std::string peer_fingerprint;
    std::string storage_path;
    // Si se dejan vacíos se usan AppData/identity y AppData/paired-devices.tsv.
    std::string identity_directory;
    std::string pairing_store_path;
    TlsTransportConfig tls;
    std::function<std::unique_ptr<SessionTransport>()> create_transport;
};

class Application {
public:
    explicit Application(ApplicationOptions options = {});
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Construye la ventana, los adaptadores y el coordinador, y arranca el
    // reloj de 100 ms. Es idempotente.
    void start();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
