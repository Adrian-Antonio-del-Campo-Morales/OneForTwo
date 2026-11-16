#include "mock_portal.hpp"

#include <gio/gunixfdlist.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

namespace oneintwo::test {
namespace {

constexpr char kPortalName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kScreenCastInterface[] = "org.freedesktop.portal.ScreenCast";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kSessionInterface[] = "org.freedesktop.portal.Session";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

const char kPortalXml[] = R"(
<node>
  <interface name="org.freedesktop.portal.ScreenCast">
    <method name="CreateSession">
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <method name="SelectSources">
      <arg type="o" name="session_handle" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <method name="Start">
      <arg type="o" name="session_handle" direction="in"/>
      <arg type="s" name="parent_window" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <method name="OpenPipeWireRemote">
      <arg type="o" name="session_handle" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="h" name="fd" direction="out"/>
    </method>
  </interface>
</node>
)";

const char kPropertiesXml[] = R"(
<node>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get">
      <arg type="s" name="interface" direction="in"/>
      <arg type="s" name="property" direction="in"/>
      <arg type="v" name="value" direction="out"/>
    </method>
    <method name="GetAll">
      <arg type="s" name="interface" direction="in"/>
      <arg type="a{sv}" name="properties" direction="out"/>
    </method>
  </interface>
</node>
)";

const char kRequestXml[] = R"(
<node>
  <interface name="org.freedesktop.portal.Request">
    <method name="Close"/>
  </interface>
</node>
)";

const char kSessionXml[] = R"(
<node>
  <interface name="org.freedesktop.portal.Session">
    <method name="Close"/>
    <signal name="Closed"/>
  </interface>
</node>
)";

std::string senderToken(const char* sender)
{
    if (sender == nullptr) {
        return "unknown";
    }
    std::string token = sender[0] == ':' ? sender + 1 : sender;
    for (char& character : token) {
        if (character == '.') {
            character = '_';
        }
    }
    return token;
}

std::string optionString(GVariant* options, const char* key)
{
    std::string value;
    GVariant* item = g_variant_lookup_value(options, key, G_VARIANT_TYPE_STRING);
    if (item != nullptr) {
        value = g_variant_get_string(item, nullptr);
        g_variant_unref(item);
    }
    return value;
}

std::uint32_t optionUint32(GVariant* options, const char* key, std::uint32_t fallback)
{
    std::uint32_t value = fallback;
    GVariant* item = g_variant_lookup_value(options, key, G_VARIANT_TYPE_UINT32);
    if (item != nullptr) {
        value = g_variant_get_uint32(item);
        g_variant_unref(item);
    }
    return value;
}

bool optionBool(GVariant* options, const char* key, bool fallback)
{
    bool value = fallback;
    GVariant* item = g_variant_lookup_value(options, key, G_VARIANT_TYPE_BOOLEAN);
    if (item != nullptr) {
        value = g_variant_get_boolean(item) != FALSE;
        g_variant_unref(item);
    }
    return value;
}

GVariant* vardictWithString(const char* key, const char* value)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", key, g_variant_new_string(value));
    return g_variant_builder_end(&builder);
}

GVariant* startResults(const MockPortalConfig& config, std::uint32_t node,
                       std::uint32_t source_type)
{
    GVariantBuilder properties;
    g_variant_builder_init(&properties, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&properties, "{sv}", "size",
                          g_variant_new("(ii)", static_cast<gint32>(config.stream_width),
                                        static_cast<gint32>(config.stream_height)));
    if (config.include_source_type) {
        const std::uint32_t reported = source_type == 2 ? config.window_source_type
                                                        : config.monitor_source_type;
        g_variant_builder_add(&properties, "{sv}", "source_type",
                              g_variant_new_uint32(reported));
    }
    g_variant_builder_add(&properties, "{sv}", "id",
                          g_variant_new_string("mock-stream-1"));
    g_variant_builder_add(&properties, "{sv}", "mapping_id",
                          g_variant_new_string("mock-mapping"));

    GVariantBuilder streams;
    g_variant_builder_init(&streams, G_VARIANT_TYPE("a(ua{sv})"));
    g_variant_builder_add(&streams, "(u@a{sv})", node, g_variant_builder_end(&properties));

    GVariantBuilder results;
    g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&results, "{sv}", "streams", g_variant_builder_end(&streams));
    g_variant_builder_add(&results, "{sv}", "restore_token",
                          g_variant_new_string("oneintwo-mock-token"));
    return g_variant_builder_end(&results);
}

