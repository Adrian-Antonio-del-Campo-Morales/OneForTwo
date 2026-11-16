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
 * Native port of the Windows adapter. The selection, status and validation of frames live in
 * capture_model.cpp, which can be tested with a false port. This port provides the native identity
 * (HWND/HMONITOR), the retained capture object and the measurement of actual availability.
 *
 * The font tag is only for the local selection: it is never used to relocate the window or send it
 * to the pair.
 */

namespace oneintwo::windows {

struct NativeSource {
    std::uint64_t handle = 0; // native identity withheld; not searched by title
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
    // Start the acquisition. Callbacks come from the native thread.
    virtual void start(Sink sink) = 0;
    // Synchronous: no more callbacks are delivered on return.
    virtual void stop() = 0;
};

class WindowsCapturePort {
public:
    virtual ~WindowsCapturePort() = default;

    virtual VisualCaptureCapabilities capabilities() = 0;
    virtual std::vector<NativeSource> enumerate(VisualSourceKind kind) = 0;
    // Asynchronous local dialogue. std::nullopt means user cancellation.
    virtual void prompt(VisualSourceKind kind, const std::vector<NativeSource>& sources,
                        std::function<void(std::optional<NativeSource>)> complete) = 0;
    virtual void cancelPrompt() = 0;
    // Creates and retains the capture object for the chosen identity.
    virtual std::unique_ptr<NativeCapture> adopt(const NativeSource& source,
                                                 VisualSourceKind kind) = 0;
};

std::unique_ptr<WindowsCapturePort> makeUnavailableWindowsCapturePort();

} // namespace oneintwo::windows
