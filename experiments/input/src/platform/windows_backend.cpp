// Responsabilidad: adaptador Windows. Captura con WH_KEYBOARD_LL y
// WH_MOUSE_LL, inyecta con SendInput y ofrece una salida local Ctrl+Alt+F12.
// No se inicializa en self-tests; solo lo usan los modos manuales.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "core/keymap.hpp"
#include "platform/backend.hpp"
#include "platform/ring.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace oit {
class WinBackend;
}

static oit::WinBackend* g_winBackend = nullptr;
static LRESULT CALLBACK oitKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK oitMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

namespace oit {
namespace {

constexpr ULONG_PTR kInjectedMarker = static_cast<ULONG_PTR>(0x4F495432);  // "OIT2"

int clampInt16(int value) {
  if (value < -32768) return -32768;
  if (value > 32767) return 32767;
  return value;
}

BOOL CALLBACK countMonitorProc(HMONITOR, HDC, LPRECT, LPARAM data) {
  int* count = reinterpret_cast<int*>(data);
  ++(*count);
  return TRUE;
}

}  // namespace

class WinBackend : public Backend {
 public:
  bool init(std::string& error) override;
  Geo geometry() const override { return geo_; }
  int monitorCount() const override { return monitorCount_; }
  BackendCaps caps() const override { return caps_; }
  std::string sessionType() const override { return sessionType_; }

  // Windows instala sus hooks en startSource/startTarget; no hay autorización previa.
  bool prepareSource(Edge, std::string&) override { return true; }
  bool prepareTarget(std::string&) override { return true; }

  bool startSource(std::string& error) override;
  void stopSource() override;
  bool setCaptureActive(bool active, std::string& error) override;
  bool nextSourceEvent(RawEvent& event, int waitMs) override;
  void warpLocalCursor(int x, int y) override;

  bool startTarget(std::string& error) override;
  void stopTarget() override;
  void setTargetGuardArmed(bool armed) override;
  bool nextTargetSignal(TargetSignal& signal, int waitMs) override;

  bool injectMove(int x, int y, std::string& error) override;
  bool injectKey(uint16_t usage, bool pressed, std::string& error) override;
  bool injectButton(uint16_t button, bool pressed, std::string& error) override;
  bool injectWheel(int16_t dx, int16_t dy, std::string& error) override;
  bool cursorPosition(int& x, int& y) override;

  LRESULT handleKey(int nCode, WPARAM wParam, LPARAM lParam);
  LRESULT handleMouse(int nCode, WPARAM wParam, LPARAM lParam);

 private:
  enum class Role { None, Source, Target };

  bool startHooks(Role role, std::string& error);
  void stopHooks();
  void hookThreadMain();
  void pushSource(const RawEvent& event);
  void pushTargetSignal(TargetSignal::Kind kind);
  void pushButton(uint16_t button, bool pressed);
  void recenterIfNearEdge(POINT point);
  bool sendMouseInput(const INPUT& input, std::string& error);
  bool sendKeyInput(const INPUT& input, std::string& error);

  Geo geo_;
  int monitorCount_ = 1;
  bool remoteSession_ = false;
  BackendCaps caps_;
  std::string sessionType_;

  Role role_ = Role::None;
  std::thread hookThread_;
  std::atomic<bool> hookRunning_{false};
  std::atomic<DWORD> hookThreadId_{0};
  std::atomic<bool> hookReady_{false};
  std::string hookError_;
  std::atomic<bool> captureActive_{false};
  std::atomic<bool> guardArmed_{false};
  std::atomic<bool> resetMouse_{false};
  std::atomic<int> lastMouseX_{0};
  std::atomic<int> lastMouseY_{0};
  std::atomic<bool> haveLastMouse_{false};

