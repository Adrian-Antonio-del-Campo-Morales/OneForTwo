#include "net/device_identity.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace oneintwo {
namespace {

namespace fs = std::filesystem;

using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyContextPtr =
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

std::string opensslError() {
    const unsigned long code = ERR_get_error();
    if (code == 0) {
        return "libcrypto no devolvió un error";
    }
    char buffer[256] = {};
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return buffer;
}

void setOpenSslError(std::string* error, const char* prefix) {
    if (error != nullptr) {
        *error = std::string(prefix) + ": " + opensslError();
    }
}

std::vector<unsigned char> randomBytes(std::size_t count,
                                       std::string* error) {
    std::vector<unsigned char> bytes(count);
    if (count == 0 ||
        RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        setOpenSslError(error, "no se pudo obtener aleatoriedad de libcrypto");
        return {};
    }
    return bytes;
}

std::string randomHex(std::size_t bytes, std::string* error) {
    static constexpr char kHex[] = "0123456789abcdef";
    const std::vector<unsigned char> random = randomBytes(bytes, error);
    if (random.empty()) {
        return {};
    }
    std::string result;
    result.reserve(bytes * 2);
    for (const unsigned char value : random) {
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

std::string bioContents(BIO* bio) {
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    if (size <= 0 || data == nullptr) {
        return {};
    }
    return std::string(data, static_cast<std::size_t>(size));
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

DeviceIdentityStore::DeviceIdentityStore(std::string directory)
    : directory_(std::move(directory)) {}

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

    const std::string device_id = randomHex(16, error);
    if (device_id.empty()) {
        return std::nullopt;
    }
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

    PkeyContextPtr context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                           &EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 3072) <= 0) {
        setOpenSslError(error, "no se pudo preparar la clave RSA");
        return std::nullopt;
    }
    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_keygen(context.get(), &raw_key) <= 0 || raw_key == nullptr) {
        setOpenSslError(error, "no se pudo generar la clave RSA");
        return std::nullopt;
    }
    PkeyPtr key(raw_key, &EVP_PKEY_free);

    X509Ptr certificate(X509_new(), &X509_free);
    if (!certificate) {
        setOpenSslError(error, "no se pudo crear el certificado TLS");
        return std::nullopt;
    }
    if (X509_set_version(certificate.get(), 2) != 1) {
        setOpenSslError(error, "no se pudo versionar el certificado TLS");
        return std::nullopt;
    }
    const std::vector<unsigned char> serial_bytes = randomBytes(4, error);
    if (serial_bytes.empty()) {
        return std::nullopt;
    }
    const std::uint32_t serial =
        ((static_cast<std::uint32_t>(serial_bytes[0]) << 24) |
         (static_cast<std::uint32_t>(serial_bytes[1]) << 16) |
         (static_cast<std::uint32_t>(serial_bytes[2]) << 8) |
         static_cast<std::uint32_t>(serial_bytes[3])) &
        0x7fffffffu;
    if (ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()),
                         static_cast<long>(serial == 0 ? 1 : serial)) != 1) {
        setOpenSslError(error, "no se pudo numerar el certificado TLS");
        return std::nullopt;
    }
    if (X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) ==
            nullptr ||
        X509_gmtime_adj(X509_getm_notAfter(certificate.get()),
                        3650L * 24L * 60L * 60L) == nullptr) {
        setOpenSslError(error, "no se pudo fechar el certificado TLS");
        return std::nullopt;
    }
    if (X509_set_pubkey(certificate.get(), key.get()) != 1) {
        setOpenSslError(error, "no se pudo asociar la clave al certificado");
        return std::nullopt;
    }
    X509_NAME* subject = X509_get_subject_name(certificate.get());
    const std::string common_name = "OneInTwo-" + device_id;
    if (subject == nullptr ||
        X509_NAME_add_entry_by_txt(
            subject, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>(common_name.c_str()), -1,
            -1, 0) != 1 ||
        X509_set_issuer_name(certificate.get(), subject) != 1) {
        setOpenSslError(error, "no se pudo nombrar el certificado TLS");
        return std::nullopt;
    }
    if (X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
        setOpenSslError(error, "no se pudo firmar el certificado TLS");
        return std::nullopt;
    }

    BioPtr certificate_bio(BIO_new(BIO_s_mem()), &BIO_free);
    BioPtr key_bio(BIO_new(BIO_s_mem()), &BIO_free);
    if (!certificate_bio || !key_bio ||
        PEM_write_bio_X509(certificate_bio.get(), certificate.get()) != 1 ||
        PEM_write_bio_PrivateKey(key_bio.get(), key.get(), nullptr, nullptr, 0,
                                 nullptr, nullptr) != 1) {
        setOpenSslError(error, "no se pudo serializar la identidad TLS");
        return std::nullopt;
    }
    const std::string certificate_pem = bioContents(certificate_bio.get());
    const std::string key_pem = bioContents(key_bio.get());
    if (certificate_pem.empty() || key_pem.empty()) {
        if (error != nullptr) {
            *error = "libcrypto generó una identidad TLS vacía";
        }
        return std::nullopt;
    }
    auto identity = parseIdentity(device_id, certificate_pem, key_pem, error);
    if (!identity.has_value()) {
        return std::nullopt;
    }

    if (!writeFile(root / "identity.key",
                   QByteArray::fromStdString(key_pem), error) ||
        !writeFile(root / "identity.crt",
                   QByteArray::fromStdString(certificate_pem), error)) {
        return std::nullopt;
    }
    std::error_code ignored;
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
