// Responsabilidad: comprobaciones aisladas del núcleo con reloj, canales y
// adaptadores falsos. No abre escritorio, no captura ni inyecta entrada real.
#include "self_test.hpp"

#include "core/geometry.hpp"
#include "core/input_state.hpp"
#include "core/keymap.hpp"
#include "core/protocol.hpp"
#include "core/source_controller.hpp"
#include "core/target_controller.hpp"
#include "core/types.hpp"
#include "platform/wayland_flow.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace oit {
namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* expression, int line) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    std::printf("FALLO linea %d: %s\n", line, expression);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

Geo geo1920() {
  Geo geo;
  geo.w = 1920;
  geo.h = 1080;
  return geo;
}

Message makeMessage(MsgType type, uint32_t generation, uint32_t sequence) {
  Message message;
  message.type = type;
  message.generation = generation;
  message.sequence = sequence;
  return message;
}

Message enterRequest(uint32_t generation, uint32_t sequence, uint16_t ratio,
                     Edge edge = Edge::Right) {
  Message message = makeMessage(MsgType::EnterRequest, generation, sequence);
  message.code = static_cast<uint16_t>(edge);
  message.ratio = ratio;
  return message;
}

Message leaveMessage(uint32_t generation, LeaveReason reason) {
  Message message = makeMessage(MsgType::Leave, generation, 0);
  message.reason = static_cast<uint8_t>(reason);
  return message;
}

Message keyMessage(uint32_t generation, uint32_t sequence, uint16_t usage, bool pressed) {
  Message message = makeMessage(MsgType::Key, generation, sequence);
  message.code = usage;
  message.pressed = pressed;
  return message;
}

Message buttonMessage(uint32_t generation, uint32_t sequence, uint16_t button, bool pressed) {
  Message message = makeMessage(MsgType::Button, generation, sequence);
  message.code = button;
  message.pressed = pressed;
  return message;
}

Message motionMessage(uint32_t generation, uint32_t sequence, int16_t dx, int16_t dy) {
  Message message = makeMessage(MsgType::Motion, generation, sequence);
  message.x = dx;
  message.y = dy;
  return message;
}

Message wheelMessage(uint32_t generation, uint32_t sequence, int16_t dx, int16_t dy) {
  Message message = makeMessage(MsgType::Wheel, generation, sequence);
  message.x = dx;
  message.y = dy;
  return message;
}

Message releaseAllMessage(uint32_t generation, uint32_t sequence, LeaveReason reason) {
  Message message = makeMessage(MsgType::ReleaseAll, generation, sequence);
  message.reason = static_cast<uint8_t>(reason);
  return message;
}

class FakeSourceIO : public SourceIO {
 public:
  bool send(const Message& message) override {
    if (!sendOk) return false;
    sent.push_back(message);
    return true;
  }
  bool startObserving() override {
    observing = startOk;
    return startOk;
  }
  bool setCaptureActive(bool active) override {
    if (active && !captureOk) return false;
    if (!active) ++captureReleases;
    captureActive = active;
    return true;
  }
  void stopObserving() override { observing = false; }
  void warpCursor(int x, int y) override {
    ++warps;
    warpX = x;
    warpY = y;
  }
  void requestStop(const std::string& reason) override {
    stopRequested = true;
    stopReason = reason;
  }

  std::vector<Message> sent;
  bool sendOk = true;
  bool startOk = true;
  bool captureOk = true;
  bool observing = false;
  bool captureActive = false;
  int captureReleases = 0;
  int warps = 0;
  int warpX = 0;
  int warpY = 0;
  bool stopRequested = false;
  std::string stopReason;
};

class FakeTargetIO : public TargetIO {
 public:
  bool send(const Message& message) override {
    if (!sendOk) return false;
    sent.push_back(message);
    return true;
  }
  bool injectMove(int x, int y) override {
    if (!injectOk) return false;
    actions.push_back("move:" + std::to_string(x) + "," + std::to_string(y));
    cursorX = x;
    cursorY = y;
    return true;
  }
  bool injectKey(uint16_t usage, bool pressed) override {
    actions.push_back(std::string(pressed ? "key+:" : "key-:") + std::to_string(usage));
    return true;
  }
  bool injectButton(uint16_t button, bool pressed) override {
    actions.push_back(std::string(pressed ? "button+:" : "button-:") + std::to_string(button));
    return true;
  }
  bool injectWheel(int16_t dx, int16_t dy) override {
    actions.push_back("wheel:" + std::to_string(dx) + "," + std::to_string(dy));
    return true;
  }
  bool cursorPosition(int& x, int& y) override {
    if (!cursorOk) return false;
    x = cursorX;
    y = cursorY;
    return true;
  }
  void requestStop(const std::string& reason) override {
    stopRequested = true;
    stopReason = reason;
  }

  std::vector<std::string> actions;
  std::vector<Message> sent;
  bool sendOk = true;
  bool injectOk = true;
  bool cursorOk = true;
  int cursorX = 200;
  int cursorY = 300;
  bool stopRequested = false;
  std::string stopReason;
};

