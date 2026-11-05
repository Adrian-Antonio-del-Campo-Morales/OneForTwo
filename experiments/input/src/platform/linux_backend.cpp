// Responsabilidad: adaptador Linux/X11. El origen observa el puntero con
// eventos XI2 y captura con XGrabPointer/XGrabKeyboard; el destino inyecta con
// XTest y reserva Ctrl+Alt+F12 con XGrabKey. Wayland no se presenta como
// compatible: el portal InputCapture/libei no está implementado.
#include "core/keymap.hpp"
#include "platform/backend.hpp"
#include "platform/ring.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/select.h>
#include <thread>

namespace oit {
namespace {

bool envIsWayland() {
  const char* type = std::getenv("XDG_SESSION_TYPE");
  if (type != nullptr && std::strcmp(type, "wayland") == 0) return true;
  const char* display = std::getenv("WAYLAND_DISPLAY");
  return display != nullptr && *display != '\0';
}

bool queryXi2(Display* display, int& opcode) {
  int eventBase = 0;
  int errorBase = 0;
  if (XQueryExtension(display, "XInputExtension", &opcode, &eventBase, &errorBase) == False) {
    return false;
  }
  int major = 2;
  int minor = 0;
  return XIQueryVersion(display, &major, &minor) == Success;
}

bool queryXTest(Display* display) {
  int eventBase = 0;
  int errorBase = 0;
  int major = 0;
  int minor = 0;
  return XTestQueryExtension(display, &eventBase, &errorBase, &major, &minor) == True;
}

bool keyIsDown(Display* display, uint16_t usage) {
  const int keycode = hidToX11Keycode(usage);
  if (keycode <= 0) return false;
  char keys[32] = {};
  if (XQueryKeymap(display, keys) != 1) return false;
  return (keys[keycode / 8] & (1 << (keycode % 8))) != 0;
}

void selectRawMotion(Display* display, Window root) {
  unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {};
  XISetMask(mask, XI_RawMotion);
  XIEventMask eventMask;
  eventMask.deviceid = XIAllMasterDevices;
  eventMask.mask_len = sizeof(mask);
  eventMask.mask = mask;
  XISelectEvents(display, root, &eventMask, 1);
  XFlush(display);
}

std::atomic<bool> g_xError{false};

int xErrorHandler(Display*, XErrorEvent*) {
  g_xError.store(true);
  return 0;
}

int clampShort(int value) {
  if (value < -32768) return -32768;
  if (value > 32767) return 32767;
  return value;
}

uint16_t x11ButtonToCode(unsigned int button) {
  switch (button) {
    case 1: return 0;
    case 2: return 2;
    case 3: return 1;
    case 8: return 3;
    case 9: return 4;
    default: return 0xFFFF;
  }
}

unsigned int codeToX11Button(uint16_t button) {
  switch (button) {
    case 0: return 1;
    case 1: return 3;
    case 2: return 2;
    case 3: return 8;
    case 4: return 9;
    default: return 0;
  }
}

}  // namespace

class X11Backend : public Backend {
 public:
  bool init(std::string& error) override;
  Geo geometry() const override { return geo_; }
  int monitorCount() const override { return monitorCount_; }
  BackendCaps caps() const override { return caps_; }
  std::string sessionType() const override { return sessionType_; }

  // X11 no pide autorización a un portal; los grabs se toman en startSource/startTarget.
  bool prepareSource(Edge, std::string&) override { return true; }
  bool prepareTarget(std::string&) override { return true; }

  bool startSource(std::string& error) override;
  void stopSource() override;
  bool setCaptureActive(bool active, std::string& error) override;
  bool nextSourceEvent(RawEvent& event, int waitMs) override;
  void warpLocalCursor(int x, int y) override;

  bool startTarget(std::string& error) override;
  void stopTarget() override;
  void setTargetGuardArmed(bool armed) override { (void)armed; }
  bool nextTargetSignal(TargetSignal& signal, int waitMs) override;

