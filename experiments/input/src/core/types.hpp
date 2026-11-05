#pragma once
// Responsabilidad: tipos comunes del experimento (geometría, mensajes y eventos normalizados).

#include <cstdint>

namespace oit {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint16_t kRoleSource = 1;
constexpr uint16_t kRoleTarget = 2;

// Borde compartido que ocupa el origen. Right significa que el origen está a la
// izquierda del destino; Left, que está a su derecha.
enum class Edge : uint8_t { Right = 0, Left = 1 };

// Motivos de liberación/cierre. Se transportan como un byte.
enum class LeaveReason : uint8_t {
  Returned = 0,
  Emergency = 1,
  LocalOverride = 2,
  LinkLost = 3,
  Timeout = 4,
  Shutdown = 5,
  Stale = 6,
  InjectionFailed = 7,
};

// Rectángulo del escritorio virtual. En el alcance del experimento se espera un
// solo monitor por equipo.
struct Geo {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  bool valid() const { return w > 0 && h > 0; }
};

enum class MsgType : uint8_t {
  Hello = 1,
  HelloAck = 2,
  EnterRequest = 3,
  EnterAck = 4,
  EnterReject = 5,
  Leave = 6,
  Motion = 7,
  Button = 8,
  Wheel = 9,
  Key = 10,
  ReleaseAll = 11,
  Ping = 12,
  Pong = 13,
};

// Mensaje del canal de control. El formato de red es fijo (28 bytes) y cada
// campo tiene un significado según el tipo.
struct Message {
  MsgType type = MsgType::Ping;
  uint32_t generation = 0;
  uint32_t sequence = 0;
  int16_t x = 0;          // movimiento (delta) o rueda horizontal
  int16_t y = 0;          // movimiento (delta) o rueda vertical
  uint16_t code = 0;      // uso HID, botón o borde
  uint8_t reason = 0;     // LeaveReason
  uint16_t ratio = 0;     // entrada vertical normalizada 0..65535
  bool pressed = false;
};

enum class RawKind : uint8_t {
  MotionAbs,    // posición absoluta del cursor local
  MotionDelta,  // delta capturado para el destino
  Button,
  Wheel,
  Key,
  Hotkey,       // Ctrl+Alt+F12 detectado por el backend
  LocalActivity, // actividad física en el destino (solo backend Windows)
  PortalEdge,   // barrera InputCapture cruzada; x/y son la posición notificada
  CaptureLost   // el compositor retiró la captura; hay que volver a local
};

struct RawEvent {
  RawKind kind = RawKind::MotionAbs;
  int32_t x = 0;
  int32_t y = 0;
  int16_t dx = 0;
  int16_t dy = 0;
  uint16_t code = 0;
  bool pressed = false;
};

const char* edgeName(Edge edge);
const char* leaveReasonName(LeaveReason reason);
const char* msgTypeName(MsgType type);

}  // namespace oit
