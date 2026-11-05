#pragma once
// Responsabilidad: contrato del canal fiable de control y apertura de canales
// TLS con certificados verificados por huella SHA-256.

#include "core/types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oit {

class Link {
 public:
  virtual ~Link() = default;
  virtual bool send(const Message& message, std::string& error) = 0;

  // Espera como máximo timeoutMs. Devuelve true sin mensajes si no hay datos.
  virtual bool receive(std::vector<Message>& out, int timeoutMs, std::string& error) = 0;
  virtual void close() = 0;
};

bool netInit(std::string& error);
void netShutdown();

std::unique_ptr<Link> openTlsClient(const std::string& host, uint16_t port,
                                    const std::string& certFile, const std::string& keyFile,
                                    const std::string& peerFingerprint, std::string& error);

// shouldStop permite cancelar la espera de conexión desde Ctrl+C.
std::unique_ptr<Link> openTlsServer(uint16_t port, const std::string& certFile,
                                    const std::string& keyFile,
                                    const std::string& peerFingerprint,
                                    bool (*shouldStop)(), std::string& error);

}  // namespace oit
