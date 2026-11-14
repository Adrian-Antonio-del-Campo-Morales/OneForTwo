#include "storage/pairing_store.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace oneintwo {
namespace {

namespace fs = std::filesystem;

constexpr const char* kHeader = "oneintwo-pairing-v1";

std::string sanitize(std::string value) {
    for (char& character : value) {
        if (character == '\t' || character == '\n' || character == '\r') {
            character = ' ';
        }
    }
    while (!value.empty() && value.front() == ' ') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    return value;
}

bool safeId(const std::string& value) {
    if (value.empty() || value.size() > 128) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_' ||
               character == '.';
    });
}

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = line.find(separator, start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

bool parsePort(const std::string& text, std::uint16_t* port) {
    if (text.empty()) {
        return false;
    }
    unsigned int value = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() ||
        result.ptr != text.data() + text.size() || value > 65535) {
        return false;
    }
    *port = static_cast<std::uint16_t>(value);
    return true;
}

}  // namespace

bool TrustedDevice::trusts(const std::string& fingerprint) const {
    const std::string normalized = PairingStore::normalizePin(fingerprint);
    if (normalized.empty()) {
        return false;
    }
    return std::any_of(pins.begin(), pins.end(),
                       [&normalized](const std::string& pin) {
                           return pin == normalized;
                       });
}

PairingStore::PairingStore(std::string path) : path_(std::move(path)) {
    reload(&error_);
}

std::string PairingStore::normalizePin(const std::string& fingerprint) {
    std::string normalized;
    normalized.reserve(64);
    for (const unsigned char character : fingerprint) {
        if (std::isxdigit(character)) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        } else if (character == ':' || character == '-' || character == ' ') {
            continue;
        } else {
            return {};
        }
    }
    return normalized.size() == 64 ? normalized : std::string();
}

bool PairingStore::isValidPin(const std::string& fingerprint) {
    return !normalizePin(fingerprint).empty();
}

std::optional<TrustedDevice> PairingStore::find(
    const std::string& device_id) const {
    const auto iterator =
        std::find_if(devices_.begin(), devices_.end(),
                     [&device_id](const TrustedDevice& device) {
                         return device.device_id == device_id;
                     });
    if (iterator == devices_.end()) {
        return std::nullopt;
    }
    return *iterator;
}

bool PairingStore::reload(std::string* error) {
    error_.clear();
    devices_.clear();
    const fs::path path(path_);
    std::error_code filesystem_error;
    if (!fs::exists(path, filesystem_error)) {
        if (filesystem_error) {
            if (error != nullptr) {
                *error = "no se pudo consultar el almacén de pares: " +
                         filesystem_error.message();
            }
            return false;
        }
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        if (error != nullptr) {
            *error = "no se pudo leer el almacén de pares";
        }
        return false;
    }
    std::string line;
    if (!std::getline(input, line) || line != kHeader) {
        if (error != nullptr) {
            *error = "el almacén de pares no tiene el formato esperado";
        }
        return false;
    }
    std::vector<TrustedDevice> parsed;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split(line, '\t');
        if (fields.size() < 5 || !safeId(fields[0]) || fields[1].empty() ||
            !isValidPin(fields[4])) {
            if (error != nullptr) {
                *error = "el almacén de pares contiene una entrada no válida";
            }
            return false;
        }
        TrustedDevice device;
        device.device_id = fields[0];
        device.name = sanitize(fields[1]);
        device.host = sanitize(fields[2]);
        if (!parsePort(fields[3], &device.port)) {
            if (error != nullptr) {
                *error = "el almacén de pares contiene un puerto no válido";
            }
            return false;
        }
        for (std::size_t index = 4; index < fields.size(); ++index) {
            const std::string pin = normalizePin(fields[index]);
            if (pin.empty()) {
                if (error != nullptr) {
                    *error =
                        "el almacén de pares contiene una huella no válida";
                }
                return false;
            }
            if (std::find(device.pins.begin(), device.pins.end(), pin) ==
                device.pins.end()) {
                device.pins.push_back(pin);
            }
        }
        if (device.pins.empty() || device.pins.size() > 2) {
            if (error != nullptr) {
                *error = "el almacén de pares contiene demasiadas huellas";
            }
            return false;
        }
        if (std::any_of(parsed.begin(), parsed.end(),
                        [&device](const TrustedDevice& other) {
                            return other.device_id == device.device_id;
                        })) {
            if (error != nullptr) {
                *error = "el almacén de pares contiene un dispositivo duplicado";
            }
            return false;
        }
        parsed.push_back(std::move(device));
    }
    devices_ = std::move(parsed);
    return true;
}