// Doble del portal: registra el orden de las llamadas y simula fallos por etapa.
class FakePortal : public wl::PortalOps {
 public:
  bool createInputSession(std::string& session, std::string& error) override {
    if (!record("createInputSession", error)) return false;
    session = inputSession;
    return true;
  }
  bool startInputSession(const std::string&, std::string& error) override {
    return record("startInputSession", error);
  }
  bool connectInputEis(const std::string&, int& fd, std::string& error) override {
    if (!record("connectInputEis", error)) return false;
    fd = eisFd;
    return true;
  }
  bool inputZones(const std::string&, std::vector<wl::Zone>& out, uint32_t& zoneSet,
                  std::string& error) override {
    if (!record("inputZones", error)) return false;
    out = zones;
    zoneSet = this->zoneSet;
    return true;
  }
  bool setPointerBarriers(const std::string&, const std::vector<wl::Barrier>& list, uint32_t set,
                          std::vector<uint32_t>& failed, std::string& error) override {
    if (!record("setPointerBarriers", error)) return false;
    barriers = list;
    barrierZoneSet = set;
    failed = failedBarriers;
    return true;
  }
  bool enableInput(const std::string&, std::string& error) override {
    return record("enableInput", error);
  }
  bool disableInput(const std::string&, std::string& error) override {
    return record("disableInput", error);
  }
  bool releaseInput(const std::string&, uint32_t id, double x, double y,
                    std::string& error) override {
    if (!record("releaseInput", error)) return false;
    ++releaseCount;
    releaseId = id;
    releaseX = x;
    releaseY = y;
    return true;
  }
  void closeInputSession(const std::string&) override {
    ++inputSessionCloses;
    calls.push_back("closeInputSession");
  }
  void closeEis(int fd) override {
    ++eisCloses;
    closedFd = fd;
    calls.push_back("closeEis");
  }

  bool createRemoteSession(std::string& session, std::string& error) override {
    if (!record("createRemoteSession", error)) return false;
    session = remoteSession;
    return true;
  }
  bool selectRemoteDevices(const std::string&, uint32_t types, std::string& error) override {
    if (!record("selectRemoteDevices", error)) return false;
    selectedTypes = types;
    return true;
  }
  bool startRemoteSession(const std::string&, uint32_t& granted, std::string& error) override {
    if (!record("startRemoteSession", error)) return false;
    granted = grantedDevices;
    return true;
  }
  bool connectRemoteEis(const std::string&, int& fd, std::string& error) override {
    if (!record("connectRemoteEis", error)) return false;
    fd = remoteEisFd;
    return true;
  }
  void closeRemoteSession(const std::string&) override {
    ++remoteCloses;
    calls.push_back("closeRemoteSession");
  }
  bool createShortcutSession(std::string& session, std::string& error) override {
    if (!record("createShortcutSession", error)) return false;
    session = shortcutSession;
    return true;
  }
  bool bindEmergencyShortcut(const std::string&, bool& bound, std::string& error) override {
    if (!record("bindEmergencyShortcut", error)) return false;
    bound = shortcutBound;
    return true;
  }
  bool verifyEmergencyShortcut(const std::string&, std::string& error) override {
    return record("verifyEmergencyShortcut", error);
  }
  void closeShortcutSession(const std::string&) override {
    ++shortcutCloses;
    calls.push_back("closeShortcutSession");
  }

  bool record(const std::string& call, std::string& error) {
    calls.push_back(call);
    if (call == failAt) {
      error = "fallo simulado: " + call;
      return false;
    }
    return true;
  }

  std::vector<std::string> calls;
  std::string failAt;
  std::string inputSession = "/org/freedesktop/portal/desktop/session/1/in";
  std::string remoteSession = "/org/freedesktop/portal/desktop/session/1/rd";
  std::string shortcutSession = "/org/freedesktop/portal/desktop/session/1/gs";
  std::vector<wl::Zone> zones{{1920, 1080, 0, 0}};
  std::vector<uint32_t> failedBarriers;
  std::vector<wl::Barrier> barriers;
  uint32_t zoneSet = 9;
  uint32_t barrierZoneSet = 0;
  int eisFd = 42;
  int remoteEisFd = 43;
  uint32_t grantedDevices = 3;
  uint32_t selectedTypes = 0;
  bool shortcutBound = true;
  int releaseCount = 0;
  uint32_t releaseId = 0;
  double releaseX = 0.0;
  double releaseY = 0.0;
  int eisCloses = 0;
  int closedFd = -1;
  int inputSessionCloses = 0;
  int remoteCloses = 0;
  int shortcutCloses = 0;
};

void testProtocol() {
  const MsgType types[] = {MsgType::Hello,      MsgType::HelloAck,  MsgType::EnterRequest,
                           MsgType::EnterAck,   MsgType::EnterReject, MsgType::Leave,
                           MsgType::Motion,     MsgType::Button,    MsgType::Wheel,
                           MsgType::Key,        MsgType::ReleaseAll, MsgType::Ping,
                           MsgType::Pong};

  for (MsgType type : types) {
    Message original = makeMessage(type, 0x11223344u, 0xA1B2C3D4u);
    original.x = -123;
    original.y = 456;
    original.code = type == MsgType::EnterRequest ? 0 : 0x33;
    original.reason = static_cast<uint8_t>(LeaveReason::Returned);
    original.ratio = 32768;
    original.pressed = true;

    const Frame frame = encodeMessage(original);
    Message decoded;
    std::string error;
    CHECK(decodeMessage(frame.data(), frame.size(), decoded, error));
    CHECK(decoded.type == original.type);
    CHECK(decoded.generation == original.generation);
    CHECK(decoded.sequence == original.sequence);
    CHECK(decoded.x == original.x);
    CHECK(decoded.y == original.y);
    CHECK(decoded.code == original.code);
    CHECK(decoded.pressed == original.pressed);
    CHECK(decoded.ratio == original.ratio);
  }

  Message message = makeMessage(MsgType::Motion, 1, 2);
  Frame frame = encodeMessage(message);
  Message decoded;
  std::string error;

  Frame wrong = frame;
  wrong[0] = 'X';
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  wrong = frame;
  wrong[4] = 9;
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  wrong = frame;
  wrong[5] = 200;
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  wrong = frame;
  wrong[6] = 8;
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  wrong = frame;
  wrong[26] = 1;
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  wrong = frame;
  wrong[22] = 2;
  CHECK(!decodeMessage(wrong.data(), wrong.size(), decoded, error));

  CHECK(!decodeMessage(frame.data(), kFrameSize - 1, decoded, error));

  Message invalidEdge = enterRequest(1, 1, 0);
  invalidEdge.code = 2;
  Frame edgeFrame = encodeMessage(invalidEdge);
  CHECK(!decodeMessage(edgeFrame.data(), edgeFrame.size(), decoded, error));

  Message invalidReason = leaveMessage(1, LeaveReason::Returned);
  invalidReason.reason = 200;
  Frame reasonFrame = encodeMessage(invalidReason);
  CHECK(!decodeMessage(reasonFrame.data(), reasonFrame.size(), decoded, error));
}

