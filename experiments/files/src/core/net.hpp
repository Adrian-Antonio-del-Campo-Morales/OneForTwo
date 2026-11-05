// Responsabilidad: transporte TCP mínimo y envoltura TLS con verificación por
// huella SHA-256. La red solo se usa en modos manuales explícitos.
#pragma once

#include "core/stream.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace o2f {

bool tlsAvailable();
std::string tlsBackend();
// Huella SHA-256 (hex) del certificado PEM indicado, para compararla fuera de banda.
std::string certificateFingerprint(const std::string& pemPath);

class TcpStream : public Stream {
public:
    ~TcpStream() override;
    std::size_t readSome(void* buffer, std::size_t len) override;
    void writeAll(const void* buffer, std::size_t len) override;
    void close() override;
    // Limita la espera de lectura para que un par mudo no bloquee el proceso.
    void setReadTimeoutMs(int ms);
    std::intptr_t nativeHandle() const { return handle_; }

private:
    friend class TcpListener;
    friend std::unique_ptr<TcpStream> tcpConnect(const std::string&, std::uint16_t, int);
    explicit TcpStream(std::intptr_t handle) : handle_(handle) {}
    std::intptr_t handle_ = -1;
};

class TcpListener {
public:
    ~TcpListener();
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    static std::unique_ptr<TcpListener> bind(const std::string& address, std::uint16_t port,
                                             std::string& err);
    std::uint16_t port() const { return port_; }
    // Devuelve nullptr si expira el plazo sin conexiones.
    std::unique_ptr<TcpStream> accept(int timeoutMs);
    void close();

private:
    TcpListener() = default;
    std::intptr_t handle_ = -1;
    std::uint16_t port_ = 0;
};

std::unique_ptr<TcpStream> tcpConnect(const std::string& host, std::uint16_t port, int timeoutMs);

// TLS mutuo: el servidor exige y fija la huella del certificado de cliente; el
// cliente fija la del servidor y presenta su propio certificado.
std::unique_ptr<Stream> wrapTlsServer(std::unique_ptr<TcpStream> raw, const std::string& certFile,
                                      const std::string& keyFile,
                                      const std::string& expectedClientPinHex);
std::unique_ptr<Stream> wrapTlsClient(std::unique_ptr<TcpStream> raw,
                                      const std::string& expectedSha256Hex,
                                      const std::string& clientCertFile,
                                      const std::string& clientKeyFile);

}  // namespace o2f
