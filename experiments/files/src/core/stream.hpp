// Responsabilidad: abstracción de flujo bidireccional. Permite probar el
// protocolo con un par en memoria sin abrir sockets, y reutilizar el mismo
// código sobre TCP/TLS en las pruebas manuales.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace o2f {

class Stream {
public:
    virtual ~Stream() = default;
    // Devuelve 0 al llegar al final; lanza std::runtime_error en error de E/S.
    virtual std::size_t readSome(void* buffer, std::size_t len) = 0;
    virtual void writeAll(const void* buffer, std::size_t len) = 0;
    virtual void close() = 0;
};

void readExact(Stream& stream, void* buffer, std::size_t len);

// Par de extremos conectados en memoria, solo para self-tests y pruebas locales.
struct LoopbackPair {
    std::unique_ptr<Stream> a;
    std::unique_ptr<Stream> b;
};
LoopbackPair makeLoopbackPair();

}  // namespace o2f