bool PairingStore::save(const TrustedDevice& device, std::string* error) {
    if (!safeId(device.device_id)) {
        if (error != nullptr) {
            *error = "el identificador del dispositivo no es válido";
        }
        return false;
    }
    TrustedDevice cleaned;
    cleaned.device_id = device.device_id;
    cleaned.name = sanitize(device.name);
    if (cleaned.name.empty()) {
        cleaned.name = cleaned.device_id;
    }
    cleaned.host = sanitize(device.host);
    cleaned.port = device.port;
    for (const std::string& pin : device.pins) {
        const std::string normalized = normalizePin(pin);
        if (normalized.empty()) {
            if (error != nullptr) {
                *error = "la huella del dispositivo no es un SHA-256 válido";
            }
            return false;
        }
        if (std::find(cleaned.pins.begin(), cleaned.pins.end(), normalized) ==
            cleaned.pins.end()) {
            cleaned.pins.push_back(normalized);
        }
    }
    if (cleaned.pins.empty() || cleaned.pins.size() > 2) {
        if (error != nullptr) {
            *error = "un par debe tener una o dos huellas como máximo";
        }
        return false;
    }

    const auto existing =
        std::find_if(devices_.begin(), devices_.end(),
                     [&cleaned](const TrustedDevice& candidate) {
                         return candidate.device_id == cleaned.device_id;
                     });
    if (existing == devices_.end()) {
        devices_.push_back(cleaned);
    } else {
        *existing = cleaned;
    }
    return writeFile(error);
}

bool PairingStore::remove(const std::string& device_id, std::string* error) {
    const auto iterator =
        std::find_if(devices_.begin(), devices_.end(),
                     [&device_id](const TrustedDevice& device) {
                         return device.device_id == device_id;
                     });
    if (iterator == devices_.end()) {
        return true;
    }
    devices_.erase(iterator);
    return writeFile(error);
}

bool PairingStore::pinTrusted(const std::string& device_id,
                              const std::string& fingerprint) const {
    const auto device = find(device_id);
    return device.has_value() && device->trusts(fingerprint);
}

bool PairingStore::rotatePin(const std::string& device_id,
                             const std::string& new_fingerprint,
                             std::string* error) {
    auto device = find(device_id);
    if (!device.has_value()) {
        if (error != nullptr) {
            *error = "el dispositivo no está emparejado";
        }
        return false;
    }
    const std::string normalized = normalizePin(new_fingerprint);
    if (normalized.empty()) {
        if (error != nullptr) {
            *error = "la huella nueva no es un SHA-256 válido";
        }
        return false;
    }
    device->pins.erase(
        std::remove(device->pins.begin(), device->pins.end(), normalized),
        device->pins.end());
    device->pins.insert(device->pins.begin(), normalized);
    if (device->pins.size() > 2) {
        device->pins.resize(2);
    }
    return save(*device, error);
}

bool PairingStore::writeFile(std::string* error) const {
    const fs::path path(path_);
    std::error_code filesystem_error;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), filesystem_error);
    }
    if (filesystem_error) {
        if (error != nullptr) {
            *error = "no se pudo crear el directorio del almacén de pares";
        }
        return false;
    }

    const fs::path temporary = path.string() + ".tmp";
    std::ostringstream content;
    content << kHeader << '\n';
    for (const TrustedDevice& device : devices_) {
        content << device.device_id << '\t' << sanitize(device.name) << '\t'
                << sanitize(device.host) << '\t' << device.port;
        for (const std::string& pin : device.pins) {
            content << '\t' << normalizePin(pin);
        }
        content << '\n';
    }

    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            if (error != nullptr) {
                *error = "no se pudo escribir el almacén de pares";
            }
            return false;
        }
        output << content.str();
        output.flush();
        if (!output) {
            if (error != nullptr) {
                *error = "no se pudo guardar el almacén de pares";
            }
            fs::remove(temporary, filesystem_error);
            return false;
        }
    }
    fs::permissions(temporary,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, filesystem_error);
    // En POSIX rename reemplaza de forma atómica. En Windows el destino debe
    // desaparecer antes; se conserva el archivo temporal si el reemplazo falla.
    fs::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        std::error_code ignored;
        fs::remove(path, ignored);
        filesystem_error.clear();
        fs::rename(temporary, path, filesystem_error);
    }
    if (filesystem_error) {
        if (error != nullptr) {
            *error = "no se pudo reemplazar el almacén de pares: " +
                     filesystem_error.message();
        }
        fs::remove(temporary, filesystem_error);
        return false;
    }
    return true;
}

}  // namespace oneintwo
