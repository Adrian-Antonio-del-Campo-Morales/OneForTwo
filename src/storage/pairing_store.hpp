#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oneintwo {

// A reliable pair is identified by device_id, not by name or IP.
// `pins` maintains the active footprint first and, during a rotation, the previous one.
struct TrustedDevice {
    std::string device_id;
    std::string name;
    // empty host means this computer should listen in `port`; with host,
    // the application is connected to that end.
    std::string host;
    std::uint16_t port = 0;
    std::vector<std::string> pins;

    bool trusts(const std::string& fingerprint) const;
};

class PairingStore {
public:
    enum class PinRole {
        none,
        primary,
        fallback,
    };

    explicit PairingStore(std::string path);

    const std::string& path() const { return path_; }
    const std::string& lastError() const { return error_; }

    bool reload(std::string* error = nullptr);
    const std::vector<TrustedDevice>& devices() const { return devices_; }

    std::optional<TrustedDevice> find(const std::string& device_id) const;
    std::optional<std::string> primaryPin(const std::string& device_id) const;
    std::optional<std::string> fallbackPin(const std::string& device_id) const;
    PinRole pinRole(const std::string& device_id,
                    const std::string& fingerprint) const;

    bool save(const TrustedDevice& device, std::string* error = nullptr);
    bool remove(const std::string& device_id, std::string* error = nullptr);
    // Compatibility: accepts both the main footprint and the backrest.
    bool pinTrusted(const std::string& device_id,
                    const std::string& fingerprint) const;
    // For decisions that must only accept active identity.
    bool primaryPinTrusted(const std::string& device_id,
                           const std::string& fingerprint) const;

    // Remove the backup when an authenticated connection has already been established
    // It's idepotent if there's no backup.
    bool retireFallback(const std::string& device_id,
                        std::string* error = nullptr);

    // Rotation: the new footprint becomes active and the previous one is preserved
    // at most during transition. Never accumulate more than two.
    bool rotatePin(const std::string& device_id,
                   const std::string& new_fingerprint,
                   std::string* error = nullptr);

    // Option for a re-pair: also updates name, host and
    // port. `device.pins.front()` is the new footprint; it retains the previous ones
    // as high schools and writes the warehouse once.
    std::optional<TrustedDevice> rotatePin(const TrustedDevice& device,
                                           std::string* error = nullptr);

    static std::string normalizePin(const std::string& fingerprint);
    static bool isValidPin(const std::string& fingerprint);

private:
    bool writeFile(const std::vector<TrustedDevice>& devices,
                   std::string* error) const;
    bool commit(std::vector<TrustedDevice> devices, std::string* error);
    void setError(std::string message, std::string* error);

    std::string path_;
    std::string error_;
    std::vector<TrustedDevice> devices_;
};

}  // namespace oneintwo