void dispatchMethod(GDBusConnection*, const gchar* sender, const gchar* object_path,
                    const gchar* interface_name, const gchar* method_name, GVariant* parameters,
                    GDBusMethodInvocation* invocation, gpointer user)
{
    auto* self = static_cast<MockPortal*>(user);
    self->handleMethod(sender, object_path, interface_name, method_name, parameters, invocation);
}

const GDBusInterfaceVTable& portalVTable()
{
    static GDBusInterfaceVTable vtable = [] {
        GDBusInterfaceVTable table{};
        table.method_call = &dispatchMethod;
        table.get_property = nullptr;
        table.set_property = nullptr;
        return table;
    }();
    return vtable;
}

std::string requestPathFor(const char* sender, const std::string& token)
{
    return std::string(kPortalPath) + "/request/" + senderToken(sender) + "/" + token;
}

} // namespace

MockPortal::MockPortal() = default;

MockPortal::~MockPortal()
{
    stop();
}

bool MockPortal::start()
{
    if (connection_ != nullptr) {
        return true;
    }

    gchar* argv[] = {const_cast<gchar*>("dbus-daemon"), const_cast<gchar*>("--session"),
                     const_cast<gchar*>("--nofork"), const_cast<gchar*>("--print-address=1"),
                     nullptr};
    GError* error = nullptr;
    gint output_fd = -1;
    const GSpawnFlags flags = static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD
                                                       | G_SPAWN_SEARCH_PATH);
    if (g_spawn_async_with_pipes(nullptr, argv, nullptr, flags, nullptr, nullptr, &daemon_pid_,
                                 nullptr, &output_fd, nullptr, &error)
        == FALSE) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return false;
    }

    std::string address;
    char buffer[512];
    for (;;) {
        const ssize_t count = read(output_fd, buffer, sizeof(buffer));
        if (count <= 0) {
            break;
        }
        address.append(buffer, static_cast<std::size_t>(count));
        if (address.find('\n') != std::string::npos) {
            break;
        }
    }
    close(output_fd);
    while (!address.empty() && (address.back() == '\n' || address.back() == '\r')) {
        address.pop_back();
    }
    if (address.empty()) {
        return false;
    }
    g_setenv("DBUS_SESSION_BUS_ADDRESS", address.c_str(), TRUE);
    connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (connection_ == nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return false;
    }

    GDBusNodeInfo* portal_info = g_dbus_node_info_new_for_xml(kPortalXml, &error);
    if (portal_info == nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return false;
    }
    portal_registration_ = g_dbus_connection_register_object(
        connection_, kPortalPath, portal_info->interfaces[0], &portalVTable(), this, nullptr,
        &error);
    g_dbus_node_info_unref(portal_info);

    GDBusNodeInfo* properties_info = g_dbus_node_info_new_for_xml(kPropertiesXml, &error);
    if (properties_info == nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return false;
    }
    properties_registration_ = g_dbus_connection_register_object(
        connection_, kPortalPath, properties_info->interfaces[0], &portalVTable(), this, nullptr,
        &error);
    g_dbus_node_info_unref(properties_info);

    return portal_registration_ != 0 && properties_registration_ != 0;
}

