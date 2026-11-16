#include "win32_capture_port.hpp"

#include <optional>
#include <utility>

namespace oneintwo::windows {
namespace {

/*
 * Sustituto cuando la compilación no incluye Windows Graphics Capture.
 * Informa de indisponibilidad por el contrato; no simula una captura.
 */
class UnavailablePort final : public WindowsCapturePort {
public:
    VisualCaptureCapabilities capabilities() override { return {}; }

    std::vector<NativeSource> enumerate(VisualSourceKind) override { return {}; }

    void prompt(VisualSourceKind, const std::vector<NativeSource>&,
                std::function<void(std::optional<NativeSource>)> complete) override
    {
        if (complete) {
            complete(std::nullopt);
        }
    }

    void cancelPrompt() override {}

    std::unique_ptr<NativeCapture> adopt(const NativeSource&, VisualSourceKind) override
    {
        return nullptr;
    }
};

} // namespace

std::unique_ptr<WindowsCapturePort> makeUnavailableWindowsCapturePort()
{
    return std::make_unique<UnavailablePort>();
}

} // namespace oneintwo::windows
