#if defined(_WIN32) && defined(__has_include)
#if __has_include(<winrt/Windows.Graphics.Capture.h>)
#define ONEINTWO_WGC_AVAILABLE 1
#endif
#endif

#if defined(ONEINTWO_WGC_AVAILABLE)

#include "win32_port.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/base.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace oneintwo::windows {
namespace {

constexpr std::uint32_t kMaxDimension = 16384;
constexpr std::uint64_t kMaxPixels = 64ull * 1024ull * 1024ull;

bool saneSize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) {
        return false;
    }
    return static_cast<std::uint64_t>(width) * height <= kMaxPixels;
}

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

class WgcCapture final : public NativeCapture {
public:
    WgcCapture(winrt::Windows::Graphics::Capture::GraphicsCaptureItem item,
               winrt::com_ptr<ID3D11Device> device, winrt::com_ptr<ID3D11DeviceContext> context,
               winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device,
               VisualSourceKind kind, std::uint64_t native_handle, DWORD process_id,
               std::wstring monitor_name, VisualSize size)
        : item_(std::move(item))
        , device_(std::move(device))
        , context_(std::move(context))
        , render_device_(std::move(render_device))
        , kind_(kind)
        , native_handle_(native_handle)
        , process_id_(process_id)
        , monitor_name_(std::move(monitor_name))
        , size_(size)
    {
    }

    ~WgcCapture() override { stop(); }

    void start(Sink sink) override
    {
        if (thread_.joinable() || !item_) {
            if (sink.state) {
                sink.state(VisualSourceState::closed);
            }
            return;
        }
        sink_ = std::move(sink);
        stop_.store(false);
        suspended_.store(false);
        last_state_.reset();
        initialiseApartment();

        try {
            pool_ = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                render_device_,
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                item_.Size());
            pool_size_ = size_;
            session_ = pool_.CreateCaptureSession(item_);
            try {
                session_.IsCursorCaptureEnabled(false);
            } catch (...) {
            }
            try {
                // Windows 11; en versiones anteriores la propiedad no existe.
                session_.IsBorderRequired(false);
            } catch (...) {
            }
            session_.StartCapture();
        } catch (...) {
            releaseGraphics();
            if (sink_.state) {
                sink_.state(VisualSourceState::closed);
            }
            return;
        }

        thread_ = std::thread([this] { poll(); });
    }

    void stop() override
    {
        stop_.store(true);
        if (thread_.joinable()) {
            thread_.join();
        }
        releaseGraphics();
        sink_ = {};
    }

private:
    void releaseGraphics()
    {
        if (session_) {
            try {
                session_.Close();
            } catch (...) {
            }
            session_ = nullptr;
        }
        if (pool_) {
            try {
                pool_.Close();
            } catch (...) {
            }
            pool_ = nullptr;
        }
        staging_ = nullptr;
        context_ = nullptr;
        device_ = nullptr;
        render_device_ = nullptr;
        item_ = nullptr;
    }

    bool sourceAvailable()
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
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info) == FALSE) {
            return false;
        }
        return monitor_name_.empty() || monitor_name_ == info.szDevice;
    }

    bool suspendedNow() const
    {
        if (kind_ == VisualSourceKind::window) {
            return IsIconic(reinterpret_cast<HWND>(native_handle_)) != FALSE;
        }
        return false;
    }

    void publishState(VisualSourceState state)
    {
        if (last_state_ && *last_state_ == state) {
            return;
        }
        last_state_ = state;
        if (sink_.state) {
            sink_.state(state);
        }
    }

    void poll()
    {
        initialiseApartment();
        auto last_check = std::chrono::steady_clock::now();
        while (!stop_.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_check >= std::chrono::milliseconds(200)) {
                last_check = now;
                if (!sourceAvailable()) {
                    publishState(VisualSourceState::closed);
                    stop_.store(true);
                    break;
                }
                const bool suspended = suspendedNow();
                if (suspended != suspended_.load()) {
                    suspended_.store(suspended);
                    publishState(suspended ? VisualSourceState::suspended
                                           : VisualSourceState::available);
                }
            }
            if (suspended_.load() || session_ == nullptr || pool_ == nullptr) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame{nullptr};
            try {
                frame = pool_.TryGetNextFrame();
            } catch (...) {
                publishState(VisualSourceState::closed);
                stop_.store(true);
                break;
            }
            if (!frame) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            const auto content = frame.ContentSize();
            const std::uint32_t width = static_cast<std::uint32_t>(std::max(0, content.Width));
            const std::uint32_t height = static_cast<std::uint32_t>(std::max(0, content.Height));
            if (saneSize(width, height)) {
                if (width != pool_size_.width || height != pool_size_.height) {
                    try {
                        pool_.Recreate(
                            render_device_,
                            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
                                B8G8R8A8UIntNormalized,
                            2, content);
                        pool_size_ = {width, height};
                    } catch (...) {
                        publishState(VisualSourceState::closed);
                        stop_.store(true);
                        break;
                    }
                }
                VisualFrame visual;
                if (copyFrame(frame, width, height, visual)) {
                    if (sink_.frame) {
                        sink_.frame(std::move(visual));
                    }
                }
            }
            try {
                frame.Close();
            } catch (...) {
            }
        }
    }

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

    bool copyFrame(const winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame& frame,
                   std::uint32_t width, std::uint32_t height, VisualFrame& out)
    {
        try {
            const auto surface = frame.Surface();
            const auto access = surface.as<
                ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> texture;
            winrt::check_hresult(access->GetDXGIInterface(
                winrt::guid_of<ID3D11Texture2D>(), texture.put_void()));
            D3D11_TEXTURE2D_DESC description{};
            texture->GetDesc(&description);
            const std::uint32_t copy_width = std::min(width, description.Width);
            const std::uint32_t copy_height = std::min(height, description.Height);
            if (!saneSize(copy_width, copy_height)
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
            context_->CopySubresourceRegion(staging_.get(), 0, 0, 0, 0, texture.get(), 0, &box);

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

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item_{nullptr};
    winrt::com_ptr<ID3D11Device> device_;
    winrt::com_ptr<ID3D11DeviceContext> context_;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice render_device_{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool_{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session_{nullptr};
    winrt::com_ptr<ID3D11Texture2D> staging_;
    VisualSize staging_size_{};
    DXGI_FORMAT staging_format_ = DXGI_FORMAT_UNKNOWN;
    VisualSourceKind kind_ = VisualSourceKind::monitor;
    std::uint64_t native_handle_ = 0;
    DWORD process_id_ = 0;
    std::wstring monitor_name_;
    VisualSize size_{};
    VisualSize pool_size_{};
    std::atomic<bool> stop_{false};
    std::atomic<bool> suspended_{false};
    std::optional<VisualSourceState> last_state_;
    Sink sink_;
    std::thread thread_;
};

} // namespace

bool windowsGraphicsCaptureSupported()
{
    try {
        initialiseApartment();
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
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
                window, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
                winrt::put_abi(item)));
        } else {
            const HMONITOR monitor = reinterpret_cast<HMONITOR>(handle);
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(monitor, &info) == FALSE) {
                return nullptr;
            }
            monitor_name = info.szDevice;
            winrt::check_hresult(interop->CreateForMonitor(
                monitor, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
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
        if (!saneSize(size.width, size.height)) {
            return nullptr;
        }

        return std::make_unique<WgcCapture>(std::move(item), std::move(device),
                                            std::move(context), std::move(render_device), kind,
                                            handle, process, std::move(monitor_name), size);
    } catch (...) {
        return nullptr;
    }
}

} // namespace oneintwo::windows

#endif
