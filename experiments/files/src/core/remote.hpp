// Liability: minimum offer and transfer protocol between two teams.
// The server only responds for identifiers included in its current offer.
#pragma once

#include "core/model.hpp"
#include "core/store.hpp"
#include "core/transfer.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace o2f {

class Stream;

constexpr std::uint16_t kDefaultPort = 47654;

class RemoteLink {
public:
    ~RemoteLink();

    // Connect with TLS and give the protocol greeting.
    static std::unique_ptr<RemoteLink> connect(const std::string& host, std::uint16_t port,
                                               const std::string& pin,
                                               const std::string& clientCertFile,
                                               const std::string& clientKeyFile,
                                               const std::string& mode, int timeoutMs,
                                               const Limits& limits);
    // Attach an already established flow (only self-tests or already encrypted transport).
    static std::unique_ptr<RemoteLink> attach(std::unique_ptr<Stream> stream,
                                              const std::string& mode, const Limits& limits);

    const Offer& offer() const { return offer_; }
    void requestManifest();
    void fetchFile(const Entry& entry, StagedFile& stage, CancelToken& cancel,
                   const ProgressFn& progress);
    std::size_t readAt(const Entry& entry, std::uint64_t offset, void* buffer, std::size_t len);
    void sayGoodbye();

private:
    RemoteLink(std::unique_ptr<Stream> stream, const Limits& limits);
    void handshake(const std::string& mode);

    std::unique_ptr<Stream> stream_;
    Limits limits_;
    Offer offer_;
    std::mutex mutex_;
};

struct ServerOptions {
    std::string bindAddress = "0.0.0.0";
    std::uint16_t port = kDefaultPort;
    std::string certFile;
    std::string keyFile;
    std::string clientPin;
    int readTimeoutMs = 15000;
};

// It serves the offer to successive customers until the wait is cancelled.
void serveOffer(const OfferStore& store, const ServerOptions& options, CancelToken& cancel,
                const std::function<void(const std::string&)>& note);

// A session on an already authenticated flow. Exposed for isolated tests.
void serveSession(const OfferStore& store, Stream& stream,
                  const std::function<void(const std::string&)>& note);

}  // namespace o2f
