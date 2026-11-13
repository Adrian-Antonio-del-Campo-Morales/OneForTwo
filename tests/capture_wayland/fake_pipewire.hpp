#pragma once

#include "platform/linux/visual/pipewire_source.hpp"
#include "platform/linux/visual/video_frame_convert.hpp"

#include <unistd.h>

#include <cstdint>
#include <utility>

namespace oneintwo::test {

/*
 * Consumidor PipeWire falso: registra el nodo autorizado, permite inyectar
 * fotogramas y conserva los callbacks después de stop() para comprobar que el
 * adaptador descarta entregas tardías.
 */
class FakePipeWireSource final : public wayland::PipeWireSource {
public:
    bool start_result = true;
    int starts = 0;
    bool stopped = false;
    int fd_received = -1;
    std::uint32_t node_received = 0;
    Callbacks saved;

    bool start(int fd, std::uint32_t node_id, Callbacks callbacks) override
    {
        ++starts;
        fd_received = fd;
        node_received = node_id;
        if (fd >= 0) {
            ::close(fd);
        }
        if (!start_result) {
            return false;
        }
        saved = std::move(callbacks);
        return true;
    }

    void stop() override { stopped = true; }

    void pushFrame(VisualFrame frame)
    {
        if (saved.frame) {
            saved.frame(std::move(frame));
        }
    }

    void pushSize(VisualSize size)
    {
        if (saved.size_changed) {
            saved.size_changed(size);
        }
    }

    void pushClosed()
    {
        if (saved.closed) {
            saved.closed();
        }
    }

    bool pushRaw(const wayland::VideoBufferView& view)
    {
        VisualFrame frame;
        if (wayland::convertToBgra(view, frame) != wayland::FrameConvertResult::ok) {
            return false;
        }
        pushFrame(std::move(frame));
        return true;
    }
};

} // namespace oneintwo::test
