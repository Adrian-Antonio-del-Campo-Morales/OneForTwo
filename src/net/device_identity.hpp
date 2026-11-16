#pragma once

#include <optional>
#include <string>

namespace oneintwo {

// Identidad TLS local. La clave privada nunca sale de este objeto salvo para
// entregársela a QtSslSocket al abrir el canal.
struct DeviceIdentity {
    std::string device_id;
    std::string certificate_pem;
    std::string private_key_pem;
    std::string fingerprint;
};

// Crea/lee identity.id, identity.crt e identity.key en un directorio privado.
// La generación usa libcrypto enlazado; no depende de un binario externo.
class DeviceIdentityStore {
public:
    struct Rotation {
        DeviceIdentity identity;
        std::string previous_fingerprint;
    };

    explicit DeviceIdentityStore(std::string directory);

    const std::string& directory() const { return directory_; }

    std::optional<DeviceIdentity> load(std::string* error = nullptr) const;
    std::optional<DeviceIdentity> loadOrCreate(std::string* error = nullptr) const;

    // Conserva device_id y reemplaza el par cert/clave. El llamador debe
    // actualizar la confianza del par (volver a emparejar) con la huella nueva.
    std::optional<Rotation> rotate(std::string* error = nullptr);

private:
    std::optional<DeviceIdentity> create(std::string* error) const;
    std::optional<DeviceIdentity> generate(const std::string& device_id,
                                           std::string* error) const;

    std::string directory_;
};

}  // namespace oneintwo
