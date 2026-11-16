// Responsibility: adapter Linux/Wayland. Origin authorizes InputCapture and
// receives entry by a receiving libei context; destination authorizes
// RemoteDesktop, inject with libei emitter and reserve Ctrl+Shift+F12 with
// GlobalShortcuts. It is only compiled with gio-2.0 and libei-1.0.
#include "core/keymap.hpp"
#include "platform/backend.hpp"
#include "platform/ring.hpp"
#include "platform/wayland_flow.hpp"

#include <gio/gio.h>
#include <libei.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace oit {
namespace {

constexpr const char* kPortalService = "org.freedesktop.portal.Desktop";
constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
constexpr int kAuthTimeoutMs = 120000;
constexpr int kCallTimeoutMs = 8000;

bool isWaylandSession() {
  const char* type = std::getenv("XDG_SESSION_TYPE");
  if (type != nullptr && std::strcmp(type, "wayland") == 0) return true;
  const char* display = std::getenv("WAYLAND_DISPLAY");
  return display != nullptr && *display != '\0';
}

int16_t clampDelta(double value) {
  const long rounded = std::lround(value);
  if (rounded < -32768) return -32768;
  if (rounded > 32767) return 32767;
  return static_cast<int16_t>(rounded);
}

uint32_t evdevButton(uint16_t button) {
  switch (button) {
    case 0: return 272;  // BTN_LEFT
    case 1: return 273;  // BTN_RIGHT
    case 2: return 274;  // BTN_MIDDLE
    case 3: return 275;  // BTN_SIDE
    case 4: return 276;  // BTN_EXTRA
    default: return 0;
  }
}

uint16_t protocolButton(uint32_t evdev) {
  switch (evdev) {
    case 272: return 0;
    case 273: return 1;
    case 274: return 2;
    case 275: return 3;
    case 276: return 4;
    default: return 0xFFFF;
  }
}

// Signals of the portals that the backend translates to its internal state.
// It receives its own dictionary reference and returns (a{sv}), as callRaw.
GVariant* portalResponseTuple(GVariant* dictionary) {
  GVariant* tuple = g_variant_ref_sink(g_variant_new_tuple(&dictionary, 1));
  g_variant_unref(dictionary);
  return tuple;
}

// RemoteDesktop retains the s type for compatibility, although it contains a path.
bool remoteSessionHandle(GVariant* dictionary, std::string& session) {
  const gchar* handle = nullptr;
  if (!g_variant_lookup(dictionary, "session_handle", "&s", &handle) ||
      !g_variant_is_object_path(handle)) return false;
  session = handle;
  return true;
}

class PortalEvents {
 public:
  virtual ~PortalEvents() = default;
  virtual void portalInputActivated(uint32_t activationId, double x, double y,
                                    uint32_t barrierId) = 0;
  virtual void portalInputDeactivated(uint32_t activationId) = 0;
  virtual void portalInputDisabled() = 0;
  virtual void portalInputZonesChanged() = 0;
  virtual void portalInputClosed() = 0;
  virtual void portalRemoteClosed() = 0;
  virtual void portalShortcutActivated() = 0;
};

class PortalClient : public wl::PortalOps {
 public:
  ~PortalClient() override;
  void setEvents(PortalEvents* events) { events_ = events; }
  bool connect(std::string& error);
  bool queryUint(const char* iface, const char* name, uint32_t& value);
  void pump();
  static bool checkResponseDecoding();
  static bool checkShortcutSignals();

  bool createInputSession(std::string& session, std::string& error) override;
  bool startInputSession(const std::string& session, std::string& error) override;
  bool connectInputEis(const std::string& session, int& fd, std::string& error) override;
  bool inputZones(const std::string& session, std::vector<wl::Zone>& zones, uint32_t& zoneSet,
                  std::string& error) override;
  bool setPointerBarriers(const std::string& session, const std::vector<wl::Barrier>& barriers,
                          uint32_t zoneSet, std::vector<uint32_t>& failed,
                          std::string& error) override;
  bool enableInput(const std::string& session, std::string& error) override;
  bool disableInput(const std::string& session, std::string& error) override;
  bool releaseInput(const std::string& session, uint32_t activationId, double x, double y,
                    std::string& error) override;
  void closeInputSession(const std::string& session) override;
  void closeEis(int fd) override;

  bool createRemoteSession(std::string& session, std::string& error) override;
  bool selectRemoteDevices(const std::string& session, uint32_t types,
                           std::string& error) override;
  bool startRemoteSession(const std::string& session, uint32_t& granted,
                          std::string& error) override;
  bool connectRemoteEis(const std::string& session, int& fd, std::string& error) override;
  void closeRemoteSession(const std::string& session) override;

  bool createShortcutSession(std::string& session, std::string& error) override;
  bool bindEmergencyShortcut(const std::string& session, bool& bound,
                             std::string& error) override;
  bool verifyEmergencyShortcut(const std::string& session, std::string& error) override;
  void closeShortcutSession(const std::string& session) override;

 private:
  struct RequestWait {
    bool done = false;
    uint32_t response = 2;
    GVariant* results = nullptr;
  };

  static void onResponse(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
                         GVariant*, gpointer);
  static void onInputActivated(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                               const gchar*, GVariant*, gpointer);
  static void onInputDeactivated(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                 const gchar*, GVariant*, gpointer);
  static void onInputDisabled(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                              const gchar*, GVariant*, gpointer);
  static void onInputZonesChanged(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                  const gchar*, GVariant*, gpointer);
  static void onInputClosed(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                            const gchar*, GVariant*, gpointer);
  static void onRemoteClosed(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                             const gchar*, GVariant*, gpointer);
  static void onShortcutActivated(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                  const gchar*, GVariant*, gpointer);
  static void onShortcutDeactivated(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                    const gchar*, GVariant*, gpointer);

  std::string nextToken();
  std::string requestPath(const std::string& token) const;
  GVariant* callRaw(const char* iface, const char* method, GVariant* parameters,
                    const char* replyType, std::string& error, int timeoutMs);
  bool callVoid(const char* iface, const char* method, GVariant* parameters,
                std::string& error);
  bool request(const char* iface, const char* method, GVariant* parameters,
               const std::string& token, GVariant** results, std::string& error,
               int timeoutMs);
  bool connectEis(const char* iface, const std::string& session, int& fd, std::string& error);
  void closeAt(const std::string& path);
  guint subscribe(const std::string& path, const char* iface, const char* member,
                  GDBusSignalCallback callback);
  void unsubscribe(std::vector<guint>& ids);
  void subscribeInput(const std::string& session);
  void subscribeRemote(const std::string& session);
  void subscribeShortcuts(const std::string& session);

  GDBusConnection* bus_ = nullptr;
  PortalEvents* events_ = nullptr;
  uint32_t token_ = 0;
  std::vector<guint> inputSubs_;
  std::vector<guint> remoteSubs_;
  std::vector<guint> shortcutSubs_;
  std::string shortcutSession_;
  bool checkingShortcut_ = false;
  bool shortcutActivated_ = false;
  bool shortcutReleased_ = false;
  bool shortcutClosed_ = false;
};

PortalClient::~PortalClient() {
  unsubscribe(inputSubs_);
  unsubscribe(remoteSubs_);
  unsubscribe(shortcutSubs_);
  if (bus_ != nullptr) {
    g_object_unref(bus_);
    bus_ = nullptr;
  }
}

bool PortalClient::connect(std::string& error) {
  if (bus_ != nullptr) return true;
  GError* gerror = nullptr;
  bus_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &gerror);
  if (bus_ == nullptr) {
    error = std::string("Session bus: ") + (gerror ? gerror->message : "unavailable");
    if (gerror) g_error_free(gerror);
    return false;
  }
  return true;
}

bool PortalClient::queryUint(const char* iface, const char* name, uint32_t& value) {
  if (bus_ == nullptr) return false;
  std::string ignored;
  GVariant* reply = callRaw("org.freedesktop.DBus.Properties", "Get",
                            g_variant_new("(ss)", iface, name), "(v)", ignored, 2500);
  if (reply == nullptr) return false;
  GVariant* boxed = nullptr;
  g_variant_get(reply, "(v)", &boxed);
  g_variant_unref(reply);
  if (boxed == nullptr) return false;
  const bool ok = g_variant_is_of_type(boxed, G_VARIANT_TYPE_UINT32);
  if (ok) value = g_variant_get_uint32(boxed);
  g_variant_unref(boxed);
  return ok;
}

