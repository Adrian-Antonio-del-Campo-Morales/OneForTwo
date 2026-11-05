// Responsabilidad: SHA-256 portable para verificar integridad de transferencias.
// Se implementa el algoritmo estándar FIPS 180-4 para no depender de OpenSSL en
// las pruebas aisladas ni en plataformas sin biblioteca criptográfica.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace o2f {

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    // Finaliza el cálculo y devuelve 64 caracteres hexadecimales en minúsculas.
    std::string hexDigest();

private:
    void transform(const std::uint8_t* block);

    std::uint32_t state_[8];
    std::uint64_t bitCount_;
    std::uint8_t buffer_[64];
    std::size_t bufferLen_;
};

std::string sha256Hex(const void* data, std::size_t len);

}  // namespace o2f