bool MockPortal::ownName()
{
    if (connection_ == nullptr) {
        return false;
    }
    if (owner_id_ == 0) {
        owner_id_ = g_bus_own_name_on_connection(connection_, kPortalName,
                                                 G_BUS_NAME_OWNER_FLAGS_NONE, &onNameAcquired,
                                                 &onNameLost, this, nullptr);
    }
    return pumpUntil([this] { return owns_name_; }, 3000);
}

void MockPortal::unownName()
{
    if (owner_id_ != 0) {
        g_bus_unown_name(owner_id_);
        owner_id_ = 0;
    }
    owns_name_ = false;
}

void MockPortal::onNameAcquired(GDBusConnection*, const gchar*, gpointer user)
{
    static_cast<MockPortal*>(user)->owns_name_ = true;
}

void MockPortal::onNameLost(GDBusConnection*, const gchar*, gpointer user)
{
    static_cast<MockPortal*>(user)->owns_name_ = false;
}

void MockPortal::stop()
{
    unownName();
    if (connection_ != nullptr) {
        for (const guint registration : dynamic_registrations_) {
            if (registration != 0) {
                g_dbus_connection_unregister_object(connection_, registration);
            }
        }
        dynamic_registrations_.clear();
        if (portal_registration_ != 0) {
            g_dbus_connection_unregister_object(connection_, portal_registration_);
            portal_registration_ = 0;
        }
        if (properties_registration_ != 0) {
            g_dbus_connection_unregister_object(connection_, properties_registration_);
            properties_registration_ = 0;
        }
        g_object_unref(connection_);
        connection_ = nullptr;
    }
    if (daemon_pid_ > 0) {
        kill(daemon_pid_, SIGTERM);
        int status = 0;
        waitpid(daemon_pid_, &status, 0);
        g_spawn_close_pid(daemon_pid_);
        daemon_pid_ = 0;
    }
}

bool MockPortal::pumpUntil(const std::function<bool()>& done, int timeout_ms) const
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        while (g_main_context_iteration(nullptr, FALSE) != FALSE) {
        }
        if (done()) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return done();
        }
        g_usleep(2000);
    }
}

void MockPortal::registerRequest(const std::string& path)
{
    GError* error = nullptr;
    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(kRequestXml, &error);
    if (info == nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return;
    }
    const guint registration = g_dbus_connection_register_object(
        connection_, path.c_str(), info->interfaces[0], &portalVTable(), this, nullptr, &error);
    g_dbus_node_info_unref(info);
    if (registration != 0) {
        dynamic_registrations_.push_back(registration);
    } else if (error != nullptr) {
        g_error_free(error);
    }
}

void MockPortal::registerSession(const std::string& path)
{
    GError* error = nullptr;
    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(kSessionXml, &error);
    if (info == nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return;
    }
    const guint registration = g_dbus_connection_register_object(
        connection_, path.c_str(), info->interfaces[0], &portalVTable(), this, nullptr, &error);
    g_dbus_node_info_unref(info);
    if (registration != 0) {
        dynamic_registrations_.push_back(registration);
    } else if (error != nullptr) {
        g_error_free(error);
    }
}

void MockPortal::emitResponse(const std::string& path, guint32 code, GVariant* results)
{
    if (connection_ == nullptr) {
        return;
    }
    GVariantBuilder empty;
    g_variant_builder_init(&empty, G_VARIANT_TYPE_VARDICT);
    GVariant* dictionary = results != nullptr ? results : g_variant_builder_end(&empty);
    GError* error = nullptr;
    g_dbus_connection_emit_signal(connection_, nullptr, path.c_str(), kRequestInterface,
                                  "Response", g_variant_new("(u@a{sv})", code, dictionary),
                                  &error);
    if (error != nullptr) {
        g_error_free(error);
    }
}