void testKeymap() {
  CHECK(winScanToHid(0x27, false) == hid::Semicolon);      // ñ en español
  CHECK(winScanToHid(0x1A, false) == 0x2F);                // tecla de ´
  CHECK(winScanToHid(0x56, false) == hid::NonUsBackslash); // < > ISO
  CHECK(winScanToHid(0x38, true) == hid::RightAlt);        // AltGr
  CHECK(winScanToHid(0x38, false) == hid::LeftAlt);
  CHECK(winScanToHid(0x45, false) == 0x53);                // NumLock
  CHECK(winScanToHid(0x45, true) == 0x48);                 // Pause
  CHECK(winScanToHid(0x99, false) == 0);

  const uint16_t usages[] = {hid::A,       hid::Z,          hid::F12,        hid::Semicolon,
                             hid::NonUsBackslash, hid::LeftCtrl, hid::RightAlt, hid::RightGui,
                             0x1E,         0x27,           0x28,            0x2C,
                             0x4F,         0x52,           0x62,            0x63};

  for (uint16_t usage : usages) {
    uint32_t scan = 0;
    bool extended = false;
    CHECK(hidToWinScan(usage, scan, extended));
    CHECK(winScanToHid(scan, extended) == usage);

    CHECK(hidToLinuxCode(usage) != 0);
    CHECK(linuxCodeToHid(hidToLinuxCode(usage)) == usage);

    const uint16_t keycode = hidToX11Keycode(usage);
    CHECK(keycode != 0);
    CHECK(x11KeycodeToHid(keycode) == usage);
  }

  CHECK(hidToX11Keycode(hid::A) == 38);        // KEY_A = 30
  CHECK(hidToX11Keycode(hid::LeftCtrl) == 37);// KEY_LEFTCTRL = 29
  CHECK(hidToX11Keycode(hid::RightAlt) == 108);// KEY_RIGHTALT = 100
  CHECK(hidToX11Keycode(hid::F12) == 96);      // KEY_F12 = 88
  CHECK(x11KeycodeToHid(47) == hid::Semicolon);
  CHECK(!isMappedUsage(0x66));
  CHECK(isKeyboardUsage(0x66));
  CHECK(isModifierUsage(hid::LeftGui));
  CHECK(!isModifierUsage(hid::A));
}

void testGeometry() {
  const Geo geo = geo1920();
  CHECK(ratioFromY(geo, 0) == 0);
  CHECK(ratioFromY(geo, 1079) == 65535);
  CHECK(ratioFromY(geo, -50) == 0);
  CHECK(ratioFromY(geo, 5000) == 65535);
  const int32_t mapped = yFromRatio(geo, 32768);
  CHECK(mapped >= 538 && mapped <= 541);
  CHECK(yFromRatio(geo, 0) == 0);
  CHECK(yFromRatio(geo, 65535) == 1079);
  CHECK(returnEdge(Edge::Right) == Edge::Left);
  CHECK(returnEdge(Edge::Left) == Edge::Right);

  EdgeGate gate(geo, Edge::Right, 48, 300);
  uint16_t ratio = 0;
  CHECK(!gate.observe(1000, 500, 1000, ratio));
  CHECK(gate.observe(1919, 540, 1100, ratio));
  CHECK(ratio > 32000 && ratio < 33500);
  CHECK(!gate.observe(1919, 540, 1110, ratio));

  gate.noteReturn(1200);
  CHECK(!gate.observe(1895, 540, 1300, ratio)); // dentro de la banda y sin enfriar
  CHECK(!gate.observe(1850, 540, 1499, ratio)); // arma, aún sin enfriar
  CHECK(!gate.observe(1850, 540, 1499, ratio));
  CHECK(gate.observe(1919, 540, 1500, ratio));
  CHECK(!gate.observe(1919, 540, 1510, ratio));

  EdgeGate leftGate(geo, Edge::Left, 48, 0);
  CHECK(!leftGate.observe(900, 500, 0, ratio));
  CHECK(leftGate.observe(0, 500, 10, ratio));
}

void testInputState() {
  InputState state;
  CHECK(state.keyDown(hid::A));
  CHECK(!state.keyDown(hid::A));
  CHECK(state.buttonDown(0));
  CHECK(state.keyDown(hid::LeftCtrl));
  CHECK(state.size() == 3);

  const std::vector<HeldInput> held = state.releaseAll();
  CHECK(held.size() == 3);
  CHECK(!held[0].button && held[0].code == hid::LeftCtrl);
  CHECK(held[1].button && held[1].code == 0);
  CHECK(!held[2].button && held[2].code == hid::A);
  CHECK(state.empty());
  CHECK(!state.keyUp(hid::A));
}

