// Responsabilidad: sesión de prueba entre dos instancias por dirección
// explícita. Usa TLS con certificado fijado, intercambia mensajes del
// protocolo y aplica la política del motor de sincronización. No descubre
// equipos, no persiste confianza y no construye una interfaz de producto.

#include "net/peer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "engine.hpp"
#include "model.hpp"
#include "net/tls_channel.hpp"
#include "protocol.hpp"

namespace oit {
namespace {

using Clock = std::chrono::steady_clock;

struct SessionResult {
  bool clean = true;
  std::string error;
};

std::string elapsed_text(Clock::time_point start) {
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            start)
          .count();
  std::ostringstream out;
  out << "+" << milliseconds << "ms";
  return out.str();
}

void log_info(Clock::time_point start, const std::string& text) {
  std::cout << "[" << elapsed_text(start) << "] " << text << "\n"
            << std::flush;
}

void log_problem(Clock::time_point start, const std::string& text) {
  std::cerr << "[" << elapsed_text(start) << "] " << text << "\n"
            << std::flush;
}

std::string safe(std::string_view value, std::size_t max_length = 240) {
  std::string out;
  out.reserve(std::min(value.size(), max_length));
  for (const char c : value) {
    if (out.size() >= max_length) {
      out += "...";
      break;
    }
    const unsigned char byte = static_cast<unsigned char>(c);
    out.push_back((byte < 0x20 || byte == 0x7F) ? ' ' : c);
  }
  return out;
}

std::string yes_no(bool value) { return value ? "yes" : "no"; }

std::string caps_text(const ClipboardCapabilities& capabilities) {
  std::ostringstream out;
  out << "read=" << yes_no(capabilities.read)
      << " write=" << yes_no(capabilities.write)
      << " watch=" << yes_no(capabilities.watch)
      << " independent_of_input="
      << yes_no(capabilities.independent_of_input)
      << " backend=" << capabilities.backend;
  return out.str();
}

void print_content(const std::string& text) {
  std::cout << "--- content begin (" << text.size() << " bytes) ---\n";
  std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (text.empty() || text.back() != '\n') {
    std::cout << '\n';
  }
  std::cout << "--- content end ---\n" << std::flush;
}

void log_engine_event(Clock::time_point start, const EngineEvent& event,
                      bool show_content) {
  std::ostringstream out;
  switch (event.kind) {
    case EngineEventKind::LocalSent:
      out << "local copy captured bytes=" << event.update.text.size()
          << " logical=" << event.update.logical
          << (event.status.ok() ? " sent" : " NOT sent");
      if (!event.status.ok()) {
        out << " (" << safe(event.status.detail) << ")";
      }
      break;
    case EngineEventKind::LocalEchoSuppressed:
      out << "echo suppressed: " << safe(event.detail);
      break;
    case EngineEventKind::LocalRejected:
      out << "local text ignored: " << status_name(event.status.code)
          << " (" << safe(event.status.detail) << ")";
      break;
    case EngineEventKind::RemoteApplied:
      out << "remote update applied origin=" << event.update.origin
          << " logical=" << event.update.logical
          << " bytes=" << event.update.text.size();
      break;
    case EngineEventKind::RemoteRejected:
      out << "remote update rejected: " << status_name(event.status.code)
          << " (" << safe(event.status.detail) << ")";
      break;
    case EngineEventKind::RemoteWriteFailed:
      out << "remote update not applied: " << status_name(event.status.code)
          << " (" << safe(event.status.detail) << ")";
      break;
  }
  log_info(start, out.str());
  if (show_content && (event.kind == EngineEventKind::RemoteApplied ||
                       event.kind == EngineEventKind::LocalSent)) {
    print_content(event.update.text);
  }
}

bool send_frame(TlsChannel& channel, const Bytes& frame, std::string& error) {
  return channel.write_all(frame.data(), frame.size(), error);
}

bool receive_frame(TlsChannel& channel, DecodedMessage& message,
                   std::string& error) {
  std::uint8_t header[4];
  if (!channel.read_all(header, sizeof(header), error)) {
    return false;
  }
  const std::size_t length =
      (static_cast<std::size_t>(header[0]) << 24) |
      (static_cast<std::size_t>(header[1]) << 16) |
      (static_cast<std::size_t>(header[2]) << 8) |
      static_cast<std::size_t>(header[3]);
  if (length == 0 || length > kMaxFrameBytes) {
    error = "invalid frame length from peer";
    return false;
  }
  Bytes frame(4 + length);
  std::memcpy(frame.data(), header, sizeof(header));
  if (!channel.read_all(frame.data() + 4, length, error)) {
    return false;
  }
  Status status;
  if (!decode_message(frame, message, status)) {
    error = "invalid message from peer: " + status.detail;
    return false;
  }
  return true;
}

ClipboardCapabilities caps_from_hello(const HelloMessage& hello) {
  ClipboardCapabilities capabilities;
  capabilities.read = hello.can_read;
  capabilities.write = hello.can_write;
  capabilities.watch = hello.can_watch;
  capabilities.backend = "remote";
  capabilities.session = "remote endpoint";
  return capabilities;
}

bool server_handshake(TlsChannel& channel, const std::string& self_id,
                      const ClipboardCapabilities& local_caps,
                      std::uint64_t& generation, std::string& peer_id,
                      ClipboardCapabilities& peer_caps, std::string& error) {
  DecodedMessage first;
  if (!receive_frame(channel, first, error)) {
    return false;
  }
  if (first.type != MessageType::Hello) {
    error = "first message is not hello";
    return false;
  }
  const Status validation = validate_hello(first.hello);
  if (!validation.ok()) {
    ErrorMessage message{validation.code, validation.detail};
    std::string ignored;
    (void)send_frame(channel, encode_error(message), ignored);
    error = "rejected hello: " + validation.detail;
    return false;
  }

  generation = first.hello.generation;
  peer_id = first.hello.endpoint;
  peer_caps = caps_from_hello(first.hello);

  HelloMessage reply;
  reply.version = kProtocolVersion;
  reply.generation = generation;
  reply.endpoint = self_id;
  reply.can_read = local_caps.read;
  reply.can_write = local_caps.write;
  reply.can_watch = local_caps.watch;
  return send_frame(channel, encode_hello(reply), error);
}

bool client_handshake(TlsChannel& channel, const std::string& self_id,
                      const ClipboardCapabilities& local_caps,
                      std::uint64_t& generation, std::string& peer_id,
                      ClipboardCapabilities& peer_caps, std::string& error) {
  generation = make_generation();
  HelloMessage hello;
  hello.version = kProtocolVersion;
  hello.generation = generation;
  hello.endpoint = self_id;
  hello.can_read = local_caps.read;
  hello.can_write = local_caps.write;
  hello.can_watch = local_caps.watch;
  if (!send_frame(channel, encode_hello(hello), error)) {
    return false;
  }

  DecodedMessage response;
  if (!receive_frame(channel, response, error)) {
    return false;
  }
  if (response.type != MessageType::Hello) {
    error = "peer did not answer with hello";
    return false;
  }
  const Status validation = validate_hello(response.hello);
  if (!validation.ok()) {
    error = "invalid hello: " + validation.detail;
    return false;
  }
  if (response.hello.generation != generation) {
    error = "peer answered with a different session generation";
    return false;
  }
  peer_id = response.hello.endpoint;
  peer_caps = caps_from_hello(response.hello);
  return true;
}

struct IncomingQueue {
  std::mutex mutex;
  std::deque<DecodedMessage> items;
  bool closed = false;
  std::string error;