  bool injectMove(int x, int y, std::string& error) override;
  bool injectKey(uint16_t usage, bool pressed, std::string& error) override;
  bool injectButton(uint16_t button, bool pressed, std::string& error) override;
  bool injectWheel(int16_t dx, int16_t dy, std::string& error) override;
  bool cursorPosition(int& x, int& y) override;

 private:
  void sourceThreadMain();
  void handleRequest(Display* display, Window root, int screenWidth, int screenHeight,
                     bool& haveLast, int& lastX, int& lastY, bool& skipMotion,
                     bool& ctrlDown, bool& altDown);
  void processEvent(Display* display, Window root, int screenWidth, int screenHeight,
                    XEvent& event, int xiOpcode, bool& haveLast, int& lastX, int& lastY,
                    bool& skipMotion, bool& ctrlDown, bool& altDown);

  Display* display_ = nullptr;
  Window root_ = 0;
  int screen_ = 0;
  int xiOpcode_ = 0;
  bool xi2_ = false;
  bool xtest_ = false;
  bool wayland_ = false;
  bool targetGrabbed_ = false;
  Geo geo_;
  int monitorCount_ = 1;
  BackendCaps caps_;
  std::string sessionType_;

  std::thread sourceThread_;
  std::atomic<bool> sourceRunning_{false};
  std::atomic<bool> sourceReady_{false};
  std::atomic<bool> sourceCapture_{false};
  std::string sourceError_;
  std::mutex requestMutex_;
  std::condition_variable requestCv_;
  bool requestPending_ = false;
  bool requestActive_ = false;
  bool requestDone_ = false;
  bool requestOk_ = false;
  std::atomic<bool> warpPending_{false};
  std::atomic<int> warpX_{0};
  std::atomic<int> warpY_{0};
  SpscRing<RawEvent, 1024> sourceQueue_;
  SpscRing<TargetSignal, 64> targetQueue_;
};

bool X11Backend::init(std::string& error) {
  (void)error;
  XInitThreads();
  wayland_ = envIsWayland();
  display_ = XOpenDisplay(nullptr);
  if (display_ != nullptr) {
    screen_ = DefaultScreen(display_);
    root_ = RootWindow(display_, screen_);
    geo_.x = 0;
    geo_.y = 0;
    geo_.w = DisplayWidth(display_, screen_);
    geo_.h = DisplayHeight(display_, screen_);
    xi2_ = queryXi2(display_, xiOpcode_);
    xtest_ = queryXTest(display_);
  }

  caps_.name = "Linux X11 (XInput2 + XTest)";
  caps_.capture = !wayland_ && display_ != nullptr && xi2_;
  caps_.inject = !wayland_ && display_ != nullptr && xtest_;
  caps_.targetLocalOverride = false;

  const char* session = std::getenv("XDG_SESSION_TYPE");
  if (wayland_) {
    sessionType_ = "Wayland";
  } else if (display_ != nullptr) {
    sessionType_ = "X11";
  } else {
    sessionType_ = "sin DISPLAY";
  }
  if (session != nullptr) sessionType_ += std::string(" (XDG_SESSION_TYPE=") + session + ")";

  if (wayland_) {
    caps_.notes =
        "Wayland: el portal InputCapture/libei no esta implementado en este prototipo. "
        "Use una sesion 'Ubuntu on Xorg' para esta prueba.";
  } else if (display_ == nullptr) {
    caps_.notes = "Sin DISPLAY: ejecute dentro de una sesion grafica X11.";
  } else {
    caps_.notes =
        "X11 con XInput2 y XTest; se asume evdev y un solo monitor. "
        "El destino no detecta automaticamente actividad fisica local; use Ctrl+Alt+F12.";
    if (!xi2_) caps_.notes += " XInput2 no disponible.";
    if (!xtest_) caps_.notes += " XTest no disponible.";
  }
  return true;
}

bool X11Backend::startSource(std::string& error) {
  if (wayland_) {
    error = "Wayland no soportado: falta la integracion InputCapture/libei";
    return false;
  }
  if (display_ == nullptr) {
    error = "DISPLAY no disponible";
    return false;
  }
  if (!xi2_) {
    error = "XInput2 no disponible; instale libxi-dev y use un servidor X11";
    return false;
  }
  if (sourceThread_.joinable()) {
    error = "la observacion X11 ya esta iniciada";
    return false;
  }

  sourceRunning_.store(true, std::memory_order_relaxed);
  sourceReady_.store(false, std::memory_order_relaxed);
  sourceError_.clear();
  sourceThread_ = std::thread([this] { sourceThreadMain(); });

  for (int i = 0; i < 400; ++i) {
    if (sourceReady_.load(std::memory_order_acquire)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!sourceReady_.load(std::memory_order_acquire)) {
    sourceRunning_.store(false, std::memory_order_relaxed);
    if (sourceThread_.joinable()) sourceThread_.join();
    error = "tiempo de espera agotado iniciando X11";
    return false;
  }
  if (!sourceError_.empty()) {
    error = sourceError_;
    sourceRunning_.store(false, std::memory_order_relaxed);
    if (sourceThread_.joinable()) sourceThread_.join();
    return false;
  }
  return true;
}

void X11Backend::handleRequest(Display* display, Window root, int screenWidth, int screenHeight,
                               bool& haveLast, int& lastX, int& lastY, bool& skipMotion,
                               bool& ctrlDown, bool& altDown) {
  bool pending = false;
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(requestMutex_);
    pending = requestPending_;
    active = requestActive_;
    requestPending_ = false;
  }
  if (!pending) return;

  bool ok = true;
  if (active) {
    if (!sourceCapture_.load(std::memory_order_relaxed)) {
      const int pointerStatus =
          XGrabPointer(display, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                       GrabModeAsync, GrabModeAsync, root, None, CurrentTime);
      if (pointerStatus == GrabSuccess) {
        const int keyboardStatus =
            XGrabKeyboard(display, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
        if (keyboardStatus == GrabSuccess) {
          sourceCapture_.store(true, std::memory_order_relaxed);
          skipMotion = true;
          ctrlDown = false;
          altDown = false;
          const int cx = screenWidth / 2;
          const int cy = screenHeight / 2;
          XWarpPointer(display, None, root, 0, 0, 0, 0, cx, cy);
          XFlush(display);
          lastX = cx;
          lastY = cy;
          haveLast = true;
        } else {
          XUngrabPointer(display, CurrentTime);
          ok = false;
        }
      } else {
        ok = false;
      }
    }
  } else if (sourceCapture_.load(std::memory_order_relaxed)) {
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    sourceCapture_.store(false, std::memory_order_relaxed);
    XFlush(display);
  }

  {
    std::lock_guard<std::mutex> lock(requestMutex_);
    requestDone_ = true;
    requestOk_ = ok;
  }
  requestCv_.notify_all();
}

void X11Backend::processEvent(Display* display, Window root, int screenWidth, int screenHeight,
                              XEvent& event, int xiOpcode, bool& haveLast, int& lastX, int& lastY,
                              bool& skipMotion, bool& ctrlDown, bool& altDown) {
  if (event.type == GenericEvent && event.xcookie.extension == xiOpcode) {
    if (XGetEventData(display, &event.xcookie) == True) {
      if (event.xcookie.evtype == XI_RawMotion &&
          !sourceCapture_.load(std::memory_order_relaxed)) {
        Window rootReturn = 0;
        Window childReturn = 0;
        int rootX = 0;
        int rootY = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY,
                          &mask) == True) {
          RawEvent raw;
          raw.kind = RawKind::MotionAbs;
          raw.x = rootX;
          raw.y = rootY;
          sourceQueue_.push(raw);
        }
      }
      XFreeEventData(display, &event.xcookie);
    }
    return;
  }

