#pragma once

#include "../../../integration/feature_contracts.hpp"
#include "../../../platform/desktop.hpp"
#include "win32_capture_port.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

/*
 * Modelo portable del adaptador de captura Windows: selección local, respuesta
 * terminal única, identidad de la fuente, orden de estados y descarte de
 * callbacks tardíos. Se compila y se prueba sin Windows mediante un puerto
 * falso; el puerto real usa Windows Graphics Capture.
 */

namespace oneintwo::windows {

class CaptureModel final : public DesktopVisualCapture {
public:
    CaptureModel(PlatformContext context, std::unique_ptr<WindowsCapturePort> port);
    ~CaptureModel() override;

    VisualCaptureCapabilities visualCapabilities() const override;
    void selectSource(VisualShareRef offer, VisualSourceKind kind,
                      std::function<void(VisualShareRef, DesktopResult)> selected) override;
    void startCapture(VisualShareRef offer, VisualCaptureCallbacks callbacks) override;
    void stopCapture(VisualShareRef offer) override;

private:
    struct Lifetime;
    struct Selection;
    struct Active;

    void post(std::function<void()> task);
    void finishSelection(std::uint64_t epoch, DesktopResult result);
    void onPrompt(std::uint64_t epoch, std::optional<NativeSource> chosen);
    void handleFrame(std::uint64_t epoch, VisualShareRef offer, VisualFrame frame);
    void handleState(std::uint64_t epoch, VisualShareRef offer, VisualSourceState state);
    void announce(Active& active, VisualSourceState state);

    ModuleRuntime runtime_;
    std::shared_ptr<Lifetime> lifetime_;
    std::unique_ptr<WindowsCapturePort> port_;
    VisualCaptureCapabilities capabilities_{};
    std::unique_ptr<Selection> selection_;
    std::shared_ptr<Active> active_;
    std::uint64_t epoch_ = 0;
};

} // namespace oneintwo::windows
