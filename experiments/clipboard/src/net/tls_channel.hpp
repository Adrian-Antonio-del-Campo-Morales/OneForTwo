#pragma once

// Responsabilidad: canal de transporte entre dos instancias, cifrado con TLS
// y autenticado fijando el certificado del par. La implementación usa OpenSSL
// y queda fuera de este encabezado para no imponerla al núcleo del experimento.

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

// Calcula la huella SHA-256 del certificado local para compararla a mano.
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

  // Desbloquea una lectura pendiente sin liberar el objeto SSL.
  void interrupt();
  // Cierra la conexión actual y libera SSL; conserva el listener si es servidor.
  void shutdown_peer();
  std::uint16_t local_port() const;
  std::string peer_fingerprint() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace oit
