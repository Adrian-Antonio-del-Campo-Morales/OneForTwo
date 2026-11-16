// Liability: SHA-256 portable to verify transfer integrity.
// The standard FIPS 180-4 algorithm is implemented to avoid relying on OpenSSL in
// isolated tests or on platforms without cryptographic library.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace o2f {

class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t len);
    // Finish the calculation and return 64 hexadecimal characters in lower case.
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