void PortalClient::pump() {
  int guard = 0;
  while (g_main_context_pending(nullptr) && guard++ < 64) {
    g_main_context_iteration(nullptr, FALSE);
  }
}

std::string PortalClient::nextToken() {
  char buffer[64] = {};
  std::snprintf(buffer, sizeof(buffer), "oneintwo%u_%08x", ++token_, g_random_int());
  return std::string(buffer);
}

std::string PortalClient::requestPath(const std::string& token) const {
  std::string sender = g_dbus_connection_get_unique_name(bus_);
  if (!sender.empty() && sender[0] == ':') sender.erase(0, 1);
  for (char& character : sender) {
    if (character == '.') character = '_';
  }
  return std::string("/org/freedesktop/portal/desktop/request/") + sender + "/" + token;
}

GVariant* PortalClient::callRaw(const char* iface, const char* method, GVariant* parameters,
                                const char* replyType, std::string& error, int timeoutMs) {
  if (bus_ == nullptr) {
    error = "no connection to the session bus";
    if (parameters != nullptr) g_variant_unref(g_variant_ref_sink(parameters));
    return nullptr;
  }
  GError* gerror = nullptr;
  GVariantType* type = replyType != nullptr ? g_variant_type_new(replyType) : nullptr;
  GVariant* reply = g_dbus_connection_call_sync(
      bus_, kPortalService, kPortalPath, iface, method, parameters, type,
      G_DBUS_CALL_FLAGS_NONE, timeoutMs, nullptr, &gerror);
  if (type != nullptr) g_variant_type_free(type);
  if (reply == nullptr) {
    error = std::string(method) + ": " + (gerror ? gerror->message : "no response");
    if (gerror) g_error_free(gerror);
  }
  return reply;
}

bool PortalClient::callVoid(const char* iface, const char* method, GVariant* parameters,
                            std::string& error) {
  GVariant* reply = callRaw(iface, method, parameters, nullptr, error, 5000);
  if (reply == nullptr) return false;
  g_variant_unref(reply);
  return true;
}

void PortalClient::onResponse(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                              const gchar*, GVariant* parameters, gpointer data) {
  auto* wait = static_cast<RequestWait*>(data);
  guint32 response = 2;
  GVariant* results = nullptr;
  g_variant_get(parameters, "(u@a{sv})", &response, &results);
  if (wait->results != nullptr) g_variant_unref(wait->results);
  wait->results = results;
  wait->response = response;
  wait->done = true;
}

bool PortalClient::request(const char* iface, const char* method, GVariant* parameters,
                           const std::string& token, GVariant** results, std::string& error,
                           int timeoutMs) {
  if (bus_ == nullptr) {
    error = "no connection to the session bus";
    if (parameters != nullptr) g_variant_unref(g_variant_ref_sink(parameters));
    return false;
  }
  const std::string expected = requestPath(token);
  RequestWait wait;
  const auto subscribeResponse = [&](const std::string& path) {
    // onResponse receives RequestWait, not PortalClient as session signals.
    return g_dbus_connection_signal_subscribe(
        bus_, kPortalService, "org.freedesktop.portal.Request", "Response",
        path.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NONE, onResponse, &wait, nullptr);
  };
  guint subscription = subscribeResponse(expected);
  GError* gerror = nullptr;
  GVariant* reply = g_dbus_connection_call_sync(
      bus_, kPortalService, kPortalPath, iface, method, parameters, G_VARIANT_TYPE("(o)"),
      G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &gerror);
  if (reply == nullptr) {
    error = std::string(method) + ": " + (gerror ? gerror->message : "no response");
    if (gerror) g_error_free(gerror);
    if (subscription != 0) g_dbus_connection_signal_unsubscribe(bus_, subscription);
    return false;
  }
  const gchar* handle = nullptr;
  g_variant_get(reply, "(&o)", &handle);
  const std::string returned = handle != nullptr ? handle : "";
  g_variant_unref(reply);
  if (returned.empty()) {
    error = std::string(method) + ": the portal did not return a Request";
    if (subscription != 0) g_dbus_connection_signal_unsubscribe(bus_, subscription);
    return false;
  }
  if (returned != expected) {
    if (subscription != 0) g_dbus_connection_signal_unsubscribe(bus_, subscription);
    subscription = subscribeResponse(returned);
  }

  const gint64 deadline = g_get_monotonic_time() + static_cast<gint64>(timeoutMs) * 1000;
  while (!wait.done && g_get_monotonic_time() < deadline) {
    g_main_context_iteration(nullptr, FALSE);
    if (!wait.done) g_usleep(2000);
  }
  if (subscription != 0) g_dbus_connection_signal_unsubscribe(bus_, subscription);
  if (!wait.done) {
    if (wait.results != nullptr) g_variant_unref(wait.results);
    error = std::string(method) + ": the portal did not respond in time";
    return false;
  }
  if (wait.response != 0) {
    if (wait.results != nullptr) g_variant_unref(wait.results);
    error = wait.response == 1 ? "the user cancelled the authorisation"
                               : "the portal has rejected the request";
    return false;
  }
  GVariant* dict = wait.results != nullptr
      ? wait.results
      : g_variant_ref_sink(g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0));
  // Maintain the contract (a{sv}) that consumes all the required operations.
  *results = portalResponseTuple(dict);
  return true;
}

bool PortalClient::checkResponseDecoding() {
  RequestWait wait;
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "devices", g_variant_new_uint32(3));
  g_variant_builder_add(&options, "{sv}", "session_handle",
                        g_variant_new_string("/org/freedesktop/portal/desktop/session/test"));
  GVariant* parameters = g_variant_ref_sink(
      g_variant_new("(u@a{sv})", 0u, g_variant_builder_end(&options)));
  onResponse(nullptr, nullptr, nullptr, nullptr, nullptr, parameters, &wait);
  g_variant_unref(parameters);
  if (!wait.done || wait.response != 0 || wait.results == nullptr) return false;
  GVariant* tuple = portalResponseTuple(wait.results);
  GVariant* dict = g_variant_get_child_value(tuple, 0);
  guint32 devices = 0;
  std::string session;
  const bool ok = g_variant_is_of_type(tuple, G_VARIANT_TYPE("(a{sv})")) &&
                  g_variant_lookup(dict, "devices", "u", &devices) && devices == 3 &&
                  remoteSessionHandle(dict, session) &&
                  session == "/org/freedesktop/portal/desktop/session/test";
  g_variant_unref(dict);
  g_variant_unref(tuple);
  return ok;
}

guint PortalClient::subscribe(const std::string& path, const char* iface, const char* member,
                              GDBusSignalCallback callback) {
  if (bus_ == nullptr || path.empty()) return 0;
  return g_dbus_connection_signal_subscribe(bus_, kPortalService, iface, member, path.c_str(),
                                            nullptr, G_DBUS_SIGNAL_FLAGS_NONE, callback, this,
                                            nullptr);
}

void PortalClient::unsubscribe(std::vector<guint>& ids) {
  if (bus_ != nullptr) {
    for (guint id : ids) {
      if (id != 0) g_dbus_connection_signal_unsubscribe(bus_, id);
    }
  }
  ids.clear();
}

void PortalClient::subscribeInput(const std::string& session) {
  inputSubs_.push_back(subscribe(session, "org.freedesktop.portal.InputCapture", "Activated",
                                 onInputActivated));
  inputSubs_.push_back(subscribe(session, "org.freedesktop.portal.InputCapture", "Deactivated",
                                 onInputDeactivated));
  inputSubs_.push_back(subscribe(session, "org.freedesktop.portal.InputCapture", "Disabled",
                                 onInputDisabled));
  inputSubs_.push_back(subscribe(session, "org.freedesktop.portal.InputCapture", "ZonesChanged",
                                 onInputZonesChanged));
  inputSubs_.push_back(
      subscribe(session, "org.freedesktop.portal.Session", "Closed", onInputClosed));
}

void PortalClient::subscribeRemote(const std::string& session) {
  remoteSubs_.push_back(
      subscribe(session, "org.freedesktop.portal.Session", "Closed", onRemoteClosed));
}

