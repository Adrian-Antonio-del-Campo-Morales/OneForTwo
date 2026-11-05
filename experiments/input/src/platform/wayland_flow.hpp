#pragma once
// Responsabilidad: lógica portable de las sesiones Wayland. El origen usa el
// portal InputCapture; el destino, RemoteDesktop + GlobalShortcuts. Esta capa no
// habla D-Bus ni libei: recibe esas operaciones por PortalOps, de modo que puede
// probarse con dobles y compilarse en cualquier plataforma.

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace oit {
namespace wl {

// GetZones devuelve a(uuii): ancho, alto, desplazamiento x, desplazamiento y.
struct Zone {
  uint32_t width = 0;
  uint32_t height = 0;
  int32_t x = 0;
  int32_t y = 0;
};

// Barrera vertical en el borde exterior de la zona. id no puede ser cero.
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

constexpr const char* kEmergencyShortcutId = "oneintwo-emergency";
constexpr const char* kEmergencyShortcutTrigger = "CTRL+ALT+F12";

// El alcance del experimento es un solo monitor: una única zona.
bool zoneGeometry(const std::vector<Zone>& zones, Geo& geo, std::string& error);

// Barrera vertical en el borde compartido: x = zone.x + zone.width (Right) o
// x = zone.x (Left); y1 = zone.y, y2 = zone.y + zone.height - 1.
bool sourceBarrier(const std::vector<Zone>& zones, Edge edge, Barrier& barrier,
                   std::string& error);

// Convierte una región EIS en geometría de escritorio.
bool regionGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Geo& geo);

// Operaciones del portal, implementadas por GDBus en Linux y por un doble en las
// pruebas. Los descriptores EIS transferidos pasan a ser propiedad del llamador.
class PortalOps {
 public:
  virtual ~PortalOps() = default;

  // Origen: org.freedesktop.portal.InputCapture (versión 2).
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
  // Cierra un descriptor EIS que todavía no se transfirió a libei.
  virtual void closeEis(int fd) = 0;

  // Destino: org.freedesktop.portal.RemoteDesktop (versión 2).
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
  virtual void closeShortcutSession(const std::string& session) = 0;
};

struct Activation {
  uint32_t id = 0;
  double x = 0.0;
  double y = 0.0;
  uint32_t barrierId = 0;
};

enum class CaptureStage { Idle, Armed, Active, Closed, Failed };

// Secuencia de autorización y estado del origen. open() ejecuta, en orden:
// CreateSession2, Start, ConnectToEIS, GetZones, SetPointerBarriers y Enable.
class InputCaptureFlow {
 public:
  InputCaptureFlow(PortalOps& ops, Edge edge) : ops_(ops), edge_(edge) {}

  bool open(std::string& error);
  // Tras ZonesChanged o Disabled. Re-consulta zonas y barreras solo si cambiaron.
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

enum class TargetStage { Idle, Ready, Closed, Failed };

// Secuencia del destino. El orden es CreateSession, SelectDevices, Start,
// ConnectToEIS, CreateSession (GlobalShortcuts) y BindShortcuts. Si el
// escritorio no concede Ctrl+Alt+F12 no se habilita la inyección: sin salida
// local no hay forma de recuperar el control desde el destino.
class RemoteDesktopFlow {
 public:
  explicit RemoteDesktopFlow(PortalOps& ops) : ops_(ops) {}

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
