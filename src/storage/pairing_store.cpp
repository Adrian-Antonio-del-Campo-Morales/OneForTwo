#include "storage/pairing_store.hpp"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

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

bool cleanDevice(const TrustedDevice& device, TrustedDevice* cleaned,
                 std::string* error) {
    if (!safeId(device.device_id)) {
        if (error != nullptr) {
            *error = "el identificador del dispositivo no es válido";
        }
        return false;
    }
    TrustedDevice result;
    result.device_id = device.device_id;
    result.name = sanitize(device.name);
    if (result.name.empty()) {
        result.name = result.device_id;
    }
    result.host = sanitize(device.host);
    result.port = device.port;
    for (const std::string& pin : device.pins) {
        const std::string normalized = PairingStore::normalizePin(pin);
        if (normalized.empty()) {
            if (error != nullptr) {
                *error = "la huella del dispositivo no es un SHA-256 válido";
            }
            return false;
        }
        if (std::find(result.pins.begin(), result.pins.end(), normalized) ==
            result.pins.end()) {
            result.pins.push_back(normalized);
        }
    }
    if (result.pins.empty() || result.pins.size() > 2) {
        if (error != nullptr) {
            *error = "un par debe tener una o dos huellas como máximo";
        }
        return false;
    }
    *cleaned = std::move(result);
    return true;
}

#ifndef _WIN32
void syncPath(const fs::path& path, bool directory) {
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    if (directory) {
        flags |= O_DIRECTORY;
    }
#endif
    const int descriptor = ::open(path.c_str(), flags);
    if (descriptor < 0) {
        return;
    }
    ::fsync(descriptor);
    ::close(descriptor);
}
#endif

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

std::optional<std::string> PairingStore::primaryPin(
    const std::string& device_id) const {
    const auto device = find(device_id);
    if (!device.has_value() || device->pins.empty()) {
        return std::nullopt;
    }
    return device->pins.front();
}

std::optional<std::string> PairingStore::fallbackPin(
    const std::string& device_id) const {
    const auto device = find(device_id);
    if (!device.has_value() || device->pins.size() < 2) {
        return std::nullopt;
    }
    return device->pins[1];
}

PairingStore::PinRole PairingStore::pinRole(
    const std::string& device_id, const std::string& fingerprint) const {
    const auto device = find(device_id);
    if (!device.has_value()) {
        return PinRole::none;
    }
    const std::string normalized = normalizePin(fingerprint);
    if (normalized.empty()) {
        return PinRole::none;
    }
    for (std::size_t index = 0; index < device->pins.size(); ++index) {
        if (device->pins[index] != normalized) {
            continue;
        }
        return index == 0 ? PinRole::primary : PinRole::fallback;
    }
    return PinRole::none;
}

bool PairingStore::reload(std::string* error) {
    error_.clear();
    if (error != nullptr) {
        error->clear();
    }
    const auto fail = [this, error](std::string message) {
        setError(std::move(message), error);
        return false;
    };

    const fs::path path(path_);
    std::error_code filesystem_error;
    if (!fs::exists(path, filesystem_error)) {
        if (filesystem_error) {
            return fail("no se pudo consultar el almacén de pares: " +
                        filesystem_error.message());
        }
        // La ausencia del archivo es un almacén vacío, no un error.
        devices_.clear();
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        return fail("no se pudo leer el almacén de pares");
    }
    std::string line;
    if (!std::getline(input, line) || line != kHeader) {
        return fail("el almacén de pares no tiene el formato esperado");
    }
    std::vector<TrustedDevice> parsed;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split(line, '\t');
        if (fields.size() < 5 || !safeId(fields[0]) || fields[1].empty() ||
            !isValidPin(fields[4])) {
            return fail("el almacén de pares contiene una entrada no válida");
        }
        TrustedDevice device;
        device.device_id = fields[0];
        device.name = sanitize(fields[1]);
        device.host = sanitize(fields[2]);
        if (!parsePort(fields[3], &device.port)) {
            return fail("el almacén de pares contiene un puerto no válido");
        }
        for (std::size_t index = 4; index < fields.size(); ++index) {
            const std::string pin = normalizePin(fields[index]);
            if (pin.empty()) {
                return fail(
                    "el almacén de pares contiene una huella no válida");
            }
            if (std::find(device.pins.begin(), device.pins.end(), pin) ==
                device.pins.end()) {
                device.pins.push_back(pin);
            }
        }
        if (device.pins.empty() || device.pins.size() > 2) {
            return fail("el almacén de pares contiene demasiadas huellas");
        }
        if (std::any_of(parsed.begin(), parsed.end(),
                        [&device](const TrustedDevice& other) {
                            return other.device_id == device.device_id;
                        })) {
            return fail("el almacén de pares contiene un dispositivo duplicado");
        }
        parsed.push_back(std::move(device));
    }
    // Solo se publica el estado nuevo cuando el archivo entero es válido.
    devices_ = std::move(parsed);
    return true;
}