void MockPortal::resetObservations()
{
    calls.clear();
    observed_types = 0;
    observed_multiple = true;
    observed_parent.clear();
    session_handle.clear();
    request_close_count = 0;
    session_close_count = 0;
    created_sessions = 0;
    start_pending_ = false;
    pending_start_path_.clear();
    current_session_.clear();
}

void MockPortal::respondStart(guint32 code)
{
    if (!start_pending_) {
        return;
    }
    start_pending_ = false;
    const std::uint32_t node
        = current_source_type_ == 2 ? config.window_node : config.monitor_node;
    if (code == 0) {
        emitResponse(pending_start_path_, 0, startResults(config, node, current_source_type_));
    } else {
        emitResponse(pending_start_path_, code, nullptr);
    }
}

void MockPortal::emitSessionClosed()
{
    if (connection_ == nullptr || current_session_.empty()) {
        return;
    }
    GError* error = nullptr;
    g_dbus_connection_emit_signal(connection_, nullptr, current_session_.c_str(),
                                  kSessionInterface, "Closed", g_variant_new("()"), &error);
    if (error != nullptr) {
        g_error_free(error);
    }
}

bool MockPortal::handleMethod(const char* sender, const char* object_path,
                              const char* interface_name, const char* method_name,
                              GVariant* parameters, GDBusMethodInvocation* invocation)
{
    if (g_strcmp0(interface_name, kPropertiesInterface) == 0) {
        if (g_strcmp0(method_name, "Get") == 0) {
            const char* interface = nullptr;
            const char* property = nullptr;
            g_variant_get(parameters, "(&s&s)", &interface, &property);
            if (g_strcmp0(interface, kScreenCastInterface) == 0
                && g_strcmp0(property, "AvailableSourceTypes") == 0) {
                g_dbus_method_invocation_return_value(
                    invocation,
                    g_variant_new("(v)", g_variant_new_uint32(config.available_types)));
                return true;
            }
            if (g_strcmp0(interface, kScreenCastInterface) == 0
                && g_strcmp0(property, "version") == 0) {
                g_dbus_method_invocation_return_value(
                    invocation, g_variant_new("(v)", g_variant_new_uint32(5)));
                return true;
            }
            g_dbus_method_invocation_return_dbus_error(
                invocation, "org.freedesktop.DBus.Error.UnknownProperty",
                "unsimulated property");
            return true;
        }
        if (g_strcmp0(method_name, "GetAll") == 0) {
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&builder, "{sv}", "AvailableSourceTypes",
                                  g_variant_new_uint32(config.available_types));
            g_variant_builder_add(&builder, "{sv}", "version", g_variant_new_uint32(5));
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(a{sv})", &builder));
            return true;
        }
    }

    if (g_strcmp0(interface_name, kRequestInterface) == 0
        && g_strcmp0(method_name, "Close") == 0) {
        ++request_close_count;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        emitResponse(object_path, 1, nullptr);
        return true;
    }

    if (g_strcmp0(interface_name, kSessionInterface) == 0
        && g_strcmp0(method_name, "Close") == 0) {
        ++session_close_count;
        g_dbus_method_invocation_return_value(invocation, nullptr);
        return true;
    }

    if (g_strcmp0(interface_name, kScreenCastInterface) != 0) {
        g_dbus_method_invocation_return_dbus_error(
            invocation, "org.freedesktop.DBus.Error.UnknownInterface", "non-simulated interface");
        return true;
    }

    if (g_strcmp0(method_name, "CreateSession") == 0) {
        calls.push_back("CreateSession");
        GVariant* options = g_variant_get_child_value(parameters, 0);
        std::string token = optionString(options, "handle_token");
        g_variant_unref(options);
        if (token.empty()) {
            token = "create" + std::to_string(++session_counter_);
        }
        const std::string path = requestPathFor(sender, token);
        registerRequest(path);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", path.c_str()));
        if (config.create_response != 0) {
            emitResponse(path, config.create_response, nullptr);
            return true;
        }
        ++session_counter_;
        current_session_ = std::string(kPortalPath) + "/session/" + senderToken(sender)
            + "/oneintwo" + std::to_string(session_counter_);
        session_handle = current_session_;
        ++created_sessions;
        registerSession(current_session_);
        emitResponse(path, 0, vardictWithString("session_handle", current_session_.c_str()));
        return true;
    }

    if (g_strcmp0(method_name, "SelectSources") == 0) {
        calls.push_back("SelectSources");
        GVariant* session = g_variant_get_child_value(parameters, 0);
        GVariant* options = g_variant_get_child_value(parameters, 1);
        observed_types = optionUint32(options, "types", 0);
        observed_multiple = optionBool(options, "multiple", true);
        current_source_type_ = (observed_types & 2u) != 0 ? 2u : 1u;
        selected_session_ = g_variant_get_string(session, nullptr);
        std::string token = optionString(options, "handle_token");
        g_variant_unref(session);
        g_variant_unref(options);
        if (token.empty()) {
            token = "select" + std::to_string(++session_counter_);
        }
        const std::string path = requestPathFor(sender, token);
        registerRequest(path);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", path.c_str()));
        emitResponse(path, config.select_response, nullptr);
        return true;
    }

    if (g_strcmp0(method_name, "Start") == 0) {
        calls.push_back("Start");
        GVariant* session = g_variant_get_child_value(parameters, 0);
        selected_session_ = g_variant_get_string(session, nullptr);
        g_variant_unref(session);
        const char* parent = nullptr;
        g_variant_get_child(parameters, 1, "&s", &parent);
        observed_parent = parent != nullptr ? parent : "";
        GVariant* options = g_variant_get_child_value(parameters, 2);
        std::string token = optionString(options, "handle_token");
        g_variant_unref(options);
        if (token.empty()) {
            token = "start" + std::to_string(++session_counter_);
        }
        const std::string path = requestPathFor(sender, token);
        registerRequest(path);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", path.c_str()));
        if (config.defer_start_response) {
            start_pending_ = true;
            pending_start_path_ = path;
            return true;
        }
        const std::uint32_t node
            = current_source_type_ == 2 ? config.window_node : config.monitor_node;
        if (config.start_response == 0) {
            emitResponse(path, 0, startResults(config, node, current_source_type_));
        } else {
            emitResponse(path, config.start_response, nullptr);
        }
        return true;
    }

    if (g_strcmp0(method_name, "OpenPipeWireRemote") == 0) {
        calls.push_back("OpenPipeWireRemote");
        if (config.fail_open_pipewire) {
            g_dbus_method_invocation_return_dbus_error(
                invocation, "org.freedesktop.portal.Error.Failed", "simulated failure");
            return true;
        }
        int descriptors[2] = {-1, -1};
        if (pipe(descriptors) != 0) {
            g_dbus_method_invocation_return_dbus_error(
                invocation, "org.freedesktop.portal.Error.Failed", "pipe simulado");
            return true;
        }
        GUnixFDList* list = g_unix_fd_list_new();
        GError* append_error = nullptr;
        if (g_unix_fd_list_append(list, descriptors[0], &append_error) < 0) {
            if (append_error != nullptr) {
                g_error_free(append_error);
            }
            g_object_unref(list);
            close(descriptors[0]);
            close(descriptors[1]);
            g_dbus_method_invocation_return_dbus_error(
                invocation, "org.freedesktop.portal.Error.Failed", "descriptor simulado");
            return true;
        }
        g_dbus_method_invocation_return_value_with_unix_fd_list(
            invocation, g_variant_new("(h)", 0), list);
        g_object_unref(list);
        close(descriptors[0]);
        close(descriptors[1]);
        return true;
    }

    g_dbus_method_invocation_return_dbus_error(invocation,
                                               "org.freedesktop.DBus.Error.UnknownMethod",
                                               "method not simulated");
    return true;
}

} // namespace oneintwo::test
