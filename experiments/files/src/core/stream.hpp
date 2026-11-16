// Responsibility: two-way flow abstraction.
// protocol with a pair in memory without opening sockets, and reuse it
// code about TCP/TLS in manual tests.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace o2f {

class Stream {
public:
    virtual ~Stream() = default;
    // Returns 0 at the end; launches std::runtime_error in E/S. error
    virtual std::size_t readSome(void* buffer, std::size_t len) = 0;
    virtual void writeAll(const void* buffer, std::size_t len) = 0;
    virtual void close() = 0;
};

void readExact(Stream& stream, void* buffer, std::size_t len);

// Pair of ends connected in memory, only for self-tests and local tests.
struct LoopbackPair {
    std::unique_ptr<Stream> a;
    std::unique_ptr<Stream> b;
};
LoopbackPair makeLoopbackPair();

}  // namespace o2f