void PortalClient::subscribeShortcuts(const std::string& session) {
  shortcutSession_ = session;
  shortcutClosed_ = false;
  // Activated is broadcast in the object of the portal; its first argument is session.
  shortcutSubs_.push_back(subscribe(kPortalPath, "org.freedesktop.portal.GlobalShortcuts",
                                   "Activated", onShortcutActivated));
  shortcutSubs_.push_back(subscribe(kPortalPath, "org.freedesktop.portal.GlobalShortcuts",
                                   "Deactivated", onShortcutDeactivated));
  shortcutSubs_.push_back(subscribe(session, "org.freedesktop.portal.Session",
                                   "Closed", onRemoteClosed));
}

void PortalClient::onInputActivated(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                    const gchar*, GVariant* parameters, gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ == nullptr || parameters == nullptr) return;
  GVariant* options = g_variant_get_child_value(parameters, 1);
  guint32 activation = 0;
  guint32 barrier = 0;
  gdouble x = 0.0;
  gdouble y = 0.0;
  g_variant_lookup(options, "activation_id", "u", &activation);
  g_variant_lookup(options, "cursor_position", "(dd)", &x, &y);
  g_variant_lookup(options, "barrier_id", "u", &barrier);
  g_variant_unref(options);
  client->events_->portalInputActivated(activation, x, y, barrier);
}

void PortalClient::onInputDeactivated(GDBusConnection*, const gchar*, const gchar*,
                                      const gchar*, const gchar*, GVariant* parameters,
                                      gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ == nullptr || parameters == nullptr) return;
  GVariant* options = g_variant_get_child_value(parameters, 1);
  guint32 activation = 0;
  g_variant_lookup(options, "activation_id", "u", &activation);
  g_variant_unref(options);
  client->events_->portalInputDeactivated(activation);
}

void PortalClient::onInputDisabled(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                   const gchar*, GVariant*, gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ != nullptr) client->events_->portalInputDisabled();
}

void PortalClient::onInputZonesChanged(GDBusConnection*, const gchar*, const gchar*,
                                       const gchar*, const gchar*, GVariant*, gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ != nullptr) client->events_->portalInputZonesChanged();
}

void PortalClient::onInputClosed(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                 const gchar*, GVariant*, gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ != nullptr) client->events_->portalInputClosed();
}

void PortalClient::onRemoteClosed(GDBusConnection*, const gchar*, const gchar* path, const gchar*,
                                  const gchar*, GVariant*, gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (path != nullptr && client->shortcutSession_ == path) client->shortcutClosed_ = true;
  if (client->events_ != nullptr) client->events_->portalRemoteClosed();
}

void PortalClient::onShortcutActivated(GDBusConnection*, const gchar*, const gchar*,
                                       const gchar*, const gchar*, GVariant* parameters,
                                       gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  if (client->events_ == nullptr || parameters == nullptr) return;
  GVariant* session = g_variant_get_child_value(parameters, 0);
  const bool ownSession = client->shortcutSession_ == g_variant_get_string(session, nullptr);
  g_variant_unref(session);
  if (!ownSession) return;
  GVariant* identifier = g_variant_get_child_value(parameters, 1);
  const gchar* text = g_variant_get_string(identifier, nullptr);
  const bool emergency = text != nullptr && std::strcmp(text, wl::kEmergencyShortcutId) == 0;
  g_variant_unref(identifier);
  if (emergency) {
    client->shortcutActivated_ = true;
    client->shortcutReleased_ = false;
    if (!client->checkingShortcut_) client->events_->portalShortcutActivated();
  }
}

void PortalClient::onShortcutDeactivated(GDBusConnection*, const gchar*, const gchar*,
                                         const gchar*, const gchar*, GVariant* parameters,
                                         gpointer data) {
  auto* client = static_cast<PortalClient*>(data);
  const gchar* session = nullptr;
  const gchar* identifier = nullptr;
  guint64 timestamp = 0;
  GVariant* options = nullptr;
  g_variant_get(parameters, "(&o&st@a{sv})", &session, &identifier, &timestamp, &options);
  if (client->shortcutSession_ == session &&
      std::strcmp(identifier, wl::kEmergencyShortcutId) == 0 && client->shortcutActivated_) {
    client->shortcutReleased_ = true;
  }
  g_variant_unref(options);
}

bool PortalClient::checkShortcutSignals() {
  struct Events final : PortalEvents {
    int emergencies = 0;
    void portalInputActivated(uint32_t, double, double, uint32_t) override {}
    void portalInputDeactivated(uint32_t) override {}
    void portalInputDisabled() override {}
    void portalInputZonesChanged() override {}
    void portalInputClosed() override {}
    void portalRemoteClosed() override {}
    void portalShortcutActivated() override { ++emergencies; }
  } events;
  PortalClient client;  // No connect(): does not open the bus or create sessions.
  client.setEvents(&events);
  client.shortcutSession_ = "/org/freedesktop/portal/desktop/session/test";
  const auto signal = [&](bool activated, const char* session, const char* identifier) {
    GVariant* parameters = g_variant_ref_sink(g_variant_new(
        "(ost@a{sv})", session, identifier, guint64{1},
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), nullptr, 0)));
    if (activated)
      onShortcutActivated(nullptr, nullptr, nullptr, nullptr, nullptr, parameters, &client);
    else
      onShortcutDeactivated(nullptr, nullptr, nullptr, nullptr, nullptr, parameters, &client);
    g_variant_unref(parameters);
  };
  const char* own = client.shortcutSession_.c_str();
  const char* other = "/org/freedesktop/portal/desktop/session/other";
  signal(true, other, wl::kEmergencyShortcutId);
  signal(true, own, "oneintwo-emergency");  // Ignoring the ancient concession.
  if (events.emergencies != 0 || client.shortcutActivated_) return false;
  signal(true, own, wl::kEmergencyShortcutId);
  if (events.emergencies != 1 || !client.shortcutActivated_ || client.shortcutReleased_) return false;
  signal(false, own, wl::kEmergencyShortcutId);
  if (!client.shortcutReleased_) return false;
  client.checkingShortcut_ = true;
  client.shortcutActivated_ = false;
  client.shortcutReleased_ = false;
  signal(false, own, wl::kEmergencyShortcutId);
  if (client.shortcutReleased_) return false;
  signal(true, own, wl::kEmergencyShortcutId);
  signal(false, other, wl::kEmergencyShortcutId);
  if (client.shortcutReleased_ || events.emergencies != 1) return false;
  signal(false, own, wl::kEmergencyShortcutId);
  onRemoteClosed(nullptr, nullptr, own, nullptr, nullptr, nullptr, &client);
  return client.shortcutActivated_ && client.shortcutReleased_ && client.shortcutClosed_;
}

void PortalClient::closeAt(const std::string& path) {
  if (bus_ == nullptr || path.empty()) return;
  GError* gerror = nullptr;
  GVariant* reply = g_dbus_connection_call_sync(
      bus_, kPortalService, path.c_str(), "org.freedesktop.portal.Session", "Close", nullptr,
      nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &gerror);
  if (reply != nullptr) g_variant_unref(reply);
  if (gerror != nullptr) g_error_free(gerror);
}

void PortalClient::closeEis(int fd) {
  if (fd >= 0) ::close(fd);
}

// === Portal: InputCapture ===

bool PortalClient::createInputSession(std::string& session, std::string& error) {
  if (bus_ == nullptr) {
    error = "no connection to the session bus";
    return false;
  }
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  const std::string token = nextToken();
  g_variant_builder_add(&options, "{sv}", "session_handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* reply = callRaw("org.freedesktop.portal.InputCapture", "CreateSession2",
                            g_variant_new("(a{sv})", &options), "(a{sv})", error, 5000);
  if (reply == nullptr) return false;
  GVariant* dict = g_variant_get_child_value(reply, 0);
  const gchar* handle = nullptr;
  const bool ok = g_variant_lookup(dict, "session_handle", "&o", &handle) != 0;
  if (ok && handle != nullptr) session = handle;
  g_variant_unref(dict);
  g_variant_unref(reply);
  if (!ok || session.empty()) {
    error = "CreateSession2 did not return session_handle";
    return false;
  }
  subscribeInput(session);
  return true;
}

