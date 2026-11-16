#pragma once

#include "win32_capture_port.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

/*
 * Núcleo de WGC independiente de WinRT/D3D11.
 *
 * El backend Windows implementa estas interfaces con objetos reales; las
 * pruebas nativas las implementan con dobles para ejecutar el mismo ciclo de
 * vida en cualquier plataforma. La decisión de ocultar el cursor vive en la
 * sesión inyectada, que en producción hace el QI de
 * IGraphicsCaptureSession2.
 */

namespace oneintwo::windows::detail {

bool saneSize(VisualSize size);

class WgcSession {
public:
    virtual ~WgcSession() = default;

    // Producción: QI de IGraphicsCaptureSession2 y, si existe, propiedad.
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
 * Realiza el QI y sólo llama a la propiedad si la interfaz está presente.
 * Es una plantilla para poder probar la rama de ausencia y la de excepción con
 * un objeto falso; producción la instancia con GraphicsCaptureSession y
 * ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2.
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
        // Ocultar el cursor es cosmético: la captura debe continuar.
    }
    return false;
}

} // namespace oneintwo::windows::detail
