#pragma once

#include "win32_capture_port.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

/*
 * WGC core independent of WinRT/D3D11.
 *
 * Windows backend implements these interfaces with real objects; native tests implement them with
 * doubles to run the same life cycle on any platform. The decision to hide the cursor lives in the
 * injected session, which in production makes the IGraphicsCaptureSession2 QI.
 */

namespace oneintwo::windows::detail {

bool saneSize(VisualSize size);

class WgcSession {
public:
    virtual ~WgcSession() = default;

    // Production: QI of IGraphicsCaptureSession2 and, if it exists, property.
    virtual void configureCursor() = 0;
    virtual void start() = 0;
    virtual void close() = 0;
};

class WgcFrame {
public:
    virtual ~WgcFrame() = default;

    virtual VisualSize contentSize() = 0;
    virtual bool copyTo(VisualFrame& frame) = 0;
    virtual void close() = 0;
};

class WgcFramePool {
public:
    virtual ~WgcFramePool() = default;

    virtual std::unique_ptr<WgcSession> createSession() = 0;
    virtual std::unique_ptr<WgcFrame> nextFrame() = 0;
    virtual bool recreate(VisualSize size) = 0;
    virtual void close() = 0;
};

class WgcFramePoolFactory {
public:
    virtual ~WgcFramePoolFactory() = default;

    virtual std::unique_ptr<WgcFramePool> create(VisualSize initial_size) = 0;
};

class WgcSource {
public:
    virtual ~WgcSource() = default;

    virtual bool available() = 0;
    virtual bool suspended() = 0;
};

class WgcCaptureCore final : public NativeCapture {
public:
    WgcCaptureCore(std::unique_ptr<WgcFramePoolFactory> factory,
                   std::unique_ptr<WgcSource> source, VisualSize initial_size,
                   std::function<void()> initialise_thread = {});
    ~WgcCaptureCore() override;

    WgcCaptureCore(const WgcCaptureCore&) = delete;
    WgcCaptureCore& operator=(const WgcCaptureCore&) = delete;

    void start(Sink sink) override;
    void stop() override;

private:
    void poll();
    void publishState(VisualSourceState state);
    void releaseGraphics();

    std::unique_ptr<WgcFramePoolFactory> factory_;
    std::unique_ptr<WgcSource> source_;
    std::function<void()> initialise_thread_;
    std::unique_ptr<WgcFramePool> pool_;
    std::unique_ptr<WgcSession> session_;
    VisualSize initial_size_{};
    VisualSize pool_size_{};
    std::atomic<bool> stop_{false};
    std::atomic<bool> suspended_{false};
    std::optional<VisualSourceState> last_state_;
    bool started_ = false;
    Sink sink_;
    std::thread thread_;
};

/*
 * Performs the QI and only calls the property if the interface is present. It is a template to be
 * able to test the missing branch and the exception one with a fake object; producing the instance
 * with GraphicsCaptureSession and ABI::Windows::Graphics::Capture::IGraphicsCaptureSsession2.
 */
template <typename Session, typename CursorSession>
bool tryDisableCursor(Session& session)
{
    try {
        if (const auto cursor = session.template try_as<CursorSession>()) {
            cursor->put_IsCursorCaptureEnabled(false);
            return true;
        }
    } catch (...) {
        // Hiding the cursor is cosmetic: the capture must continue.
    }
    return false;
}

} // namespace oneintwo::windows::detail
