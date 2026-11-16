#if defined(_WIN32) && defined(__has_include)
#if __has_include(<winrt/Windows.Graphics.Capture.h>) \
    && __has_include(<windows.graphics.capture.interop.h>) \
    && __has_include(<windows.graphics.directx.direct3d11.interop.h>)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <dwmapi.h>

#include "win32_parent.hpp"
#include "win32_port.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oneintwo::windows {
namespace {

constexpr wchar_t kPickerClass[] = L"OneInTwoCapturePickerWindow";
constexpr int kListId = 2001;
constexpr int kOkId = IDOK;
constexpr int kCancelId = IDCANCEL;

std::string wideToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        size, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                        size);
    return result;
}

std::string sizeLabel(std::uint32_t width, std::uint32_t height)
{
    return std::to_string(width) + "\xC3\x97" + std::to_string(height);
}

struct PromptState {
    HWND window = nullptr;
    HWND owner = nullptr;
    std::vector<NativeSource> sources;
    std::function<void(std::optional<NativeSource>)> complete;
    std::optional<NativeSource> result;
    bool delivered = false;
};

LRESULT CALLBACK pickerProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<PromptState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<PromptState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->window = window;

        const int margin = 12;
        RECT client{};
        GetClientRect(window, &client);
        const int listHeight = client.bottom - 64;
        HWND list = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            margin, margin, client.right - margin * 2, listHeight, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)), nullptr, nullptr);
        HWND ok = CreateWindowExW(0, L"BUTTON", L"Compartir",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                  client.right - margin - 220, client.bottom - 44, 100, 30, window,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOkId)), nullptr,
                                  nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        client.right - margin - 110, client.bottom - 44, 100, 30, window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelId)), nullptr, nullptr);

        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        for (HWND child : {list, ok}) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        for (const NativeSource& source : state->sources) {
            const std::wstring label = utf8ToWide(source.label);
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(list, LB_SETCURSEL, 0, 0);
        SetFocus(list);
        return 0;
    }
    case WM_COMMAND:
        if (state == nullptr) {
            break;
        }
        if (LOWORD(wparam) == kOkId) {
            const int index = static_cast<int>(
                SendDlgItemMessageW(window, kListId, LB_GETCURSEL, 0, 0));
            if (index >= 0 && static_cast<std::size_t>(index) < state->sources.size()) {
                state->result = state->sources[static_cast<std::size_t>(index)];
            }
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wparam) == kCancelId) {
            state->result.reset();
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state != nullptr) {
            state->result.reset();
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (state != nullptr) {
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            if (state->owner != nullptr) {
                EnableWindow(state->owner, TRUE);
            }
            if (!state->delivered) {
                state->delivered = true;
                auto callback = state->complete;
                auto result = state->result;
                state->window = nullptr;
                if (callback) {
                    callback(std::move(result));
                }
            }
            delete state;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void registerPickerClass()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &pickerProcedure;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = kPickerClass;
        RegisterClassExW(&window_class);
    });
}

struct MonitorEnumeration {
    std::vector<NativeSource> sources;
};

BOOL CALLBACK enumerateMonitorCallback(HMONITOR monitor, HDC, LPRECT, LPARAM parameter)
{
    auto* enumeration = reinterpret_cast<MonitorEnumeration*>(parameter);
    MONITORINFOEXW info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    if (GetMonitorInfoW(monitor, &info) == FALSE) {
        return TRUE;
    }
    NativeSource source;
    source.handle = reinterpret_cast<std::uint64_t>(monitor);
    source.width = static_cast<std::uint32_t>(info.rcMonitor.right - info.rcMonitor.left);
    source.height = static_cast<std::uint32_t>(info.rcMonitor.bottom - info.rcMonitor.top);
    source.label = wideToUtf8(info.szDevice) + " \xE2\x80\x94 " + sizeLabel(source.width, source.height);
    if ((info.dwFlags & MONITORINFOF_PRIMARY) != 0) {
        source.label += " (principal)";
    }
    enumeration->sources.push_back(std::move(source));
    return TRUE;
}

struct WindowEnumeration {
    DWORD own_process = 0;
    HWND ignored = nullptr;
    std::vector<NativeSource> sources;
};

