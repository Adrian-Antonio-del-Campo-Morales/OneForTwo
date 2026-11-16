#pragma once

#include "../../../features/visual_share.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/*
 * Customer org.freedesktop.portal.ScreenCast.
 *
 * One session per operation is used: CreateSession, SelectSources with the exact type requested
 * and a single source, Start and OpenPipeWireRemote. The application and the session are kept
 * together until the consumer closes or the portal issues Closed. Cancellation also closes a
 * request that still awaits a response.
 */

namespace oneintwo::wayland {

struct ScreenCastSupport {
    bool monitor = false;
    bool window = false;
};

struct ScreenCastSource {
    std::uint32_t node_id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool has_source_type = false;
    std::uint32_t source_type = 0; // 1 monitor, 2 ventana, 4 virtual
    std::string stream_id;
    std::string mapping_id;
};

enum class ScreenCastStatus {
    ready,
    cancelled,
    denied,
    unavailable,
    error,
};

class PortalScreencastClient {
public:
    using CapabilitiesResult = std::function<void(ScreenCastSupport, bool available)>;
    using SessionResult = std::function<void(ScreenCastStatus, ScreenCastSource)>;
    using SessionClosed = std::function<void()>;

    virtual ~PortalScreencastClient() = default;

    virtual void probe(CapabilitiesResult complete) = 0;
    // Retains the portal session and its descriptor PipeWire up to close().
    // parent_window is the local identifier of the parent of the dialog ("" if not).
    virtual void open(VisualSourceKind kind, std::string parent_window, SessionClosed on_closed,
                      SessionResult complete) = 0;
    // Terminal response only once; closes an outstanding request.
    virtual void cancel() = 0;
    // Releases session and descriptor. Does not deliver states to the consumer.
    virtual void close() = 0;
    // Copy of the authorized descriptor, owned by the recipient.
    virtual int duplicatePipeWireFd() const = 0;
};

std::unique_ptr<PortalScreencastClient> makePortalScreencastClient();

} // namespace oneintwo::wayland
