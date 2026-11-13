#pragma once

#include "../../../features/visual_share.hpp"

#include <cstdint>
#include <functional>
#include <memory>

/*
 * Consumo del nodo PipeWire autorizado por la sesión ScreenCast.
 *
 * La implementación real conecta con el descriptor del portal, negocia vídeo
 * BGRA/BGRx/RGBA/RGBx, convierte cada búfer al contrato común y descarta los
 * fotogramas por encima de 30/s. Los callbacks llegan en el hilo de PipeWire;
 * el adaptador los reenvía a ModuleRuntime.post.
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
    // Adquiere el descriptor; devuelve false si no puede iniciar el consumo.
    virtual bool start(int fd, std::uint32_t node_id, Callbacks callbacks) = 0;
    // Síncrono: al volver no se entregan más callbacks.
    virtual void stop() = 0;
};

std::unique_ptr<PipeWireSource> makePipeWireSource();

} // namespace oneintwo::wayland
