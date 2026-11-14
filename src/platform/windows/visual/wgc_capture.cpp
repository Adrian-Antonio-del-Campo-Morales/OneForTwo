#if defined(_WIN32) && defined(__has_include)
#if __has_include(<winrt/Windows.Graphics.Capture.h>) \
    && __has_include(<windows.graphics.capture.interop.h>) \
    && __has_include(<windows.graphics.directx.direct3d11.interop.h>)
#define ONEINTWO_WGC_AVAILABLE 1
#endif
#endif

#if defined(ONEINTWO_WGC_AVAILABLE)

#include "wgc_capture_core.hpp"
#include "win32_port.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/base.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace oneintwo::windows {
namespace {

void makeOpaque(VisualFrame& frame)
{
    const std::size_t pixels = static_cast<std::size_t>(frame.size.width) * frame.size.height;
    for (std::size_t index = 0; index < pixels; ++index) {
        frame.bgra[index * 4 + 3] = 0xFF;
    }
}

bool initialiseApartment()
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
        // La interfaz ya puede estar inicializada como STA por el hilo de UI.
    }
    return true;
}

winrt::Windows::Graphics::SizeInt32 toSizeInt32(VisualSize size)
{
    return {static_cast<std::int32_t>(size.width), static_cast<std::int32_t>(size.height)};
}

class WinRtFrameCopier {
public:
    WinRtFrameCopier(winrt::com_ptr<ID3D11Device> device,
                     winrt::com_ptr<ID3D11DeviceContext> context)
        : device_(std::move(device))
        , context_(std::move(context))
    {
    }

