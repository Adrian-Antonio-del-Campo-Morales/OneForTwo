// Responsabilidad: implementar la política del motor de sincronización.
// El orden es determinista y no depende de relojes de pared.

#include "engine.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace oit {
namespace {
constexpr std::size_t kSeenLimit = 128;

std::int64_t steady_now_ms() {
  return static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}
}  // namespace

SyncEngine::SyncEngine(ClipboardPort* port, EngineOptions options, SendFn send,
                       EventFn event)
    : port_(port),
      options_(std::move(options)),
      send_(std::move(send)),
      event_(std::move(event)) {
  if (options_.echo_window_ms < 0) {
    options_.echo_window_ms = 0;
  }
}

void SyncEngine::emit(EngineEventKind kind, Status status, std::string detail,
                      const ClipboardUpdate& update) {
  if (!event_) {
    return;
  }
  EngineEvent event;
  event.kind = kind;
  event.status = std::move(status);
  event.detail = std::move(detail);
  event.update = update;
  event_(event);
}

bool SyncEngine::seen(const std::string& key) const {
  return std::find(seen_keys_.begin(), seen_keys_.end(), key) !=
         seen_keys_.end();
}

void SyncEngine::remember(const std::string& key) {
  seen_keys_.push_back(key);
  while (seen_keys_.size() > kSeenLimit) {
    seen_keys_.pop_front();
  }
}

std::string SyncEngine::make_marker(const ClipboardUpdate& update) const {
  return "oit1:" + options_.self_id + ":" + hex_u64(update.generation) + ":" +
         update.origin + ":" + std::to_string(update.logical);
}

bool SyncEngine::is_self_marker(std::string_view marker) const {
  if (marker.empty()) {
    return false;
  }
  const std::string prefix = "oit1:" + options_.self_id + ":";
  return marker.size() <= 160 && marker.compare(0, prefix.size(), prefix) == 0;
}

void SyncEngine::on_local_clipboard(const ClipboardContent& content) {
  const Validation validation =
      validate_text(content.text, options_.max_text_bytes);
  if (!validation.valid) {
    emit(EngineEventKind::LocalRejected, validation.status,
         validation.status.detail, ClipboardUpdate{});
    return;
  }

  // 1. Eco identificado por el marcador que escribió este proceso.
  if (!content.marker.empty() && is_self_marker(content.marker)) {
    self_marker_.clear();
    emit(EngineEventKind::LocalEchoSuppressed,
         {StatusCode::Ok, ""},
         "own clipboard write detected via marker", ClipboardUpdate{});
    return;
  }

  // 2. Respaldo: algunas aplicaciones o gestores de portapapeles eliminan
  // formatos personalizados. Se suprime una única notificación idéntica
  // dentro de una ventana corta. Es una heurística local, no autenticación.
  const std::int64_t now =
      options_.now_ms ? options_.now_ms() : steady_now_ms();
  if (content.marker.empty() && has_self_write_fingerprint_ &&
      now <= self_write_deadline_ms_ &&
      fingerprint(validation.normalized) == self_write_fingerprint_) {
    has_self_write_fingerprint_ = false;
    emit(EngineEventKind::LocalEchoSuppressed, {StatusCode::Ok, ""},
         "own clipboard write detected via content fingerprint",
         ClipboardUpdate{});
    return;
  }

  if (current_.has_value() && current_->logical > logical_) {
    logical_ = current_->logical;
  }
  ClipboardUpdate update;
  update.generation = options_.generation;
  update.origin = options_.self_id;
  update.logical = ++logical_;
  update.text = validation.normalized;
  current_ = update;

  const bool delivered = send_ ? send_(update) : false;
  Status status;
  std::string detail;
  if (delivered) {
    status = {StatusCode::Ok, ""};
    detail = "sent";
  } else {
    status = {StatusCode::IoError, "peer transport is not available"};
    detail = "not sent";
  }
  emit(EngineEventKind::LocalSent, status, detail, update);
}

ApplyResult SyncEngine::apply_remote(const ClipboardUpdate& update) {
  ApplyResult result;

  if (update.generation != options_.generation) {
    result.wrong_generation = true;
    result.status = {StatusCode::WrongGeneration,
                     "update belongs to another session generation"};
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }
  if (!valid_endpoint_id(update.origin)) {
    result.status = {StatusCode::InvalidText, "invalid origin endpoint"};
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }
  if (update.logical == 0) {
    result.status = {StatusCode::InvalidText,
                     "logical clock must be greater than zero"};
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }

  const Validation validation =
      validate_text(update.text, options_.max_text_bytes);
  if (!validation.valid) {
    result.status = validation.status;
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }

  logical_ = std::max(logical_, update.logical);
  const std::string key =
      hex_u64(update.generation) + ":" + update.origin + ":" +
      std::to_string(update.logical);
  if (seen(key)) {
    result.duplicate = true;
    result.status = {StatusCode::Duplicate, "operation already processed"};
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }

  if (current_.has_value() && !update_is_newer(update, *current_)) {
    remember(key);
    result.stale = true;
    result.status = {StatusCode::Stale,
                     "update is older than the current clipboard state"};
    emit(EngineEventKind::RemoteRejected, result.status, result.status.detail,
         update);
    return result;
  }

  ClipboardUpdate applied = update;
  applied.text = validation.normalized;

  // El marcador se registra antes de escribir para que una notificación
  // concurrente del sistema ya se reconozca como eco propio.
  const std::string marker = make_marker(applied);
  self_marker_ = marker;
  self_write_fingerprint_ = fingerprint(applied.text);
  has_self_write_fingerprint_ = true;
  self_write_deadline_ms_ =
      (options_.now_ms ? options_.now_ms() : steady_now_ms()) +
      options_.echo_window_ms;

  Status write_status = {StatusCode::Unavailable,
                         "clipboard backend unavailable"};
  if (port_ != nullptr) {
    write_status = port_->write(applied.text, marker);
  }
  if (!write_status.ok()) {
    self_marker_.clear();
    has_self_write_fingerprint_ = false;
    remember(key);
    result.status = write_status;
    emit(EngineEventKind::RemoteWriteFailed, write_status,
         write_status.detail, applied);
    return result;
  }

  current_ = applied;
  remember(key);
  result.applied = true;
  result.status = {StatusCode::Ok, ""};
  emit(EngineEventKind::RemoteApplied, result.status, "", applied);
  return result;
}

}  // namespace oit