bool PortalClient::startInputSession(const std::string& session, std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  g_variant_builder_add(&options, "{sv}", "capabilities",
                        g_variant_new_uint32(wl::kCapabilityKeyboard |
                                             wl::kCapabilityPointer));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.InputCapture", "Start",
               g_variant_new("(osa{sv})", session.c_str(), "", &options), token, &results,
               error, kAuthTimeoutMs)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  guint32 granted = 0;
  g_variant_lookup(dict, "capabilities", "u", &granted);
  g_variant_unref(dict);
  g_variant_unref(results);
  const guint32 required = wl::kCapabilityKeyboard | wl::kCapabilityPointer;
  if ((granted & required) != required) {
    error = "desktop did not grant keyboard and pointer to InputCapture";
    return false;
  }
  return true;
}

bool PortalClient::connectEis(const char* iface, const std::string& session, int& fd,
                              std::string& error) {
  if (bus_ == nullptr) {
    error = "no connection to the session bus";
    return false;
  }
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  GUnixFDList* outList = nullptr;
  GError* gerror = nullptr;
  GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
      bus_, kPortalService, kPortalPath, iface, "ConnectToEIS",
      g_variant_new("(oa{sv})", session.c_str(), &options), G_VARIANT_TYPE("(h)"),
      G_DBUS_CALL_FLAGS_NONE, 10000, nullptr, &outList, nullptr, &gerror);
  if (reply == nullptr) {
    error = std::string("ConnectToEIS: ") + (gerror ? gerror->message : "no response");
    if (gerror) g_error_free(gerror);
    return false;
  }
  gint32 index = -1;
  g_variant_get(reply, "(h)", &index);
  GError* fdError = nullptr;
  const int result = outList != nullptr ? g_unix_fd_list_get(outList, index, &fdError) : -1;
  if (outList != nullptr) g_object_unref(outList);
  g_variant_unref(reply);
  if (result < 0) {
    error = std::string("ConnectToEIS did not deliver a descriptor: ") +
            (fdError ? fdError->message : "empty list");
    if (fdError) g_error_free(fdError);
    return false;
  }
  fd = result;
  return true;
}

bool PortalClient::connectInputEis(const std::string& session, int& fd, std::string& error) {
  return connectEis("org.freedesktop.portal.InputCapture", session, fd, error);
}

bool PortalClient::inputZones(const std::string& session, std::vector<wl::Zone>& zones,
                              uint32_t& zoneSet, std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.InputCapture", "GetZones",
               g_variant_new("(oa{sv})", session.c_str(), &options), token, &results, error,
               8000)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  GVariant* array = g_variant_lookup_value(dict, "zones", G_VARIANT_TYPE("a(uuii)"));
  if (array == nullptr) {
    error = "GetZones did not return the list of areas";
    g_variant_unref(dict);
    g_variant_unref(results);
    return false;
  }
  zones.clear();
  GVariantIter iter;
  g_variant_iter_init(&iter, array);
  guint32 width = 0;
  guint32 height = 0;
  gint32 x = 0;
  gint32 y = 0;
  while (g_variant_iter_next(&iter, "(uuii)", &width, &height, &x, &y)) {
    zones.push_back(wl::Zone{width, height, x, y});
  }
  g_variant_unref(array);
  guint32 set = 0;
  const bool haveSet = g_variant_lookup(dict, "zone_set", "u", &set) != 0;
  g_variant_unref(dict);
  g_variant_unref(results);
  if (!haveSet) {
    error = "GetZones did not return zone_set";
    return false;
  }
  zoneSet = set;
  return true;
}

bool PortalClient::setPointerBarriers(const std::string& session,
                                      const std::vector<wl::Barrier>& barriers,
                                      uint32_t zoneSet, std::vector<uint32_t>& failed,
                                      std::string& error) {
  GVariantBuilder list;
  g_variant_builder_init(&list, G_VARIANT_TYPE("aa{sv}"));
  for (const wl::Barrier& barrier : barriers) {
    GVariantBuilder entry;
    g_variant_builder_init(&entry, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&entry, "{sv}", "barrier_id", g_variant_new_uint32(barrier.id));
    g_variant_builder_add(&entry, "{sv}", "position",
                          g_variant_new("(iiii)", barrier.x1, barrier.y1, barrier.x2,
                                        barrier.y2));
    g_variant_builder_add_value(&list, g_variant_builder_end(&entry));
  }
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.InputCapture", "SetPointerBarriers",
               g_variant_new("(o@a{sv}@aa{sv}u)", session.c_str(),
                             g_variant_builder_end(&options),
                             g_variant_builder_end(&list), zoneSet),
               token, &results, error, 10000)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  GVariant* array = g_variant_lookup_value(dict, "failed_barriers", G_VARIANT_TYPE("au"));
  if (array != nullptr) {
    GVariantIter iter;
    g_variant_iter_init(&iter, array);
    guint32 id = 0;
    while (g_variant_iter_next(&iter, "u", &id)) failed.push_back(id);
    g_variant_unref(array);
  }
  g_variant_unref(dict);
  g_variant_unref(results);
  return true;
}

bool PortalClient::enableInput(const std::string& session, std::string& error) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  return callVoid("org.freedesktop.portal.InputCapture", "Enable",
                  g_variant_new("(oa{sv})", session.c_str(), &options), error);
}

bool PortalClient::disableInput(const std::string& session, std::string& error) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  return callVoid("org.freedesktop.portal.InputCapture", "Disable",
                  g_variant_new("(oa{sv})", session.c_str(), &options), error);
}

bool PortalClient::releaseInput(const std::string& session, uint32_t activationId, double x,
                                double y, std::string& error) {
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "activation_id",
                        g_variant_new_uint32(activationId));
  g_variant_builder_add(&options, "{sv}", "cursor_position",
                        g_variant_new("(dd)", x, y));
  return callVoid("org.freedesktop.portal.InputCapture", "Release",
                  g_variant_new("(oa{sv})", session.c_str(), &options), error);
}

void PortalClient::closeInputSession(const std::string& session) {
  unsubscribe(inputSubs_);
  closeAt(session);
}

// === Portal: RemoteDesktop ===

bool PortalClient::createRemoteSession(std::string& session, std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  g_variant_builder_add(&options, "{sv}", "session_handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.RemoteDesktop", "CreateSession",
               g_variant_new("(a{sv})", &options), token, &results, error, 8000)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  const bool ok = remoteSessionHandle(dict, session);
  g_variant_unref(dict);
  g_variant_unref(results);
  if (!ok || session.empty()) {
    error = "RemoteDesktop.CreateSession did not return session_handle";
    return false;
  }
  subscribeRemote(session);
  return true;
}

bool PortalClient::selectRemoteDevices(const std::string& session, uint32_t types,
                                       std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(types));
  GVariant* results = nullptr;
  const bool ok = request("org.freedesktop.portal.RemoteDesktop", "SelectDevices",
                          g_variant_new("(oa{sv})", session.c_str(), &options), token,
                          &results, error, 8000);
  if (results != nullptr) g_variant_unref(results);
  return ok;
}

bool PortalClient::startRemoteSession(const std::string& session, uint32_t& granted,
                                      std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.RemoteDesktop", "Start",
               g_variant_new("(osa{sv})", session.c_str(), "", &options), token, &results,
               error, kAuthTimeoutMs)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  guint32 devices = 0;
  g_variant_lookup(dict, "devices", "u", &devices);
  g_variant_unref(dict);
  g_variant_unref(results);
  granted = devices;
  return true;
}

bool PortalClient::connectRemoteEis(const std::string& session, int& fd, std::string& error) {
  return connectEis("org.freedesktop.portal.RemoteDesktop", session, fd, error);
}

void PortalClient::closeRemoteSession(const std::string& session) {
  unsubscribe(remoteSubs_);
  closeAt(session);
}

// === Portal: GlobalShortcuts ===

bool PortalClient::createShortcutSession(std::string& session, std::string& error) {
  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  g_variant_builder_add(&options, "{sv}", "session_handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.GlobalShortcuts", "CreateSession",
               g_variant_new("(a{sv})", &options), token, &results, error, 8000)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  const gchar* handle = nullptr;
  // The portal documents this field as s, not as o, by compatibility.
  const bool ok = g_variant_lookup(dict, "session_handle", "&s", &handle) != 0;
  if (ok && handle != nullptr) session = handle;
  g_variant_unref(dict);
  g_variant_unref(results);
  if (!ok || session.empty()) {
    error = "GlobalShortcuts.CreateSession did not return session_handle";
    return false;
  }
  subscribeShortcuts(session);
  return true;
}