void testSourceHappyPath() {
  FakeSourceIO io;
  SourceTuning tuning;
  tuning.pingIntervalMs = 500;
  tuning.antiBouncePx = 48;
  tuning.cooldownMs = 300;

  SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
  CHECK(controller.start());
  CHECK(io.observing);

  controller.onRaw(RawEvent{RawKind::MotionAbs, 1850, 540, 0, 0, 0, false}, 1050);
  CHECK(io.sent.empty());
  controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 540, 0, 0, 0, false}, 1060);
  CHECK(io.sent.size() == 1);
  CHECK(io.sent[0].type == MsgType::EnterRequest);
  CHECK(io.sent[0].generation == 1);
  CHECK(io.sent[0].code == static_cast<uint16_t>(Edge::Right));
  CHECK(std::abs(static_cast<int>(io.sent[0].ratio) - 32796) <= 2);
  CHECK(controller.state() == SourceState::Preparing);

  Message ack = makeMessage(MsgType::EnterAck, 1, 1);
  controller.onMessage(ack, 1070);
  CHECK(controller.state() == SourceState::Remote);
  CHECK(io.captureActive);
  CHECK(io.warpX == 960 && io.warpY == 540);

  controller.onRaw(RawEvent{RawKind::Key, 0, 0, 0, 0, hid::A, true}, 1080);
  CHECK(io.sent.back().type == MsgType::Key);
  CHECK(io.sent.back().code == hid::A);
  CHECK(io.sent.back().pressed);
  CHECK(io.sent.back().generation == 1);

  controller.onRaw(RawEvent{RawKind::MotionDelta, 0, 0, 7, -4, 0, false}, 1090);
  CHECK(io.sent.back().type == MsgType::Motion);
  CHECK(io.sent.back().x == 7 && io.sent.back().y == -4);

  controller.onRaw(RawEvent{RawKind::Button, 0, 0, 0, 0, 0, true}, 1100);
  CHECK(io.sent.back().type == MsgType::Button);
  CHECK(io.sent.back().pressed);

  controller.onRaw(RawEvent{RawKind::Wheel, 0, 0, 0, 1, 0, false}, 1110);
  CHECK(io.sent.back().type == MsgType::Wheel);
  CHECK(io.sent.back().y == 1);

  controller.onMessage(leaveMessage(1, LeaveReason::Returned), 1200);
  CHECK(controller.state() == SourceState::Idle);
  CHECK(!io.captureActive);
  CHECK(io.warpX == 1895 && io.warpY == 540);

  // Segundo cruce: la generación aumenta y el anti-rebote exige salir de la banda.
  controller.onRaw(RawEvent{RawKind::MotionAbs, 1850, 540, 0, 0, 0, false}, 1600);
  controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 540, 0, 0, 0, false}, 1610);
  CHECK(io.sent.back().type == MsgType::EnterRequest);
  CHECK(io.sent.back().generation == 2);

  controller.onMessage(leaveMessage(1, LeaveReason::Emergency), 1620);
  CHECK(controller.state() == SourceState::Preparing);

  controller.onMessage(makeMessage(MsgType::EnterAck, 2, 2), 1630);
  CHECK(controller.state() == SourceState::Remote);

  controller.stop(1700);
  CHECK(controller.state() == SourceState::Stopped);
  CHECK(controller.stopRequested());
  CHECK(!io.observing);
  CHECK(!io.captureActive);

  bool sawRelease = false;
  bool sawLeave = false;
  for (const Message& message : io.sent) {
    if (message.type == MsgType::ReleaseAll && message.reason == static_cast<uint8_t>(LeaveReason::Shutdown)) {
      sawRelease = true;
    }
    if (message.type == MsgType::Leave && message.reason == static_cast<uint8_t>(LeaveReason::Shutdown)) {
      sawLeave = true;
    }
  }
  CHECK(sawRelease);
  CHECK(sawLeave);
}

void testSourceTimeouts() {
  {
    FakeSourceIO io;
    SourceTuning tuning;
    tuning.ackTimeoutMs = 500;
    tuning.linkTimeoutMs = 5000;
    SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1910, 500, 0, 0, 0, false}, 1010);
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 500, 0, 0, 0, false}, 1020);
    CHECK(controller.state() == SourceState::Preparing);
    controller.tick(1600);
    CHECK(controller.state() == SourceState::Idle);
    CHECK(!io.captureActive);
    bool sawLeave = false;
    for (const Message& message : io.sent) {
      if (message.type == MsgType::Leave) sawLeave = true;
    }
    CHECK(sawLeave);
    controller.stop(1700);
  }

  {
    FakeSourceIO io;
    SourceTuning tuning;
    tuning.linkTimeoutMs = 2000;
    tuning.pingIntervalMs = 250;
    SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1910, 500, 0, 0, 0, false}, 1010);
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 500, 0, 0, 0, false}, 1020);
    controller.onMessage(makeMessage(MsgType::EnterAck, 1, 1), 1030);
    CHECK(controller.state() == SourceState::Remote);
    controller.onRaw(RawEvent{RawKind::Key, 0, 0, 0, 0, hid::LeftCtrl, true}, 1040);
    controller.tick(3100);
    CHECK(controller.state() == SourceState::Stopped);
    CHECK(controller.stopRequested());
    CHECK(!io.captureActive);
    CHECK(!io.observing);
    CHECK(io.warpX == 1895);
    CHECK(!io.stopReason.empty());
  }

  {
    FakeSourceIO io;
    SourceTuning tuning;
    tuning.maxSessionMs = 1000;
    SourceController controller(geo1920(), Edge::Left, io, tuning, 0);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::MotionAbs, 10, 500, 0, 0, 0, false}, 10);
    controller.onRaw(RawEvent{RawKind::MotionAbs, 0, 500, 0, 0, 0, false}, 20);
    controller.onMessage(makeMessage(MsgType::EnterAck, 1, 1), 30);
    CHECK(controller.state() == SourceState::Remote);
    controller.tick(1001);
    CHECK(controller.state() == SourceState::Stopped);
    CHECK(!io.captureActive);
    bool sawTimeout = false;
    for (const Message& message : io.sent) {
      if (message.type == MsgType::Leave && message.reason == static_cast<uint8_t>(LeaveReason::Timeout)) {
        sawTimeout = true;
      }
    }
    CHECK(sawTimeout);
  }

  {
    FakeSourceIO io;
    io.captureOk = false;
    SourceTuning tuning;
    SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1910, 500, 0, 0, 0, false}, 1010);
    controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 500, 0, 0, 0, false}, 1020);
    controller.onMessage(makeMessage(MsgType::EnterAck, 1, 1), 1030);
    CHECK(controller.state() == SourceState::Stopped);
    CHECK(controller.stopRequested());
    CHECK(!io.captureActive);
    bool sawFailure = false;
    for (const Message& message : io.sent) {
      if (message.type == MsgType::Leave &&
          message.reason == static_cast<uint8_t>(LeaveReason::InjectionFailed)) {
        sawFailure = true;
      }
    }
    CHECK(sawFailure);
  }
}

