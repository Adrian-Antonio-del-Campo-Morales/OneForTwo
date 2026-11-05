// Responsabilidad: adaptador nativo de Windows.
// Observa CF_UNICODETEXT con AddClipboardFormatListener en una ventana
// oculta, y lee/escribe mediante Win32. No captura ni inyecta entrada.
// El marcador de escritura propia usa un formato privado del portapapeles.

#include "platform/clipboard.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace oit {
namespace {

constexpr wchar_t kWindowClass[] = L"OneInTwoClipboardWatcherWindow";
constexpr wchar_t kProbeFormatName[] = L"OneInTwo.ClipboardProbe";
constexpr UINT kStopMessage = WM_APP + 1;
constexpr int kOpenAttempts = 10;
constexpr DWORD kOpenRetryMs = 20;
constexpr std::size_t kMaxMarkerBytes = 160;

UINT probe_format() {
  static const UINT format = RegisterClipboardFormatW(kProbeFormatName);
  return format;
}

bool open_with_retry(HWND owner, std::string& error) {
  for (int attempt = 0; attempt < kOpenAttempts; ++attempt) {
    if (OpenClipboard(owner) != FALSE) {
      return true;
    }
    Sleep(kOpenRetryMs);
  }
  error = "clipboard busy: OpenClipboard failed with error " +
          std::to_string(GetLastError());
  return false;
}

bool utf16_to_utf8(const wchar_t* text, std::size_t units, std::string& out,
                   std::string& error) {
  if (units == 0) {
    out.clear();
    return true;
  }
  const int length = static_cast<int>(units);
  const int needed =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length, nullptr,
                          0, nullptr, nullptr);
  if (needed <= 0) {
    error = "clipboard text is not valid UTF-16";
    return false;
  }
  out.resize(static_cast<std::size_t>(needed));
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, length,
                          out.data(), needed, nullptr, nullptr) <= 0) {
    error = "UTF-16 to UTF-8 conversion failed";
    return false;
  }
  return true;
}

bool utf8_to_utf16(std::string_view text, std::wstring& out,
                   std::string& error) {
  if (text.empty()) {
    out.clear();
    return true;
  }
  const int length = static_cast<int>(text.size());
  const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         text.data(), length, nullptr, 0);
  if (needed <= 0) {
    error = "text is not valid UTF-8";
    return false;
  }
  out.resize(static_cast<std::size_t>(needed));
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length,
                          out.data(), needed) <= 0) {
    error = "UTF-8 to UTF-16 conversion failed";
    return false;
  }
  return true;
}

ClipboardCapabilities windows_capabilities() {
  ClipboardCapabilities capabilities;
  capabilities.read = true;
  capabilities.write = true;
  capabilities.watch = true;
  capabilities.independent_of_input = true;
  capabilities.backend = "win32-clipboard";
  capabilities.session = "Windows desktop session";
  capabilities.limitation =
      "Another process can hold the clipboard; after bounded retries the "
      "operation reports busy. Elevated (UIPI) applications are not covered.";
  capabilities.notes = {
      "Text only: CF_UNICODETEXT.",
      "Background observation uses AddClipboardFormatListener on a "
      "message-only window; no keyboard or mouse capture.",
      "Line endings are normalized to LF in the model and written as CRLF.",
  };
  return capabilities;
}

}  // namespace

class WindowsClipboardPort final : public ClipboardPort {
 public:
  WindowsClipboardPort() = default;
  ~WindowsClipboardPort() override;

  ClipboardCapabilities capabilities() const override;
  Status read(ClipboardContent& out) override;
  Status write(std::string_view text, std::string_view marker) override;
  WaitResult wait_for_change(ClipboardContent& out, int timeout_ms,
                             std::string& detail) override;
  void request_stop() override;

