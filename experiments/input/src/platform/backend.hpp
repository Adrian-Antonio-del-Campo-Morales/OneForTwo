#pragma once
// Responsibility: common contract of the desktop adapters. The kernel does not
// conoce Windows, X11, hooks ni portales.

#include "core/types.hpp"

#include <memory>
#include <string>

namespace oit {

struct BackendCaps {
  std::string name;
  bool capture = false;
  bool inject = false;
  bool targetLocalOverride = false;
  std::string notes;
};

struct TargetSignal {
  enum class Kind : uint8_t { LocalOverride, Emergency, Stop };
  Kind kind = Kind::Stop;
};

class Backend {
 public:
  virtual ~Backend() = default;

  virtual bool init(std::string& error) = 0;
  virtual Geo geometry() const = 0;
  virtual int monitorCount() const = 0;
  virtual BackendCaps caps() const = 0;
  virtual std::string sessionType() const = 0;

  // Authorizes and prepares the desk mechanism before requiring geometry or
  // red: portal InputCapture (origen) o RemoteDesktop + GlobalShortcuts
  // (destiny) Backends without prior authorization implement it as non-op.
  virtual bool prepareSource(Edge edge, std::string& error) = 0;
  virtual bool prepareTarget(std::string& error) = 0;

  // Wayland separates it from RemoteDesktop/EIS to test the output without control.
  virtual bool prepareTargetExit(std::string& error) {
    return prepareTarget(error) && startTarget(error);
  }
  virtual const char* targetExitShortcut() const { return "Ctrl+Alt+F12"; }

  // Origin: Observe the desktop unblocked until capture is activated.
  virtual bool startSource(std::string& error) = 0;
  virtual void stopSource() = 0;
  virtual bool setCaptureActive(bool active, std::string& error) = 0;
  virtual bool nextSourceEvent(RawEvent& event, int waitMs) = 0;
  virtual void warpLocalCursor(int x, int y) = 0;
  // Describes an unrecoverable catch failure (e.g. closed portal).
  virtual bool sourceFatal(std::string& reason) {
    (void)reason;
    return false;
  }

  // Destination: install the local guards and enable the injection.
  virtual bool startTarget(std::string& error) = 0;
  virtual void stopTarget() = 0;
  virtual void setTargetGuardArmed(bool armed) = 0;
  virtual bool nextTargetSignal(TargetSignal& signal, int waitMs) = 0;

  virtual bool injectMove(int x, int y, std::string& error) = 0;
  virtual bool injectKey(uint16_t usage, bool pressed, std::string& error) = 0;
  virtual bool injectButton(uint16_t button, bool pressed, std::string& error) = 0;
  virtual bool injectWheel(int16_t dx, int16_t dy, std::string& error) = 0;
  virtual bool cursorPosition(int& x, int& y) = 0;
};

std::unique_ptr<Backend> makePlatformBackend(std::string& error);

#ifdef OIT_WITH_WAYLAND
// Response regressions and shortcut signals in memory; no bus or desktop.
bool runWaylandPortalResponseChecks();
#endif

}  // namespace oit