void testSourceEmergency() {
  FakeSourceIO io;
  SourceTuning tuning;
  SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
  CHECK(controller.start());
  controller.onRaw(RawEvent{RawKind::MotionAbs, 1910, 500, 0, 0, 0, false}, 1010);
  controller.onRaw(RawEvent{RawKind::MotionAbs, 1919, 500, 0, 0, 0, false}, 1020);
  controller.onMessage(makeMessage(MsgType::EnterAck, 1, 1), 1030);
  controller.onRaw(RawEvent{RawKind::Key, 0, 0, 0, 0, hid::LeftShift, true}, 1040);
  controller.onRaw(RawEvent{RawKind::Hotkey, 0, 0, 0, 0, 0, false}, 1050);

  CHECK(controller.state() == SourceState::Stopped);
  CHECK(controller.stopRequested());
  CHECK(!io.captureActive);
  CHECK(!io.observing);

  bool sawRelease = false;
  bool sawEmergency = false;
  for (const Message& message : io.sent) {
    if (message.type == MsgType::ReleaseAll &&
        message.reason == static_cast<uint8_t>(LeaveReason::Emergency)) {
      sawRelease = true;
    }
    if (message.type == MsgType::Leave &&
        message.reason == static_cast<uint8_t>(LeaveReason::Emergency)) {
      sawEmergency = true;
    }
  }
  CHECK(sawRelease);
  CHECK(sawEmergency);
}

void testTargetHappyPath() {
  FakeTargetIO io;
  TargetTuning tuning;
  tuning.watchdogMs = 500;
  TargetController controller(geo1920(), io, tuning, 0);

  controller.onMessage(enterRequest(1, 10, 32768), 100);
  CHECK(controller.state() == TargetState::Remote);
  CHECK(io.cursorX == 0);
  CHECK(io.cursorY >= 538 && io.cursorY <= 541);
  CHECK(io.sent.size() == 1);
  CHECK(io.sent[0].type == MsgType::EnterAck);
  CHECK(io.sent[0].generation == 1);
  const int entryY = io.cursorY;

  controller.onMessage(keyMessage(1, 11, hid::LeftShift, true), 110);
  CHECK(io.actions.back() == "key+:225");
  controller.onMessage(keyMessage(1, 12, hid::A, true), 120);
  CHECK(io.actions.back() == "key+:4");
  controller.onMessage(keyMessage(1, 13, hid::A, false), 130);
  CHECK(io.actions.back() == "key-:4");

  const size_t beforeDuplicate = io.actions.size();
  controller.onMessage(keyMessage(1, 14, hid::A, false), 140);
  CHECK(io.actions.size() == beforeDuplicate);

  controller.onMessage(keyMessage(1, 15, hid::A, true), 150);
  CHECK(io.actions.back() == "key+:4");

  const size_t beforeStale = io.actions.size();
  controller.onMessage(keyMessage(2, 16, hid::B, true), 160);
  CHECK(io.actions.size() == beforeStale);
  controller.onMessage(keyMessage(1, 14, hid::B, true), 170);
  CHECK(io.actions.size() == beforeStale);
  controller.onMessage(keyMessage(1, 16, 0x66, true), 180);
  CHECK(io.actions.size() == beforeStale);

  controller.onMessage(wheelMessage(1, 17, 100, 0), 190);
  CHECK(io.actions.back() == "wheel:16,0");

  controller.onMessage(motionMessage(1, 18, -1, 0), 200);
  CHECK(controller.state() == TargetState::Idle);
  CHECK(io.cursorX == 200 && io.cursorY == 300);
  CHECK(io.actions.back() == "move:200,300");
  CHECK(io.sent.back().type == MsgType::Leave);
  CHECK(io.sent.back().reason == static_cast<uint8_t>(LeaveReason::Returned));
  CHECK(controller.stopRequested() == false);

  controller.onMessage(enterRequest(2, 100, 1000), 300);
  CHECK(controller.state() == TargetState::Remote);
  CHECK(io.cursorX == 0);
  CHECK(io.cursorY == yFromRatio(geo1920(), 1000));
  CHECK(io.sent.back().type == MsgType::EnterAck);

  controller.onMessage(enterRequest(1, 200, 0), 310);
  CHECK(controller.state() == TargetState::Remote);
  CHECK(io.sent.back().type == MsgType::EnterReject);

  controller.onMessage(enterRequest(2, 101, 0), 320);
  CHECK(controller.state() == TargetState::Remote);
  CHECK(io.sent.back().type == MsgType::EnterAck);
  CHECK(io.cursorY == yFromRatio(geo1920(), 1000));

  controller.onMessage(keyMessage(2, 102, hid::LeftCtrl, true), 330);
  CHECK(io.actions.back() == "key+:224");
  controller.onMessage(releaseAllMessage(2, 103, LeaveReason::Shutdown), 340);
  CHECK(io.actions.back() == "key-:224");
  CHECK(controller.state() == TargetState::Remote);

  controller.onMessage(keyMessage(2, 104, hid::LeftAlt, true), 350);
  CHECK(io.actions.back() == "key+:226");
  controller.tick(350 + 501);
  CHECK(controller.state() == TargetState::Stopped);
  CHECK(controller.stopRequested());
  CHECK(io.actions[io.actions.size() - 2] == "key-:226");
  CHECK(io.actions.back() == "move:200,300");
  CHECK(io.sent.back().type == MsgType::Leave);
  CHECK(io.sent.back().reason == static_cast<uint8_t>(LeaveReason::LinkLost));
  (void)entryY;
}

