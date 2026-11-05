// Responsabilidad: adaptador Linux/Wayland real mediante xdg-desktop-portal.
//
// Flujo oficial (xdg-desktop-portal 1.x):
//   RemoteDesktop.CreateSession
//   Clipboard.RequestClipboard
//   RemoteDesktop.SelectDevices(types=0)   // no se solicita entrada
//   RemoteDesktop.Start
//   Clipboard.SetSelection / SelectionRead / SelectionWrite / SelectionWriteDone
//
// El cliente no llama nunca a Notify* ni ConnectToEIS, no captura entrada y no
// usa X11 como sustituto. GNOME 46+ exige habilitar "Allow Remote Interaction"
// o seleccionar una pantalla en su diálogo para habilitar Share; con
// SelectDevices(types=0) ese interruptor no concede dispositivos de entrada. El
// adaptador rechaza cualquier mask distinto de cero y no simula entrada.
//
// Dependencia: libdbus-1 (API pública). La compilación sin las cabeceras de
// desarrollo compila el adaptador de limitación, no este archivo.

#include "platform/portal_clipboard.hpp"

#include <dbus/dbus.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include "model.hpp"

namespace oit {
namespace {

constexpr const char* kPortalService = "org.freedesktop.portal.Desktop";
constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
constexpr const char* kRemoteDesktopInterface =
    "org.freedesktop.portal.RemoteDesktop";
constexpr const char* kClipboardInterface = "org.freedesktop.portal.Clipboard";
constexpr const char* kRequestInterface = "org.freedesktop.portal.Request";
constexpr const char* kSessionInterface = "org.freedesktop.portal.Session";

constexpr int kRequestTimeoutMs = 30000;
constexpr int kStartTimeoutMs = 180000;
constexpr std::size_t kMaxPendingChanges = 16;

struct MessageDeleter {
  void operator()(DBusMessage* message) const {
    dbus_message_unref(message);
  }
};

using MsgPtr = std::unique_ptr<DBusMessage, MessageDeleter>;

MsgPtr take_message(DBusMessage* message) { return MsgPtr(message); }

struct DbusError {
  DBusError value;

  DbusError() { dbus_error_init(&value); }
  ~DbusError() { dbus_error_free(&value); }
  DbusError(const DbusError&) = delete;
  DbusError& operator=(const DbusError&) = delete;
};

std::string error_text(const DBusError& error) {
  std::string text;
  if (error.name != nullptr) {
    text += error.name;
  } else {
    text += "unknown D-Bus error";
  }
  if (error.message != nullptr && error.message[0] != '\0') {
    text += ": ";
    text += error.message;
  }
  return text;
}

StatusCode error_code(const char* name) {
  if (name == nullptr) {
    return StatusCode::IoError;
  }
  if (std::strstr(name, "ServiceUnknown") != nullptr ||
      std::strstr(name, "NameHasNoOwner") != nullptr ||
      std::strstr(name, "NoServer") != nullptr) {
    return StatusCode::Unavailable;
  }
  if (std::strstr(name, "AccessDenied") != nullptr ||
      std::strstr(name, "Cancelled") != nullptr) {
    return StatusCode::Denied;
  }
  if (std::strstr(name, "Timeout") != nullptr ||
      std::strstr(name, "NoReply") != nullptr) {
    return StatusCode::Busy;
  }
  if (std::strstr(name, "InvalidArgs") != nullptr ||
      std::strstr(name, "NotSupported") != nullptr ||
      std::strstr(name, "NotFound") != nullptr) {
    return StatusCode::NotSupported;
  }
  return StatusCode::IoError;
}

Status status_from_error(const DBusError& error, const char* context) {
  Status status;
  status.code = error_code(error.name);
  status.detail = std::string(context) + ": " + error_text(error);
  return status;
}

Status status_from_error_reply(DBusMessage* reply, const char* context) {
  const char* name = dbus_message_get_error_name(reply);
  const char* message = nullptr;
  DbusError error;
  if (!dbus_message_get_args(reply, &error.value, DBUS_TYPE_STRING, &message,
                             DBUS_TYPE_INVALID)) {
    message = nullptr;
  }
  Status status;
  status.code = error_code(name);
  status.detail = std::string(context) + ": ";
  status.detail += name == nullptr ? "D-Bus error" : name;
  if (message != nullptr && message[0] != '\0') {
    status.detail += ": ";
    status.detail += message;
  }
  return status;
}

std::string make_token(const char* prefix) {
  static std::mutex mutex;
  static std::mt19937_64 generator(std::random_device{}());
  std::uint64_t value = 0;
  {
    std::lock_guard<std::mutex> lock(mutex);
    value = generator();
  }
  std::ostringstream out;
  out << prefix << std::hex << value;
  return out.str();
}

std::vector<std::string> text_mime_candidates() {
  return {"text/plain;charset=utf-8", "text/plain;charset=UTF-8",
          "text/plain", "UTF8_STRING"};
}

bool is_text_mime(const std::string& mime) {
  const std::vector<std::string> candidates = text_mime_candidates();
  return std::find(candidates.begin(), candidates.end(), mime) !=
         candidates.end();
}

std::string choose_text_mime(const std::vector<std::string>& advertised) {
  const std::vector<std::string> candidates = text_mime_candidates();
  for (const std::string& candidate : candidates) {
    if (std::find(advertised.begin(), advertised.end(), candidate) !=
        advertised.end()) {
      return candidate;
    }
  }
  return {};
}

bool append_basic(DBusMessageIter* iter, int type, const void* value) {
  return dbus_message_iter_append_basic(iter, type, value) != FALSE;
}

bool append_string(DBusMessageIter* iter, const std::string& value) {
  const char* text = value.c_str();
  return append_basic(iter, DBUS_TYPE_STRING, &text);
}

bool append_object_path(DBusMessageIter* iter, const std::string& value) {
  const char* text = value.c_str();
  return append_basic(iter, DBUS_TYPE_OBJECT_PATH, &text);
}

class VariantDict {
 public:
  explicit VariantDict(DBusMessageIter* parent) : parent_(parent) {
    open_ = dbus_message_iter_open_container(parent_, DBUS_TYPE_ARRAY, "{sv}",
                                             &dict_) != FALSE;
  }

