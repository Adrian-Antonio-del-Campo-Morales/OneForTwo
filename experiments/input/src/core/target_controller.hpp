#pragma once
// Responsabilidad: máquina de estados del destino: aceptar una entrada,
// inyectar eventos, detectar el retorno y liberar todo ante cualquier salida.

#include "input_state.hpp"
#include "types.hpp"
#include <string>

namespace oit {

struct TargetTuning {
  int watchdogMs = 1500;
  int maxSessionMs = 180000;
  int maxWheelTicks = 16;
};

class TargetIO {
 public:
  virtual ~TargetIO() = default;
  virtual bool send(const Message& message) = 0;
  virtual bool injectMove(int x, int y) = 0;
  virtual bool injectKey(uint16_t usage, bool pressed) = 0;
  virtual bool injectButton(uint16_t button, bool pressed) = 0;
  virtual bool injectWheel(int16_t dx, int16_t dy) = 0;
  virtual bool cursorPosition(int& x, int& y) = 0;
  virtual void requestStop(const std::string& reason) = 0;
};

enum class TargetState { Idle, Remote, Stopped };

class TargetController {
 public:
  TargetController(const Geo& geo, TargetIO& io, const TargetTuning& tuning, int64_t startMs);

  void onMessage(const Message& message, int64_t nowMs);
  void onLocalOverride(int64_t nowMs);
  void onEmergency(int64_t nowMs);
  void onLinkLost(int64_t nowMs);
  void tick(int64_t nowMs);
  void stop(int64_t nowMs);

  TargetState state() const { return state_; }
  bool stopRequested() const { return stopRequested_; }
  const std::string& stopReason() const { return stopReason_; }
  uint32_t activeGeneration() const { return activeGen_; }
  int cursorX() const { return cursorX_; }
  int cursorY() const { return cursorY_; }

 private:
  bool validControl(const Message& message);
  void enterRemote(const Message& message, int64_t nowMs);
  void reject(uint32_t generation, LeaveReason reason);
  void sendAck(int64_t nowMs);
  void applyMotion(const Message& message, int64_t nowMs);
  void applyButton(const Message& message);
  void applyKey(const Message& message);
  void applyWheel(const Message& message);
  void releaseHeld();
  void restoreCursor();
  void leave(LeaveReason reason, int64_t nowMs, bool sendLeave);
  void onSourceLeave(const Message& message, int64_t nowMs);

  Geo geo_;
  TargetIO& io_;
  TargetTuning cfg_;
  TargetState state_ = TargetState::Idle;
  bool stopRequested_ = false;
  std::string stopReason_;
  uint32_t activeGen_ = 0;
  uint32_t lastSeq_ = 0;
  uint32_t seq_ = 0;
  int64_t startMs_ = 0;
  int64_t lastSourceMs_ = 0;
  Edge edge_ = Edge::Right;
  int cursorX_ = 0;
  int cursorY_ = 0;
  int preX_ = 0;
  int preY_ = 0;
  bool havePre_ = false;
  InputState inputState_;
};

}  // namespace oit