  void push(DecodedMessage message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (items.size() >= 32) {
      items.pop_front();
    }
    items.push_back(std::move(message));
  }

  bool pop(DecodedMessage& message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (items.empty()) {
      return false;
    }
    message = std::move(items.front());
    items.pop_front();
    return true;
  }

  void close(std::string detail) {
    std::lock_guard<std::mutex> lock(mutex);
    closed = true;
    error = std::move(detail);
  }

  bool is_closed(std::string& detail) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!closed) {
      return false;
    }
    detail = error;
    return true;
  }
};

void reader_loop(TlsChannel& channel, IncomingQueue& incoming,
                 std::atomic<bool>& stop) {
  while (!stop.load()) {
    DecodedMessage message;
    std::string error;
    if (!receive_frame(channel, message, error)) {
      if (!stop.load()) {
        incoming.close(error);
      } else {
        incoming.close({});
      }
      return;
    }
    incoming.push(std::move(message));
  }
  incoming.close({});
}

SessionResult run_session(TlsChannel& channel, const std::string& self_id,
                          std::uint64_t generation,
                          const std::string& peer_id,
                          const ClipboardCapabilities& peer_caps,
                          const ClipboardCapabilities& local_caps,
                          ClipboardPort* clipboard, bool show_content) {
  const auto start = Clock::now();
  log_info(start, "session ready: generation=" + hex_u64(generation) +
                      " peer=" + peer_id);
  log_info(start, "peer capabilities: " + caps_text(peer_caps));
  log_info(start, "local capabilities: " + caps_text(local_caps));
  if (!local_caps.limitation.empty()) {
    log_info(start, "local limitation: " +
                        safe(local_caps.limitation, 500));
  }
  if (!peer_caps.write) {
    log_info(start, "warning: peer says it cannot write text");
  }
  if (!peer_caps.read) {
    log_info(start, "warning: peer says it cannot read text");
  }

  std::atomic<bool> reader_stop{false};
  IncomingQueue incoming;
  std::thread reader(reader_loop, std::ref(channel), std::ref(incoming),
                     std::ref(reader_stop));

  std::mutex send_mutex;
  std::string send_error;
  const auto send_bytes = [&](const Bytes& frame) {
    std::lock_guard<std::mutex> lock(send_mutex);
    if (!send_error.empty()) {
      return false;
    }
    std::string error;
    if (!send_frame(channel, frame, error)) {
      send_error = error;
      return false;
    }
    return true;
  };

  std::unique_ptr<SyncEngine> engine;
  if (clipboard != nullptr) {
    EngineOptions options;
    options.self_id = self_id;
    options.generation = generation;
    engine = std::make_unique<SyncEngine>(
        clipboard, options,
        [&](const ClipboardUpdate& update) {
          ClipboardMessage message;
          message.generation = update.generation;
          message.logical = update.logical;
          message.origin = update.origin;
          message.text = update.text;
          return send_bytes(encode_clipboard(message));
        },
        [&](const EngineEvent& event) {
          log_engine_event(start, event, show_content);
        });
  }

  SessionResult result;
  bool stop_session = false;
  while (!stop_session) {
    DecodedMessage message;
    while (incoming.pop(message)) {
      if (message.type == MessageType::Clipboard) {
        const ClipboardMessage& clipboard_message = message.clipboard;
        log_info(start, "received update origin=" + clipboard_message.origin +
                            " gen=" + hex_u64(clipboard_message.generation) +
                            " logical=" +
                            std::to_string(clipboard_message.logical) +
                            " bytes=" +
                            std::to_string(clipboard_message.text.size()));
        ClipboardUpdate update;
        update.generation = clipboard_message.generation;
        update.logical = clipboard_message.logical;
        update.origin = clipboard_message.origin;
        update.text = clipboard_message.text;

        Status applied_status;
        if (engine) {
          applied_status = engine->apply_remote(update).status;
        } else {
          applied_status = {
              StatusCode::Unavailable,
              local_caps.limitation.empty()
                  ? "local clipboard unavailable"
                  : local_caps.limitation};
        }

        AppliedMessage reply;
        reply.generation = generation;
        reply.logical = clipboard_message.logical;
        reply.origin = clipboard_message.origin;
        reply.code = applied_status.code;
        reply.detail = safe(applied_status.detail);
        if (!send_bytes(encode_applied(reply))) {
          stop_session = true;
          result.clean = false;
          result.error = "send failed: " + send_error;
          break;
        }
      } else if (message.type == MessageType::Applied) {
        const Status validation = validate_applied(message.applied);
        if (!validation.ok()) {
          stop_session = true;
          result.clean = false;
          result.error = validation.detail;
          break;
        }
        std::ostringstream line;
        line << "peer applied origin=" << message.applied.origin
             << " logical=" << message.applied.logical
             << " status=" << status_name(message.applied.code);
        if (!message.applied.detail.empty()) {
          line << " detail=" << safe(message.applied.detail);
        }
        log_info(start, line.str());
      } else if (message.type == MessageType::Bye) {
        log_info(start, "peer closed: " + safe(message.bye.reason));
        stop_session = true;
      } else if (message.type == MessageType::Error) {
        stop_session = true;
        result.clean = false;
        result.error = "peer error: " + safe(message.error.detail);
      } else {
        stop_session = true;
        result.clean = false;
        result.error = "unexpected hello after handshake";
      }
    }
    if (stop_session) {
      break;
    }

    std::string closed_detail;
    if (incoming.is_closed(closed_detail)) {
      if (closed_detail.empty()) {
        log_info(start, "peer closed the connection");
      } else {
        result.clean = false;
        result.error = "connection ended: " + closed_detail;
        log_info(start, result.error);
      }
      break;
    }
    if (!send_error.empty()) {
      result.clean = false;
      result.error = "send failed: " + send_error;
      break;
    }

    if (clipboard != nullptr) {
      ClipboardContent content;
      std::string detail;
      const WaitResult waited = clipboard->wait_for_change(content, 100, detail);
      if (waited == WaitResult::Change) {
        engine->on_local_clipboard(content);
      } else if (waited == WaitResult::Error) {
        result.clean = false;
        result.error = "clipboard listener error: " + detail;
        break;
      } else if (waited == WaitResult::Stopped) {
        break;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  if (result.clean) {
    const ByeMessage bye{"session finished"};
    (void)send_bytes(encode_bye(bye));
  }
  reader_stop.store(true);
  channel.interrupt();
  if (reader.joinable()) {
    reader.join();
  }
  channel.shutdown_peer();
  log_info(start, result.clean ? "session ended cleanly"
                               : "session ended: " + safe(result.error));
  return result;
}

}  // namespace

int run_peer(const PeerOptions& options, ClipboardPort* clipboard,
             const ClipboardCapabilities& capabilities) {
  const auto start = Clock::now();
  if (options.port == 0) {
    std::cerr << "peer mode requires --port\n";
    return 3;
  }

  const TlsFiles& files = options.tls;
  LocalTlsInfo tls_info;
  std::string error;
  if (!describe_local_certificate(files, tls_info, error)) {
    log_problem(start, "TLS setup failed: " + error);
    return 3;
  }

  const std::string self_id = make_endpoint_id();
  std::cout << "oneintwo-clipboard peer mode\n";
  std::cout << "tls=" << tls_info.backend << "\n";
  std::cout << "local_certificate_sha256=" << tls_info.cert_fingerprint
            << "\n";
  std::cout << "local_capabilities=" << caps_text(capabilities) << "\n";
  if (!capabilities.limitation.empty()) {
    std::cout << "local_limitation="
              << safe(capabilities.limitation, 500) << "\n";
  }
  std::cout << "max_text_bytes=" << kMaxTextBytes << "\n";
  std::cout << "endpoint=" << self_id << "\n" << std::flush;

  TlsChannel channel;
  if (options.server) {
    if (!channel.listen_on(options.bind_address, options.port, files,
                           error)) {
      log_problem(start, "listen failed: " + error);
      return 3;
    }
    std::cout << "listening=" << options.bind_address << ":"
              << channel.local_port() << "\n"
              << std::flush;

    for (;;) {
      if (!channel.accept_next(error)) {
        log_problem(start, "accept/TLS failed: " + error);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }
      std::cout << "peer_certificate_sha256=" << channel.peer_fingerprint()
                << "\n"
                << std::flush;

      std::uint64_t generation = 0;
      std::string peer_id;
      ClipboardCapabilities peer_caps;
      if (!server_handshake(channel, self_id, capabilities, generation,
                            peer_id, peer_caps, error)) {
        log_problem(start, "handshake failed: " + error);
        channel.shutdown_peer();
        continue;
      }

      (void)run_session(channel, self_id, generation, peer_id, peer_caps,
                        capabilities, clipboard, options.show_content);
      std::cout << "waiting for another connection...\n" << std::flush;
    }
  }

  if (!channel.connect_to(options.host, options.port, files, error)) {
    log_problem(start, "connect failed: " + error);
    return 3;
  }
  std::cout << "connected_to=" << options.host << ":" << options.port
            << "\n";
  std::cout << "peer_certificate_sha256=" << channel.peer_fingerprint()
            << "\n"
            << std::flush;

  std::uint64_t generation = 0;
  std::string peer_id;
  ClipboardCapabilities peer_caps;
  if (!client_handshake(channel, self_id, capabilities, generation, peer_id,
                        peer_caps, error)) {
    log_problem(start, "handshake failed: " + error);
    channel.shutdown_peer();
    return 3;
  }

  const SessionResult result =
      run_session(channel, self_id, generation, peer_id, peer_caps,
                  capabilities, clipboard, options.show_content);
  return result.clean ? 0 : 3;
}

}  // namespace oit