  ~VariantDict() {
    if (open_) {
      dbus_message_iter_close_container(parent_, &dict_);
    }
  }

  VariantDict(const VariantDict&) = delete;
  VariantDict& operator=(const VariantDict&) = delete;

  bool valid() const { return open_ && !failed_; }

  void add_string(const char* key, const std::string& value) {
    DBusMessageIter entry{};
    DBusMessageIter variant{};
    if (!begin(key, &entry, &variant, "s")) {
      return;
    }
    const bool ok = append_string(&variant, value);
    end(&entry, &variant);
    if (!ok) {
      failed_ = true;
    }
  }

  void add_bool(const char* key, bool value) {
    DBusMessageIter entry{};
    DBusMessageIter variant{};
    if (!begin(key, &entry, &variant, "b")) {
      return;
    }
    const dbus_bool_t raw = value ? TRUE : FALSE;
    const bool ok = append_basic(&variant, DBUS_TYPE_BOOLEAN, &raw);
    end(&entry, &variant);
    if (!ok) {
      failed_ = true;
    }
  }

  void add_uint32(const char* key, std::uint32_t value) {
    DBusMessageIter entry{};
    DBusMessageIter variant{};
    if (!begin(key, &entry, &variant, "u")) {
      return;
    }
    const bool ok = append_basic(&variant, DBUS_TYPE_UINT32, &value);
    end(&entry, &variant);
    if (!ok) {
      failed_ = true;
    }
  }

  void add_string_array(const char* key,
                        const std::vector<std::string>& values) {
    DBusMessageIter entry{};
    DBusMessageIter variant{};
    if (!begin(key, &entry, &variant, "as")) {
      return;
    }
    DBusMessageIter array{};
    bool ok = dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s",
                                               &array) != FALSE;
    for (const std::string& value : values) {
      if (!ok) {
        break;
      }
      ok = append_string(&array, value);
    }
    if (ok && dbus_message_iter_close_container(&variant, &array) == FALSE) {
      ok = false;
    }
    end(&entry, &variant);
    if (!ok) {
      failed_ = true;
    }
  }

 private:
  bool begin(const char* key, DBusMessageIter* entry,
             DBusMessageIter* variant, const char* signature) {
    if (failed_) {
      return false;
    }
    if (dbus_message_iter_open_container(&dict_, DBUS_TYPE_DICT_ENTRY, nullptr,
                                         entry) == FALSE) {
      failed_ = true;
      return false;
    }
    const char* name = key;
    if (!append_basic(entry, DBUS_TYPE_STRING, &name) ||
        dbus_message_iter_open_container(entry, DBUS_TYPE_VARIANT, signature,
                                         variant) == FALSE) {
      failed_ = true;
      dbus_message_iter_close_container(&dict_, entry);
      return false;
    }
    return true;
  }

  void end(DBusMessageIter* entry, DBusMessageIter* variant) {
    dbus_message_iter_close_container(entry, variant);
    dbus_message_iter_close_container(&dict_, entry);
  }

  DBusMessageIter* parent_ = nullptr;
  DBusMessageIter dict_{};
  bool open_ = false;
  bool failed_ = false;
};

struct ParsedOptions {
  bool has_session = false;
  std::string session_handle;
  bool has_owner = false;
  bool session_is_owner = false;
  bool has_clipboard_enabled = false;
  bool clipboard_enabled = false;
  bool has_devices = false;
  std::uint32_t devices = 0;
  std::vector<std::string> mime_types;
};

void parse_options_array(DBusMessageIter* array, ParsedOptions& out) {
  if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY) {
    return;
  }
  DBusMessageIter entries{};
  dbus_message_iter_recurse(array, &entries);
  while (dbus_message_iter_get_arg_type(&entries) == DBUS_TYPE_DICT_ENTRY) {
    DBusMessageIter entry{};
    dbus_message_iter_recurse(&entries, &entry);
    if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_STRING) {
      break;
    }
    const char* key = nullptr;
    dbus_message_iter_get_basic(&entry, &key);
    const std::string name = key == nullptr ? std::string() : std::string(key);
    if (!dbus_message_iter_next(&entry) ||
        dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) {
      break;
    }
    DBusMessageIter variant{};
    dbus_message_iter_recurse(&entry, &variant);
    const int type = dbus_message_iter_get_arg_type(&variant);
    if (type == DBUS_TYPE_STRING) {
      const char* value = nullptr;
      dbus_message_iter_get_basic(&variant, &value);
      if (name == "session_handle" && value != nullptr) {
        out.has_session = true;
        out.session_handle = value;
      }
    } else if (type == DBUS_TYPE_BOOLEAN) {
      dbus_bool_t value = FALSE;
      dbus_message_iter_get_basic(&variant, &value);
      if (name == "session_is_owner") {
        out.has_owner = true;
        out.session_is_owner = value != FALSE;
      } else if (name == "clipboard_enabled") {
        out.has_clipboard_enabled = true;
        out.clipboard_enabled = value != FALSE;
      }
    } else if (type == DBUS_TYPE_UINT32) {
      dbus_uint32_t value = 0;
      dbus_message_iter_get_basic(&variant, &value);
      if (name == "devices") {
        out.has_devices = true;
        out.devices = value;
      }
    } else if (type == DBUS_TYPE_OBJECT_PATH && name == "session_handle") {
      const char* value = nullptr;
      dbus_message_iter_get_basic(&variant, &value);
      if (value != nullptr) {
        out.has_session = true;
        out.session_handle = value;
      }
    } else if (name == "mime_types" &&
               (type == DBUS_TYPE_ARRAY || type == DBUS_TYPE_STRUCT)) {
      DBusMessageIter values = variant;
      if (type == DBUS_TYPE_STRUCT) {
        // xdg-desktop-portal-gnome 46 emite el valor como tupla (as), aunque
        // la especificación del portal describe mime_types como as. Se aceptan
        // ambas formas para no perder las señales reales.
        dbus_message_iter_recurse(&variant, &values);
      }
      if (dbus_message_iter_get_arg_type(&values) == DBUS_TYPE_ARRAY) {
        DBusMessageIter strings{};
        dbus_message_iter_recurse(&values, &strings);
        while (dbus_message_iter_get_arg_type(&strings) == DBUS_TYPE_STRING) {
          const char* value = nullptr;
          dbus_message_iter_get_basic(&strings, &value);
          if (value != nullptr) {
            out.mime_types.emplace_back(value);
          }
          if (!dbus_message_iter_next(&strings)) {
            break;
          }
        }
      }
    }
    dbus_message_iter_next(&entries);
  }
}

