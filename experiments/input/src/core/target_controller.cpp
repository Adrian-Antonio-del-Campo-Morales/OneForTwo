// Responsabilidad: validar generación/secuencia, inyectar solo entradas
// autorizadas y liberar teclas/botones ante retorno, fallo o cierre.
#include "target_controller.hpp"

#include "geometry.hpp"
#include "keymap.hpp"

#include <algorithm>

namespace oit {

TargetController::TargetController(const Geo& geo, TargetIO& io, const TargetTuning& tuning,
                                   int64_t startMs)
    : geo_(geo), io_(io), cfg_(tuning), startMs_(startMs) {}

void TargetController::onMessage(const Message& message, int64_t nowMs) {
  if (state_ == TargetState::Stopped) return;

  switch (message.type) {
    case MsgType::Ping: {
      // Un origen que ya no controla (generación 0 o distinta) no renueva el
      // watchdog; de lo contrario un ping inactivo mantendría viva la sesión.
      if (state_ != TargetState::Remote || message.generation == activeGen_) {
        lastSourceMs_ = nowMs;
      }
      Message pong;
      pong.type = MsgType::Pong;
      pong.sequence = ++seq_;
      if (!io_.send(pong)) onLinkLost(nowMs);
      return;
    }
    case MsgType::Pong:
      lastSourceMs_ = nowMs;
      return;
    case MsgType::EnterRequest:
      enterRemote(message, nowMs);
      return;
    case MsgType::Leave:
      onSourceLeave(message, nowMs);
      return;
    default:
      break;
  }

  if (!validControl(message)) return;
  lastSourceMs_ = nowMs;

  switch (message.type) {
    case MsgType::ReleaseAll:
      releaseHeld();
      break;
    case MsgType::Motion:
      applyMotion(message, nowMs);
      break;
    case MsgType::Button:
      applyButton(message);
      break;
    case MsgType::Key:
      applyKey(message);
      break;
    case MsgType::Wheel:
      applyWheel(message);
      break;
    default:
      break;
  }
}

bool TargetController::validControl(const Message& message) {
  if (state_ != TargetState::Remote) return false;
  if (message.generation != activeGen_) return false;
  if (message.sequence <= lastSeq_) return false;
  lastSeq_ = message.sequence;
  return true;
}

void TargetController::enterRemote(const Message& message, int64_t nowMs) {
  if (message.generation == 0) {
    reject(message.generation, LeaveReason::Stale);
    return;
  }
  if (state_ == TargetState::Remote && message.generation == activeGen_) {
    lastSourceMs_ = nowMs;
    sendAck(nowMs);
    return;
  }
  if (activeGen_ != 0 && message.generation <= activeGen_) {
    reject(message.generation, LeaveReason::Stale);
    return;
  }

  if (state_ == TargetState::Remote) {
    releaseHeld();
    restoreCursor();
  }

  int preX = geo_.x + geo_.w / 2;
  int preY = geo_.y + geo_.h / 2;
  if (io_.cursorPosition(preX, preY)) {
    preX_ = preX;
    preY_ = preY;
    havePre_ = true;
  } else {
    havePre_ = false;
  }

  edge_ = message.code == static_cast<uint16_t>(Edge::Left) ? Edge::Left : Edge::Right;
  const int entryX = edge_ == Edge::Right ? geo_.x : geo_.x + geo_.w - 1;
  const int entryY = yFromRatio(geo_, message.ratio);
  if (!io_.injectMove(entryX, entryY)) {
    reject(message.generation, LeaveReason::InjectionFailed);
    state_ = TargetState::Idle;
    return;
  }

  inputState_.clear();
  activeGen_ = message.generation;
  lastSeq_ = message.sequence;
  cursorX_ = entryX;
  cursorY_ = entryY;
  state_ = TargetState::Remote;
  lastSourceMs_ = nowMs;
  sendAck(nowMs);
}

void TargetController::reject(uint32_t generation, LeaveReason reason) {
  Message message;
  message.type = MsgType::EnterReject;
  message.generation = generation;
  message.sequence = ++seq_;
  message.reason = static_cast<uint8_t>(reason);
  io_.send(message);
}

void TargetController::sendAck(int64_t nowMs) {
  Message message;
  message.type = MsgType::EnterAck;
  message.generation = activeGen_;
  message.sequence = ++seq_;
  if (!io_.send(message)) {
    onLinkLost(nowMs);
  }
}

void TargetController::applyMotion(const Message& message, int64_t nowMs) {
  int nx = cursorX_ + message.x;
  int ny = cursorY_ + message.y;

  // El borde de retorno del destino es el opuesto al compartido por el origen.
  if (edge_ == Edge::Right) {
    if (nx <= geo_.x && message.x < 0) {
      leave(LeaveReason::Returned, nowMs, true);
      return;
    }
  } else {
    if (nx >= geo_.x + geo_.w - 1 && message.x > 0) {
      leave(LeaveReason::Returned, nowMs, true);
      return;
    }
  }

  nx = std::max(geo_.x, std::min(geo_.x + geo_.w - 1, nx));
  ny = std::max(geo_.y, std::min(geo_.y + geo_.h - 1, ny));
  if (io_.injectMove(nx, ny)) {
    cursorX_ = nx;
    cursorY_ = ny;
  }
}

void TargetController::applyButton(const Message& message) {
  if (message.code > 4) return;
  const bool changed = message.pressed ? inputState_.buttonDown(message.code)
                                       : inputState_.buttonUp(message.code);
  if (changed) io_.injectButton(message.code, message.pressed);
}

void TargetController::applyKey(const Message& message) {
  if (!isMappedUsage(message.code)) return;
  const bool changed =
      message.pressed ? inputState_.keyDown(message.code) : inputState_.keyUp(message.code);
  if (changed) io_.injectKey(message.code, message.pressed);
}

void TargetController::applyWheel(const Message& message) {
  const int maxTicks = std::max<int>(1, cfg_.maxWheelTicks);
  const int16_t dx = static_cast<int16_t>(std::max(-maxTicks, std::min(maxTicks, static_cast<int>(message.x))));
  const int16_t dy = static_cast<int16_t>(std::max(-maxTicks, std::min(maxTicks, static_cast<int>(message.y))));
  if (dx == 0 && dy == 0) return;
  io_.injectWheel(dx, dy);
}

void TargetController::releaseHeld() {
  const std::vector<HeldInput> held = inputState_.releaseAll();
  for (const HeldInput& input : held) {
    if (input.button) {
      io_.injectButton(input.code, false);
    } else {
      io_.injectKey(input.code, false);
    }
  }
}

void TargetController::restoreCursor() {
  if (!havePre_) return;
  io_.injectMove(preX_, preY_);
  havePre_ = false;
}

void TargetController::leave(LeaveReason reason, int64_t nowMs, bool sendLeave) {
  if (state_ != TargetState::Remote) return;
  const uint32_t generation = activeGen_;
  state_ = TargetState::Idle;
  releaseHeld();
  restoreCursor();
  lastSourceMs_ = nowMs;

  if (sendLeave) {
    Message message;
    message.type = MsgType::Leave;
    message.generation = generation;
    message.sequence = ++seq_;
    message.reason = static_cast<uint8_t>(reason);
    if (!io_.send(message)) {
      state_ = TargetState::Stopped;
      stopRequested_ = true;
      stopReason_ = "conexión perdida al liberar el control";
      io_.requestStop(stopReason_);
      return;
    }
  }

  if (reason != LeaveReason::Returned) {
    state_ = TargetState::Stopped;
    stopRequested_ = true;
    stopReason_ = std::string("control finalizado: ") + leaveReasonName(reason);
    io_.requestStop(stopReason_);
  }
}

void TargetController::onSourceLeave(const Message& message, int64_t nowMs) {
  if (state_ != TargetState::Remote || message.generation != activeGen_) return;
  const LeaveReason reason = static_cast<LeaveReason>(message.reason);
  state_ = TargetState::Idle;
  releaseHeld();
  restoreCursor();
  lastSourceMs_ = nowMs;
  if (reason != LeaveReason::Returned) {
    state_ = TargetState::Stopped;
    stopRequested_ = true;
    stopReason_ = std::string("el origen terminó el control (") + leaveReasonName(reason) + ")";
    io_.requestStop(stopReason_);
  }
}

void TargetController::onLocalOverride(int64_t nowMs) {
  if (state_ != TargetState::Remote) return;
  leave(LeaveReason::LocalOverride, nowMs, true);
}

void TargetController::onEmergency(int64_t nowMs) {
  if (state_ == TargetState::Remote) {
    leave(LeaveReason::Emergency, nowMs, true);
  } else {
    state_ = TargetState::Stopped;
    stopRequested_ = true;
    stopReason_ = "salida local";
    io_.requestStop(stopReason_);
  }
}

void TargetController::onLinkLost(int64_t nowMs) {
  if (state_ == TargetState::Stopped) return;
  if (state_ == TargetState::Remote) {
    state_ = TargetState::Idle;
    releaseHeld();
    restoreCursor();
  }
  state_ = TargetState::Stopped;
  stopRequested_ = true;
  stopReason_ = "conexión perdida";
  io_.requestStop(stopReason_);
  (void)nowMs;
}

void TargetController::tick(int64_t nowMs) {
  if (state_ == TargetState::Stopped) return;

  if (cfg_.maxSessionMs > 0 && nowMs - startMs_ >= cfg_.maxSessionMs) {
    if (state_ == TargetState::Remote) {
      Message release;
      release.type = MsgType::ReleaseAll;
      release.generation = activeGen_;
      release.sequence = ++seq_;
      release.reason = static_cast<uint8_t>(LeaveReason::Timeout);
      io_.send(release);
      leave(LeaveReason::Timeout, nowMs, true);
    }
    state_ = TargetState::Stopped;
    stopRequested_ = true;
    if (stopReason_.empty()) stopReason_ = "tiempo máximo de sesión";
    io_.requestStop(stopReason_);
    return;
  }

  if (state_ == TargetState::Remote && nowMs - lastSourceMs_ > cfg_.watchdogMs) {
    leave(LeaveReason::LinkLost, nowMs, true);
    if (state_ != TargetState::Stopped) {
      state_ = TargetState::Stopped;
      stopRequested_ = true;
      stopReason_ = "sin datos del origen";
      io_.requestStop(stopReason_);
    }
  }
}

void TargetController::stop(int64_t nowMs) {
  if (state_ == TargetState::Stopped) return;
  if (state_ == TargetState::Remote) {
    Message release;
    release.type = MsgType::ReleaseAll;
    release.generation = activeGen_;
    release.sequence = ++seq_;
    release.reason = static_cast<uint8_t>(LeaveReason::Shutdown);
    io_.send(release);
    leave(LeaveReason::Shutdown, nowMs, true);
    return;
  }
  state_ = TargetState::Stopped;
  stopRequested_ = true;
  stopReason_ = "cierre solicitado";
  io_.requestStop(stopReason_);
}

}  // namespace oit
