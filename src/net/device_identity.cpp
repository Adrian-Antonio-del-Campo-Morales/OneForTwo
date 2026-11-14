#include "net/device_identity.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <random>
#include <system_error>

namespace oneintwo {
namespace {

namespace fs = std::filesystem;

std::string randomHex(std::size_t bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::random_device device;
    std::uniform_int_distribution<int> distribution(0, 255);
    std::string result;
    result.reserve(bytes * 2);
    for (std::size_t index = 0; index < bytes; ++index) {
        const int value = distribution(device);
        result.push_back(kHex[(value >> 4) & 0x0f]);
        result.push_back(kHex[value & 0x0f]);
    }
    return result;
}

bool isDeviceId(const std::string& value) {
    return value.size() == 32 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) ||
                      (character >= 'a' && character <= 'f');
           });
}

std::string readFile(const fs::path& path, std::string* error) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = "no se pudo leer " + path.string();
        }
        return {};
    }
    return file.readAll().toStdString();
}

bool writeFile(const fs::path& path, const QByteArray& data,
               std::string* error) {
    QSaveFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = "no se pudo escribir " + path.string();
        }
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error != nullptr) {
            *error = "no se pudo guardar " + path.string();
        }
        return false;
    }
    return true;
}

std::string certFingerprint(const QSslCertificate& certificate) {
    return certificate.digest(QCryptographicHash::Sha256).toHex().toStdString();
}

std::optional<DeviceIdentity> parseIdentity(const std::string& device_id,
                                            const std::string& certificate_pem,
                                            const std::string& private_key_pem,
                                            std::string* error) {
    if (!isDeviceId(device_id) || certificate_pem.empty() ||
        private_key_pem.empty()) {
        if (error != nullptr) {
            *error = "la identidad TLS guardada está incompleta";
        }
        return std::nullopt;
    }
    const QSslCertificate certificate(
        QByteArray::fromStdString(certificate_pem), QSsl::Pem);
    const QSslKey key(QByteArray::fromStdString(private_key_pem), QSsl::Rsa,
                      QSsl::Pem, QSsl::PrivateKey);
    if (certificate.isNull() || key.isNull()) {
        if (error != nullptr) {
            *error = "el certificado o la clave TLS guardados no son válidos";
        }
        return std::nullopt;
    }
    const std::string fingerprint = certFingerprint(certificate);
    if (fingerprint.size() != 64) {
        if (error != nullptr) {
            *error = "el certificado TLS no tiene una huella SHA-256 válida";
        }
        return std::nullopt;
    }
    return DeviceIdentity{device_id, certificate_pem, private_key_pem,
                          fingerprint};
}

}  // namespace

DeviceIdentityStore::DeviceIdentityStore(std::string directory,
                                         std::string openssl_program)
    : directory_(std::move(directory)),
      openssl_program_(std::move(openssl_program)) {}

std::optional<DeviceIdentity> DeviceIdentityStore::load(
    std::string* error) const {
    const fs::path root(directory_);
    const std::string id = readFile(root / "identity.id", error);
    if (id.empty()) {
        if (error != nullptr && error->empty()) {
            *error = "no hay identidad TLS guardada";
        }
        return std::nullopt;
    }
    const std::string certificate =
        readFile(root / "identity.crt", error);
    const std::string key = readFile(root / "identity.key", error);
    return parseIdentity(id, certificate, key, error);
}

std::optional<DeviceIdentity> DeviceIdentityStore::create(
    std::string* error) const {
    std::error_code filesystem_error;
    const fs::path root(directory_);
    fs::create_directories(root, filesystem_error);
    if (filesystem_error) {
        if (error != nullptr) {
            *error = "no se pudo crear el directorio de identidad: " +
                     filesystem_error.message();
        }
        return std::nullopt;
    }
    fs::permissions(root, fs::perms::owner_all, fs::perm_options::replace,
                    filesystem_error);

    const std::string device_id = randomHex(16);
    if (!writeFile(root / "identity.id",
                   QByteArray::fromStdString(device_id), error)) {
        return std::nullopt;
    }
    auto identity = generate(device_id, error);
    if (!identity.has_value()) {
        std::error_code ignored;
        fs::remove(root / "identity.id", ignored);
    }
    return identity;
}