    bool copy(ID3D11Texture2D* texture, VisualSize content, VisualFrame& out)
    {
        try {
            if (!texture) {
                return false;
            }
            D3D11_TEXTURE2D_DESC description{};
            texture->GetDesc(&description);
            if (description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
                return false; // El contrato exige BGRA; no se entrega otro formato.
            }
            const std::uint32_t copy_width = std::min(content.width, description.Width);
            const std::uint32_t copy_height = std::min(content.height, description.Height);
            if (!detail::saneSize({copy_width, copy_height})
                || !ensureStaging(copy_width, copy_height, description.Format)) {
                return false;
            }

            D3D11_BOX box{};
            box.left = 0;
            box.top = 0;
            box.front = 0;
            box.right = copy_width;
            box.bottom = copy_height;
            box.back = 1;
            context_->CopySubresourceRegion(staging_.get(), 0, 0, 0, 0, texture, 0, &box);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context_->Map(staging_.get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                return false;
            }
            out.size = {copy_width, copy_height};
            out.bgra.resize(static_cast<std::size_t>(copy_width) * copy_height * 4ull);
            const auto* source = static_cast<const std::uint8_t*>(mapped.pData);
            for (std::uint32_t row = 0; row < copy_height; ++row) {
                std::memcpy(out.bgra.data() + static_cast<std::size_t>(row) * copy_width * 4ull,
                            source + static_cast<std::size_t>(row) * mapped.RowPitch,
                            static_cast<std::size_t>(copy_width) * 4ull);
            }
            context_->Unmap(staging_.get(), 0);
            makeOpaque(out);
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    bool ensureStaging(std::uint32_t width, std::uint32_t height, DXGI_FORMAT format)
    {
        if (staging_ && staging_size_.width == width && staging_size_.height == height
            && staging_format_ == format) {
            return true;
        }
        D3D11_TEXTURE2D_DESC description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_STAGING;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        winrt::com_ptr<ID3D11Texture2D> staging;
        if (FAILED(device_->CreateTexture2D(&description, nullptr, staging.put()))) {
            return false;
        }
        staging_ = std::move(staging);
        staging_size_ = {width, height};
        staging_format_ = format;
        return true;
    }

    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
    winrt::com_ptr<ID3D11Texture2D> staging_;
    VisualSize staging_size_{};
    DXGI_FORMAT staging_format_ = DXGI_FORMAT_UNKNOWN;
};

class WinRtWgcFrame final : public detail::WgcFrame {
public:
    WinRtWgcFrame(winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame,
                  std::shared_ptr<WinRtFrameCopier> copier)
        : frame_(std::move(frame))
        , copier_(std::move(copier))
    {
    }

    VisualSize contentSize() override
    {
        const auto content = frame_.ContentSize();
        return {static_cast<std::uint32_t>(std::max(0, content.Width)),
                static_cast<std::uint32_t>(std::max(0, content.Height))};
    }

    bool copyTo(VisualFrame& out) override
    {
        try {
            const auto surface = frame_.Surface();
            const auto access = surface.as<
                ::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(access->GetInterface(
                __uuidof(ID3D11Texture2D), texture.put_void()));
            return copier_->copy(texture.get(), contentSize(), out);
        } catch (...) {
            return false;
        }
    }

    void close() override
    {
        if (frame_) {
            try {
                frame_.Close();
            } catch (...) {
            }
            frame_ = nullptr;
        }
    }

private:
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame_{nullptr};
    std::shared_ptr<WinRtFrameCopier> copier_;
};

class WinRtWgcSession final : public detail::WgcSession {
public:
    explicit WinRtWgcSession(
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession session)
        : session_(std::move(session))
    {
    }

    void configureCursor() override
    {
        (void)detail::tryDisableCursor<
            winrt::Windows::Graphics::Capture::GraphicsCaptureSession,
            ABI::Windows::Graphics::Capture::IGraphicsCaptureSession2>(session_);
    }

    void start() override { session_.StartCapture(); }

    void close() override
    {
        if (session_) {
            try {
                session_.Close();
            } catch (...) {
            }
            session_ = nullptr;
        }
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{nullptr};
};

class WinRtWgcFramePool final : public detail::WgcFramePool {
public:
    WinRtWgcFramePool(
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item,
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device,
        winrt::com_ptr<ID3D11Device> device, winrt::com_ptr<ID3D11DeviceContext> context,
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool)
        : item_(std::move(item))
        , render_device_(std::move(render_device))
        , pool_(std::move(pool))
        , copier_(std::make_shared<WinRtFrameCopier>(std::move(device), std::move(context)))
    {
    }

    std::unique_ptr<detail::WgcSession> createSession() override
    {
        return std::make_unique<WinRtWgcSession>(pool_.CreateCaptureSession(item_));
    }

    std::unique_ptr<detail::WgcFrame> nextFrame() override
    {
        auto frame = pool_.TryGetNextFrame();
        if (!frame) {
            return nullptr;
        }
        return std::make_unique<WinRtWgcFrame>(std::move(frame), copier_);
    }

    bool recreate(VisualSize size) override
    {
        try {
            pool_.Recreate(render_device_,
                           winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
                               B8G8R8A8UIntNormalized,
                           2, toSizeInt32(size));
            return true;
        } catch (...) {
            return false;
        }
    }

    void close() override
    {
        if (pool_) {
            try {
                pool_.Close();
            } catch (...) {
            }
            pool_ = nullptr;
        }
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{nullptr};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool_{nullptr};
    std::shared_ptr<WinRtFrameCopier> copier_;
};

class WinRtWgcPoolFactory final : public detail::WgcFramePoolFactory {
public:
    WinRtWgcPoolFactory(
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item,
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device,
        winrt::com_ptr<ID3D11Device> device, winrt::com_ptr<ID3D11DeviceContext> context)
        : item_(std::move(item))
        , render_device_(std::move(render_device))
        , device_(std::move(device))
        , context_(std::move(context))
    {
    }

    std::unique_ptr<detail::WgcFramePool> create(VisualSize initial_size) override
    {
        auto pool =
            winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                render_device_,
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
                    B8G8R8A8UIntNormalized,
                2, toSizeInt32(initial_size));
        return std::make_unique<WinRtWgcFramePool>(item_, render_device_, device_, context_,
                                                   std::move(pool));
    }

private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{nullptr};
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device_{nullptr};
    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
};

class WinRtWgcSource final : public detail::WgcSource {
public:
    WinRtWgcSource(VisualSourceKind kind, std::uint64_t native_handle, DWORD process_id,
                   std::wstring monitor_name)
        : kind_(kind)
        , native_handle_(native_handle)
        , process_id_(process_id)
        , monitor_name_(std::move(monitor_name))
    {
    }

    bool available() override
    {
        if (kind_ == VisualSourceKind::window) {
            const HWND window = reinterpret_cast<HWND>(native_handle_);
            DWORD process = 0;
            if (IsWindow(window) == FALSE) {
                return false;
            }
            GetWindowThreadProcessId(window, &process);
            return process == process_id_;
        }
        const HMONITOR monitor = reinterpret_cast<HMONITOR>(native_handle_);
        MONITORINFOEXW info{};
        info.cbSize = static_cast<DWORD>(sizeof(info));
        if (GetMonitorInfoW(monitor, &info) == FALSE) {
            return false;
        }
        return monitor_name_.empty() || monitor_name_ == info.szDevice;
    }

    bool suspended() override
    {
        if (kind_ == VisualSourceKind::window) {
            return IsIconic(reinterpret_cast<HWND>(native_handle_)) != FALSE;
        }
        return false;
    }

private:
    VisualSourceKind kind_ = VisualSourceKind::monitor;
    std::uint64_t native_handle_ = 0;
    DWORD process_id_ = 0;
    std::wstring monitor_name_;
};

} // namespace

bool windowsGraphicsCaptureSupported()
{
    try {
        initialiseApartment();
        if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported()) {
            return false;
        }
        // IsSupported puede ser cierto en sistemas anteriores a 1903, donde la
        // interfaz de interoperabilidad para HWND/HMONITOR todavía no existe.
        const auto interop = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();
        return static_cast<bool>(interop);
    } catch (...) {
        return false;
    }
}

std::unique_ptr<NativeCapture> createWin32Capture(std::uint64_t handle, VisualSourceKind kind)
{
    if (!windowsGraphicsCaptureSupported()) {
        return nullptr;
    }
    try {
        initialiseApartment();
        auto interop = winrt::get_activation_factory<
            winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
            IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
        std::wstring monitor_name;
        DWORD process = 0;
        HWND window = nullptr;
        if (kind == VisualSourceKind::window) {
            window = reinterpret_cast<HWND>(handle);
            if (IsWindow(window) == FALSE) {
                return nullptr;
            }
            GetWindowThreadProcessId(window, &process);
            winrt::check_hresult(interop->CreateForWindow(
                window, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                winrt::put_abi(item)));
        } else {
            const HMONITOR monitor = reinterpret_cast<HMONITOR>(handle);
            MONITORINFOEXW info{};
            info.cbSize = static_cast<DWORD>(sizeof(info));
            if (GetMonitorInfoW(monitor, &info) == FALSE) {
                return nullptr;
            }
            monitor_name = info.szDevice;
            winrt::check_hresult(interop->CreateForMonitor(
                monitor, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                winrt::put_abi(item)));
        }
        if (!item) {
            return nullptr;
        }

        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
            0, D3D11_SDK_VERSION, device.put(), nullptr, context.put()));
        winrt::com_ptr<IDXGIDevice> dxgi;
        winrt::check_hresult(device->QueryInterface(IID_PPV_ARGS(dxgi.put())));
        winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device{nullptr};
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
            dxgi.get(), reinterpret_cast<::IInspectable**>(winrt::put_abi(render_device))));

        const auto item_size = item.Size();
        VisualSize size{};
        if (item_size.Width > 0 && item_size.Height > 0) {
            size.width = static_cast<std::uint32_t>(item_size.Width);
            size.height = static_cast<std::uint32_t>(item_size.Height);
        }
        if (!detail::saneSize(size)) {
            return nullptr;
        }

        auto source = std::make_unique<WinRtWgcSource>(
            kind, handle, process, std::move(monitor_name));
        auto pool_factory = std::make_unique<WinRtWgcPoolFactory>(
            std::move(item), std::move(render_device), std::move(device), std::move(context));
        return std::make_unique<detail::WgcCaptureCore>(
            std::move(pool_factory), std::move(source), size,
            [] { initialiseApartment(); });
    } catch (...) {
        return nullptr;
    }
}

} // namespace oneintwo::windows

#endif