bool parse_response(DBusMessage* message, std::uint32_t& response,
                    ParsedOptions& options) {
  DBusMessageIter iter{};
  if (dbus_message_iter_init(message, &iter) == FALSE) {
    return false;
  }
  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_UINT32) {
    return false;
  }
  dbus_message_iter_get_basic(&iter, &response);
  if (!dbus_message_iter_next(&iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
    return false;
  }
  parse_options_array(&iter, options);
  return true;
}

bool parse_owner_options(DBusMessage* message, std::string& session_path,
                         bool& owner,
                         std::vector<std::string>& mime_types) {
  DBusMessageIter iter{};
  if (dbus_message_iter_init(message, &iter) == FALSE ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
    return false;
  }
  const char* path = nullptr;
  dbus_message_iter_get_basic(&iter, &path);
  if (path == nullptr) {
    return false;
  }
  session_path = path;
  if (!dbus_message_iter_next(&iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
    return false;
  }
  ParsedOptions options;
  parse_options_array(&iter, options);
  owner = options.has_owner && options.session_is_owner;
  mime_types = std::move(options.mime_types);
  return true;
}

bool parse_transfer(DBusMessage* message, std::string& session_path,
                    std::string& mime_type, std::uint32_t& serial) {
  DBusMessageIter iter{};
  if (dbus_message_iter_init(message, &iter) == FALSE ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH) {
    return false;
  }
  const char* path = nullptr;
  dbus_message_iter_get_basic(&iter, &path);
  if (path == nullptr) {
    return false;
  }
  session_path = path;
  if (!dbus_message_iter_next(&iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
    return false;
  }
  const char* mime = nullptr;
  dbus_message_iter_get_basic(&iter, &mime);
  if (mime != nullptr) {
    mime_type = mime;
  }
  if (!dbus_message_iter_next(&iter) ||
      dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_UINT32) {
    return false;
  }
  dbus_message_iter_get_basic(&iter, &serial);
  return true;
}

MsgPtr new_call(const char* path, const char* interface, const char* method) {
  return take_message(dbus_message_new_method_call(
      kPortalService, path, interface, method));
}

Status invoke(DBusConnection* connection, DBusMessage* call, int timeout_ms,
              MsgPtr& reply) {
  if (connection == nullptr) {
    return {StatusCode::Unavailable, "session D-Bus connection is closed"};
  }
  DbusError error;
  DBusMessage* raw = dbus_connection_send_with_reply_and_block(
      connection, call, timeout_ms, &error.value);
  if (raw == nullptr) {
    return status_from_error(error.value, "portal method call failed");
  }
  reply = take_message(raw);
  if (dbus_message_get_type(raw) == DBUS_MESSAGE_TYPE_ERROR) {
    return status_from_error_reply(raw, "portal method call failed");
  }
  return {StatusCode::Ok, ""};
}

bool reply_object_path(DBusMessage* reply, std::string& path) {
  const char* value = nullptr;
  DbusError error;
  if (!dbus_message_get_args(reply, &error.value, DBUS_TYPE_OBJECT_PATH,
                             &value, DBUS_TYPE_INVALID) ||
      value == nullptr) {
    return false;
  }
  path = value;
  return true;
}

bool write_all(int fd, const std::string& text) {
  std::size_t offset = 0;
  while (offset < text.size()) {
    const ssize_t written =
        ::write(fd, text.data() + offset, text.size() - offset);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (written == 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

class PortalClipboardPort final : public ClipboardPort {
 public:
  PortalClipboardPort() = default;
  ~PortalClipboardPort() override;

  ClipboardCapabilities capabilities() const override {
    return portal_clipboard_capabilities();
  }

  Status initialize();
  Status read(ClipboardContent& out) override;
  Status write(std::string_view text, std::string_view marker) override;
  WaitResult wait_for_change(ClipboardContent& out, int timeout_ms,
                             std::string& detail) override;
  void request_stop() override;

 private:
  Status create_session();
  Status request_clipboard_access();
  Status select_no_input_devices();
  Status start_session();
  Status wait_response(const std::string& request_path, int timeout_ms,
                       std::uint32_t& response, ParsedOptions& results);
  Status read_selection_locked(const std::string& mime_type,
                               std::string& text);
  Status set_selection_locked(const std::string& text);
  Status selection_write_locked(std::uint32_t serial, const std::string& text);
  void selection_write_done(std::uint32_t serial, bool success);
  void handle_message(DBusMessage* message);
  void handle_selection_owner_changed(DBusMessage* message);
  void handle_selection_transfer(DBusMessage* message);
  void queue_change(std::string text);
  void close_request(const std::string& request_path);
  void close_session();
  void shutdown();

  DBusConnection* connection_ = nullptr;
  std::string session_handle_;
  std::string failure_;
  std::mutex dbus_mutex_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::deque<ClipboardContent> pending_;
  std::vector<std::string> mime_types_;
  std::string cached_text_;
  bool session_owner_ = false;
  bool started_ = false;
  bool closed_ = false;
  std::atomic<bool> stop_{false};
};

PortalClipboardPort::~PortalClipboardPort() { shutdown(); }

Status PortalClipboardPort::initialize() {
  static std::once_flag dbus_threads_once;
  static bool dbus_threads_ready = false;
  std::call_once(dbus_threads_once, [] {
    dbus_threads_ready = dbus_threads_init_default() != FALSE;
  });
  if (!dbus_threads_ready) {
    return {StatusCode::IoError,
            "libdbus could not initialize thread support; refusing an "
            "unsafe shared connection"};
  }

  DbusError error;
  connection_ = dbus_bus_get(DBUS_BUS_SESSION, &error.value);
  if (connection_ == nullptr) {
    return status_from_error(error.value,
                             "cannot connect to the D-Bus session bus");
  }
  dbus_connection_set_exit_on_disconnect(connection_, FALSE);

  const char* rules[] = {
      "type='signal',interface='org.freedesktop.portal.Request',member='Response'",
      "type='signal',interface='org.freedesktop.portal.Clipboard',member='SelectionOwnerChanged'",
      "type='signal',interface='org.freedesktop.portal.Clipboard',member='SelectionTransfer'",
  };
  for (const char* rule : rules) {
    DbusError match_error;
    dbus_bus_add_match(connection_, rule, &match_error.value);
    if (dbus_error_is_set(&match_error.value)) {
      return status_from_error(match_error.value,
                               "cannot subscribe to portal signals");
    }
  }

  Status status = create_session();
  if (!status.ok()) {
    return status;
  }
  status = request_clipboard_access();
  if (!status.ok()) {
    return status;
  }
  status = select_no_input_devices();
  if (!status.ok()) {
    return status;
  }
  return start_session();
}

Status PortalClipboardPort::create_session() {
  MsgPtr call =
      new_call(kPortalPath, kRemoteDesktopInterface, "CreateSession");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate CreateSession message"};
  }

  const std::string handle_token = make_token("oneintwo_r");
  const std::string session_token = make_token("oneintwo_s");
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  {
    VariantDict options(&root);
    options.add_string("handle_token", handle_token);
    options.add_string("session_handle_token", session_token);
    if (!options.valid()) {
      return {StatusCode::IoError, "cannot build CreateSession options"};
    }
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }
  std::string request_path;
  if (!reply_object_path(reply.get(), request_path)) {
    return {StatusCode::IoError,
            "CreateSession reply has no Request object path"};
  }

  std::uint32_t response = 0;
  ParsedOptions results;
  status = wait_response(request_path, kRequestTimeoutMs, response, results);
  if (!status.ok()) {
    close_request(request_path);
    return status;
  }
  if (response != 0) {
    close_request(request_path);
    return {StatusCode::Denied,
            "RemoteDesktop.CreateSession was cancelled or failed (response " +
                std::to_string(response) + ")"};
  }
  if (!results.has_session || results.session_handle.empty()) {
    return {StatusCode::IoError,
            "CreateSession response did not include session_handle"};
  }
  session_handle_ = results.session_handle;
  return {StatusCode::Ok, ""};
}

Status PortalClipboardPort::request_clipboard_access() {
  MsgPtr call =
      new_call(kPortalPath, kClipboardInterface, "RequestClipboard");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate RequestClipboard message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_)) {
    return {StatusCode::IoError, "cannot append session_handle"};
  }
  {
    VariantDict options(&root);
    if (!options.valid()) {
      return {StatusCode::IoError, "cannot build RequestClipboard options"};
    }
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    if (status.code == StatusCode::NotSupported) {
      status.detail +=
          " (the desktop portal does not expose Clipboard v1 for this session)";
    }
    return status;
  }
  return {StatusCode::Ok, ""};
}

Status PortalClipboardPort::select_no_input_devices() {
  MsgPtr call =
      new_call(kPortalPath, kRemoteDesktopInterface, "SelectDevices");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate SelectDevices message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_)) {
    return {StatusCode::IoError, "cannot append session_handle"};
  }
  {
    VariantDict options(&root);
    options.add_string("handle_token", make_token("oneintwo_devices"));
    options.add_uint32("types", 0);
    if (!options.valid()) {
      return {StatusCode::IoError, "cannot build SelectDevices options"};
    }
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }
  std::string request_path;
  if (!reply_object_path(reply.get(), request_path)) {
    return {StatusCode::IoError,
            "SelectDevices reply has no Request object path"};
  }
  std::uint32_t response = 0;
  ParsedOptions results;
  status = wait_response(request_path, kRequestTimeoutMs, response, results);
  if (!status.ok()) {
    close_request(request_path);
    return status;
  }
  if (response != 0) {
    close_request(request_path);
    return {StatusCode::Denied,
            "RemoteDesktop.SelectDevices with types=0 was rejected (response " +
                std::to_string(response) + ")"};
  }
  return {StatusCode::Ok, ""};
}

