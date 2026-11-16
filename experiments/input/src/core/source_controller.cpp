// Responsibility: coordinate the capture of the origin with the channel: crossing of
// border, waiting for acceptance, sending events, return and recovery.
#include "source_controller.hpp"

#include <algorithm>

namespace oit {

SourceController::SourceController(const Geo& geo, Edge edge, SourceIO& io,
                                   const SourceTuning& tuning, int64_t startMs)
    : geo_(geo),
      edge_(edge),
      io_(io),
      cfg_(tuning),
      gate_(geo, edge, tuning.antiBouncePx, tuning.cooldownMs),
      startMs_(startMs) {}

bool SourceController::start() {
  if (state_ != SourceState::Idle) return false;
  if (!io_.startObserving()) {
    state_ = SourceState::Stopped;
    stopRequested_ = true;
    stopReason_ = "the local entrance could not be observed";
    io_.requestStop(stopReason_);
    return false;
  }
  lastPeerMs_ = startMs_;
  nextPingMs_ = startMs_ + std::max(50, cfg_.pingIntervalMs);
  lastReturnMs_ = startMs_ - std::max(1, cfg_.cooldownMs);
  return true;
}

void SourceController::onRaw(const RawEvent& event, int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;

  if (event.kind == RawKind::Hotkey) {
    if (state_ != SourceState::Idle) {
      beginStop("salida local Ctrl+Alt+F12", true, LeaveReason::Emergency, nowMs);
    }
    return;
  }

  if (event.kind == RawKind::CaptureLost) {
    // The composer withdrew the capture (Deactivated/Disabled/Cerrado).
    // a local without closing the process: the rearming adapter if the session is still alive.
    if (state_ == SourceState::Remote || state_ == SourceState::Preparing) {
      beginLocal(nowMs, true);
    }
    return;
  }

  if (event.kind == RawKind::PortalEdge) {
    // In Wayland the composer already applied the barrier; it is not re-evaluated with EdgeGate.
    // Cooling prevents reacting to late activation after Release.
    if (state_ != SourceState::Idle) return;
    if (nowMs - lastReturnMs_ < std::max(0, cfg_.cooldownMs)) return;
    pendingRatio_ = ratioFromY(geo_, event.y);
    requestEnter(nowMs);
    return;
  }

  if (state_ == SourceState::Idle && event.kind == RawKind::MotionAbs) {
    uint16_t ratio = 0;
    if (gate_.observe(event.x - geo_.x, event.y - geo_.y, nowMs, ratio)) {
      pendingRatio_ = ratio;
      requestEnter(nowMs);
    }
    return;
  }

  if (state_ != SourceState::Remote) return;

  Message message;
  message.generation = activeGen_;
  message.sequence = ++seq_;
  switch (event.kind) {
    case RawKind::MotionDelta:
      message.type = MsgType::Motion;
      message.x = event.dx;
      message.y = event.dy;
      io_.send(message);
      break;
    case RawKind::Button:
      message.type = MsgType::Button;
      message.code = event.code;
      message.pressed = event.pressed;
      io_.send(message);
      break;
    case RawKind::Wheel:
      message.type = MsgType::Wheel;
      message.x = event.dx;
      message.y = event.dy;
      io_.send(message);
      break;
    case RawKind::Key:
      message.type = MsgType::Key;
      message.code = event.code;
      message.pressed = event.pressed;
      io_.send(message);
      break;
    default:
      break;
  }
}

void SourceController::requestEnter(int64_t nowMs) {
  if (state_ != SourceState::Idle) return;
  pendingGen_ = ++genCounter_;
  if (pendingGen_ == 0) pendingGen_ = ++genCounter_;
  pendingSinceMs_ = nowMs;
  state_ = SourceState::Preparing;

  Message message;
  message.type = MsgType::EnterRequest;
  message.generation = pendingGen_;
  message.sequence = ++seq_;
  message.code = static_cast<uint8_t>(edge_);
  message.ratio = pendingRatio_;
  if (!io_.send(message)) onLinkLost(nowMs);
}

void SourceController::onMessage(const Message& message, int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;

  if (message.type == MsgType::Ping) {
    lastPeerMs_ = nowMs;
    Message pong;
    pong.type = MsgType::Pong;
    pong.sequence = ++seq_;
    if (!io_.send(pong)) onLinkLost(nowMs);
    return;
  }
  if (message.type == MsgType::Pong) {
    lastPeerMs_ = nowMs;
    return;
  }

  if (message.type == MsgType::EnterAck) {
    if (state_ != SourceState::Preparing || message.generation != pendingGen_) return;
    if (!io_.setCaptureActive(true)) {
      Message leave;
      leave.type = MsgType::Leave;
      leave.generation = pendingGen_;
      leave.sequence = ++seq_;
      leave.reason = static_cast<uint8_t>(LeaveReason::InjectionFailed);
      io_.send(leave);
      beginLocal(nowMs, false);
      io_.stopObserving();
      state_ = SourceState::Stopped;
      stopRequested_ = true;
      stopReason_ = "could not activate capture at source";
      io_.requestStop(stopReason_);
      return;
    }
    state_ = SourceState::Remote;
    activeGen_ = pendingGen_;
    lastPeerMs_ = nowMs;
    // It maintains room to calculate deltas while the physical pointer moves.
    io_.warpCursor(centerX(), centerY());
    return;
  }

  if (message.type == MsgType::EnterReject) {
    if (state_ == SourceState::Preparing && message.generation == pendingGen_) {
      beginLocal(nowMs, true);
    }
    return;
  }

  if (message.type == MsgType::Leave) {
    const bool applies = (state_ == SourceState::Remote && message.generation == activeGen_) ||
                         (state_ == SourceState::Preparing && message.generation == pendingGen_);
    if (!applies) return;
    const LeaveReason reason = static_cast<LeaveReason>(message.reason);
    beginLocal(nowMs, true);
    lastPeerMs_ = nowMs;
    if (reason != LeaveReason::Returned) {
      io_.stopObserving();
      state_ = SourceState::Stopped;
      stopRequested_ = true;
      stopReason_ = std::string("the destination has finished the control (") + leaveReasonName(reason) + ")";
      io_.requestStop(stopReason_);
    }
    return;
  }

  // ReleaseAll of the destination requires no action: the origin never injects.
}

void SourceController::beginLocal(int64_t nowMs, bool warp) {
  // It also releases a pre-armed capture in Preparing: the portal may have
  // enabled before the destination confirms the EnterRequest.
  io_.setCaptureActive(false);
  if (warp) io_.warpCursor(parkX(), parkY());
  gate_.noteReturn(nowMs);
  lastReturnMs_ = nowMs;
  state_ = SourceState::Idle;
}

void SourceController::beginStop(const std::string& reason, bool notifyPeer, LeaveReason code,
                                 int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;
  const bool hadControl = state_ == SourceState::Remote || state_ == SourceState::Preparing;
  const uint32_t generation = state_ == SourceState::Remote ? activeGen_ : pendingGen_;

  if (notifyPeer && hadControl) {
    Message release;
    release.type = MsgType::ReleaseAll;
    release.generation = generation;
    release.sequence = ++seq_;
    release.reason = static_cast<uint8_t>(code);
    io_.send(release);

    Message leave;
    leave.type = MsgType::Leave;
    leave.generation = generation;
    leave.sequence = ++seq_;
    leave.reason = static_cast<uint8_t>(code);
    io_.send(leave);
  }

  io_.setCaptureActive(false);
  if (hadControl) io_.warpCursor(parkX(), parkY());
  gate_.noteReturn(nowMs);
  io_.stopObserving();
  state_ = SourceState::Stopped;
  stopRequested_ = true;
  stopReason_ = reason;
  io_.requestStop(stopReason_);
}

void SourceController::onLinkLost(int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;
  const bool hadControl = state_ == SourceState::Remote || state_ == SourceState::Preparing;
  io_.setCaptureActive(false);
  if (hadControl) io_.warpCursor(parkX(), parkY());
  gate_.noteReturn(nowMs);
  io_.stopObserving();
  state_ = SourceState::Stopped;
  stopRequested_ = true;
  stopReason_ = "missing connection";
  io_.requestStop(stopReason_);
}

void SourceController::onBackendFailure(const std::string& reason, int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;
  const bool hadControl = state_ == SourceState::Remote || state_ == SourceState::Preparing;
  io_.setCaptureActive(false);
  if (hadControl) io_.warpCursor(parkX(), parkY());
  gate_.noteReturn(nowMs);
  lastReturnMs_ = nowMs;
  io_.stopObserving();
  state_ = SourceState::Stopped;
  stopRequested_ = true;
  stopReason_ = reason;
  io_.requestStop(reason);
}

void SourceController::tick(int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;

  if (cfg_.maxSessionMs > 0 && nowMs - startMs_ >= cfg_.maxSessionMs) {
    const bool hadControl = state_ == SourceState::Remote || state_ == SourceState::Preparing;
    beginStop("maximum session time", hadControl, LeaveReason::Timeout, nowMs);
    return;
  }

  if (state_ == SourceState::Preparing && nowMs - pendingSinceMs_ > cfg_.ackTimeoutMs) {
    Message leave;
    leave.type = MsgType::Leave;
    leave.generation = pendingGen_;
    leave.sequence = ++seq_;
    leave.reason = static_cast<uint8_t>(LeaveReason::LinkLost);
    io_.send(leave);
    beginLocal(nowMs, true);
  }

  if (nowMs >= nextPingMs_) {
    Message ping;
    ping.type = MsgType::Ping;
    ping.generation = state_ == SourceState::Remote ? activeGen_ : 0;
    ping.sequence = ++seq_;
    if (!io_.send(ping)) {
      onLinkLost(nowMs);
      return;
    }
    nextPingMs_ = nowMs + std::max(50, cfg_.pingIntervalMs);
  }

  if ((state_ == SourceState::Remote || state_ == SourceState::Preparing) &&
      nowMs - lastPeerMs_ > cfg_.linkTimeoutMs) {
    io_.setCaptureActive(false);
    io_.warpCursor(parkX(), parkY());
    gate_.noteReturn(nowMs);
    io_.stopObserving();
    state_ = SourceState::Stopped;
    stopRequested_ = true;
    stopReason_ = "the destination does not respond";
    io_.requestStop(stopReason_);
    return;
  }

  if (state_ == SourceState::Idle && nowMs - lastPeerMs_ > cfg_.linkTimeoutMs &&
      nowMs - startMs_ > cfg_.linkTimeoutMs) {
    io_.stopObserving();
    state_ = SourceState::Stopped;
    stopRequested_ = true;
    stopReason_ = "the connection does not respond";
    io_.requestStop(stopReason_);
  }
}

void SourceController::stop(int64_t nowMs) {
  if (state_ == SourceState::Stopped) return;
  const bool hadControl = state_ == SourceState::Remote || state_ == SourceState::Preparing;
  beginStop("closure requested", hadControl, LeaveReason::Shutdown, nowMs);
}

int SourceController::parkX() const {
  int inset = cfg_.parkInsetPx;
  if (inset < 1) inset = 1;
  if (inset > geo_.w / 2 && geo_.w / 2 > 0) inset = geo_.w / 2;
  return edge_ == Edge::Right ? geo_.x + geo_.w - 1 - inset : geo_.x + inset;
}

int SourceController::parkY() const { return geo_.y + geo_.h / 2; }

int SourceController::centerX() const { return geo_.x + geo_.w / 2; }

int SourceController::centerY() const { return geo_.y + geo_.h / 2; }

}  // namespace oit
