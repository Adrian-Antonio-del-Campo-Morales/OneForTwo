#pragma once

#include "../integration/feature_contracts.hpp"
#include "../net/transport.hpp"

#include <functional>
#include <memory>
#include <string>

namespace oneintwo {

// Configuración de producto que aporta el integrador. create_transport debe
// devolver un transporte ya conectado y autenticado (por ejemplo TLS con la
// huella fijada); sin él la UI arranca en modo sin sesión.
struct ApplicationOptions {
    std::string local_device_id;
    std::string peer_device_id;
    std::string peer_name = "Par";
    std::string peer_fingerprint;
    std::string storage_path;
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
