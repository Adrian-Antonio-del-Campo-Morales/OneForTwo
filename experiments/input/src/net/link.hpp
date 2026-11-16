#pragma once
// Liability: contract of reliable channel of control and opening of channels
// TLS with certificates verified by SHA-256 fingerprints.

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

  // Wait for maximum timeoutMs. Return true without messages if there is no data.
  virtual bool receive(std::vector<Message>& out, int timeoutMs, std::string& error) = 0;
  virtual void close() = 0;
};

bool netInit(std::string& error);
void netShutdown();

std::unique_ptr<Link> openTlsClient(const std::string& host, uint16_t port,
                                    const std::string& certFile, const std::string& keyFile,
                                    const std::string& peerFingerprint, std::string& error);

// shouldStop allows you to cancel the connection wait from Ctrl+C.
std::unique_ptr<Link> openTlsServer(uint16_t port, const std::string& certFile,
                                    const std::string& keyFile,
                                    const std::string& peerFingerprint,
                                    bool (*shouldStop)(), std::string& error);

}  // namespace oit