void testTargetGuards() {
  {
    FakeTargetIO io;
    TargetTuning tuning;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(1, 1, 100), 100);
    controller.onMessage(keyMessage(1, 2, hid::LeftCtrl, true), 110);
    controller.onMessage(leaveMessage(1, LeaveReason::Emergency), 120);
    CHECK(controller.state() == TargetState::Stopped);
    CHECK(controller.stopRequested());
    CHECK(io.actions[io.actions.size() - 2] == "key-:224");
    CHECK(io.actions.back() == "move:200,300");
    CHECK(io.sent.back().type == MsgType::EnterAck);
  }

  {
    FakeTargetIO io;
    TargetTuning tuning;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(1, 1, 100), 100);
    controller.onLocalOverride(110);
    CHECK(controller.state() == TargetState::Stopped);
    CHECK(controller.stopRequested());
    CHECK(io.sent.back().type == MsgType::Leave);
    CHECK(io.sent.back().reason == static_cast<uint8_t>(LeaveReason::LocalOverride));
  }

  {
    FakeTargetIO io;
    io.injectOk = false;
    TargetTuning tuning;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(1, 1, 100), 100);
    CHECK(controller.state() == TargetState::Idle);
    CHECK(io.sent.size() == 1);
    CHECK(io.sent[0].type == MsgType::EnterReject);
    CHECK(io.sent[0].reason == static_cast<uint8_t>(LeaveReason::InjectionFailed));
  }

  {
    FakeTargetIO io;
    TargetTuning tuning;
    tuning.maxSessionMs = 1000;
    TargetController controller(geo1920(), io, tuning, 5000);
    controller.onMessage(enterRequest(1, 1, 100), 5050);
    CHECK(controller.state() == TargetState::Remote);
    controller.tick(6001);
    CHECK(controller.state() == TargetState::Stopped);
    CHECK(controller.stopRequested());
    bool sawTimeout = false;
    for (const Message& message : io.sent) {
      if (message.type == MsgType::Leave &&
          message.reason == static_cast<uint8_t>(LeaveReason::Timeout)) {
        sawTimeout = true;
      }
    }
    CHECK(sawTimeout);
  }

  {
    FakeTargetIO io;
    TargetTuning tuning;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(0, 1, 100), 100);
    CHECK(controller.state() == TargetState::Idle);
    CHECK(io.sent.back().type == MsgType::EnterReject);
    CHECK(io.cursorX == 200);
  }

  {
    FakeTargetIO io;
    TargetTuning tuning;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(1, 1, 100, Edge::Left), 100);
    CHECK(controller.state() == TargetState::Remote);
    CHECK(io.cursorX == 1919);
    controller.onMessage(motionMessage(1, 2, 1, 0), 110);
    CHECK(controller.state() == TargetState::Idle);
    CHECK(io.sent.back().reason == static_cast<uint8_t>(LeaveReason::Returned));
  }

  {
    // Un ping de una generación distinta no debe mantener viva una sesión remota.
    FakeTargetIO io;
    TargetTuning tuning;
    tuning.watchdogMs = 500;
    TargetController controller(geo1920(), io, tuning, 0);
    controller.onMessage(enterRequest(1, 1, 100), 100);
    CHECK(controller.state() == TargetState::Remote);
    controller.onMessage(makeMessage(MsgType::Ping, 0, 2), 400);
    controller.tick(700);
    CHECK(controller.state() == TargetState::Stopped);
    CHECK(controller.stopRequested());

    FakeTargetIO activeIo;
    TargetController active(geo1920(), activeIo, tuning, 0);
    active.onMessage(enterRequest(1, 1, 100), 100);
    active.onMessage(makeMessage(MsgType::Ping, 1, 2), 400);
    active.tick(700);
    CHECK(active.state() == TargetState::Remote);
    active.stop(710);
  }
}

void testWaylandGeometry() {
  std::string error;
  Geo geo;
  CHECK(!wl::zoneGeometry({}, geo, error));
  CHECK(!wl::zoneGeometry({wl::Zone{1920, 0, 0, 0}}, geo, error));
  CHECK(wl::zoneGeometry({wl::Zone{2560, 1440, -100, 50}}, geo, error));
  CHECK(geo.x == -100 && geo.y == 50 && geo.w == 2560 && geo.h == 1440);

  wl::Barrier barrier;
  CHECK(wl::sourceBarrier({wl::Zone{1920, 1080, 0, 0}}, Edge::Right, barrier, error));
  CHECK(barrier.x1 == 1920 && barrier.x2 == 1920);
  CHECK(barrier.y1 == 0 && barrier.y2 == 1079);
  CHECK(wl::sourceBarrier({wl::Zone{1920, 1080, 0, 0}}, Edge::Left, barrier, error));
  CHECK(barrier.x1 == 0 && barrier.x2 == 0);
  CHECK(!wl::sourceBarrier({wl::Zone{1920, 1080, 0, 0}, wl::Zone{1920, 1080, 1920, 0}},
                           Edge::Right, barrier, error));

  CHECK(wl::regionGeometry(10, 20, 1280, 720, geo));
  CHECK(geo.x == 10 && geo.y == 20 && geo.w == 1280 && geo.h == 720);
  CHECK(!wl::regionGeometry(0, 0, 0, 720, geo));
}