bool PortalClient::bindEmergencyShortcut(const std::string& session, bool& bound,
                                         std::string& error) {
  GVariantBuilder shortcuts;
  g_variant_builder_init(&shortcuts, G_VARIANT_TYPE("a(sa{sv})"));
  GVariantBuilder properties;
  g_variant_builder_init(&properties, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&properties, "{sv}", "description",
                        g_variant_new_string("Return local control (OneInTwo)"));
  g_variant_builder_add(&properties, "{sv}", "preferred_trigger",
                        g_variant_new_string(wl::kEmergencyShortcutTrigger));
  g_variant_builder_add_value(
      &shortcuts, g_variant_new("(s@a{sv})", wl::kEmergencyShortcutId,
                                g_variant_builder_end(&properties)));

  const std::string token = nextToken();
  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&options, "{sv}", "handle_token",
                        g_variant_new_string(token.c_str()));
  GVariant* results = nullptr;
  if (!request("org.freedesktop.portal.GlobalShortcuts", "BindShortcuts",
               g_variant_new("(o@a(sa{sv})s@a{sv})", session.c_str(),
                             g_variant_builder_end(&shortcuts), "",
                             g_variant_builder_end(&options)),
               token, &results, error, kAuthTimeoutMs)) {
    return false;
  }
  GVariant* dict = g_variant_get_child_value(results, 0);
  GVariant* array = g_variant_lookup_value(dict, "shortcuts", G_VARIANT_TYPE("a(sa{sv})"));
  bound = false;
  if (array != nullptr) {
    GVariantIter iter;
    g_variant_iter_init(&iter, array);
    const gchar* identifier = nullptr;
    GVariant* propertiesOut = nullptr;
    while (g_variant_iter_next(&iter, "(&s@a{sv})", &identifier, &propertiesOut)) {
      if (identifier != nullptr && std::strcmp(identifier, wl::kEmergencyShortcutId) == 0) {
        bound = true;
        const gchar* trigger = nullptr;
        g_variant_lookup(propertiesOut, "trigger_description", "&s", &trigger);
        std::fprintf(stderr, "Exit gate granted by the portal: %s\n",
                     trigger != nullptr ? trigger : "sin descripcion");
      }
      g_variant_unref(propertiesOut);
    }
    g_variant_unref(array);
  }
  g_variant_unref(dict);
  g_variant_unref(results);
  return true;
}

void PortalClient::closeShortcutSession(const std::string& session) {
  unsubscribe(shortcutSubs_);
  shortcutSession_.clear();
  closeAt(session);
}

bool PortalClient::verifyEmergencyShortcut(const std::string& session, std::string& error) {
  if (session != shortcutSession_ || shortcutClosed_) {
    error = "shortcut session is not available";
    return false;
  }
  checkingShortcut_ = true;
  shortcutActivated_ = false;
  shortcutReleased_ = false;
  std::printf("Before opening RemoteDesktop/EIS, press and release Ctrl+Shift+F12 (30 s).\n");
  std::fflush(stdout);
  const gint64 deadline = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
  while (!shortcutClosed_ && !(shortcutActivated_ && shortcutReleased_) &&
         g_get_monotonic_time() < deadline) {
    pump();
    g_usleep(1000);
  }
  checkingShortcut_ = false;
  if (shortcutClosed_ || !shortcutActivated_ || !shortcutReleased_) {
    error = "unchecked local output; no RemoteDesktop or EIS opened";
    return false;
  }
  return true;
}

// ===Libei: recipient of origin===

class EiReceiver {
 public:
  ~EiReceiver() { close(); }

  bool start(int fd, std::string& error) {
    ei_ = ei_new_receiver(nullptr);
    if (ei_ == nullptr) {
      error = "could not create host libei context";
      ::close(fd);
      return false;
    }
    ei_configure_name(ei_, "oneintwo-input-source");
    if (ei_setup_backend_fd(ei_, fd) != 0) {
      error = "libei rejected InputCapture descriptor EIS";
      ei_unref(ei_);
      ei_ = nullptr;
      ::close(fd);
      return false;
    }
    // From here on, Libei owns the descriptor and closes it when he gets free.
    return true;
  }

  void close() {
    if (ei_ != nullptr) {
      ei_unref(ei_);
      ei_ = nullptr;
    }
    keyboard_ = nullptr;
    pointer_ = nullptr;
    disconnected_ = false;
    haveAbsolute_ = false;
  }

  int fd() const { return ei_ != nullptr ? ei_get_fd(ei_) : -1; }
  bool disconnected() const { return disconnected_; }

  void pump(SpscRing<RawEvent, 1024>& queue, bool active) {
    if (ei_ == nullptr) return;
    ei_dispatch(ei_);
    while (ei_event* event = ei_get_event(ei_)) {
      handle(event, queue, active);
      ei_event_unref(event);
    }
  }

 private:
  void handle(ei_event* event, SpscRing<RawEvent, 1024>& queue, bool active) {
    switch (ei_event_get_type(event)) {
      case EI_EVENT_DISCONNECT:
        disconnected_ = true;
        break;
      case EI_EVENT_SEAT_ADDED: {
        ei_seat* seat = ei_event_get_seat(event);
        if (seat != nullptr) {
          ei_seat_bind_capabilities(seat, EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_POINTER,
                                    EI_DEVICE_CAP_POINTER_ABSOLUTE, NULL);
        }
        break;
      }
      case EI_EVENT_DEVICE_ADDED: {
        ei_device* device = ei_event_get_device(event);
        if (device != nullptr) {
          if (ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) keyboard_ = device;
          if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER) ||
              ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
            pointer_ = device;
          }
        }
        break;
      }
      case EI_EVENT_DEVICE_REMOVED: {
        ei_device* device = ei_event_get_device(event);
        if (device == keyboard_) keyboard_ = nullptr;
        if (device == pointer_) pointer_ = nullptr;
        break;
      }
      case EI_EVENT_DEVICE_START_EMULATING: {
        // The ACK with the portal sequence is the synchronization point of
        // InputCapture; until you send it EIS does not deliver events.
        ei_device* device = ei_event_get_device(event);
        if (device != nullptr) {
          haveAbsolute_ = false;
          ei_device_start_emulating(device, ei_event_emulating_get_sequence(event));
        }
        break;
      }
      case EI_EVENT_KEYBOARD_KEY: {
        if (!active) break;
        const uint16_t usage =
            linuxCodeToHid(static_cast<uint16_t>(ei_event_keyboard_get_key(event)));
        if (usage == 0) break;
        RawEvent raw;
        raw.kind = RawKind::Key;
        raw.code = usage;
        raw.pressed = ei_event_keyboard_get_key_is_press(event);
        queue.push(raw);
        break;
      }
      case EI_EVENT_POINTER_MOTION: {
        if (!active) break;
        RawEvent raw;
        raw.kind = RawKind::MotionDelta;
        raw.dx = clampDelta(ei_event_pointer_get_dx(event));
        raw.dy = clampDelta(ei_event_pointer_get_dy(event));
        if (raw.dx != 0 || raw.dy != 0) queue.push(raw);
        break;
      }
      case EI_EVENT_POINTER_MOTION_ABSOLUTE: {
        if (!active) break;
        const double x = ei_event_pointer_get_absolute_x(event);
        const double y = ei_event_pointer_get_absolute_y(event);
        if (haveAbsolute_) {
          RawEvent raw;
          raw.kind = RawKind::MotionDelta;
          raw.dx = clampDelta(x - lastAbsoluteX_);
          raw.dy = clampDelta(y - lastAbsoluteY_);
          if (raw.dx != 0 || raw.dy != 0) queue.push(raw);
        }
        lastAbsoluteX_ = x;
        lastAbsoluteY_ = y;
        haveAbsolute_ = true;
        break;
      }
      case EI_EVENT_BUTTON_BUTTON: {
        if (!active) break;
        const uint16_t code = protocolButton(ei_event_button_get_button(event));
        if (code == 0xFFFF) break;
        RawEvent raw;
        raw.kind = RawKind::Button;
        raw.code = code;
        raw.pressed = ei_event_button_get_is_press(event);
        queue.push(raw);
        break;
      }
      case EI_EVENT_SCROLL_DISCRETE: {
        if (!active) break;
        const int32_t dx = ei_event_scroll_get_discrete_dx(event) / 120;
        const int32_t dy = ei_event_scroll_get_discrete_dy(event) / 120;
        if (dx == 0 && dy == 0) break;
        RawEvent raw;
        raw.kind = RawKind::Wheel;
        raw.dx = static_cast<int16_t>(std::max<int32_t>(-32768, std::min<int32_t>(32767, dx)));
        raw.dy = static_cast<int16_t>(std::max<int32_t>(-32768, std::min<int32_t>(32767, -dy)));
        queue.push(raw);
        break;
      }
      default:
        break;
    }
  }

  ei* ei_ = nullptr;
  ei_device* keyboard_ = nullptr;
  ei_device* pointer_ = nullptr;
  bool disconnected_ = false;
  bool haveAbsolute_ = false;
  double lastAbsoluteX_ = 0.0;
  double lastAbsoluteY_ = 0.0;
};