Status PortalClipboardPort::start_session() {
  MsgPtr call = new_call(kPortalPath, kRemoteDesktopInterface, "Start");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate Start message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_)) {
    return {StatusCode::IoError, "cannot append session_handle"};
  }
  const std::string parent_window;
  if (!append_string(&root, parent_window)) {
    return {StatusCode::IoError, "cannot append parent_window"};
  }
  {
    VariantDict options(&root);
    options.add_string("handle_token", make_token("oneintwo_start"));
    if (!options.valid()) {
      return {StatusCode::IoError, "cannot build Start options"};
    }
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }
  std::string request_path;
  if (!reply_object_path(reply.get(), request_path)) {
    return {StatusCode::IoError, "Start reply has no Request object path"};
  }

  std::uint32_t response = 0;
  ParsedOptions results;
  status = wait_response(request_path, kStartTimeoutMs, response, results);
  if (!status.ok()) {
    close_request(request_path);
    return status;
  }
  if (response != 0) {
    close_request(request_path);
    return {StatusCode::Denied,
            "the portal start request was cancelled or failed (response " +
                std::to_string(response) + ")"};
  }
  if (!results.has_clipboard_enabled || !results.clipboard_enabled) {
    close_request(request_path);
    return {StatusCode::Denied,
            "the portal started the session without clipboard access. "
            "On GNOME 46+ the Remote Desktop dialog keeps Share disabled "
            "unless 'Allow Remote Interaction' is enabled or a screen source "
            "is selected. With SelectDevices(types=0), enabling that switch "
            "does not grant input devices; Start should report devices=0, and "
            "this adapter refuses any nonzero mask."};
  }
  if (results.has_devices && results.devices != 0) {
    close_request(request_path);
    close_session();
    return {StatusCode::Denied,
            "the portal granted input devices (mask " +
                std::to_string(results.devices) +
                "); refusing to continue because text sharing must not "
                "depend on input control"};
  }
  started_ = true;
  return {StatusCode::Ok, ""};
}

