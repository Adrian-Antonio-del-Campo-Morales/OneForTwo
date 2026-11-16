// Liability: transport TCP minimum and envelope TLS with verification by
// SHA-256. The network is only used in explicit manual modes.
#pragma once

#include "core/stream.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace o2f {

bool tlsAvailable();
std::string tlsBackend();
// Print SHA-256 (hex) of the PEM certificate indicated, to compare it out of band.
std::string certificateFingerprint(const std::string& pemPath);

class TcpStream : public Stream {
public:
    ~TcpStream() override;
    std::size_t readSome(void* buffer, std::size_t len) override;
    void writeAll(const void* buffer, std::size_t len) override;
    void close() override;
    // Limits the waiting time for a mute pair not to block the process.
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
    // Returns nullptr if the deadline with no connections expires.
    std::unique_ptr<TcpStream> accept(int timeoutMs);
    void close();

private:
    TcpListener() = default;
    std::intptr_t handle_ = -1;
    std::uint16_t port_ = 0;
};

std::unique_ptr<TcpStream> tcpConnect(const std::string& host, std::uint16_t port, int timeoutMs);

// TLS mutual: the server requires and fixes the footprint of the client certificate; the
// client fixes the server's and presents its own certificate.
std::unique_ptr<Stream> wrapTlsServer(std::unique_ptr<TcpStream> raw, const std::string& certFile,
                                      const std::string& keyFile,
                                      const std::string& expectedClientPinHex);
std::unique_ptr<Stream> wrapTlsClient(std::unique_ptr<TcpStream> raw,
                                      const std::string& expectedSha256Hex,
                                      const std::string& clientCertFile,
                                      const std::string& clientKeyFile);

}  // namespace o2f
