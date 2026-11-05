#pragma once
// Responsabilidad: máquina de estados del origen (local, preparación y control
// remoto), independiente del escritorio y del canal. Se prueba con dobles.

#include "geometry.hpp"
#include "types.hpp"
#include <string>

namespace oit {

struct SourceTuning {
  int antiBouncePx = 48;
  int cooldownMs = 300;
  int ackTimeoutMs = 600;
  int linkTimeoutMs = 2000;
  int pingIntervalMs = 250;
  int maxSessionMs = 180000;
  int parkInsetPx = 24;
};

class SourceIO {
 public:
  virtual ~SourceIO() = default;
  virtual bool send(const Message& message) = 0;
  virtual bool startObserving() = 0;
  virtual bool setCaptureActive(bool active) = 0;
  virtual void stopObserving() = 0;
  virtual void warpCursor(int x, int y) = 0;
  virtual void requestStop(const std::string& reason) = 0;
};

enum class SourceState { Idle, Preparing, Remote, Stopped };

class SourceController {
 public:
  SourceController(const Geo& geo, Edge edge, SourceIO& io, const SourceTuning& tuning,
                   int64_t startMs);

  bool start();
  void onRaw(const RawEvent& event, int64_t nowMs);
  void onMessage(const Message& message, int64_t nowMs);
  void onLinkLost(int64_t nowMs);
  // Fallo irrecuperable del adaptador (por ejemplo, sesión de portal cerrada).
  void onBackendFailure(const std::string& reason, int64_t nowMs);
  void tick(int64_t nowMs);
  void stop(int64_t nowMs);

  SourceState state() const { return state_; }
  bool stopRequested() const { return stopRequested_; }
  const std::string& stopReason() const { return stopReason_; }
  uint32_t activeGeneration() const { return activeGen_; }
  uint16_t pendingRatio() const { return pendingRatio_; }

 private:
  void requestEnter(int64_t nowMs);
  void beginLocal(int64_t nowMs, bool warp);
  void beginStop(const std::string& reason, bool notifyPeer, LeaveReason code, int64_t nowMs);
  int parkX() const;
  int parkY() const;
  int centerX() const;
  int centerY() const;

  Geo geo_;
  Edge edge_;
  SourceIO& io_;
  SourceTuning cfg_;
  EdgeGate gate_;
  SourceState state_ = SourceState::Idle;
  bool stopRequested_ = false;
  std::string stopReason_;
  uint32_t genCounter_ = 0;
  uint32_t pendingGen_ = 0;
  uint32_t activeGen_ = 0;
  uint32_t seq_ = 0;
  uint16_t pendingRatio_ = 0;
  int64_t startMs_ = 0;
  int64_t pendingSinceMs_ = 0;
  int64_t lastPeerMs_ = 0;
  int64_t nextPingMs_ = 0;
  int64_t lastReturnMs_ = 0;
};

}  // namespace oit