// ===Libei: issuer of destiny ===

class EiSender {
 public:
  ~EiSender() { close(); }

  bool start(int fd, Geo& geo, std::string& error) {
    ei_ = ei_new_sender(nullptr);
    if (ei_ == nullptr) {
      error = "could not create issuing libei context";
      ::close(fd);
      return false;
    }
    ei_configure_name(ei_, "oneintwo-input-target");
    if (std::getenv("ONEINTWO_EI_DEBUG") != nullptr) {
      ei_log_set_priority(ei_, EI_LOG_PRIORITY_DEBUG);
    }
    if (ei_setup_backend_fd(ei_, fd) != 0) {
      error = "libei rejected RemoteDesktop descriptor EIS";
      ei_unref(ei_);
      ei_ = nullptr;
      return false;
    }
    // From here on, Libei owns the descriptor and closes it when he gets free.
    if (!waitForDevices(geo, error)) {
      close();
      return false;
    }
    return true;
  }

  void close() {
    if (ei_ != nullptr) {
      // Release only our states before closing the transaction and the FD.
      if (contains(emulating_, keyboard_) && contains(resumed_, keyboard_)) {
        for (uint32_t key : pressedKeys_) ei_device_keyboard_key(keyboard_, key, false);
        if (!pressedKeys_.empty()) ei_device_frame(keyboard_, ei_now(ei_));
      }
      if (contains(emulating_, pointer_) && contains(resumed_, pointer_)) {
        for (uint32_t button : pressedButtons_) ei_device_button_button(pointer_, button, false);
        if (!pressedButtons_.empty()) ei_device_frame(pointer_, ei_now(ei_));
      }
      for (ei_device* device : emulating_) {
        if (contains(resumed_, device)) ei_device_stop_emulating(device);
      }
      for (ei_device* device : devices_) ei_device_unref(device);
      ei_unref(ei_);
      ei_ = nullptr;
    }
    devices_.clear();
    resumed_.clear();
    emulating_.clear();
    pressedKeys_.clear();
    pressedButtons_.clear();
    keyboard_ = nullptr;
    pointer_ = nullptr;
    absolute_ = nullptr;
    disconnected_ = false;
    deviceLost_ = false;
    geo_ = Geo{};
  }

  bool pump(std::string& error) {
    if (ei_ == nullptr) {
      error = "the libei sender context is missing";
      return false;
    }
    ei_dispatch(ei_);
    while (ei_event* event = ei_get_event(ei_)) {
      handle(event);
      ei_event_unref(event);
    }
    if (disconnected_ || deviceLost_) {
      error = "EIS closed, paused or removed a device from the destination";
      return false;
    }
    return true;
  }

  bool injectMove(int x, int y, std::string& error) {
    if (!begin(absolute_, error)) return false;
    ei_device_pointer_motion_absolute(absolute_, x - geo_.x, y - geo_.y);
    ei_device_frame(absolute_, ei_now(ei_));
    return true;
  }

  bool injectKey(uint16_t usage, bool pressed, std::string& error) {
    if (!ready(error)) return false;
    const uint16_t code = hidToLinuxCode(usage);
    if (code == 0) {
      error = "key without evdev conversion";
      return false;
    }
    if (!begin(keyboard_, error)) return false;
    ei_device_keyboard_key(keyboard_, code, pressed);
    if (pressed) {
      if (std::find(pressedKeys_.begin(), pressedKeys_.end(), code) == pressedKeys_.end())
        pressedKeys_.push_back(code);
    } else {
      pressedKeys_.erase(std::remove(pressedKeys_.begin(), pressedKeys_.end(), code), pressedKeys_.end());
    }
    ei_device_frame(keyboard_, ei_now(ei_));
    return true;
  }

  bool injectButton(uint16_t button, bool pressed, std::string& error) {
    if (!ready(error)) return false;
    const uint32_t code = evdevButton(button);
    if (code == 0) {
      error = "mouse button not supported";
      return false;
    }
    if (!begin(pointer_, error)) return false;
    ei_device_button_button(pointer_, code, pressed);
    if (pressed) {
      if (std::find(pressedButtons_.begin(), pressedButtons_.end(), code) == pressedButtons_.end())
        pressedButtons_.push_back(code);
    } else {
      pressedButtons_.erase(std::remove(pressedButtons_.begin(), pressedButtons_.end(), code), pressedButtons_.end());
    }
    ei_device_frame(pointer_, ei_now(ei_));
    return true;
  }

  bool injectWheel(int16_t dx, int16_t dy, std::string& error) {
    if (!ready(error)) return false;
    if (!begin(pointer_, error)) return false;
    // IE uses evdev: positive displacement is down and wheel
    // discreetly expressed in multiples of 120.
    ei_device_scroll_discrete(pointer_, static_cast<int32_t>(dx) * 120,
                              static_cast<int32_t>(-dy) * 120);
    ei_device_frame(pointer_, ei_now(ei_));
    return true;
  }

 private:
  static bool contains(const std::vector<ei_device*>& list, ei_device* device) {
    return device != nullptr && std::find(list.begin(), list.end(), device) != list.end();
  }

  bool begin(ei_device* device, std::string& error) {
    if (!ready(error)) return false;
    if (!contains(emulating_, device)) {
      // Do not start during ADDED or when preparing permissions: only in front of a real event.
      ei_device_start_emulating(device, ++sequence_);
      emulating_.push_back(device);
    }
    return true;
  }

  bool devicesReady() const {
    return contains(resumed_, keyboard_) && contains(resumed_, pointer_) &&
           contains(resumed_, absolute_);
  }

  bool ready(std::string& error) const {
    if (disconnected_) {
      error = "the connection EIS of the destination is closed";
      return false;
    }
    if (deviceLost_ || !devicesReady()) {
      error = "EIS devices are not resumed or have been removed";
      return false;
    }
    return true;
  }

  bool waitForDevices(Geo& geo, std::string& error) {
    const gint64 deadline = g_get_monotonic_time() + 5000 * 1000;
    while (g_get_monotonic_time() < deadline) {
      if (devicesReady()) break;
      struct pollfd descriptor {
        ei_get_fd(ei_), POLLIN, 0
      };
      const int ready = poll(&descriptor, 1, 20);
      if (ready > 0) {
        ei_dispatch(ei_);
        while (ei_event* event = ei_get_event(ei_)) {
          handle(event);
          ei_event_unref(event);
        }
      }
      if (disconnected_) {
        error = "EIS disconnected before offering devices";
        return false;
      }
    }
    if (keyboard_ == nullptr) {
      error = "the EIS did not offer a keyboard device";
      return false;
    }
    if (absolute_ == nullptr) {
      error =
          "the EIS desktop did not offer absolute pointer "
          "(EI_DEVICE_CAP_POINTER_ABSOLUTE); the Wayland destination cannot position the "
          "absolute cursor";
      return false;
    }
    if (pointer_ == nullptr) {
      error = "EIS did not offer a pointer device for buttons and wheel";
      return false;
    }

    if (deviceLost_ || !devicesReady()) {
      error = "EIS did not resume devices before the preparation limit";
      return false;
    }
    ei_region* region = ei_device_get_region(absolute_, 0);
    if (region == nullptr) region = ei_device_get_region(pointer_, 0);
    if (region == nullptr) {
      error =
          "the device EIS does not expose a desktop region; without it the destination does not "
          "you can know your geometry";
      return false;
    }
    if (ei_device_get_region(absolute_, 1) != nullptr ||
        ei_device_get_region(pointer_, 1) != nullptr) {
      error = "EIS exposes several regions; the experiment only admits one monitor";
      return false;
    }
    if (!wl::regionGeometry(ei_region_get_x(region), ei_region_get_y(region),
                            ei_region_get_width(region), ei_region_get_height(region), geo)) {
      error = "region EIS does not have a usable size";
      return false;
    }
    geo_ = geo;
    return true;
  }

