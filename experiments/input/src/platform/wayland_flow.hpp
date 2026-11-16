#pragma once
// Responsibility: Wayland session portable logic. Origin uses the
// InputCapture portal; destination, RemoteDesktop + GlobalShortcuts. This layer does not
// This is D-Bus or Libei: receive these operations by PortalOps, so you can
// be tested with doubles and compiled on any platform.

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace oit {
namespace wl {

// GetZones returns to(uuii): width, height, displacement x, displacement y.
struct Zone {
  uint32_t width = 0;
  uint32_t height = 0;
  int32_t x = 0;
  int32_t y = 0;
};

// Vertical barrier on the outer edge of the area. id cannot be zero.
struct Barrier {
  uint32_t id = 1;
  int32_t x1 = 0;
  int32_t y1 = 0;
  int32_t x2 = 0;
  int32_t y2 = 0;
};

constexpr uint32_t kCapabilityKeyboard = 1;
constexpr uint32_t kCapabilityPointer = 2;
constexpr uint32_t kDeviceKeyboard = 1;
constexpr uint32_t kDevicePointer = 2;

// New identifier: do not reuse a persistent Ctrl+Alt+F12 concession.
constexpr const char* kEmergencyShortcutId = "oneintwo-emergency-v2";
constexpr const char* kEmergencyShortcutTrigger = "CTRL+SHIFT+F12";

// The scope of the experiment is a single monitor: a single area.
bool zoneGeometry(const std::vector<Zone>& zones, Geo& geo, std::string& error);

// Vertical barrier on shared edge: x = zone.x + zone.width (Right) or
// x = zone.x (Left); y1 = zone.y, y2 = zone.and + zone.height - 1.
bool sourceBarrier(const std::vector<Zone>& zones, Edge edge, Barrier& barrier,
                   std::string& error);

// It converts a EIS region into desktop geometry.
bool regionGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Geo& geo);

// Portal operations, implemented by GDBus on Linux and by a double in the
// The EIS descriptors transferred become the property of the caller.
class PortalOps {
 public:
  virtual ~PortalOps() = default;

  // Origin: org.freedesktop.portal.InputCapture (version 2).
  virtual bool createInputSession(std::string& session, std::string& error) = 0;
  virtual bool startInputSession(const std::string& session, std::string& error) = 0;
  virtual bool connectInputEis(const std::string& session, int& fd, std::string& error) = 0;
  virtual bool inputZones(const std::string& session, std::vector<Zone>& zones, uint32_t& zoneSet,
                          std::string& error) = 0;
  virtual bool setPointerBarriers(const std::string& session, const std::vector<Barrier>& barriers,
                                  uint32_t zoneSet, std::vector<uint32_t>& failed,
                                  std::string& error) = 0;
  virtual bool enableInput(const std::string& session, std::string& error) = 0;
  virtual bool disableInput(const std::string& session, std::string& error) = 0;
  virtual bool releaseInput(const std::string& session, uint32_t activationId, double x, double y,
                            std::string& error) = 0;
  virtual void closeInputSession(const std::string& session) = 0;
  // It closes a EIS descriptor that has not yet been transferred to libei.
  virtual void closeEis(int fd) = 0;

  // Destination: org.freedesktop.portal.RemoteDesktop (version 2).
  virtual bool createRemoteSession(std::string& session, std::string& error) = 0;
  virtual bool selectRemoteDevices(const std::string& session, uint32_t types,
                                   std::string& error) = 0;
  virtual bool startRemoteSession(const std::string& session, uint32_t& granted,
                                  std::string& error) = 0;
  virtual bool connectRemoteEis(const std::string& session, int& fd, std::string& error) = 0;
  virtual void closeRemoteSession(const std::string& session) = 0;

  // Destino: org.freedesktop.portal.GlobalShortcuts.
  virtual bool createShortcutSession(std::string& session, std::string& error) = 0;
  virtual bool bindEmergencyShortcut(const std::string& session, bool& bound,
                                     std::string& error) = 0;
  // Requires real activation and release before opening RemoteDesktop/EIS.
  virtual bool verifyEmergencyShortcut(const std::string& session, std::string& error) = 0;
  virtual void closeShortcutSession(const std::string& session) = 0;
};

struct Activation {
  uint32_t id = 0;
  double x = 0.0;
  double y = 0.0;
  uint32_t barrierId = 0;
};

enum class CaptureStage { Idle, Armed, Active, Closed, Failed };

// Authorization sequence and state of origin. open() executes, in order:
// CreateSession2, Start, ConnectToEIS, GetZones, SetPointerBarriers and Enable.
class InputCaptureFlow {
 public:
  InputCaptureFlow(PortalOps& ops, Edge edge) : ops_(ops), edge_(edge) {}

  bool open(std::string& error);
  // After ZonesChanged or Disabled. Consult zones and barriers only if they changed.
  bool rearm(std::string& error);
  bool release(double x, double y, std::string& error);
  void close();

  void onActivated(uint32_t activationId, double x, double y, uint32_t barrierId);
  void onDeactivated(uint32_t activationId);
  void onDisabled();
  void onZonesChanged();
  void onClosed();

  bool takeActivation(Activation& activation);
  bool takeCaptureLost();
  bool needsRearm() const { return zonesStale_ || disabled_; }
  bool fatal(std::string& reason) const;
  bool active() const { return stage_ == CaptureStage::Active; }
  bool armed() const { return stage_ == CaptureStage::Armed || stage_ == CaptureStage::Active; }
  CaptureStage stage() const { return stage_; }
  Edge edge() const { return edge_; }
  const std::vector<Zone>& zones() const { return zones_; }
  Geo geometry() const { return geo_; }
  const std::string& session() const { return session_; }
  int takeEisFd();

 private:
  bool abort(const std::string& error);
  bool applyZones(const std::vector<Zone>& zones, uint32_t zoneSet, std::string& error);

  PortalOps& ops_;
  Edge edge_;
  std::string session_;
  std::vector<Zone> zones_;
  Geo geo_;
  uint32_t zoneSet_ = 0;
  CaptureStage stage_ = CaptureStage::Idle;
  uint32_t activeId_ = 0;
  Activation pending_;
  bool havePending_ = false;
  bool captureLost_ = false;
  bool disabled_ = false;
  bool zonesStale_ = false;
  int eisFd_ = -1;
  std::string closeReason_;
};

enum class TargetStage { Idle, ExitReady, Ready, Closed, Failed };

// First GlobalShortcuts and physical check; then RemoteDesktop/EIS.
// prepareExit() allows to test only the shortcut, without control session nor EIS.
class RemoteDesktopFlow {
 public:
  explicit RemoteDesktopFlow(PortalOps& ops) : ops_(ops) {}

  bool prepareExit(std::string& error);
  bool open(int& eisFd, std::string& error);
  void close();
  void onClosed();
  bool fatal(std::string& reason) const;
  TargetStage stage() const { return stage_; }
  uint32_t grantedDevices() const { return granted_; }
  bool shortcutBound() const { return shortcutBound_; }
  const std::string& session() const { return remote_; }

 private:
  bool fail(const std::string& error);

  PortalOps& ops_;
  std::string remote_;
  std::string shortcuts_;
  uint32_t granted_ = 0;
  TargetStage stage_ = TargetStage::Idle;
  bool shortcutBound_ = false;
  std::string closeReason_;
};

}  // namespace wl
}  // namespace oit
