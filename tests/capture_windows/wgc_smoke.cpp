// Smoke test real de WGC: crea una ventana, captura al menos un frame y
// verifica que el cierre no entrega callbacks posteriores. No intenta sustituir
// a las pruebas con dobles; sólo valida el camino Win32/WinRT/D3D11 real.
#include "platform/windows/visual/win32_port.hpp"

#include <dwmapi.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <utility>

namespace {

constexpr int kSkipCode = 77;
constexpr wchar_t kWindowClass[] = L"OneInTwoWgcSmokeWindow";

int skip(const char* reason)
{
    std::printf("SKIP: WGC smoke test: %s\n", reason);
    return kSkipCode;
}

int fail(const char* reason)
{
    std::fprintf(stderr, "FAIL: WGC smoke test: %s\n", reason);
    return 1;
}

bool hasVisibleWindowStation()
{
    const HWINSTA station = GetProcessWindowStation();
    if (!station) {
        return false;
    }
    USEROBJECTFLAGS flags{};
    DWORD needed = 0;
    if (!GetUserObjectInformationW(station, UOI_FLAGS, &flags, sizeof(flags), &needed)) {
        return false;
    }
    return (flags.dwFlags & WSF_VISIBLE) != 0;
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(window, &paint);
        RECT area{};
        GetClientRect(window, &area);
        const HBRUSH brush = CreateSolidBrush(RGB(0, 120, 215));
        FillRect(dc, &area, brush);
        DeleteObject(brush);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

void pumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

struct SmokeState {
    std::atomic<int> callbacks{0};
    std::atomic<int> frames{0};
    std::atomic<int> closed{0};
    std::atomic<bool> valid_frame{false};
};

bool waitForFrame(HWND window, const SmokeState& state, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        pumpMessages();
        InvalidateRect(window, nullptr, TRUE);
        UpdateWindow(window);
        DwmFlush();
        if (state.valid_frame.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    pumpMessages();
    return state.valid_frame.load();
}

bool waitForClosed(const SmokeState& state, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        pumpMessages();
        if (state.closed.load() > 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    pumpMessages();
    return state.closed.load() > 0;
}

} // namespace

int main()
{
    if (!hasVisibleWindowStation()) {
        return skip("no visible window station/interactive desktop");
    }
    if (!oneintwo::windows::windowsGraphicsCaptureSupported()) {
        return skip("Windows Graphics Capture is not supported");
    }

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &windowProcedure;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return fail("RegisterClassExW failed");
    }

    const HWND window = CreateWindowExW(
        WS_EX_APPWINDOW, kWindowClass, L"OneInTwo WGC smoke",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 80, 80, 360, 260, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        return fail("CreateWindowExW failed");
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    DwmFlush();

    auto capture = oneintwo::windows::createWin32Capture(
        reinterpret_cast<std::uint64_t>(window), oneintwo::VisualSourceKind::window);
    if (!capture) {
        DestroyWindow(window);
        return fail("createWin32Capture returned null");
    }

    SmokeState state;
    oneintwo::windows::NativeCapture::Sink sink;
    sink.frame = [&state](oneintwo::VisualFrame frame) {
        ++state.callbacks;
        ++state.frames;
        const std::size_t expected = static_cast<std::size_t>(frame.size.width)
            * frame.size.height * 4u;
        if (expected > 0 && frame.bgra.size() == expected) {
            state.valid_frame.store(true);
        }
    };
    sink.state = [&state](oneintwo::VisualSourceState value) {
        ++state.callbacks;
        if (value == oneintwo::VisualSourceState::closed) {
            ++state.closed;
        }
    };

    try {
        capture->start(std::move(sink));
    } catch (...) {
        capture->stop();
        DestroyWindow(window);
        return fail("capture->start threw");
    }

    if (!waitForFrame(window, state, std::chrono::seconds(5))) {
        capture->stop();
        DestroyWindow(window);
        return fail("no valid WGC frame received");
    }

    DestroyWindow(window);
    const bool closed = waitForClosed(state, std::chrono::seconds(3));
    capture->stop();
    const int callbacks_after_stop = state.callbacks.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (state.callbacks.load() != callbacks_after_stop) {
        return fail("callbacks delivered after stop");
    }
    if (!closed) {
        return fail("window close did not produce closed state");
    }

    std::printf("OK: WGC smoke test received a valid frame and closed cleanly\n");
    return 0;
}