  void handle(ei_event* event) {
    switch (ei_event_get_type(event)) {
      case EI_EVENT_DISCONNECT:
        disconnected_ = true;
        break;
      case EI_EVENT_SEAT_ADDED: {
        ei_seat* seat = ei_event_get_seat(event);
        if (seat != nullptr) {
          ei_seat_bind_capabilities(seat, EI_DEVICE_CAP_KEYBOARD, EI_DEVICE_CAP_POINTER,
                                    EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_BUTTON,
                                    EI_DEVICE_CAP_SCROLL, NULL);
        }
        break;
      }
      case EI_EVENT_DEVICE_ADDED: {
        ei_device* device = ei_event_get_device(event);
        if (device == nullptr) break;
        devices_.push_back(ei_device_ref(device));
        if (ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) keyboard_ = device;
        if (ei_device_has_capability(device, EI_DEVICE_CAP_BUTTON) &&
            ei_device_has_capability(device, EI_DEVICE_CAP_SCROLL)) pointer_ = device;
        if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
          absolute_ = device;
        }
        break;
      }
      case EI_EVENT_DEVICE_RESUMED: {
        ei_device* device = ei_event_get_device(event);
        if (!contains(resumed_, device)) resumed_.push_back(device);
        break;
      }
      case EI_EVENT_DEVICE_PAUSED: {
        ei_device* device = ei_event_get_device(event);
        resumed_.erase(std::remove(resumed_.begin(), resumed_.end(), device), resumed_.end());
        // Never quietly resume a transaction after losing devices.
        if (device == keyboard_ || device == pointer_ || device == absolute_) deviceLost_ = true;
        break;
      }
      case EI_EVENT_DEVICE_REMOVED: {
        ei_device* device = ei_event_get_device(event);
        if (device == keyboard_ || device == pointer_ || device == absolute_) deviceLost_ = true;
        if (device == keyboard_) keyboard_ = nullptr;
        if (device == pointer_) pointer_ = nullptr;
        if (device == absolute_) absolute_ = nullptr;
        if (contains(emulating_, device)) deviceLost_ = true;
        resumed_.erase(std::remove(resumed_.begin(), resumed_.end(), device), resumed_.end());
        emulating_.erase(std::remove(emulating_.begin(), emulating_.end(), device), emulating_.end());
        auto it = std::find(devices_.begin(), devices_.end(), device);
        if (it != devices_.end()) {
          ei_device_unref(*it);
          devices_.erase(it);
        }
        break;
      }
      default:
        break;
    }
  }

  ei* ei_ = nullptr;
  ei_device* keyboard_ = nullptr;
  ei_device* pointer_ = nullptr;
  ei_device* absolute_ = nullptr;
  Geo geo_;
  bool disconnected_ = false;
  bool deviceLost_ = false;
  uint32_t sequence_ = 0;
  std::vector<ei_device*> devices_;
  std::vector<ei_device*> resumed_;
  std::vector<ei_device*> emulating_;
  std::vector<uint32_t> pressedKeys_;
  std::vector<uint32_t> pressedButtons_;
};

// === Backend ===

class WaylandBackend : public Backend, public PortalEvents {
 public:
  WaylandBackend() : remote_(portal_) {}

  bool init(std::string& error) override;
  Geo geometry() const override { return geo_; }
  int monitorCount() const override { return 1; }
  BackendCaps caps() const override { return caps_; }
  std::string sessionType() const override { return sessionType_; }

  bool prepareSource(Edge edge, std::string& error) override;
  bool prepareTarget(std::string& error) override;
  bool prepareTargetExit(std::string& error) override { return remote_.prepareExit(error); }
  const char* targetExitShortcut() const override { return "Ctrl+Shift+F12"; }
  bool startSource(std::string& error) override;
  void stopSource() override;
  bool setCaptureActive(bool active, std::string& error) override;
  bool nextSourceEvent(RawEvent& event, int waitMs) override;
  void warpLocalCursor(int x, int y) override;
  bool sourceFatal(std::string& reason) override;

  bool startTarget(std::string& error) override;
  void stopTarget() override;
  void setTargetGuardArmed(bool armed) override { (void)armed; }
  bool nextTargetSignal(TargetSignal& signal, int waitMs) override;

  bool injectMove(int x, int y, std::string& error) override;
  bool injectKey(uint16_t usage, bool pressed, std::string& error) override;
  bool injectButton(uint16_t button, bool pressed, std::string& error) override;
  bool injectWheel(int16_t dx, int16_t dy, std::string& error) override;
  bool cursorPosition(int& x, int& y) override;

  void portalInputActivated(uint32_t activationId, double x, double y,
                            uint32_t barrierId) override {
    if (capture_) capture_->onActivated(activationId, x, y, barrierId);
  }
  void portalInputDeactivated(uint32_t activationId) override {
    if (capture_) capture_->onDeactivated(activationId);
  }
  void portalInputDisabled() override {
    if (capture_) capture_->onDisabled();
  }
  void portalInputZonesChanged() override {
    if (capture_) capture_->onZonesChanged();
  }
  void portalInputClosed() override {
    if (capture_) capture_->onClosed();
  }
  void portalRemoteClosed() override { remote_.onClosed(); }
  void portalShortcutActivated() override { shortcutFired_.store(true); }

 private:
  void sourceThreadMain();
  void drainCaptureState();
  void drainTargetState();

  PortalClient portal_;
  wl::RemoteDesktopFlow remote_;
  std::unique_ptr<wl::InputCaptureFlow> capture_;
  std::unique_ptr<EiReceiver> receiver_;
  std::unique_ptr<EiSender> sender_;
  Geo geo_;
  BackendCaps caps_;
  std::string sessionType_;

  std::thread sourceThread_;
  std::atomic<bool> sourceRunning_{false};
  std::atomic<bool> captureActive_{false};
  std::atomic<bool> eiLost_{false};
  std::atomic<bool> shortcutFired_{false};
  double lastCursorY_ = 0.0;
  std::string fatalReason_;
  SpscRing<RawEvent, 1024> sourceQueue_;
  SpscRing<TargetSignal, 64> targetQueue_;
};

bool WaylandBackend::init(std::string& error) {
  if (!isWaylandSession()) {
    error = "the current session is not Wayland";
    return false;
  }
  portal_.setEvents(this);
  std::string busError;
  const bool connected = portal_.connect(busError);

  uint32_t inputVersion = 0;
  uint32_t inputCaps = 0;
  uint32_t remoteVersion = 0;
  uint32_t shortcutVersion = 0;
  const bool inputV = connected &&
      portal_.queryUint("org.freedesktop.portal.InputCapture", "version", inputVersion) &&
      inputVersion >= 2;
  const bool inputC = connected &&
      portal_.queryUint("org.freedesktop.portal.InputCapture", "SupportedCapabilities",
                        inputCaps) &&
      (inputCaps & (wl::kCapabilityKeyboard | wl::kCapabilityPointer)) ==
          (wl::kCapabilityKeyboard | wl::kCapabilityPointer);
  const bool remoteV = connected &&
      portal_.queryUint("org.freedesktop.portal.RemoteDesktop", "version", remoteVersion) &&
      remoteVersion >= 2;
  const bool shortcutV = connected &&
      portal_.queryUint("org.freedesktop.portal.GlobalShortcuts", "version", shortcutVersion) &&
      shortcutVersion >= 1;

  caps_.name = "Linux Wayland (InputCapture + RemoteDesktop + libei)";
  caps_.capture = inputV && inputC;
  caps_.inject = remoteV && shortcutV;
  caps_.targetLocalOverride = shortcutV;

  if (!connected) {
    caps_.notes = "No session bus (" + busError +
                  "): No Wayland portal can be authorized.";
  } else {
    caps_.notes = "InputCapture v";
    caps_.notes += std::to_string(inputVersion);
    caps_.notes += inputC ? " with keyboard and pointer" : " without all capacities";
    caps_.notes += "; RemoteDesktop v" + std::to_string(remoteVersion);
    caps_.notes += "; GlobalShortcuts v" + std::to_string(shortcutVersion);
    caps_.notes +=
        ". Destination requires that EIS offer POINTER_ABSOLUTE and a region of "
        "desktop; checked when preparing it. Wayland does not allow to consult or "
        "deform the global position of the cursor.";
  }

  const char* session = std::getenv("XDG_SESSION_TYPE");
  sessionType_ = "Wayland (portales XDG + libei)";
  if (session != nullptr) sessionType_ += std::string(", XDG_SESSION_TYPE=") + session;
  return true;
}