bool PairingStore::save(const TrustedDevice& device, std::string* error) {
    TrustedDevice cleaned;
    std::string detail;
    if (!cleanDevice(device, &cleaned, &detail)) {
        setError(std::move(detail), error);
        return false;
    }

    std::vector<TrustedDevice> candidate = devices_;
    const auto existing =
        std::find_if(candidate.begin(), candidate.end(),
                     [&cleaned](const TrustedDevice& item) {
                         return item.device_id == cleaned.device_id;
                     });
    if (existing == candidate.end()) {
        candidate.push_back(cleaned);
    } else {
        *existing = cleaned;
    }
    return commit(std::move(candidate), error);
}

bool PairingStore::remove(const std::string& device_id, std::string* error) {
    std::vector<TrustedDevice> candidate = devices_;
    const auto iterator =
        std::find_if(candidate.begin(), candidate.end(),
                     [&device_id](const TrustedDevice& device) {
                         return device.device_id == device_id;
                     });
    if (iterator == candidate.end()) {
        error_.clear();
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    candidate.erase(iterator);
    return commit(std::move(candidate), error);
}

bool PairingStore::pinTrusted(const std::string& device_id,
                              const std::string& fingerprint) const {
    const auto device = find(device_id);
    return device.has_value() && device->trusts(fingerprint);
}

bool PairingStore::primaryPinTrusted(const std::string& device_id,
                                     const std::string& fingerprint) const {
    return pinRole(device_id, fingerprint) == PinRole::primary;
}

bool PairingStore::retireFallback(const std::string& device_id,
                                  std::string* error) {
    const auto existing = find(device_id);
    if (!existing.has_value()) {
        setError("el dispositivo no está emparejado", error);
        return false;
    }
    if (existing->pins.size() < 2) {
        error_.clear();
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    std::vector<TrustedDevice> candidate = devices_;
    const auto iterator =
        std::find_if(candidate.begin(), candidate.end(),
                     [&device_id](const TrustedDevice& device) {
                         return device.device_id == device_id;
                     });
    if (iterator == candidate.end()) {
        setError("el dispositivo no está emparejado", error);
        return false;
    }
    iterator->pins.resize(1);
    return commit(std::move(candidate), error);
}

bool PairingStore::rotatePin(const std::string& device_id,
                             const std::string& new_fingerprint,
                             std::string* error) {
    const auto current = find(device_id);
    if (!current.has_value()) {
        setError("el dispositivo no está emparejado", error);
        return false;
    }
    TrustedDevice updated = *current;
    updated.pins = {new_fingerprint};
    return rotatePin(updated, error).has_value();
}

std::optional<TrustedDevice> PairingStore::rotatePin(
    const TrustedDevice& device, std::string* error) {
    const auto existing = find(device.device_id);
    if (!existing.has_value()) {
        setError("el dispositivo no está emparejado", error);
        return std::nullopt;
    }
    if (device.pins.empty()) {
        setError("la huella nueva no es un SHA-256 válido", error);
        return std::nullopt;
    }
    const std::string new_pin = normalizePin(device.pins.front());
    if (new_pin.empty()) {
        setError("la huella nueva no es un SHA-256 válido", error);
        return std::nullopt;
    }

    TrustedDevice candidate = device;
    candidate.pins.clear();
    candidate.pins.push_back(new_pin);
    for (const std::string& pin : existing->pins) {
        const std::string normalized = normalizePin(pin);
        if (!normalized.empty() && normalized != new_pin &&
            std::find(candidate.pins.begin(), candidate.pins.end(),
                      normalized) == candidate.pins.end()) {
            candidate.pins.push_back(normalized);
        }
    }
    if (candidate.pins.size() > 2) {
        candidate.pins.resize(2);
    }

    TrustedDevice cleaned;
    std::string detail;
    if (!cleanDevice(candidate, &cleaned, &detail)) {
        setError(std::move(detail), error);
        return std::nullopt;
    }

    std::vector<TrustedDevice> next = devices_;
    const auto iterator =
        std::find_if(next.begin(), next.end(),
                     [&cleaned](const TrustedDevice& item) {
                         return item.device_id == cleaned.device_id;
                     });
    if (iterator == next.end()) {
        setError("el dispositivo no está emparejado", error);
        return std::nullopt;
    }
    *iterator = cleaned;
    if (!commit(std::move(next), error)) {
        return std::nullopt;
    }
    return cleaned;
}

bool PairingStore::commit(std::vector<TrustedDevice> devices,
                          std::string* error) {
    std::string detail;
    if (!writeFile(devices, &detail)) {
        setError(std::move(detail), error);
        return false;
    }
    devices_ = std::move(devices);
    error_.clear();
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool PairingStore::writeFile(const std::vector<TrustedDevice>& devices,
                             std::string* error) const {
    const fs::path path(path_);
    std::error_code filesystem_error;
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, filesystem_error);
    }
    if (filesystem_error) {
        if (error != nullptr) {
            *error = "no se pudo crear el directorio del almacén de pares";
        }
        return false;
    }

    std::ostringstream content;
    content << kHeader << '\n';
    for (const TrustedDevice& device : devices) {
        content << device.device_id << '\t' << sanitize(device.name) << '\t'
                << sanitize(device.host) << '\t' << device.port;
        for (const std::string& pin : device.pins) {
            content << '\t' << normalizePin(pin);
        }
        content << '\n';
    }

    // QSaveFile usa un temporal único y un reemplazo atómico, también en
    // Windows. Si algo falla, el archivo anterior permanece intacto.
    QSaveFile output(QString::fromStdString(path.string()));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = "no se pudo escribir el almacén de pares: " +
                     output.errorString().toStdString();
        }
        return false;
    }
    const QByteArray bytes = QByteArray::fromStdString(content.str());
    if (output.write(bytes) != static_cast<qint64>(bytes.size())) {
        output.cancelWriting();
        if (error != nullptr) {
            *error = "no se pudo guardar el almacén de pares";
        }
        return false;
    }
    if (!output.commit()) {
        if (error != nullptr) {
            *error = "no se pudo reemplazar el almacén de pares: " +
                     output.errorString().toStdString();
        }
        return false;
    }

    // El contenido ya está reemplazado; los permisos y el fsync son la última
    // barrera de integridad y durabilidad. Los errores de fsync no se ignoran
    // para la lógica de memoria, pero no pueden revertir el reemplazo.
    std::error_code permissions_error;
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, permissions_error);
#ifndef _WIN32
    syncPath(path, false);
    syncPath(parent.empty() ? fs::path(".") : parent, true);
#endif
    return true;
}

void PairingStore::setError(std::string message, std::string* error) {
    error_ = std::move(message);
    if (error != nullptr) {
        *error = error_;
    }
}

}  // namespace oneintwo
