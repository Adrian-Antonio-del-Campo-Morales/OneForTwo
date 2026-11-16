#pragma once

// Responsibility: transport channel between two instances, encrypted with TLS
// and authenticated by setting the torque certificate. The implementation uses OpenSSL
// And it's out of this header not to impose it on the core of the experiment.

#include <cstdint>
#include <memory>
#include <string>

namespace oit {

struct TlsFiles {
  std::string cert_file;
  std::string key_file;
  std::string peer_cert_file;
};

struct LocalTlsInfo {
  std::string cert_fingerprint;
  std::string backend;
};

// Calculates the footprint SHA-256 of the local certificate to be compared by hand.
bool describe_local_certificate(const TlsFiles& files, LocalTlsInfo& out,
                                std::string& error);

class TlsChannel {
 public:
  TlsChannel();
  ~TlsChannel();
  TlsChannel(const TlsChannel&) = delete;
  TlsChannel& operator=(const TlsChannel&) = delete;

  bool listen_on(const std::string& bind_address, std::uint16_t port,
                 const TlsFiles& files, std::string& error);
  bool accept_next(std::string& error);
  bool connect_to(const std::string& host, std::uint16_t port,
                  const TlsFiles& files, std::string& error);

  bool write_all(const std::uint8_t* data, std::size_t size,
                 std::string& error);
  bool read_all(std::uint8_t* data, std::size_t size, std::string& error);

  // Unlock a pending reading without releasing the SSL object.
  void interrupt();
  // Closes the current connection and releases SSL; retains the listener if it is a server.
  void shutdown_peer();
  std::uint16_t local_port() const;
  std::string peer_fingerprint() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace oit