  if (!sourceCapture_.load(std::memory_order_relaxed)) return;

  if (event.type == MotionNotify) {
    const int x = event.xmotion.x_root;
    const int y = event.xmotion.y_root;
    if (skipMotion) {
      skipMotion = false;
    } else if (haveLast) {
      const int dx = clampShort(x - lastX);
      const int dy = clampShort(y - lastY);
      if (dx != 0 || dy != 0) {
        RawEvent raw;
        raw.kind = RawKind::MotionDelta;
        raw.dx = static_cast<int16_t>(dx);
        raw.dy = static_cast<int16_t>(dy);
        sourceQueue_.push(raw);
      }
    }
    lastX = x;
    lastY = y;
    haveLast = true;

    constexpr int kMargin = 48;
    if (x < kMargin || x > screenWidth - 1 - kMargin || y < kMargin ||
        y > screenHeight - 1 - kMargin) {
      const int cx = screenWidth / 2;
      const int cy = screenHeight / 2;
      XWarpPointer(display, None, root, 0, 0, 0, 0, cx, cy);
      XFlush(display);
      lastX = cx;
      lastY = cy;
      haveLast = true;
      skipMotion = true;
    }
    return;
  }

  if (event.type == ButtonPress || event.type == ButtonRelease) {
    const unsigned int button = event.xbutton.button;
    const bool pressed = event.type == ButtonPress;
    if (button >= 4 && button <= 7) {
      RawEvent raw;
      raw.kind = RawKind::Wheel;
      if (button == 4) raw.dy = 1;
      if (button == 5) raw.dy = -1;
      if (button == 6) raw.dx = -1;
      if (button == 7) raw.dx = 1;
      sourceQueue_.push(raw);
    } else {
      const uint16_t code = x11ButtonToCode(button);
      if (code != 0xFFFF) {
        RawEvent raw;
        raw.kind = RawKind::Button;
        raw.code = code;
        raw.pressed = pressed;
        sourceQueue_.push(raw);
      }
    }
    return;
  }