std::optional<DeviceIdentity> DeviceIdentityStore::generate(
    const std::string& device_id, std::string* error) const {
    const fs::path root(directory_);
    const fs::path temporary_key =
        root / ("identity-" + randomHex(6) + ".key.tmp");
    const fs::path temporary_certificate =
        root / ("identity-" + randomHex(6) + ".crt.tmp");

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QString::fromStdString(openssl_program_),
                  QStringList{
                      QStringLiteral("req"),
                      QStringLiteral("-x509"),
                      QStringLiteral("-newkey"),
                      QStringLiteral("rsa:3072"),
                      QStringLiteral("-nodes"),
                      QStringLiteral("-sha256"),
                      QStringLiteral("-days"),
                      QStringLiteral("3650"),
                      QStringLiteral("-keyout"),
                      QString::fromStdString(temporary_key.string()),
                      QStringLiteral("-out"),
                      QString::fromStdString(temporary_certificate.string()),
                      QStringLiteral("-subj"),
                      QString::fromStdString("/CN=OneInTwo-" + device_id),
                  });
    if (!process.waitForStarted(5000)) {
        if (error != nullptr) {
            *error =
                "no se pudo iniciar openssl; instala OpenSSL para generar la "
                "identidad del equipo";
        }
        return std::nullopt;
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(2000);
        std::error_code ignored;
        fs::remove(temporary_key, ignored);
        fs::remove(temporary_certificate, ignored);
        if (error != nullptr) {
            *error = "openssl no terminó de generar la identidad del equipo";
        }
        return std::nullopt;
    }
    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        const std::string detail =
            process.readAllStandardError().toStdString();
        std::error_code ignored;
        fs::remove(temporary_key, ignored);
        fs::remove(temporary_certificate, ignored);
        if (error != nullptr) {
            *error = "openssl no pudo generar la identidad del equipo" +
                     (detail.empty() ? std::string() : ": " + detail);
        }
        return std::nullopt;
    }

    const std::string key = readFile(temporary_key, error);
    const std::string certificate = readFile(temporary_certificate, error);
    std::error_code ignored;
    fs::remove(temporary_key, ignored);
    fs::remove(temporary_certificate, ignored);
    if (key.empty() || certificate.empty()) {
        if (error != nullptr) {
            *error = "openssl generó una identidad TLS vacía";
        }
        return std::nullopt;
    }
    auto identity = parseIdentity(device_id, certificate, key, error);
    if (!identity.has_value()) {
        return std::nullopt;
    }

    if (!writeFile(root / "identity.key", QByteArray::fromStdString(key),
                   error) ||
        !writeFile(root / "identity.crt",
                   QByteArray::fromStdString(certificate), error)) {
        return std::nullopt;
    }
    fs::permissions(root / "identity.key",
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, ignored);
    return identity;
}

std::optional<DeviceIdentity> DeviceIdentityStore::loadOrCreate(
    std::string* error) const {
    const fs::path root(directory_);
    const bool has_certificate = fs::exists(root / "identity.crt");
    const bool has_key = fs::exists(root / "identity.key");
    if (has_certificate && has_key) {
        return load(error);
    }
    if (has_certificate || has_key) {
        if (error != nullptr) {
            *error = "la identidad TLS guardada está incompleta";
        }
        return std::nullopt;
    }
    return create(error);
}

std::optional<DeviceIdentityStore::Rotation> DeviceIdentityStore::rotate(
    std::string* error) {
    auto current = load(error);
    if (!current.has_value()) {
        return std::nullopt;
    }
    auto identity = generate(current->device_id, error);
    if (!identity.has_value()) {
        return std::nullopt;
    }
    return Rotation{std::move(*identity), current->fingerprint};
}

}  // namespace oneintwo