  SpscRing<RawEvent, 1024> sourceQueue_;
  SpscRing<TargetSignal, 64> targetQueue_;
  std::atomic<bool> targetSignalPending_{false};
};

bool WinBackend::init(std::string& error) {
  g_winBackend = this;
  SetProcessDPIAware();

  geo_.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  geo_.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  geo_.w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  geo_.h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (!geo_.valid()) {
    error = "no se pudo obtener la geometría del escritorio virtual";
    return false;
  }

  int monitors = 0;
  EnumDisplayMonitors(nullptr, nullptr, countMonitorProc, reinterpret_cast<LPARAM>(&monitors));
  monitorCount_ = std::max(1, monitors);
  remoteSession_ = GetSystemMetrics(SM_REMOTESESSION) != 0;
  sessionType_ = remoteSession_ ? "Windows, sesión remota" : "Windows, sesión local";

  caps_.name = "Windows Win32 (hooks + SendInput)";
  caps_.capture = true;
  caps_.inject = true;
  caps_.targetLocalOverride = true;
  caps_.notes =
      "Una instancia por proceso. Ventanas elevadas fuera del alcance por UIPI. "
      "Se espera un monitor por equipo; la detección de actividad física del "
      "destino solo está disponible en Windows.";
  return true;
}

bool WinBackend::startHooks(Role role, std::string& error) {
  if (hookThread_.joinable()) {
    error = "los hooks ya están iniciados";
    return false;
  }
  role_ = role;
  hookRunning_.store(true, std::memory_order_relaxed);
  hookReady_.store(false, std::memory_order_relaxed);
  hookError_.clear();
  hookThread_ = std::thread([this] { hookThreadMain(); });

  for (int i = 0; i < 400; ++i) {
    if (hookReady_.load(std::memory_order_acquire)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!hookReady_.load(std::memory_order_acquire)) {
    hookRunning_.store(false, std::memory_order_relaxed);
    stopHooks();
    error = "tiempo de espera agotado instalando los hooks";
    return false;
  }
  if (!hookError_.empty()) {
    error = hookError_;
    stopHooks();
    return false;
  }
  return true;
}

void WinBackend::hookThreadMain() {
  hookThreadId_.store(GetCurrentThreadId(), std::memory_order_relaxed);

  HHOOK keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, oitKeyboardProc, GetModuleHandleW(nullptr), 0);
  HHOOK mouse = SetWindowsHookExW(WH_MOUSE_LL, oitMouseProc, GetModuleHandleW(nullptr), 0);
  if (keyboard == nullptr || mouse == nullptr) {
    if (keyboard) UnhookWindowsHookEx(keyboard);
    if (mouse) UnhookWindowsHookEx(mouse);
    hookError_ = "no se pudieron instalar los hooks de entrada";
    hookReady_.store(true, std::memory_order_release);
    return;
  }

  MSG message;
  PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  hookReady_.store(true, std::memory_order_release);

  while (hookRunning_.load(std::memory_order_relaxed)) {
    const BOOL result = GetMessageW(&message, nullptr, 0, 0);
    if (result <= 0) break;
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  UnhookWindowsHookEx(keyboard);
  UnhookWindowsHookEx(mouse);
}

void WinBackend::stopHooks() {
  hookRunning_.store(false, std::memory_order_relaxed);
  const DWORD id = hookThreadId_.load(std::memory_order_relaxed);
  if (id != 0) PostThreadMessageW(id, WM_QUIT, 0, 0);
  if (hookThread_.joinable()) hookThread_.join();
  role_ = Role::None;
  captureActive_.store(false, std::memory_order_relaxed);
  guardArmed_.store(false, std::memory_order_relaxed);
}

bool WinBackend::startSource(std::string& error) { return startHooks(Role::Source, error); }

void WinBackend::stopSource() { stopHooks(); }

bool WinBackend::setCaptureActive(bool active, std::string& error) {
  if (role_ != Role::Source) {
    error = "la captura solo puede activarse en el origen";
    return false;
  }
  resetMouse_.store(true, std::memory_order_relaxed);
  haveLastMouse_.store(false, std::memory_order_relaxed);
  captureActive_.store(active, std::memory_order_relaxed);
  return true;
}

bool WinBackend::startTarget(std::string& error) {
  guardArmed_.store(false, std::memory_order_relaxed);
  return startHooks(Role::Target, error);
}

void WinBackend::stopTarget() { stopHooks(); }

void WinBackend::setTargetGuardArmed(bool armed) {
  guardArmed_.store(armed, std::memory_order_relaxed);
}

void WinBackend::pushSource(const RawEvent& event) {
  sourceQueue_.push(event);
}

void WinBackend::pushTargetSignal(TargetSignal::Kind kind) {
  bool expected = false;
  if (!targetSignalPending_.compare_exchange_strong(expected, true)) return;
  TargetSignal signal;
  signal.kind = kind;
  if (!targetQueue_.push(signal)) {
    targetSignalPending_.store(false, std::memory_order_relaxed);
  }
}

void WinBackend::pushButton(uint16_t button, bool pressed) {
  RawEvent event;
  event.kind = RawKind::Button;
  event.code = button;
  event.pressed = pressed;
  sourceQueue_.push(event);
}

void WinBackend::recenterIfNearEdge(POINT point) {
  constexpr int kMargin = 48;
  const bool inside = point.x > geo_.x + kMargin && point.x < geo_.x + geo_.w - kMargin &&
                      point.y > geo_.y + kMargin && point.y < geo_.y + geo_.h - kMargin;
  if (inside) return;
  const int cx = geo_.x + geo_.w / 2;
  const int cy = geo_.y + geo_.h / 2;
  SetCursorPos(cx, cy);
  lastMouseX_.store(cx, std::memory_order_relaxed);
  lastMouseY_.store(cy, std::memory_order_relaxed);
}

LRESULT WinBackend::handleKey(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode != HC_ACTION) return CallNextHookEx(nullptr, nCode, wParam, lParam);
  const KBDLLHOOKSTRUCT& info = *reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
  if ((info.flags & LLKHF_INJECTED) != 0 || info.dwExtraInfo == kInjectedMarker) {
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
  const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  const bool hotkey = down && info.vkCode == VK_F12 && ctrl && alt;

  if (role_ == Role::Target) {
    if (guardArmed_.load(std::memory_order_relaxed)) {
      pushTargetSignal(hotkey ? TargetSignal::Kind::Emergency : TargetSignal::Kind::LocalOverride);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  if (hotkey && captureActive_.load(std::memory_order_relaxed)) {
    // Restaura la entrada local de inmediato; el bucle principal envía la liberación.
    captureActive_.store(false, std::memory_order_relaxed);
    RawEvent event;
    event.kind = RawKind::Hotkey;
    sourceQueue_.push(event);
    return 1;
  }

  if (!captureActive_.load(std::memory_order_relaxed)) {
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  uint16_t usage = 0;
  if (info.vkCode == VK_PAUSE) {
    usage = 0x48;
  } else {
    usage = winScanToHid(info.scanCode, (info.flags & LLKHF_EXTENDED) != 0);
  }
  if (usage != 0) {
    RawEvent event;
    event.kind = RawKind::Key;
    event.code = usage;
    event.pressed = down;
    sourceQueue_.push(event);
  }
  return 1;
}

LRESULT WinBackend::handleMouse(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode != HC_ACTION) return CallNextHookEx(nullptr, nCode, wParam, lParam);
  const MSLLHOOKSTRUCT& info = *reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
  if ((info.flags & LLMHF_INJECTED) != 0 || info.dwExtraInfo == kInjectedMarker) {
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  if (role_ == Role::Target) {
    if (guardArmed_.load(std::memory_order_relaxed)) {
      pushTargetSignal(TargetSignal::Kind::LocalOverride);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  const bool active = captureActive_.load(std::memory_order_relaxed);
  if (!active) {
    if (wParam == WM_MOUSEMOVE) {
      RawEvent event;
      event.kind = RawKind::MotionAbs;
      event.x = info.pt.x;
      event.y = info.pt.y;
      sourceQueue_.push(event);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  switch (wParam) {
    case WM_MOUSEMOVE: {
      int dx = info.pt.x - lastMouseX_.load(std::memory_order_relaxed);
      int dy = info.pt.y - lastMouseY_.load(std::memory_order_relaxed);
      if (resetMouse_.exchange(false, std::memory_order_relaxed) ||
          !haveLastMouse_.load(std::memory_order_relaxed)) {
        dx = 0;
        dy = 0;
        haveLastMouse_.store(true, std::memory_order_relaxed);
      }
      lastMouseX_.store(info.pt.x, std::memory_order_relaxed);
      lastMouseY_.store(info.pt.y, std::memory_order_relaxed);
      RawEvent event;
      event.kind = RawKind::MotionDelta;
      event.dx = static_cast<int16_t>(clampInt16(dx));
      event.dy = static_cast<int16_t>(clampInt16(dy));
      sourceQueue_.push(event);
      recenterIfNearEdge(info.pt);
      break;
    }
    case WM_LBUTTONDOWN:
      pushButton(0, true);
      break;
    case WM_LBUTTONUP:
      pushButton(0, false);
      break;
    case WM_RBUTTONDOWN:
      pushButton(1, true);
      break;
    case WM_RBUTTONUP:
      pushButton(1, false);
      break;
    case WM_MBUTTONDOWN:
      pushButton(2, true);
      break;
    case WM_MBUTTONUP:
      pushButton(2, false);
      break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP: {
      const WORD button = HIWORD(info.mouseData);
      const uint16_t code = button == XBUTTON1 ? 3 : 4;
      pushButton(code, wParam == WM_XBUTTONDOWN);
      break;
    }
    case WM_MOUSEWHEEL: {
      const int delta = GET_WHEEL_DELTA_WPARAM(static_cast<WPARAM>(info.mouseData));
      RawEvent event;
      event.kind = RawKind::Wheel;
      event.dy = static_cast<int16_t>(delta / WHEEL_DELTA);
      sourceQueue_.push(event);
      break;
    }
    case WM_MOUSEHWHEEL: {
      const int delta = GET_WHEEL_DELTA_WPARAM(static_cast<WPARAM>(info.mouseData));
      RawEvent event;
      event.kind = RawKind::Wheel;
      event.dx = static_cast<int16_t>(delta / WHEEL_DELTA);
      sourceQueue_.push(event);
      break;
    }
    default:
      break;
  }
  return 1;
}

bool WinBackend::nextSourceEvent(RawEvent& event, int waitMs) {
  for (;;) {
    if (sourceQueue_.pop(event)) return true;
    if (waitMs <= 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    --waitMs;
  }
}

bool WinBackend::nextTargetSignal(TargetSignal& signal, int waitMs) {
  for (;;) {
    if (targetQueue_.pop(signal)) {
      targetSignalPending_.store(false, std::memory_order_relaxed);
      return true;
    }
    if (waitMs <= 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    --waitMs;
  }
}

void WinBackend::warpLocalCursor(int x, int y) {
  SetCursorPos(x, y);
  resetMouse_.store(true, std::memory_order_relaxed);
  haveLastMouse_.store(false, std::memory_order_relaxed);
}

bool WinBackend::sendMouseInput(const INPUT& input, std::string& error) {
  if (SendInput(1, const_cast<INPUT*>(&input), sizeof(INPUT)) != 1) {
    error = "SendInput de ratón falló (¿ventana elevada o escritorio bloqueado?)";
    return false;
  }
  return true;
}

bool WinBackend::sendKeyInput(const INPUT& input, std::string& error) {
  if (SendInput(1, const_cast<INPUT*>(&input), sizeof(INPUT)) != 1) {
    error = "SendInput de teclado falló (¿ventana elevada o escritorio bloqueado?)";
    return false;
  }
  return true;
}

bool WinBackend::injectMove(int x, int y, std::string& error) {
  const long long spanX = std::max<long long>(1, geo_.w - 1);
  const long long spanY = std::max<long long>(1, geo_.h - 1);
  long long nx = (static_cast<long long>(x - geo_.x) * 65535) / spanX;
  long long ny = (static_cast<long long>(y - geo_.y) * 65535) / spanY;
  nx = std::max<long long>(0, std::min<long long>(65535, nx));
  ny = std::max<long long>(0, std::min<long long>(65535, ny));

  INPUT input{};
  input.type = INPUT_MOUSE;
  input.mi.dx = static_cast<LONG>(nx);
  input.mi.dy = static_cast<LONG>(ny);
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  input.mi.dwExtraInfo = kInjectedMarker;
  return sendMouseInput(input, error);
}

bool WinBackend::injectKey(uint16_t usage, bool pressed, std::string& error) {
  uint32_t scan = 0;
  bool extended = false;
  if (!hidToWinScan(usage, scan, extended)) {
    error = "tecla sin conversión a Windows";
    return false;
  }
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wScan = static_cast<WORD>(scan);
  input.ki.dwFlags = KEYEVENTF_SCANCODE;
  if (extended) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  if (!pressed) input.ki.dwFlags |= KEYEVENTF_KEYUP;
  input.ki.dwExtraInfo = kInjectedMarker;
  return sendKeyInput(input, error);
}

bool WinBackend::injectButton(uint16_t button, bool pressed, std::string& error) {
  INPUT input{};
  input.type = INPUT_MOUSE;
  input.mi.dwExtraInfo = kInjectedMarker;
  switch (button) {
    case 0:
      input.mi.dwFlags = pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
      break;
    case 1:
      input.mi.dwFlags = pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
      break;
    case 2:
      input.mi.dwFlags = pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
      break;
    case 3:
    case 4:
      input.mi.dwFlags = pressed ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
      input.mi.mouseData = button == 3 ? XBUTTON1 : XBUTTON2;
      break;
    default:
      error = "botón de ratón no soportado";
      return false;
  }
  return sendMouseInput(input, error);
}

bool WinBackend::injectWheel(int16_t dx, int16_t dy, std::string& error) {
  if (dy != 0) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(static_cast<int>(dy) * WHEEL_DELTA);
    input.mi.dwExtraInfo = kInjectedMarker;
    if (!sendMouseInput(input, error)) return false;
  }
  if (dx != 0) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    input.mi.mouseData = static_cast<DWORD>(static_cast<int>(dx) * WHEEL_DELTA);
    input.mi.dwExtraInfo = kInjectedMarker;
    if (!sendMouseInput(input, error)) return false;
  }
  return true;
}

bool WinBackend::cursorPosition(int& x, int& y) {
  POINT point{};
  if (!GetCursorPos(&point)) return false;
  x = point.x;
  y = point.y;
  return true;
}

std::unique_ptr<Backend> makePlatformBackend(std::string& error) {
  (void)error;
  return std::unique_ptr<Backend>(new WinBackend());
}

}  // namespace oit

static LRESULT CALLBACK oitKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (g_winBackend == nullptr) return CallNextHookEx(nullptr, nCode, wParam, lParam);
  return g_winBackend->handleKey(nCode, wParam, lParam);
}

static LRESULT CALLBACK oitMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (g_winBackend == nullptr) return CallNextHookEx(nullptr, nCode, wParam, lParam);
  return g_winBackend->handleMouse(nCode, wParam, lParam);
}