 private:
  static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                      WPARAM wparam, LPARAM lparam);
  void watcher_main();
  void capture_current();
  void ensure_started(std::string& error);
  void fail_start(const std::string& detail);
  Status read_locked(ClipboardContent& out);
  Status write_locked(std::string_view text, std::string_view marker);
  HWND owner_for_open() const { return hwnd_.load(); }

  std::mutex clipboard_mutex_;
  std::mutex state_mutex_;
  std::condition_variable state_cv_;
  std::optional<ClipboardContent> pending_;
  bool started_ = false;
  bool start_failed_ = false;
  bool stop_requested_ = false;
  std::string start_error_;
  std::atomic<HWND> hwnd_{nullptr};
  std::thread thread_;
};

WindowsClipboardPort::~WindowsClipboardPort() {
  request_stop();
  if (thread_.joinable()) {
    thread_.join();
  }
}

ClipboardCapabilities WindowsClipboardPort::capabilities() const {
  return windows_capabilities();
}

Status WindowsClipboardPort::read(ClipboardContent& out) {
  std::lock_guard<std::mutex> lock(clipboard_mutex_);
  std::string error;
  if (!open_with_retry(owner_for_open(), error)) {
    return {StatusCode::Busy, error};
  }
  const Status status = read_locked(out);
  CloseClipboard();
  return status;
}

Status WindowsClipboardPort::write(std::string_view text,
                                   std::string_view marker) {
  std::lock_guard<std::mutex> lock(clipboard_mutex_);
  return write_locked(text, marker);
}

Status WindowsClipboardPort::read_locked(ClipboardContent& out) {
  if (IsClipboardFormatAvailable(CF_UNICODETEXT) == FALSE) {
    return {StatusCode::Unavailable,
            "clipboard contains no CF_UNICODETEXT"};
  }
  HANDLE handle = GetClipboardData(CF_UNICODETEXT);
  if (handle == nullptr) {
    return {StatusCode::IoError,
            "GetClipboardData(CF_UNICODETEXT) failed with error " +
                std::to_string(GetLastError())};
  }
  const SIZE_T byte_count = GlobalSize(handle);
  if (byte_count < sizeof(wchar_t)) {
    return {StatusCode::InvalidText, "clipboard text is empty"};
  }
  if (byte_count > kMaxTextBytes * 2 + 16) {
    return {StatusCode::TooLarge,
            "clipboard text is larger than the prototype read cap"};
  }

  const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(handle));
  if (text == nullptr) {
    return {StatusCode::IoError, "GlobalLock failed for clipboard text"};
  }
  const std::size_t units = byte_count / sizeof(wchar_t);
  std::size_t length = 0;
  while (length < units && text[length] != L'\0') {
    ++length;
  }
  std::string utf8;
  std::string error;
  const bool converted = utf16_to_utf8(text, length, utf8, error);
  GlobalUnlock(handle);
  if (!converted) {
    return {StatusCode::InvalidText, error};
  }

  ClipboardContent content;
  content.text = normalize_line_endings(utf8);

  const UINT format = probe_format();
  if (format != 0 && IsClipboardFormatAvailable(format) != FALSE) {
    HANDLE marker_handle = GetClipboardData(format);
    if (marker_handle != nullptr) {
      const SIZE_T marker_bytes = GlobalSize(marker_handle);
      if (marker_bytes > 0 && marker_bytes <= 512) {
        const void* raw = GlobalLock(marker_handle);
        if (raw != nullptr) {
          std::string marker(static_cast<const char*>(raw), marker_bytes);
          GlobalUnlock(marker_handle);
          while (!marker.empty() && marker.back() == '\0') {
            marker.pop_back();
          }
          if (marker.size() <= kMaxMarkerBytes) {
            content.marker = std::move(marker);
          }
        }
      }
    }
  }

  out = std::move(content);
  return {StatusCode::Ok, ""};
}

