#pragma once

// Responsabilidad: decidir qué cambios locales se envían, aplicar cambios
// remotos, suprimir el eco de nuestras escrituras y rechazar operaciones
// duplicadas, atrasadas o de otra generación. No conoce el escritorio ni la red.

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>

#include "model.hpp"
#include "platform/clipboard.hpp"

namespace oit {

enum class EngineEventKind {
  LocalSent,
  LocalEchoSuppressed,
  LocalRejected,
  RemoteApplied,
  RemoteRejected,
  RemoteWriteFailed,
};

struct EngineEvent {
  EngineEventKind kind = EngineEventKind::LocalRejected;
  Status status;
  std::string detail;
  ClipboardUpdate update;
};

struct ApplyResult {
  Status status;
  bool applied = false;
  bool duplicate = false;
  bool stale = false;
  bool wrong_generation = false;
};

struct EngineOptions {
  std::string self_id;
  std::uint64_t generation = 0;
  std::size_t max_text_bytes = kMaxTextBytes;
  int echo_window_ms = kEchoWindowMs;
  // Inyectable para las auto-pruebas; por defecto usa un reloj monótono.
  std::function<std::int64_t()> now_ms;
};

class SyncEngine {
 public:
  // Devuelve false si el transporte no pudo entregar la operación.
  using SendFn = std::function<bool(const ClipboardUpdate&)>;
  using EventFn = std::function<void(const EngineEvent&)>;

  SyncEngine(ClipboardPort* port, EngineOptions options, SendFn send,
             EventFn event);

  // Llamar sólo con una notificación real del portapapeles local.
  void on_local_clipboard(const ClipboardContent& content);

  ApplyResult apply_remote(const ClipboardUpdate& update);

  bool has_content() const noexcept { return current_.has_value(); }
  std::optional<ClipboardUpdate> current() const { return current_; }
  std::uint64_t logical_clock() const noexcept { return logical_; }
  bool is_self_marker(std::string_view marker) const;

 private:
  void emit(EngineEventKind kind, Status status, std::string detail,
            const ClipboardUpdate& update);
  bool seen(const std::string& key) const;
  void remember(const std::string& key);
  std::string make_marker(const ClipboardUpdate& update) const;

  ClipboardPort* port_ = nullptr;
  EngineOptions options_;
  SendFn send_;
  EventFn event_;

  std::uint64_t logical_ = 0;
  std::optional<ClipboardUpdate> current_;
  std::deque<std::string> seen_keys_;
  std::string self_marker_;
  std::uint64_t self_write_fingerprint_ = 0;
  bool has_self_write_fingerprint_ = false;
  std::int64_t self_write_deadline_ms_ = 0;
};

}  // namespace oit
