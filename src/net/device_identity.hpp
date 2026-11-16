#pragma once

#include <optional>
#include <string>

namespace oneintwo {

// Local identity TLS. The private key never leaves this object except for
// Hand it over to QtSslSocket when opening the channel.
struct DeviceIdentity {
    std::string device_id;
    std::string certificate_pem;
    std::string private_key_pem;
    std::string fingerprint;
};

// Crea/lee identity.id, identity.crt e identity.key en un directorio privado.
// The generation uses linked libcrypto; it does not depend on an external binary.
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

    // Keeps device_id and replaces par cert/clave. The caller must
    // update the confidence of the pair (repair) with the new footprint.
    std::optional<Rotation> rotate(std::string* error = nullptr);

private:
    std::optional<DeviceIdentity> create(std::string* error) const;
    std::optional<DeviceIdentity> generate(const std::string& device_id,
                                           std::string* error) const;

    std::string directory_;
};

}  // namespace oneintwo
