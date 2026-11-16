#pragma once

#include "../../desktop.hpp"
#include "../../../features/visual_share.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/*
 * Puerto nativo del adaptador Windows. La selección, el estado y la validación
 * de fotogramas viven en capture_model.cpp, que se puede probar con un puerto
 * falso. Este puerto aporta la identidad nativa (HWND/HMONITOR), el objeto de
 * captura retenido y la medición de disponibilidad real.
 *
 * La etiqueta de una fuente es solo para la selección local: nunca se usa para
 * volver a localizar la ventana ni se envía al par.
 */

namespace oneintwo::windows {

struct NativeSource {
    std::uint64_t handle = 0; // identidad nativa retenida; no se busca por título
    std::string label;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class NativeCapture {
public:
    struct Sink {
        std::function<void(VisualFrame)> frame;
        std::function<void(VisualSourceState)> state;
    };

    virtual ~NativeCapture() = default;
    // Arranca la adquisición. Los callbacks llegan desde el hilo nativo.
    virtual void start(Sink sink) = 0;
    // Síncrono: al volver no se entregan más callbacks.
    virtual void stop() = 0;
};

class WindowsCapturePort {
public:
    virtual ~WindowsCapturePort() = default;

    virtual VisualCaptureCapabilities capabilities() = 0;
    virtual std::vector<NativeSource> enumerate(VisualSourceKind kind) = 0;
    // Diálogo local asíncrono. std::nullopt significa cancelación del usuario.
    virtual void prompt(VisualSourceKind kind, const std::vector<NativeSource>& sources,
                        std::function<void(std::optional<NativeSource>)> complete) = 0;
    virtual void cancelPrompt() = 0;
    // Crea y retiene el objeto de captura para la identidad elegida.
    virtual std::unique_ptr<NativeCapture> adopt(const NativeSource& source,
                                                 VisualSourceKind kind) = 0;
};

std::unique_ptr<WindowsCapturePort> makeUnavailableWindowsCapturePort();

} // namespace oneintwo::windows