Status WindowsClipboardPort::write_locked(std::string_view text,
                                      std::string_view marker) {
  const Validation validation = validate_text(text, kMaxTextBytes);
  if (!validation.valid) {
    return validation.status;
  }
  const std::string windows_text =
      to_windows_line_endings(validation.normalized);
  std::wstring wide;
  std::string error;
  if (!utf8_to_utf16(windows_text, wide, error)) {
    return {StatusCode::InvalidText, error};
  }

  const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (handle == nullptr) {
    return {StatusCode::IoError, "GlobalAlloc failed for clipboard text"};
  }
  void* target = GlobalLock(handle);
  if (target == nullptr) {
    GlobalFree(handle);
    return {StatusCode::IoError, "GlobalLock failed for clipboard text"};
  }
  std::memcpy(target, wide.c_str(), wide.size() * sizeof(wchar_t));
  static_cast<wchar_t*>(target)[wide.size()] = L'\0';
  GlobalUnlock(handle);

  std::string open_error;
  if (!open_with_retry(owner_for_open(), open_error)) {
    GlobalFree(handle);
    return {StatusCode::Busy, open_error};
  }
  if (EmptyClipboard() == FALSE) {
    const DWORD code = GetLastError();
    CloseClipboard();
    GlobalFree(handle);
    return {StatusCode::IoError,
            "EmptyClipboard failed with error " + std::to_string(code)};
  }
  if (SetClipboardData(CF_UNICODETEXT, handle) == nullptr) {
    const DWORD code = GetLastError();
    CloseClipboard();
    GlobalFree(handle);
    return {StatusCode::IoError,
            "SetClipboardData failed with error " + std::to_string(code)};
  }
  handle = nullptr;  // El sistema es propietario de la memoria.

  bool marker_stored = marker.empty();
  if (!marker.empty() && marker.size() <= kMaxMarkerBytes) {
    const UINT format = probe_format();
    if (format != 0) {
      HGLOBAL marker_handle = GlobalAlloc(GMEM_MOVEABLE, marker.size() + 1);
      if (marker_handle != nullptr) {
        void* marker_target = GlobalLock(marker_handle);
        if (marker_target != nullptr) {
          std::memcpy(marker_target, marker.data(), marker.size());
          static_cast<char*>(marker_target)[marker.size()] = '\0';
          GlobalUnlock(marker_handle);
          if (SetClipboardData(format, marker_handle) != nullptr) {
            marker_stored = true;
          } else {
            GlobalFree(marker_handle);
          }
        } else {
          GlobalFree(marker_handle);
        }
      }
    }
  }
  CloseClipboard();
  if (!marker_stored) {
    return {StatusCode::Ok,
            "text written; internal marker format was not stored"};
  }
  return {StatusCode::Ok, ""};
}

WaitResult WindowsClipboardPort::wait_for_change(ClipboardContent& out,
                                                 int timeout_ms,
                                                 std::string& detail) {
  ensure_started(detail);
  std::unique_lock<std::mutex> lock(state_mutex_);
  if (start_failed_) {
    if (detail.empty()) {
      detail = start_error_;
    }
    return WaitResult::Error;
  }
  if (stop_requested_) {
    return WaitResult::Stopped;
  }
  const auto ready = [this] {
    return pending_.has_value() || stop_requested_ || !started_;
  };
  const std::chrono::milliseconds wait_time(timeout_ms < 0 ? 0 : timeout_ms);
  if (!state_cv_.wait_for(lock, wait_time, ready)) {
    return WaitResult::Timeout;
  }
  if (pending_.has_value()) {
    out = std::move(*pending_);
    pending_.reset();
    return WaitResult::Change;
  }
  if (stop_requested_) {
    return WaitResult::Stopped;
  }
  detail = "clipboard listener stopped unexpectedly";
  return WaitResult::Error;
}

void WindowsClipboardPort::request_stop() {
  HWND window = nullptr;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    stop_requested_ = true;
    window = hwnd_.load();
  }
  state_cv_.notify_all();
  if (window != nullptr) {
    PostMessageW(window, kStopMessage, 0, 0);
  }
}

