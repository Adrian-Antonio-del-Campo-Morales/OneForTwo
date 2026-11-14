#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oneintwo {

// Un par confiable se identifica por device_id, no por nombre ni por la IP.
// `pins` mantiene la huella activa primero y, durante una rotación, la anterior.
struct TrustedDevice {
    std::string device_id;
    std::string name;
    // host vacío significa que este equipo debe escuchar en `port`; con host,
    // la aplicación se conecta a ese extremo.
    std::string host;
    std::uint16_t port = 0;
    std::vector<std::string> pins;

    bool trusts(const std::string& fingerprint) const;
};

class PairingStore {
public:
    explicit PairingStore(std::string path);

    const std::string& path() const { return path_; }
    const std::string& lastError() const { return error_; }

    bool reload(std::string* error = nullptr);
    const std::vector<TrustedDevice>& devices() const { return devices_; }

    std::optional<TrustedDevice> find(const std::string& device_id) const;
    bool save(const TrustedDevice& device, std::string* error = nullptr);
    bool remove(const std::string& device_id, std::string* error = nullptr);
    bool pinTrusted(const std::string& device_id,
                    const std::string& fingerprint) const;

    // Rotación: la huella nueva pasa a ser la activa y la anterior se conserva
    // como máximo durante la transición. Nunca se acumulan más de dos.
    bool rotatePin(const std::string& device_id,
                   const std::string& new_fingerprint,
                   std::string* error = nullptr);

    static std::string normalizePin(const std::string& fingerprint);
    static bool isValidPin(const std::string& fingerprint);

private:
    bool writeFile(std::string* error) const;

    std::string path_;
    std::string error_;
    std::vector<TrustedDevice> devices_;
};

}  // namespace oneintwo