bool WaylandBackend::prepareSource(Edge edge, std::string& error) {
  if (capture_) return true;
  capture_.reset(new wl::InputCaptureFlow(portal_, edge));
  if (!capture_->open(error)) {
    capture_.reset();
    return false;
  }
  const int fd = capture_->takeEisFd();
  if (fd < 0) {
    error = "the portal InputCapture did not deliver a descriptor EIS";
    capture_->close();
    capture_.reset();
    return false;
  }
  receiver_.reset(new EiReceiver());
  if (!receiver_->start(fd, error)) {
    receiver_.reset();
    capture_->close();
    capture_.reset();
    return false;
  }
  geo_ = capture_->geometry();
  lastCursorY_ = geo_.y + geo_.h / 2.0;
  return true;
}

bool WaylandBackend::prepareTarget(std::string& error) {
  if (remote_.stage() == wl::TargetStage::Ready) return true;
  int fd = -1;
  if (!remote_.open(fd, error)) return false;
  sender_.reset(new EiSender());
  Geo geo;
  if (!sender_->start(fd, geo, error)) {
    sender_.reset();
    remote_.close();
    return false;
  }
  geo_ = geo;
  return true;
}

bool WaylandBackend::startSource(std::string& error) {
  if (!capture_ || !receiver_) {
    error = "preparationSource was not executed for this session Wayland";
    return false;
  }
  if (sourceThread_.joinable()) {
    error = "Wayland observation is already under way";
    return false;
  }
  sourceRunning_.store(true);
  sourceThread_ = std::thread([this] { sourceThreadMain(); });
  return true;
}

void WaylandBackend::sourceThreadMain() {
  while (sourceRunning_.load(std::memory_order_relaxed)) {
    if (!receiver_) break;
    const int descriptor = receiver_->fd();
    if (descriptor < 0) break;
    struct pollfd entry {
      descriptor, POLLIN, 0
    };
    const int ready = poll(&entry, 1, 20);
    if (ready > 0) {
      receiver_->pump(sourceQueue_, captureActive_.load(std::memory_order_relaxed));
    }
    if (receiver_->disconnected()) {
      eiLost_.store(true, std::memory_order_relaxed);
      break;
    }
  }
}

void WaylandBackend::stopSource() {
  sourceRunning_.store(false, std::memory_order_relaxed);
  if (sourceThread_.joinable()) sourceThread_.join();
  captureActive_.store(false, std::memory_order_relaxed);
  receiver_.reset();
  if (capture_) {
    capture_->close();
    capture_.reset();
  }
}

bool WaylandBackend::setCaptureActive(bool active, std::string& error) {
  if (!capture_) {
    error = "Wayland capture is not ready.";
    return false;
  }
  if (active) {
    if (!capture_->armed() && !capture_->active()) {
      error = "InputCapture session is not armed";
      return false;
    }
    captureActive_.store(true, std::memory_order_relaxed);
    return true;
  }
  captureActive_.store(false, std::memory_order_relaxed);
  if (!capture_->active()) return true;

  const Geo geo = capture_->geometry();
  double y = lastCursorY_;
  y = std::max<double>(geo.y, std::min<double>(geo.y + geo.h - 1, y));
  const double x = capture_->edge() == Edge::Right ? geo.x + geo.w - 25.0 : geo.x + 25.0;
  return capture_->release(x, y, error);
}

void WaylandBackend::warpLocalCursor(int x, int y) {
  // Wayland does not allow to deform the global pointer; Release already suggested the
  // position of return to the composer. The coordinates are ignored.
  (void)x;
  (void)y;
}

bool WaylandBackend::nextSourceEvent(RawEvent& event, int waitMs) {
  if (sourceQueue_.pop(event)) return true;
  portal_.pump();
  drainCaptureState();
  if (sourceQueue_.pop(event)) return true;
  if (waitMs <= 0) return false;
  for (int elapsed = 0; elapsed < waitMs; ++elapsed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    portal_.pump();
    drainCaptureState();
    if (sourceQueue_.pop(event)) return true;
  }
  return false;
}

void WaylandBackend::drainCaptureState() {
  if (!capture_) return;
  std::string reason;
  if (capture_->fatal(reason)) {
    fatalReason_ = reason;
    return;
  }
  wl::Activation activation;
  while (capture_->takeActivation(activation)) {
    lastCursorY_ = activation.y;
    RawEvent raw;
    raw.kind = RawKind::PortalEdge;
    raw.x = static_cast<int32_t>(std::lround(activation.x));
    raw.y = static_cast<int32_t>(std::lround(activation.y));
    raw.code = static_cast<uint16_t>(activation.barrierId);
    sourceQueue_.push(raw);
  }
  if (capture_->takeCaptureLost()) {
    RawEvent raw;
    raw.kind = RawKind::CaptureLost;
    sourceQueue_.push(raw);
  }
  if (capture_->needsRearm()) {
    std::string rearmError;
    if (!capture_->rearm(rearmError)) fatalReason_ = rearmError;
  }
  if (eiLost_.exchange(false, std::memory_order_relaxed)) {
    fatalReason_ = "the connection EIS of the origin was interrupted";
  }
}

bool WaylandBackend::sourceFatal(std::string& reason) {
  if (fatalReason_.empty()) return false;
  reason = fatalReason_;
  return true;
}

bool WaylandBackend::startTarget(std::string& error) {
  if (remote_.stage() != wl::TargetStage::Ready || !sender_) {
    error = "preTarget was not executed for this Wayland session";
    return false;
  }
  return true;
}

void WaylandBackend::stopTarget() {
  sender_.reset();
  remote_.close();
  captureActive_.store(false, std::memory_order_relaxed);
}

bool WaylandBackend::nextTargetSignal(TargetSignal& signal, int waitMs) {
  portal_.pump();
  drainTargetState();
  if (targetQueue_.pop(signal)) return true;
  if (waitMs <= 0) return false;
  for (int elapsed = 0; elapsed < waitMs; ++elapsed) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    portal_.pump();
    drainTargetState();
    if (targetQueue_.pop(signal)) return true;
  }
  return false;
}

void WaylandBackend::drainTargetState() {
  if (shortcutFired_.exchange(false, std::memory_order_relaxed)) {
    TargetSignal signal;
    signal.kind = TargetSignal::Kind::Emergency;
    targetQueue_.push(signal);
  }
  std::string reason;
  if (remote_.fatal(reason)) {
    TargetSignal signal;
    signal.kind = TargetSignal::Kind::Stop;
    targetQueue_.push(signal);
    return;
  }
  if (sender_ && !sender_->pump(reason)) {
    TargetSignal signal;
    signal.kind = TargetSignal::Kind::Stop;
    targetQueue_.push(signal);
  }
}

bool WaylandBackend::injectMove(int x, int y, std::string& error) {
  if (!sender_) {
    error = "Wayland injection is not ready";
    return false;
  }
  return sender_->injectMove(x, y, error);
}

bool WaylandBackend::injectKey(uint16_t usage, bool pressed, std::string& error) {
  if (!sender_) {
    error = "Wayland injection is not ready";
    return false;
  }
  return sender_->injectKey(usage, pressed, error);
}

bool WaylandBackend::injectButton(uint16_t button, bool pressed, std::string& error) {
  if (!sender_) {
    error = "Wayland injection is not ready";
    return false;
  }
  return sender_->injectButton(button, pressed, error);
}

bool WaylandBackend::injectWheel(int16_t dx, int16_t dy, std::string& error) {
  if (!sender_) {
    error = "Wayland injection is not ready";
    return false;
  }
  return sender_->injectWheel(dx, dy, error);
}

bool WaylandBackend::cursorPosition(int& x, int& y) {
  // Wayland does not expose the overall cursor position to a client.
  // cannot save or restore the pre-session position.
  (void)x;
  (void)y;
  return false;
}

}  // namespace

std::unique_ptr<Backend> makeWaylandBackend() {
  return std::unique_ptr<Backend>(new WaylandBackend());
}

bool runWaylandPortalResponseChecks() {
  return PortalClient::checkResponseDecoding() && PortalClient::checkShortcutSignals();
}

}  // namespace oit