void testInputCaptureFlow() {
  FakePortal portal;
  wl::InputCaptureFlow flow(portal, Edge::Right);
  std::string error;
  CHECK(flow.open(error));
  CHECK((portal.calls == std::vector<std::string>{
             "createInputSession", "startInputSession", "connectInputEis", "inputZones",
             "setPointerBarriers", "enableInput"}));
  CHECK(flow.stage() == wl::CaptureStage::Armed);
  CHECK(flow.geometry().w == 1920 && flow.geometry().h == 1080);
  CHECK(portal.barriers.size() == 1);
  CHECK(portal.barriers[0].x1 == 1920 && portal.barriers[0].y2 == 1079);
  CHECK(portal.barrierZoneSet == 9);
  CHECK(flow.takeEisFd() == 42);
  CHECK(flow.takeEisFd() == -1);

  wl::Activation activation;
  CHECK(!flow.takeActivation(activation));
  flow.onActivated(77, 1931.5, 400.0, 1);
  CHECK(flow.active());
  CHECK(flow.takeActivation(activation));
  CHECK(activation.id == 77 && activation.barrierId == 1);
  CHECK(activation.x > 1931.0 && activation.x < 1932.0);
  CHECK(!flow.takeActivation(activation));
  CHECK(!flow.takeCaptureLost());
  CHECK(flow.release(1895, 540, error));
  CHECK(portal.releaseCount == 1 && portal.releaseId == 77);
  CHECK(portal.releaseX == 1895 && portal.releaseY == 540);
  CHECK(flow.stage() == wl::CaptureStage::Armed);
  CHECK(flow.release(1895, 540, error));
  CHECK(portal.releaseCount == 1);

  flow.onActivated(78, 1931.0, 600.0, 1);
  flow.onDeactivated(78);
  CHECK(flow.takeCaptureLost());
  CHECK(flow.stage() == wl::CaptureStage::Armed);
  CHECK(!flow.needsRearm());
  CHECK(!flow.takeCaptureLost());

  flow.onActivated(79, 1931.0, 700.0, 1);
  flow.onDisabled();
  CHECK(flow.takeCaptureLost());
  CHECK(flow.needsRearm());
  portal.calls.clear();
  CHECK(flow.rearm(error));
  CHECK((portal.calls == std::vector<std::string>{"enableInput"}));
  CHECK(!flow.needsRearm());
  CHECK(flow.stage() == wl::CaptureStage::Armed);

  portal.zones = {wl::Zone{2560, 1440, 0, 0}};
  flow.onZonesChanged();
  CHECK(flow.needsRearm());
  portal.calls.clear();
  CHECK(flow.rearm(error));
  CHECK((portal.calls == std::vector<std::string>{"inputZones", "setPointerBarriers",
                                                  "enableInput"}));
  CHECK(flow.geometry().w == 2560);
  CHECK(portal.barriers[0].x1 == 2560 && portal.barriers[0].y2 == 1439);

  flow.onActivated(80, 2561.0, 100.0, 1);
  flow.onClosed();
  std::string reason;
  CHECK(flow.takeCaptureLost());
  CHECK(flow.fatal(reason));
  CHECK(reason.find("cerró") != std::string::npos);
  CHECK(flow.stage() == wl::CaptureStage::Closed);

  portal.calls.clear();
  flow.close();
  CHECK(flow.stage() == wl::CaptureStage::Idle);
  CHECK(portal.inputSessionCloses == 1);
  CHECK(portal.eisCloses == 0);  // el descriptor se había transferido a libei
}

void testInputCaptureFailures() {
  std::string error;
  const char* stages[] = {"startInputSession", "connectInputEis", "inputZones",
                          "setPointerBarriers", "enableInput"};
  for (const char* stage : stages) {
    FakePortal portal;
    portal.failAt = stage;
    wl::InputCaptureFlow flow(portal, Edge::Right);
    CHECK(!flow.open(error));
    CHECK(flow.stage() == wl::CaptureStage::Failed);
    CHECK(!error.empty());
    std::string reason;
    CHECK(flow.fatal(reason));
    CHECK(portal.inputSessionCloses == 1);
    const bool eisOpen = std::string(stage) != "startInputSession" &&
                         std::string(stage) != "connectInputEis";
    CHECK(portal.eisCloses == (eisOpen ? 1 : 0));
  }

  {
    FakePortal portal;
    portal.failedBarriers = {1};
    wl::InputCaptureFlow flow(portal, Edge::Right);
    CHECK(!flow.open(error));
    CHECK(error.find("barrera") != std::string::npos);
    CHECK(portal.eisCloses == 1);
    CHECK(portal.closedFd == portal.eisFd);
    CHECK(portal.inputSessionCloses == 1);
  }

  {
    FakePortal portal;
    portal.failAt = "createInputSession";
    wl::InputCaptureFlow flow(portal, Edge::Right);
    CHECK(!flow.open(error));
    CHECK(flow.stage() == wl::CaptureStage::Failed);
    CHECK(portal.inputSessionCloses == 0);
    CHECK(portal.eisCloses == 0);
  }
}