Status PortalClipboardPort::wait_response(const std::string& request_path,
                                          int timeout_ms,
                                          std::uint32_t& response,
                                          ParsedOptions& results) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (stop_.load()) {
      return {StatusCode::Unavailable, "portal operation stopped"};
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                               deadline - std::chrono::steady_clock::now())
                               .count();
    if (remaining <= 0) {
      break;
    }
    const int slice = static_cast<int>(std::min<std::int64_t>(remaining, 200));
    if (dbus_connection_read_write(connection_, slice) == FALSE) {
      return {StatusCode::IoError, "D-Bus session connection closed"};
    }
    DBusMessage* raw = nullptr;
    while ((raw = dbus_connection_pop_message(connection_)) != nullptr) {
      MsgPtr message = take_message(raw);
      if (dbus_message_is_signal(raw, kRequestInterface, "Response") &&
          dbus_message_get_path(raw) != nullptr &&
          request_path == dbus_message_get_path(raw)) {
        if (!parse_response(raw, response, results)) {
          return {StatusCode::IoError,
                  "invalid org.freedesktop.portal.Request.Response signal"};
        }
        return {StatusCode::Ok, ""};
      }
      handle_message(raw);
    }
  }
  return {StatusCode::Busy,
          "portal request timed out after " + std::to_string(timeout_ms) +
              " ms; the permission dialog may still be open"};
}

Status PortalClipboardPort::read_selection_locked(const std::string& mime_type,
                                                  std::string& text) {
  text.clear();
  MsgPtr call =
      new_call(kPortalPath, kClipboardInterface, "SelectionRead");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate SelectionRead message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_) ||
      !append_string(&root, mime_type)) {
    return {StatusCode::IoError, "cannot build SelectionRead arguments"};
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }

  int fd = -1;
  DbusError error;
  if (!dbus_message_get_args(reply.get(), &error.value, DBUS_TYPE_UNIX_FD, &fd,
                             DBUS_TYPE_INVALID) ||
      fd < 0) {
    return status_from_error(error.value,
                             "SelectionRead returned no Unix file descriptor");
  }

  std::string result;
  char buffer[8192];
  bool too_large = false;
  for (;;) {
    const ssize_t count = ::read(fd, buffer, sizeof(buffer));
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      ::close(fd);
      return {StatusCode::IoError,
              "cannot read portal selection pipe: " +
                  std::string(std::strerror(errno))};
    }
    if (count == 0) {
      break;
    }
    result.append(buffer, static_cast<std::size_t>(count));
    if (result.size() > kMaxTextBytes) {
      too_large = true;
      break;
    }
  }
  ::close(fd);
  text = std::move(result);
  if (too_large) {
    return {StatusCode::TooLarge,
            "portal selection exceeds the 256 KiB text limit"};
  }
  if (text.empty()) {
    return {StatusCode::InvalidText, "portal selection is empty"};
  }
  return {StatusCode::Ok, ""};
}

Status PortalClipboardPort::set_selection_locked(const std::string& text) {
  MsgPtr call =
      new_call(kPortalPath, kClipboardInterface, "SetSelection");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate SetSelection message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_)) {
    return {StatusCode::IoError, "cannot append session_handle"};
  }
  {
    VariantDict options(&root);
    options.add_string_array("mime_types", text_mime_candidates());
    if (!options.valid()) {
      return {StatusCode::IoError, "cannot build SetSelection options"};
    }
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }
  session_owner_ = true;
  cached_text_ = text;
  mime_types_ = text_mime_candidates();
  return {StatusCode::Ok,
          "selection owned via xdg-desktop-portal Clipboard; the portal "
          "protocol does not carry the private marker format"};
}

