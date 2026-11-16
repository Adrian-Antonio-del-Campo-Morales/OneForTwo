// Responsibility: execute Wayland calls in order and maintain
// The activation state. It is tested with doubles; it does not open D-Bus or use libei.
#include "platform/wayland_flow.hpp"

#include <limits>
#include <utility>

namespace oit {
namespace wl {
namespace {

std::string hexMask(uint32_t value) {
  static const char* digits = "0123456789abcdef";
  std::string text = "0x";
  bool started = false;
  for (int shift = 28; shift >= 0; shift -= 4) {
    const uint32_t nibble = (value >> shift) & 0xFu;
    if (nibble != 0 || started || shift == 0) {
      text.push_back(digits[nibble]);
      started = true;
    }
  }
  return text;
}

}  // namespace

bool zoneGeometry(const std::vector<Zone>& zones, Geo& geo, std::string& error) {
  if (zones.size() != 1) {
    error = "the portal returned " + std::to_string(zones.size()) +
            " zones; the experiment only supports one monitor";
    return false;
  }
  const Zone& zone = zones.front();
  if (zone.width == 0 || zone.height == 0) {
    error = "portal returned an empty desktop area";
    return false;
  }
  constexpr uint32_t kMax = static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
  if (zone.width > kMax || zone.height > kMax) {
    error = "the portal area exceeds the protocol coordinate range";
    return false;
  }
  geo.x = zone.x;
  geo.y = zone.y;
  geo.w = static_cast<int32_t>(zone.width);
  geo.h = static_cast<int32_t>(zone.height);
  return true;
}

bool sourceBarrier(const std::vector<Zone>& zones, Edge edge, Barrier& barrier,
                   std::string& error) {
  Geo geo;
  if (!zoneGeometry(zones, geo, error)) return false;
  const Zone& zone = zones.front();
  barrier.id = 1;
  barrier.x1 = barrier.x2 = edge == Edge::Right ? zone.x + geo.w : zone.x;
  barrier.y1 = zone.y;
  barrier.y2 = zone.y + geo.h - 1;
  if (barrier.y2 < barrier.y1) {
    error = "the area does not allow a vertical barrier";
    return false;
  }
  return true;
}

bool regionGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Geo& geo) {
  if (width == 0 || height == 0) return false;
  constexpr uint32_t kMax = static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
  if (width > kMax || height > kMax) return false;
  geo.x = static_cast<int32_t>(x);
  geo.y = static_cast<int32_t>(y);
  geo.w = static_cast<int32_t>(width);
  geo.h = static_cast<int32_t>(height);
  return true;
}

bool InputCaptureFlow::open(std::string& error) {
  if (stage_ != CaptureStage::Idle) {
    error = "InputCapture session is now open";
    return false;
  }
  if (!ops_.createInputSession(session_, error)) {
    stage_ = CaptureStage::Failed;
    closeReason_ = error;
    return false;
  }
  if (!ops_.startInputSession(session_, error)) return abort(error);

  int fd = -1;
  if (!ops_.connectInputEis(session_, fd, error)) return abort(error);
  eisFd_ = fd;

  std::vector<Zone> zones;
  uint32_t zoneSet = 0;
  if (!ops_.inputZones(session_, zones, zoneSet, error)) return abort(error);
  if (!applyZones(zones, zoneSet, error)) return abort(error);
  if (!ops_.enableInput(session_, error)) return abort(error);

  stage_ = CaptureStage::Armed;
  return true;
}

bool InputCaptureFlow::applyZones(const std::vector<Zone>& zones, uint32_t zoneSet,
                                  std::string& error) {
  Geo geo;
  if (!zoneGeometry(zones, geo, error)) return false;
  Barrier barrier;
  if (!sourceBarrier(zones, edge_, barrier, error)) return false;
  std::vector<uint32_t> failed;
  if (!ops_.setPointerBarriers(session_, std::vector<Barrier>{barrier}, zoneSet, failed, error)) {
    return false;
  }
  if (!failed.empty()) {
    error = "the portal has rejected the barrier " + std::to_string(failed.front()) +
            "; cannot be activated by border";
    return false;
  }
  zones_ = zones;
  zoneSet_ = zoneSet;
  geo_ = geo;
  return true;
}

bool InputCaptureFlow::rearm(std::string& error) {
  if (stage_ == CaptureStage::Closed || stage_ == CaptureStage::Failed || session_.empty()) {
    error = closeReason_.empty() ? "InputCapture session is not available" : closeReason_;
    return false;
  }

  if (zonesStale_) {
    std::vector<Zone> zones;
    uint32_t zoneSet = 0;
    if (!ops_.inputZones(session_, zones, zoneSet, error)) return abort(error);
    if (!applyZones(zones, zoneSet, error)) return abort(error);
    zonesStale_ = false;
  }

  // Enable is necessary after Disabled and after SetPointerBarriers, which suspends the
  // session. EIS does not reconnect: the same descriptor serves the entire session.
  if (!ops_.enableInput(session_, error)) return abort(error);
  if (stage_ == CaptureStage::Active) {
    activeId_ = 0;
    captureLost_ = true;
  }
  disabled_ = false;
  stage_ = CaptureStage::Armed;
  return true;
}

bool InputCaptureFlow::release(double x, double y, std::string& error) {
  if (activeId_ == 0) return true;
  if (!ops_.releaseInput(session_, activeId_, x, y, error)) return false;
  activeId_ = 0;
  if (stage_ == CaptureStage::Active) stage_ = CaptureStage::Armed;
  return true;
}

void InputCaptureFlow::close() {
  if (eisFd_ >= 0) {
    ops_.closeEis(eisFd_);
    eisFd_ = -1;
  }
  if (!session_.empty()) {
    ops_.closeInputSession(session_);
    session_.clear();
  }
  zones_.clear();
  zoneSet_ = 0;
  activeId_ = 0;
  havePending_ = false;
  captureLost_ = false;
  disabled_ = false;
  zonesStale_ = false;
  closeReason_.clear();
  geo_ = Geo{};
  stage_ = CaptureStage::Idle;
}

void InputCaptureFlow::onActivated(uint32_t activationId, double x, double y,
                                  uint32_t barrierId) {
  if (stage_ == CaptureStage::Closed || stage_ == CaptureStage::Failed) return;
  activeId_ = activationId;
  stage_ = CaptureStage::Active;
  pending_.id = activationId;
  pending_.x = x;
  pending_.y = y;
  pending_.barrierId = barrierId;
  havePending_ = true;
  captureLost_ = false;
  disabled_ = false;
}

void InputCaptureFlow::onDeactivated(uint32_t activationId) {
  if (activeId_ != 0 && activationId != 0 && activationId != activeId_) return;
  activeId_ = 0;
  if (stage_ == CaptureStage::Active) {
    stage_ = CaptureStage::Armed;
    captureLost_ = true;
  }
}

void InputCaptureFlow::onDisabled() {
  if (stage_ == CaptureStage::Closed || stage_ == CaptureStage::Failed) return;
  disabled_ = true;
  activeId_ = 0;
  if (stage_ == CaptureStage::Active) stage_ = CaptureStage::Armed;
  captureLost_ = true;
}

void InputCaptureFlow::onZonesChanged() {
  if (stage_ == CaptureStage::Closed || stage_ == CaptureStage::Failed) return;
  zonesStale_ = true;
  if (stage_ == CaptureStage::Active) {
    // SetPointerBarriers suspends session; previous activation ceases to be
    // freeable and you have to go back to the premises before rearming.
    activeId_ = 0;
    stage_ = CaptureStage::Armed;
    captureLost_ = true;
  }
}

void InputCaptureFlow::onClosed() {
  if (stage_ == CaptureStage::Closed) return;
  stage_ = CaptureStage::Closed;
  closeReason_ = "desktop closed session InputCapture";
  captureLost_ = true;
  activeId_ = 0;
}

bool InputCaptureFlow::takeActivation(Activation& activation) {
  if (!havePending_) return false;
  activation = pending_;
  havePending_ = false;
  return true;
}

bool InputCaptureFlow::takeCaptureLost() {
  const bool lost = captureLost_;
  captureLost_ = false;
  return lost;
}

bool InputCaptureFlow::fatal(std::string& reason) const {
  if (stage_ != CaptureStage::Closed && stage_ != CaptureStage::Failed) return false;
  reason = closeReason_.empty() ? "InputCapture session is over" : closeReason_;
  return true;
}

int InputCaptureFlow::takeEisFd() {
  const int fd = eisFd_;
  eisFd_ = -1;
  return fd;
}

bool InputCaptureFlow::abort(const std::string& message) {
  closeReason_ = message;
  if (eisFd_ >= 0) {
    ops_.closeEis(eisFd_);
    eisFd_ = -1;
  }
  if (!session_.empty()) {
    ops_.closeInputSession(session_);
    session_.clear();
  }
  stage_ = CaptureStage::Failed;
  return false;
}

bool RemoteDesktopFlow::prepareExit(std::string& error) {
  if (stage_ != TargetStage::Idle) {
    error = "the exit session is now open";
    return false;
  }
  if (!ops_.createShortcutSession(shortcuts_, error)) return fail(error);
  bool bound = false;
  if (!ops_.bindEmergencyShortcut(shortcuts_, bound, error)) return fail(error);
  if (!bound) {
    error = "the desktop did not grant Ctrl+Shift+F12; does not open RemoteDesktop nor EIS";
    return fail(error);
  }
  shortcutBound_ = true;
  stage_ = TargetStage::ExitReady;
  return true;
}

bool RemoteDesktopFlow::open(int& eisFd, std::string& error) {
  eisFd = -1;
  if (stage_ == TargetStage::Idle && !prepareExit(error)) return false;
  if (stage_ != TargetStage::ExitReady) {
    error = "RemoteDesktop session is already open or output is not ready";
    return false;
  }
  if (!ops_.verifyEmergencyShortcut(shortcuts_, error)) return fail(error);
  if (!ops_.createRemoteSession(remote_, error)) return fail(error);
  if (!ops_.selectRemoteDevices(remote_, kDeviceKeyboard | kDevicePointer, error)) {
    return fail(error);
  }
  uint32_t granted = 0;
  if (!ops_.startRemoteSession(remote_, granted, error)) return fail(error);
  granted_ = granted;
  const uint32_t required = kDeviceKeyboard | kDevicePointer;
  if ((granted & required) != required) {
    error = "the desktop granted the devices " + hexMask(granted) +
            "; keyboard and pointer required";
    return fail(error);
  }
  int fd = -1;
  if (!ops_.connectRemoteEis(remote_, fd, error)) return fail(error);
  eisFd = fd;
  stage_ = TargetStage::Ready;
  return true;
}

void RemoteDesktopFlow::close() {
  if (!shortcuts_.empty()) {
    ops_.closeShortcutSession(shortcuts_);
    shortcuts_.clear();
  }
  if (!remote_.empty()) {
    ops_.closeRemoteSession(remote_);
    remote_.clear();
  }
  granted_ = 0;
  shortcutBound_ = false;
  closeReason_.clear();
  stage_ = TargetStage::Idle;
}

void RemoteDesktopFlow::onClosed() {
  if (stage_ == TargetStage::Closed) return;
  stage_ = TargetStage::Closed;
  closeReason_ = "Desktop closed RemoteDesktop session";
}

bool RemoteDesktopFlow::fatal(std::string& reason) const {
  if (stage_ != TargetStage::Closed && stage_ != TargetStage::Failed) return false;
  reason = closeReason_.empty() ? "the RemoteDesktop session ended" : closeReason_;
  return true;
}

bool RemoteDesktopFlow::fail(const std::string& message) {
  closeReason_ = message;
  if (!shortcuts_.empty()) {
    ops_.closeShortcutSession(shortcuts_);
    shortcuts_.clear();
  }
  if (!remote_.empty()) {
    ops_.closeRemoteSession(remote_);
    remote_.clear();
  }
  granted_ = 0;
  shortcutBound_ = false;
  stage_ = TargetStage::Failed;
  return false;
}

}  // namespace wl
}  // namespace oit
