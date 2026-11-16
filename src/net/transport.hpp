#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oneintwo {

enum class SessionChannelKind {
    interactive,
    data,
};

enum class TransportSendStatus {
    sent,
    busy,
    closed,
};

using TransportReceiver = std::function<void(SessionChannelKind, std::vector<std::uint8_t>)>;
using TransportClosed = std::function<void()>;

// Transport already connected and authenticated. TLS implementation (or other channel
// insurance) checks the certificate of the pair before presenting it here;
// authenticated_peer_fingerprint returns empty as long as that test does not exist.
// Only the temporary pairing channel (allow_unverified) exposes the footprint
// observed before SAS authenticates it.
class SessionTransport {
public:
    virtual ~SessionTransport() = default;

    virtual std::string authenticated_peer_fingerprint() const = 0;
    virtual TransportSendStatus send(SessionChannelKind channel,
                                     const std::vector<std::uint8_t>& frame) = 0;
    virtual void setReceiver(TransportReceiver receiver) = 0;
    virtual void setClosedHandler(TransportClosed handler) = 0;
    virtual void close() = 0;
};

} // namespace oneintwo
