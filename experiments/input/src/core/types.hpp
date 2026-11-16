#pragma once
// Responsibility: common types of the experiment (geometry, messages and normalized events).

#include <cstdint>

namespace oit {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint16_t kRoleSource = 1;
constexpr uint16_t kRoleTarget = 2;

// Shared border that occupies the origin. Right means that the origin is to the
// left of destination; Left, which is on your right.
enum class Edge : uint8_t { Right = 0, Left = 1 };

// Release and shutdown reasons are sent as a single byte.
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

// Virtual Desktop Rectangle. In the scope of the experiment a
// Only monitor per team.
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

// Control channel message. Network format is fixed (28 bytes) and each
// field has a meaning depending on the type.
struct Message {
  MsgType type = MsgType::Ping;
  uint32_t generation = 0;
  uint32_t sequence = 0;
  int16_t x = 0;          // movement (delta) or horizontal wheel
  int16_t y = 0;          // motion (delta) or vertical wheel
  uint16_t code = 0;      // use HID, button or edge
  uint8_t reason = 0;     // LeaveReason
  uint16_t ratio = 0;     // standard vertical input 0.65535
  bool pressed = false;
};

enum class RawKind : uint8_t {
  MotionAbs,    // absolute position of the local cursor
  MotionDelta,  // delta caught for destination
  Button,
  Wheel,
  Key,
  Hotkey,       // Ctrl+Alt+F12 detected by the backend
  LocalActivity, // physical activity in the destination (Windows backend only)
  PortalEdge,   // InputCapture cross barrier; x/y are the notified position
  CaptureLost   // the composer withdrew the capture; we have to go back to local
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