void WindowsClipboardPort::ensure_started(std::string& error) {
  std::unique_lock<std::mutex> lock(state_mutex_);
  if (started_ || start_failed_) {
    error = start_failed_ ? start_error_ : std::string();
    return;
  }
  if (!thread_.joinable()) {
    thread_ = std::thread(&WindowsClipboardPort::watcher_main, this);
  }
  state_cv_.wait_for(lock, std::chrono::seconds(2),
                     [this] { return started_ || start_failed_; });
  if (!started_) {
    error = start_failed_ ? start_error_
                          : "clipboard listener did not start in time";
  }
}

void WindowsClipboardPort::fail_start(const std::string& detail) {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    start_failed_ = true;
    start_error_ = detail;
  }
  state_cv_.notify_all();
}

void WindowsClipboardPort::capture_current() {
  ClipboardContent content;
  {
    std::lock_guard<std::mutex> lock(clipboard_mutex_);
    std::string error;
    if (!open_with_retry(hwnd_.load(), error)) {
      return;
    }
    const Status status = read_locked(content);
    CloseClipboard();
    if (!status.ok() || content.text.empty()) {
      return;
    }
  }
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (stop_requested_) {
      return;
    }
    pending_ = std::move(content);
  }
  state_cv_.notify_all();
}

void WindowsClipboardPort::watcher_main() {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  static std::once_flag class_once;
  std::call_once(class_once, [instance] {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &WindowsClipboardPort::window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    RegisterClassExW(&window_class);
  });

  HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, instance, this);
  if (window == nullptr) {
    fail_start("message-only window creation failed with error " +
               std::to_string(GetLastError()));
    return;
  }
  if (AddClipboardFormatListener(window) == FALSE) {
    const DWORD code = GetLastError();
    DestroyWindow(window);
    fail_start("AddClipboardFormatListener failed with error " +
               std::to_string(code));
    return;
  }

  hwnd_.store(window);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    started_ = true;
    if (stop_requested_) {
      PostMessageW(window, kStopMessage, 0, 0);
    }
  }
  state_cv_.notify_all();

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  RemoveClipboardFormatListener(window);
  DestroyWindow(window);
  hwnd_.store(nullptr);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    started_ = false;
  }
  state_cv_.notify_all();
}

LRESULT CALLBACK WindowsClipboardPort::window_proc(HWND window, UINT message,
                                                   WPARAM wparam,
                                                   LPARAM lparam) {
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return TRUE;
  }
  auto* self = reinterpret_cast<WindowsClipboardPort*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (self != nullptr) {
    if (message == WM_CLIPBOARDUPDATE) {
      self->capture_current();
      return 0;
    }
    if (message == kStopMessage) {
      PostQuitMessage(0);
      return 0;
    }
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

ClipboardCapabilities clipboard_capabilities() {
  return windows_capabilities();
}

std::string platform_name() { return "windows"; }

std::string platform_inventory_text() {
  std::ostringstream out;
  out << "os=Windows\n";

  using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
  HMODULE module = GetModuleHandleW(L"ntdll.dll");
  if (module != nullptr) {
    const auto function = reinterpret_cast<RtlGetVersionFn>(
        reinterpret_cast<void*>(GetProcAddress(module, "RtlGetVersion")));
    if (function != nullptr) {
      OSVERSIONINFOW version{};
      version.dwOSVersionInfoSize = sizeof(version);
      if (function(&version) == 0) {
        out << "windows_version=" << version.dwMajorVersion << "."
            << version.dwMinorVersion << "." << version.dwBuildNumber
            << "\n";
      }
    }
  }

  DWORD session = 0;
  if (ProcessIdToSessionId(GetCurrentProcessId(), &session) != FALSE) {
    out << "session_id=" << session << "\n";
  }
  out << "process_id=" << GetCurrentProcessId() << "\n";
  out << "clipboard_limit_bytes=" << kMaxTextBytes << "\n";
  return out.str();
}

PortCreation create_clipboard_port() {
  PortCreation creation;
  creation.port = std::make_unique<WindowsClipboardPort>();
  creation.status = {StatusCode::Ok, ""};
  return creation;
}

}  // namespace oit
