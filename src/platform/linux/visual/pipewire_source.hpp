#pragma once

#include "../../../features/visual_share.hpp"

#include <cstdint>
#include <functional>
#include <memory>

/*
 * Node consumption PipeWire authorized by ScreenCast session.
 *
 * The actual implementation connects with the portal descriptor, negotiates BGRA/BGRx/RGBA/RGBx
 * video, converts each buffer to the common contract and discards frames above 30/s. Callbacks
 * arrive in the PipeWire thread; the adapter forwards them to ModuleRuntime.post.
 */

namespace oneintwo::wayland {

class PipeWireSource {
public:
    struct Callbacks {
        std::function<void(VisualFrame)> frame;
        std::function<void(VisualSize)> size_changed;
        std::function<void()> closed;
    };

    virtual ~PipeWireSource() = default;
    // Acquires the descriptor; returns false if you cannot start the consumption.
    virtual bool start(int fd, std::uint32_t node_id, Callbacks callbacks) = 0;
    // Synchronous: no more callbacks are delivered on return.
    virtual void stop() = 0;
};

std::unique_ptr<PipeWireSource> makePipeWireSource();

} // namespace oneintwo::wayland