  if (event.type == KeyPress || event.type == KeyRelease) {
    const uint16_t usage = x11KeycodeToHid(event.xkey.keycode);
    if (usage == 0) return;
    const bool pressed = event.type == KeyPress;
    if (usage == hid::LeftCtrl || usage == hid::RightCtrl) ctrlDown = pressed;
    if (usage == hid::LeftAlt || usage == hid::RightAlt) altDown = pressed;

    // Consulta el estado real del servidor: Ctrl/Alt pueden estar pulsados
    // antes de que se instalara el grab y no haberse observado su press.
    const bool emergency =
        pressed && usage == hid::F12 &&
        (ctrlDown || keyIsDown(display, hid::LeftCtrl) || keyIsDown(display, hid::RightCtrl)) &&
        (altDown || keyIsDown(display, hid::LeftAlt) || keyIsDown(display, hid::RightAlt));
    if (emergency) {
      RawEvent hotkey;
      hotkey.kind = RawKind::Hotkey;
      sourceQueue_.push(hotkey);
      return;
    }

    RawEvent raw;
    raw.kind = RawKind::Key;
    raw.code = usage;
    raw.pressed = pressed;
    sourceQueue_.push(raw);
  }
}

void X11Backend::sourceThreadMain() {
  Display* display = XOpenDisplay(nullptr);
  if (display == nullptr) {
    sourceError_ = "no se pudo abrir DISPLAY en el hilo de captura";
    sourceReady_.store(true, std::memory_order_release);
    return;
  }

  const int screen = DefaultScreen(display);
  const Window root = RootWindow(display, screen);
  const int screenWidth = DisplayWidth(display, screen);
  const int screenHeight = DisplayHeight(display, screen);
  int xiOpcode = 0;
  if (!queryXi2(display, xiOpcode)) {
    sourceError_ = "XInput2 no disponible en el hilo de captura";
    sourceReady_.store(true, std::memory_order_release);
    XCloseDisplay(display);
    return;
  }
  selectRawMotion(display, root);
  sourceReady_.store(true, std::memory_order_release);

  bool haveLast = false;
  int lastX = 0;
  int lastY = 0;
  bool skipMotion = false;
  bool ctrlDown = false;
  bool altDown = false;

  while (sourceRunning_.load(std::memory_order_relaxed)) {
    handleRequest(display, root, screenWidth, screenHeight, haveLast, lastX, lastY, skipMotion,
                  ctrlDown, altDown);

    while (XPending(display) > 0) {
      XEvent event;
      XNextEvent(display, &event);
      processEvent(display, root, screenWidth, screenHeight, event, xiOpcode, haveLast, lastX,
                   lastY, skipMotion, ctrlDown, altDown);
    }

    if (warpPending_.exchange(false)) {
      const int wx = warpX_.load(std::memory_order_relaxed);
      const int wy = warpY_.load(std::memory_order_relaxed);
      XWarpPointer(display, None, root, 0, 0, 0, 0, wx, wy);
      XFlush(display);
      lastX = wx;
      lastY = wy;
      haveLast = true;
      skipMotion = true;
    }

    fd_set readable;
    FD_ZERO(&readable);
    const int descriptor = ConnectionNumber(display);
    FD_SET(descriptor, &readable);
    timeval timeout{0, 5000};
    select(descriptor + 1, &readable, nullptr, nullptr, &timeout);
  }

  if (sourceCapture_.load(std::memory_order_relaxed)) {
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    sourceCapture_.store(false, std::memory_order_relaxed);
  }
  sourceReady_.store(false, std::memory_order_release);
  XCloseDisplay(display);
}