BOOL CALLBACK enumerateWindowCallback(HWND window, LPARAM parameter)
{
    auto* enumeration = reinterpret_cast<WindowEnumeration*>(parameter);
    if (window == enumeration->ignored || IsWindowVisible(window) == FALSE) {
        return TRUE;
    }
    const int title_length = GetWindowTextLengthW(window);
    if (title_length <= 0) {
        return TRUE;
    }
    const LONG_PTR extended_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((extended_style & WS_EX_TOOLWINDOW) != 0) {
        return TRUE;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == enumeration->own_process || process == 0) {
        return TRUE;
    }
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked,
                                         static_cast<DWORD>(sizeof(cloaked))))
        && cloaked != FALSE) {
        return TRUE;
    }

    std::wstring title(static_cast<std::size_t>(title_length) + 1u, L'\0');
    const int copied = GetWindowTextW(window, title.data(), title_length + 1);
    if (copied <= 0) {
        return TRUE;
    }
    title.resize(static_cast<std::size_t>(copied));

    RECT bounds{};
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds,
                                     static_cast<DWORD>(sizeof(bounds))))) {
        if (GetWindowRect(window, &bounds) == FALSE) {
            return TRUE;
        }
    }
    const long width = std::max<long>(0, bounds.right - bounds.left);
    const long height = std::max<long>(0, bounds.bottom - bounds.top);

    NativeSource source;
    source.handle = reinterpret_cast<std::uint64_t>(window);
    source.label = wideToUtf8(title) + " \xE2\x80\x94 "
        + sizeLabel(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    source.width = static_cast<std::uint32_t>(width);
    source.height = static_cast<std::uint32_t>(height);
    enumeration->sources.push_back(std::move(source));
    return TRUE;
}

class Win32CapturePort final : public WindowsCapturePort {
public:
    explicit Win32CapturePort(const PlatformContext& context)
        : owner_(reinterpret_cast<HWND>(nativeParentWindow(context)))
    {
    }

    VisualCaptureCapabilities capabilities() override
    {
        VisualCaptureCapabilities capabilities;
        const bool supported = windowsGraphicsCaptureSupported();
        capabilities.monitor = supported;
        capabilities.window = supported;
        return capabilities;
    }

    std::vector<NativeSource> enumerate(VisualSourceKind kind) override
    {
        if (kind == VisualSourceKind::monitor) {
            MonitorEnumeration enumeration;
            EnumDisplayMonitors(nullptr, nullptr, &enumerateMonitorCallback,
                                reinterpret_cast<LPARAM>(&enumeration));
            return enumeration.sources;
        }
        WindowEnumeration enumeration;
        enumeration.own_process = GetCurrentProcessId();
        enumeration.ignored = prompt_window_.load();
        EnumWindows(&enumerateWindowCallback, reinterpret_cast<LPARAM>(&enumeration));
        return enumeration.sources;
    }

    void prompt(VisualSourceKind kind, const std::vector<NativeSource>& sources,
                std::function<void(std::optional<NativeSource>)> complete) override
    {
        cancelPrompt();
        registerPickerClass();

        auto* state = new PromptState();
        state->owner = owner_;
        state->sources = sources;
        state->complete = std::move(complete);

        RECT owner_rect{};
        if (owner_ != nullptr) {
            GetWindowRect(owner_, &owner_rect);
        } else {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &owner_rect, 0);
        }
        constexpr int width = 560;
        constexpr int height = 420;
        const int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
        const int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;

        HWND window = CreateWindowExW(
            WS_EX_DLGMODALFRAME, kPickerClass,
            kind == VisualSourceKind::window ? L"Compartir una ventana"
                                             : L"Compartir un monitor",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, width, height, owner_, nullptr,
            GetModuleHandleW(nullptr), state);
        if (window == nullptr) {
            auto callback = state->complete;
            delete state;
            if (callback) {
                callback(std::nullopt);
            }
            return;
        }
        prompt_window_.store(window);
        if (owner_ != nullptr) {
            EnableWindow(owner_, FALSE);
        }
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        SetForegroundWindow(window);
    }

    void cancelPrompt() override
    {
        const HWND window = prompt_window_.exchange(nullptr);
        if (window != nullptr && IsWindow(window) != FALSE) {
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
    }

    std::unique_ptr<NativeCapture> adopt(const NativeSource& source,
                                         VisualSourceKind kind) override
    {
        return createWin32Capture(source.handle, kind);
    }

private:
    HWND owner_ = nullptr;
    std::atomic<HWND> prompt_window_{nullptr};
};

} // namespace

std::unique_ptr<WindowsCapturePort> makeWin32CapturePort(const PlatformContext& context)
{
    return std::make_unique<Win32CapturePort>(context);
}

} // namespace oneintwo::windows

#endif
#endif
