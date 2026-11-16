#pragma once

#include "../integration/feature_contracts.hpp"
#include "../net/tls_transport.hpp"
#include "../net/transport.hpp"

#include <functional>
#include <memory>
#include <string>

namespace oneintwo {

// Product configuration provided by the integrator. create_transport must
// return a transport already connected and authenticated; if omitted and there is
// certificado/clave TLS, the application starts QtTlsTransport and expects its
// handshake with fingerprint fixed before creating AuthenticatedSession. No
// transport the IU starts in sessionless mode.
struct ApplicationOptions {
    std::string local_device_id;
    std::string peer_device_id;
    std::string peer_name = "Par";
    std::string peer_fingerprint;
    std::string storage_path;
    // If left empty AppData/identity and AppData/paired-devices.tsv. are used
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

    // Build the window, adapters and coordinator, and start the
    // 100 ms clock. It's idepotent.
    void start();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace oneintwo