void X11Backend::stopSource() {
  sourceRunning_.store(false, std::memory_order_relaxed);
  if (sourceThread_.joinable()) sourceThread_.join();
  sourceCapture_.store(false, std::memory_order_relaxed);
}

bool X11Backend::setCaptureActive(bool active, std::string& error) {
  if (!sourceThread_.joinable()) {
    error = "la observacion X11 no esta iniciada";
    return false;
  }
  {
    std::unique_lock<std::mutex> lock(requestMutex_);
    requestPending_ = true;
    requestActive_ = active;
    requestDone_ = false;
    requestOk_ = false;
    requestCv_.notify_all();
    if (!requestCv_.wait_for(lock, std::chrono::seconds(2), [this] { return requestDone_; })) {
      error = "X11 no respondio a la solicitud de captura";
      return false;
    }
    if (active && !requestOk_) {
      error = "no se pudieron obtener los grabs de X11";
      return false;
    }
  }
  return true;
}

bool X11Backend::nextSourceEvent(RawEvent& event, int waitMs) {
  for (;;) {
    if (sourceQueue_.pop(event)) return true;
    if (waitMs <= 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    --waitMs;
  }
}

void X11Backend::warpLocalCursor(int x, int y) {
  warpX_.store(x, std::memory_order_relaxed);
  warpY_.store(y, std::memory_order_relaxed);
  warpPending_.store(true, std::memory_order_relaxed);
}

bool X11Backend::startTarget(std::string& error) {
  if (wayland_) {
    error = "Wayland no soportado: falta la integracion InputCapture/libei";
    return false;
  }
  if (display_ == nullptr) {
    error = "DISPLAY no disponible";
    return false;
  }
  if (!xtest_) {
    error = "XTest no disponible; instale libxtst-dev";
    return false;
  }
  if (targetGrabbed_) {
    error = "el destino X11 ya esta preparado";
    return false;
  }

  const int keycode = hidToX11Keycode(hid::F12);
  if (keycode == 0) {
    error = "no hay conversion X11 para F12";
    return false;
  }

  g_xError.store(false);
  XErrorHandler previous = XSetErrorHandler(xErrorHandler);
  const unsigned int modifiers = ControlMask | Mod1Mask;
  const unsigned int lockVariants[] = {0u, LockMask, Mod2Mask, LockMask | Mod2Mask};
  for (unsigned int extra : lockVariants) {
    XGrabKey(display_, keycode, modifiers | extra, root_, False, GrabModeAsync, GrabModeAsync);
  }
  XSync(display_, False);
  XSetErrorHandler(previous);

  if (g_xError.load()) {
    for (unsigned int extra : lockVariants) {
      XUngrabKey(display_, keycode, modifiers | extra, root_);
    }
    XSync(display_, False);
    error = "no se pudo reservar Ctrl+Alt+F12 (otro cliente puede tenerlo)";
    return false;
  }

  targetGrabbed_ = true;
  return true;
}

void X11Backend::stopTarget() {
  if (display_ == nullptr || !targetGrabbed_) return;
  const int keycode = hidToX11Keycode(hid::F12);
  const unsigned int modifiers = ControlMask | Mod1Mask;
  const unsigned int lockVariants[] = {0u, LockMask, Mod2Mask, LockMask | Mod2Mask};
  for (unsigned int extra : lockVariants) {
    XUngrabKey(display_, keycode, modifiers | extra, root_);
  }
  XSync(display_, False);
  targetGrabbed_ = false;
}

bool X11Backend::nextTargetSignal(TargetSignal& signal, int waitMs) {
  for (;;) {
    if (display_ != nullptr) {
      while (XPending(display_) > 0) {
        XEvent event;
        XNextEvent(display_, &event);
        if (event.type == KeyPress && event.xkey.keycode == hidToX11Keycode(hid::F12)) {
          TargetSignal hotkey;
          hotkey.kind = TargetSignal::Kind::Emergency;
          targetQueue_.push(hotkey);
        }
      }
      if (targetQueue_.pop(signal)) return true;
    }
    if (waitMs <= 0) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    --waitMs;
  }
}

bool X11Backend::injectMove(int x, int y, std::string& error) {
  if (display_ == nullptr || !xtest_) {
    error = "XTest no disponible";
    return false;
  }
  XTestFakeMotionEvent(display_, screen_, x, y, 0);
  XFlush(display_);
  return true;
}

bool X11Backend::injectKey(uint16_t usage, bool pressed, std::string& error) {
  if (display_ == nullptr || !xtest_) {
    error = "XTest no disponible";
    return false;
  }
  const int keycode = hidToX11Keycode(usage);
  if (keycode == 0) {
    error = "tecla sin conversion X11";
    return false;
  }
  XTestFakeKeyEvent(display_, static_cast<unsigned int>(keycode), pressed ? True : False, 0);
  XFlush(display_);
  return true;
}

bool X11Backend::injectButton(uint16_t button, bool pressed, std::string& error) {
  if (display_ == nullptr || !xtest_) {
    error = "XTest no disponible";
    return false;
  }
  const unsigned int xbutton = codeToX11Button(button);
  if (xbutton == 0) {
    error = "boton de raton no soportado";
    return false;
  }
  XTestFakeButtonEvent(display_, xbutton, pressed ? True : False, 0);
  XFlush(display_);
  return true;
}

bool X11Backend::injectWheel(int16_t dx, int16_t dy, std::string& error) {
  if (display_ == nullptr || !xtest_) {
    error = "XTest no disponible";
    return false;
  }
  const int maxTicks = 16;
  const int vertical = std::max(-maxTicks, std::min(maxTicks, static_cast<int>(dy)));
  const int horizontal = std::max(-maxTicks, std::min(maxTicks, static_cast<int>(dx)));

  auto tap = [this](unsigned int button) {
    XTestFakeButtonEvent(display_, button, True, 0);
    XTestFakeButtonEvent(display_, button, False, 0);
  };
  for (int i = 0; i < vertical; ++i) tap(4);
  for (int i = 0; i < -vertical; ++i) tap(5);
  for (int i = 0; i < horizontal; ++i) tap(7);
  for (int i = 0; i < -horizontal; ++i) tap(6);
  XFlush(display_);
  return true;
}

bool X11Backend::cursorPosition(int& x, int& y) {
  if (display_ == nullptr) return false;
  Window rootReturn = 0;
  Window childReturn = 0;
  int rootX = 0;
  int rootY = 0;
  int winX = 0;
  int winY = 0;
  unsigned int mask = 0;
  if (XQueryPointer(display_, root_, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY,
                    &mask) != True) {
    return false;
  }
  x = rootX;
  y = rootY;
  return true;
}

std::unique_ptr<Backend> makeX11Backend() {
  return std::unique_ptr<Backend>(new X11Backend());
}

}  // namespace oit