void testRemoteDesktopFlow() {
  std::string error;
  // Probar la salida no crea RemoteDesktop, no conecta EIS ni exige geometría.
  {
    FakePortal portal;
    wl::RemoteDesktopFlow flow(portal);
    CHECK(flow.prepareExit(error));
    CHECK(flow.stage() == wl::TargetStage::ExitReady);
    CHECK(flow.grantedDevices() == 0);
    CHECK((portal.calls == std::vector<std::string>{"createShortcutSession", "bindEmergencyShortcut"}));
    flow.close();
    CHECK(portal.remoteCloses == 0 && portal.eisCloses == 0);
    CHECK(portal.shortcutCloses == 1);
  }
  // Sin pulsación real, ninguna solicitud de permisos de control ni descriptor.
  {
    FakePortal portal;
    portal.failAt = "verifyEmergencyShortcut";
    wl::RemoteDesktopFlow flow(portal);
    int fd = 99;
    CHECK(!flow.open(fd, error));
    CHECK(fd == -1);
    CHECK(flow.stage() == wl::TargetStage::Failed);
    CHECK((portal.calls == std::vector<std::string>{"createShortcutSession", "bindEmergencyShortcut",
                                                 "verifyEmergencyShortcut", "closeShortcutSession"}));
    CHECK(portal.remoteCloses == 0 && portal.eisCloses == 0);
    CHECK(portal.shortcutCloses == 1);
  }
  // Si el escritorio cierra el atajo preparado, tampoco se permite abrir control.
  {
    FakePortal portal;
    wl::RemoteDesktopFlow flow(portal);
    CHECK(flow.prepareExit(error));
    flow.onClosed();
    int fd = 99;
    CHECK(!flow.open(fd, error));
    CHECK(fd == -1);
    CHECK(portal.calls.size() == 2);
    flow.close();
    CHECK(portal.shortcutCloses == 1 && portal.remoteCloses == 0);
  }
  // Cada fallo de preparación cierra lo adquirido y no publica un FD utilizable.
  for (const char* step : {"createShortcutSession", "bindEmergencyShortcut", "createRemoteSession",
                           "selectRemoteDevices", "startRemoteSession", "connectRemoteEis"}) {
    FakePortal portal;
    portal.failAt = step;
    wl::RemoteDesktopFlow flow(portal);
    int fd = 99;
    CHECK(!flow.open(fd, error));
    CHECK(fd == -1);
    CHECK(!flow.shortcutBound());
    CHECK(portal.eisCloses == 0);
  }
  {
    FakePortal portal;
    wl::RemoteDesktopFlow flow(portal);
    int fd = -1;
    CHECK(flow.open(fd, error));
    CHECK(fd == 43);
    CHECK(flow.stage() == wl::TargetStage::Ready);
    CHECK(flow.grantedDevices() == 3);
    CHECK(flow.shortcutBound());
    CHECK(portal.selectedTypes == 3);
    CHECK((portal.calls == std::vector<std::string>{
               "createShortcutSession", "bindEmergencyShortcut", "verifyEmergencyShortcut",
               "createRemoteSession", "selectRemoteDevices", "startRemoteSession",
               "connectRemoteEis"}));
    flow.close();
    CHECK(flow.stage() == wl::TargetStage::Idle);
    CHECK(portal.remoteCloses == 1 && portal.shortcutCloses == 1);
  }

  {
    FakePortal portal;
    portal.grantedDevices = 1;  // el usuario solo concedió teclado
    wl::RemoteDesktopFlow flow(portal);
    int fd = -1;
    CHECK(!flow.open(fd, error));
    CHECK(error.find("teclado y puntero") != std::string::npos);
    CHECK(portal.remoteCloses == 1);
    CHECK(portal.eisCloses == 0);
  }

  {
    FakePortal portal;
    portal.shortcutBound = false;
    wl::RemoteDesktopFlow flow(portal);
    int fd = -1;
    CHECK(!flow.open(fd, error));
    CHECK(error.find("Ctrl+Shift+F12") != std::string::npos);
    CHECK(portal.eisCloses == 0);
    CHECK(portal.remoteCloses == 0 && portal.shortcutCloses == 1);
  }

  {
    FakePortal portal;
    wl::RemoteDesktopFlow flow(portal);
    int fd = -1;
    CHECK(flow.open(fd, error));
    flow.onClosed();
    std::string reason;
    CHECK(flow.fatal(reason));
    CHECK(reason.find("cerró") != std::string::npos);
  }
}

void testSourcePortalEvents() {
  std::string error;
  {
    FakeSourceIO io;
    SourceTuning tuning;
    tuning.cooldownMs = 300;
    SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::PortalEdge, 1924, 400, 0, 0, 0, false}, 1100);
    CHECK(io.sent.size() == 1);
    CHECK(io.sent[0].type == MsgType::EnterRequest);
    CHECK(std::abs(static_cast<int>(io.sent[0].ratio) -
                   static_cast<int>(ratioFromY(geo1920(), 400))) <= 1);
    controller.onRaw(RawEvent{RawKind::PortalEdge, 1924, 500, 0, 0, 0, false}, 1110);
    CHECK(io.sent.size() == 1);

    controller.onMessage(makeMessage(MsgType::EnterReject, 1, 1), 1120);
    CHECK(controller.state() == SourceState::Idle);
    CHECK(io.captureReleases > 0);
    const size_t released = io.sent.size();
    controller.onRaw(RawEvent{RawKind::PortalEdge, 1924, 500, 0, 0, 0, false}, 1300);
    CHECK(io.sent.size() == released);
    controller.onRaw(RawEvent{RawKind::PortalEdge, 1924, 500, 0, 0, 0, false}, 1450);
    CHECK(io.sent.size() == released + 1);
    CHECK(io.sent.back().generation == 2);
  }

  {
    FakeSourceIO io;
    SourceTuning tuning;
    SourceController controller(geo1920(), Edge::Right, io, tuning, 1000);
    CHECK(controller.start());
    controller.onRaw(RawEvent{RawKind::PortalEdge, 1924, 400, 0, 0, 0, false}, 1100);
    controller.onMessage(makeMessage(MsgType::EnterAck, 1, 1), 1110);
    CHECK(controller.state() == SourceState::Remote);
    controller.onRaw(RawEvent{RawKind::CaptureLost, 0, 0, 0, 0, 0, false}, 1120);
    CHECK(controller.state() == SourceState::Idle);
    CHECK(!io.captureActive);
    CHECK(io.observing);
    CHECK(!controller.stopRequested());
    controller.onBackendFailure("el escritorio cerró la sesión", 1130);
    CHECK(controller.state() == SourceState::Stopped);
    CHECK(controller.stopReason() == "el escritorio cerró la sesión");
    CHECK(!io.observing);
  }
}

}  // namespace

int runSelfTests() {
  std::printf("OneInTwo input experiment: self-tests aislados\n");
  testProtocol();
  testKeymap();
  testGeometry();
  testInputState();
  testSourceHappyPath();
  testSourceTimeouts();
  testSourceEmergency();
  testTargetHappyPath();
  testTargetGuards();
  testWaylandGeometry();
  testInputCaptureFlow();
  testInputCaptureFailures();
  testRemoteDesktopFlow();
  testSourcePortalEvents();
  std::printf("Comprobaciones: %d, fallos: %d\n", g_checks, g_failures);
  return g_failures;
}

}  // namespace oit