Status PortalClipboardPort::selection_write_locked(std::uint32_t serial,
                                                   const std::string& text) {
  MsgPtr call =
      new_call(kPortalPath, kClipboardInterface, "SelectionWrite");
  if (!call) {
    return {StatusCode::IoError, "cannot allocate SelectionWrite message"};
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  if (!append_object_path(&root, session_handle_) ||
      !append_basic(&root, DBUS_TYPE_UINT32, &serial)) {
    return {StatusCode::IoError, "cannot build SelectionWrite arguments"};
  }

  MsgPtr reply;
  Status status = invoke(connection_, call.get(), kRequestTimeoutMs, reply);
  if (!status.ok()) {
    return status;
  }

  int fd = -1;
  DbusError error;
  if (!dbus_message_get_args(reply.get(), &error.value, DBUS_TYPE_UNIX_FD, &fd,
                             DBUS_TYPE_INVALID) ||
      fd < 0) {
    return status_from_error(
        error.value, "SelectionWrite returned no Unix file descriptor");
  }
  const bool written = write_all(fd, text);
  ::close(fd);
  if (!written) {
    return {StatusCode::IoError,
            "cannot write portal selection pipe: " +
                std::string(std::strerror(errno))};
  }
  return {StatusCode::Ok, ""};
}

void PortalClipboardPort::selection_write_done(std::uint32_t serial,
                                               bool success) {
  MsgPtr call =
      new_call(kPortalPath, kClipboardInterface, "SelectionWriteDone");
  if (!call) {
    return;
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(call.get(), &root);
  const dbus_bool_t raw = success ? TRUE : FALSE;
  if (!append_object_path(&root, session_handle_) ||
      !append_basic(&root, DBUS_TYPE_UINT32, &serial) ||
      !append_basic(&root, DBUS_TYPE_BOOLEAN, &raw)) {
    return;
  }
  MsgPtr reply;
  (void)invoke(connection_, call.get(), kRequestTimeoutMs, reply);
}

void PortalClipboardPort::queue_change(std::string text) {
  if (text.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (pending_.size() >= kMaxPendingChanges) {
    pending_.pop_front();
  }
  pending_.push_back(ClipboardContent{std::move(text), ""});
  queue_cv_.notify_one();
}

void PortalClipboardPort::handle_selection_owner_changed(
    DBusMessage* message) {
  if (dbus_message_get_path(message) == nullptr ||
      std::string(kPortalPath) != dbus_message_get_path(message)) {
    return;
  }
  std::string session_path;
  bool owner = false;
  std::vector<std::string> mime_types;
  if (!parse_owner_options(message, session_path, owner, mime_types) ||
      session_path != session_handle_) {
    return;
  }
  session_owner_ = owner;
  mime_types_ = std::move(mime_types);
  if (owner) {
    return;
  }
  cached_text_.clear();
  const std::string mime = choose_text_mime(mime_types_);
  if (mime.empty()) {
    return;
  }
  std::string text;
  const Status status = read_selection_locked(mime, text);
  if (!status.ok() && status.code != StatusCode::TooLarge) {
    return;
  }
  queue_change(std::move(text));
}

void PortalClipboardPort::handle_selection_transfer(DBusMessage* message) {
  if (dbus_message_get_path(message) == nullptr ||
      std::string(kPortalPath) != dbus_message_get_path(message)) {
    return;
  }
  std::string session_path;
  std::string mime_type;
  std::uint32_t serial = 0;
  if (!parse_transfer(message, session_path, mime_type, serial) ||
      session_path != session_handle_) {
    return;
  }
  if (!is_text_mime(mime_type) || !session_owner_ || cached_text_.empty()) {
    selection_write_done(serial, false);
    return;
  }
  const Status status = selection_write_locked(serial, cached_text_);
  selection_write_done(serial, status.ok());
}

void PortalClipboardPort::handle_message(DBusMessage* message) {
  if (dbus_message_is_signal(message, kClipboardInterface,
                             "SelectionOwnerChanged")) {
    handle_selection_owner_changed(message);
  } else if (dbus_message_is_signal(message, kClipboardInterface,
                                    "SelectionTransfer")) {
    handle_selection_transfer(message);
  }
}

void PortalClipboardPort::close_request(const std::string& request_path) {
  if (connection_ == nullptr || request_path.empty()) {
    return;
  }
  MsgPtr call =
      new_call(request_path.c_str(), kRequestInterface, "Close");
  if (!call) {
    return;
  }
  MsgPtr reply;
  (void)invoke(connection_, call.get(), 1000, reply);
}

void PortalClipboardPort::close_session() {
  if (connection_ == nullptr || session_handle_.empty()) {
    return;
  }
  MsgPtr call = new_call(session_handle_.c_str(), kSessionInterface, "Close");
  if (call) {
    MsgPtr reply;
    (void)invoke(connection_, call.get(), 1000, reply);
  }
  session_handle_.clear();
}

void PortalClipboardPort::shutdown() {
  stop_.store(true);
  queue_cv_.notify_all();
  std::lock_guard<std::mutex> lock(dbus_mutex_);
  if (closed_) {
    return;
  }
  closed_ = true;
  close_session();
  if (connection_ != nullptr) {
    dbus_connection_flush(connection_);
    dbus_connection_unref(connection_);
    connection_ = nullptr;
  }
}

Status PortalClipboardPort::read(ClipboardContent& out) {
  std::lock_guard<std::mutex> lock(dbus_mutex_);
  if (!started_) {
    return {StatusCode::Unavailable, "portal clipboard session is not active"};
  }
  if (session_owner_ && !cached_text_.empty()) {
    out.text = cached_text_;
    out.marker.clear();
    return {StatusCode::Ok, ""};
  }
  if (session_owner_) {
    return {StatusCode::Unavailable,
            "this session owns an empty portal selection"};
  }

  std::vector<std::string> candidates;
  for (const std::string& mime : mime_types_) {
    if (is_text_mime(mime)) {
      candidates.push_back(mime);
    }
  }
  for (const std::string& mime : text_mime_candidates()) {
    if (std::find(candidates.begin(), candidates.end(), mime) ==
        candidates.end()) {
      candidates.push_back(mime);
    }
  }

  Status last = {StatusCode::Unavailable,
                 "portal selection advertises no text MIME type"};
  for (const std::string& mime : candidates) {
    std::string text;
    last = read_selection_locked(mime, text);
    if (last.ok()) {
      out.text = std::move(text);
      out.marker.clear();
      return {StatusCode::Ok, ""};
    }
    if (last.code == StatusCode::TooLarge) {
      out.text = std::move(text);
      return last;
    }
  }
  return last;
}

Status PortalClipboardPort::write(std::string_view text,
                                  std::string_view marker) {
  (void)marker;
  const Validation validation = validate_text(text, kMaxTextBytes);
  if (!validation.valid) {
    return validation.status;
  }
  std::lock_guard<std::mutex> lock(dbus_mutex_);
  if (!started_) {
    return {StatusCode::Unavailable, "portal clipboard session is not active"};
  }
  return set_selection_locked(validation.normalized);
}

WaitResult PortalClipboardPort::wait_for_change(ClipboardContent& out,
                                                int timeout_ms,
                                                std::string& detail) {
  if (stop_.load()) {
    return WaitResult::Stopped;
  }
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(std::max(timeout_ms, 0));
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      if (!pending_.empty()) {
        out = std::move(pending_.front());
        pending_.pop_front();
        return WaitResult::Change;
      }
    }
    if (stop_.load()) {
      return WaitResult::Stopped;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      return WaitResult::Timeout;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count();

    int slice = static_cast<int>(std::min<std::int64_t>(remaining, 100));
    if (slice <= 0) {
      slice = 1;
    }
    {
      std::lock_guard<std::mutex> lock(dbus_mutex_);
      if (connection_ == nullptr) {
        detail = "portal D-Bus connection is closed";
        return WaitResult::Error;
      }
      if (dbus_connection_read_write(connection_, slice) == FALSE) {
        failure_ = "D-Bus session connection closed while watching";
        detail = failure_;
        return WaitResult::Error;
      }
      DBusMessage* raw = nullptr;
      while ((raw = dbus_connection_pop_message(connection_)) != nullptr) {
        MsgPtr message = take_message(raw);
        handle_message(raw);
      }
    }
  }
}

void PortalClipboardPort::request_stop() {
  stop_.store(true);
  queue_cv_.notify_all();
}

}  // namespace

ClipboardCapabilities portal_clipboard_capabilities() {
  ClipboardCapabilities capabilities;
  capabilities.read = true;
  capabilities.write = true;
  capabilities.watch = true;
  capabilities.independent_of_input = true;
  capabilities.backend = "xdg-desktop-portal-clipboard";
  capabilities.session =
      "Wayland session using xdg-desktop-portal RemoteDesktop + Clipboard";
  capabilities.limitation =
      "Requires user approval of an xdg-desktop-portal RemoteDesktop session "
      "with Clipboard v1. This client requests devices=0, never calls any "
      "Notify* method, never calls ConnectToEIS and never captures input. "
      "GNOME 46+ only enables Share when 'Allow Remote Interaction' is on or "
      "a screen source is selected; with SelectDevices(types=0), enabling "
      "that switch does not grant input devices, so it is a user-visible UI "
      "gate rather than authorization to capture input. Other desktops may "
      "present different dialogs. X11 is not used as a fallback.";
  capabilities.notes = {
      "The Clipboard portal does not create its own session; it extends "
      "RemoteDesktop or InputCapture. This adapter uses RemoteDesktop with "
      "zero input devices.",
      "RemoteDesktop.Start returns clipboard_enabled and devices; a nonzero "
      "device mask is rejected by this adapter.",
      "Only UTF-8 text MIME types are advertised and read.",
      "The private Windows marker format does not exist in the portal MIME "
      "model; echo suppression uses session_is_owner plus the engine's "
      "content fingerprint window.",
      "The desktop portal may show its own permission dialog and may require "
      "a desktop file/app ID to identify the caller; no trust is persisted "
      "by this prototype.",
  };
  return capabilities;
}

bool run_portal_isolated_checks(std::string& detail) {
  auto fail = [&detail](const char* message) {
    detail = message;
    return false;
  };

  if (make_token("oneintwo_").size() <= 9) {
    return fail("portal token generator produced an invalid token");
  }
  if (choose_text_mime({"text/plain", "text/plain;charset=utf-8"}) !=
      "text/plain;charset=utf-8") {
    return fail("text MIME priority is wrong");
  }
  if (!choose_text_mime({"image/png"}).empty()) {
    return fail("non-text MIME type was selected as text");
  }

  MsgPtr message = take_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  if (!message) {
    return fail("cannot allocate synthetic D-Bus message");
  }
  DBusMessageIter root{};
  dbus_message_iter_init_append(message.get(), &root);
  {
    VariantDict options(&root);
    options.add_string("session_handle", "/org/freedesktop/portal/desktop/session/1/1");
    options.add_bool("session_is_owner", true);
    options.add_bool("clipboard_enabled", true);
    options.add_uint32("devices", 0);
    options.add_string_array("mime_types",
                             {"text/plain;charset=utf-8", "text/plain"});
    if (!options.valid()) {
      return fail("cannot encode synthetic variant dictionary");
    }
  }
  const char* signature = dbus_message_get_signature(message.get());
  if (signature == nullptr || std::string(signature) != "a{sv}") {
    return fail("encoded options do not have signature a{sv}");
  }

  DBusMessageIter read{};
  if (dbus_message_iter_init(message.get(), &read) == FALSE ||
      dbus_message_iter_get_arg_type(&read) != DBUS_TYPE_ARRAY) {
    return fail("cannot read synthetic options array");
  }
  ParsedOptions parsed;
  parse_options_array(&read, parsed);
  if (!parsed.has_session ||
      parsed.session_handle !=
          "/org/freedesktop/portal/desktop/session/1/1") {
    return fail("session_handle was not round-tripped");
  }
  if (!parsed.has_owner || !parsed.session_is_owner) {
    return fail("session_is_owner was not round-tripped");
  }
  if (!parsed.has_clipboard_enabled || !parsed.clipboard_enabled) {
    return fail("clipboard_enabled was not round-tripped");
  }
  if (!parsed.has_devices || parsed.devices != 0) {
    return fail("devices=0 was not round-tripped");
  }
  if (parsed.mime_types.size() != 2 ||
      parsed.mime_types[0] != "text/plain;charset=utf-8") {
    return fail("mime_types array was not round-tripped");
  }

  MsgPtr response = take_message(dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL));
  if (!response) {
    return fail("cannot allocate synthetic response signal");
  }
  DBusMessageIter response_root{};
  dbus_message_iter_init_append(response.get(), &response_root);
  const std::uint32_t response_code = 0;
  if (!append_basic(&response_root, DBUS_TYPE_UINT32, &response_code)) {
    return fail("cannot append Response code");
  }
  {
    VariantDict options(&response_root);
    options.add_string("session_handle",
                       "/org/freedesktop/portal/desktop/session/2/2");
    options.add_bool("clipboard_enabled", true);
    options.add_uint32("devices", 0);
    if (!options.valid()) {
      return fail("cannot encode synthetic Response options");
    }
  }
  ParsedOptions response_options;
  std::uint32_t decoded_code = 1;
  if (!parse_response(response.get(), decoded_code, response_options) ||
      decoded_code != 0 || !response_options.has_session) {
    return fail("Response signal could not be parsed");
  }

  MsgPtr owner_signal = take_message(dbus_message_new_signal(
      kPortalPath, kClipboardInterface, "SelectionOwnerChanged"));
  if (!owner_signal) {
    return fail("cannot allocate synthetic SelectionOwnerChanged signal");
  }
  DBusMessageIter owner_root{};
  dbus_message_iter_init_append(owner_signal.get(), &owner_root);
  if (!append_object_path(
          &owner_root,
          "/org/freedesktop/portal/desktop/session/3/3")) {
    return fail("cannot append owner session path");
  }
  {
    VariantDict options(&owner_root);
    options.add_bool("session_is_owner", false);
    options.add_string_array("mime_types", {"text/plain"});
    if (!options.valid()) {
      return fail("cannot encode synthetic owner options");
    }
  }
  std::string owner_session;
  bool owner = true;
  std::vector<std::string> owner_mimes;
  if (!parse_owner_options(owner_signal.get(), owner_session, owner,
                           owner_mimes) ||
      owner_session != "/org/freedesktop/portal/desktop/session/3/3" ||
      owner || owner_mimes.size() != 1 || owner_mimes[0] != "text/plain") {
    return fail("SelectionOwnerChanged arguments could not be parsed");
  }

  MsgPtr transfer_signal = take_message(dbus_message_new_signal(
      kPortalPath, kClipboardInterface, "SelectionTransfer"));
  if (!transfer_signal) {
    return fail("cannot allocate synthetic SelectionTransfer signal");
  }
  DBusMessageIter transfer_root{};
  dbus_message_iter_init_append(transfer_signal.get(), &transfer_root);
  const std::uint32_t transfer_serial = 42;
  if (!append_object_path(
          &transfer_root,
          "/org/freedesktop/portal/desktop/session/4/4") ||
      !append_string(&transfer_root, "text/plain") ||
      !append_basic(&transfer_root, DBUS_TYPE_UINT32, &transfer_serial)) {
    return fail("cannot encode synthetic SelectionTransfer signal");
  }
  std::string transfer_session;
  std::string transfer_mime;
  std::uint32_t parsed_serial = 0;
  if (!parse_transfer(transfer_signal.get(), transfer_session, transfer_mime,
                      parsed_serial) ||
      transfer_session != "/org/freedesktop/portal/desktop/session/4/4" ||
      transfer_mime != "text/plain" || parsed_serial != transfer_serial) {
    return fail("SelectionTransfer arguments could not be parsed");
  }

  detail =
      "portal message encoding, MIME priority, Response, owner and transfer "
      "parsing pass";
  return true;
}

PortCreation create_portal_clipboard_port() {
  std::unique_ptr<PortalClipboardPort> port(new PortalClipboardPort());
  const Status status = port->initialize();
  if (!status.ok()) {
    return {nullptr, status};
  }
  return {std::move(port), {StatusCode::Ok, ""}};
}

}  // namespace oit
