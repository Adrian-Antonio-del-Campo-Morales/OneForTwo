#include "dbus_notifications.hpp"

#include <dbus/dbus.h>

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unistd.h>

namespace oneintwo::linux_notifications {
namespace {

constexpr const char* service_name = "org.freedesktop.Notifications";
constexpr const char* object_path = "/org/freedesktop/Notifications";
constexpr const char* interface_name = "org.freedesktop.Notifications";
constexpr int call_timeout_ms = 5000;

bool has_session_bus() {
    const char* address = std::getenv("DBUS_SESSION_BUS_ADDRESS");
    if (address != nullptr && *address != '\0') {
        return true;
    }
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    if (runtime != nullptr && *runtime != '\0') {
        const std::string socket = std::string(runtime) + "/bus";
        if (access(socket.c_str(), F_OK) == 0) {
            return true;
        }
    }
    return false;
}

class SessionService final : public Service {
public:
    ~SessionService() override { disconnect(); }

    bool available() const override {
        return has_session_bus() || connected_.load(std::memory_order_relaxed);
    }

    PresentationResult present(const Presentation& presentation) override {
        PresentationResult result;
        if (!ensureConnected()) {
            result.detail = last_error_;
            result.unavailable = true;
            return result;
        }

        DBusMessage* message =
            dbus_message_new_method_call(service_name, object_path, interface_name, "Notify");
        if (message == nullptr) {
            result.detail = "sin memoria para la llamada Notify";
            return result;
        }

        DBusMessageIter iterator;
        dbus_message_iter_init_append(message, &iterator);
        append_string(iterator, presentation.application_name);
        dbus_uint32_t replaces_id = presentation.replaces_id;
        dbus_message_iter_append_basic(&iterator, DBUS_TYPE_UINT32, &replaces_id);
        append_string(iterator, ""); // icono: el producto no publica uno propio
        append_string(iterator, presentation.title);
        append_string(iterator, presentation.body);

        DBusMessageIter actions;
        dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "s", &actions);
        dbus_message_iter_close_container(&iterator, &actions);

        DBusMessageIter hints;
        dbus_message_iter_open_container(&iterator, DBUS_TYPE_ARRAY, "{sv}", &hints);
        append_markup_hint(&hints);
        dbus_message_iter_close_container(&iterator, &hints);

        dbus_int32_t timeout = presentation.timeout_ms;
        dbus_message_iter_append_basic(&iterator, DBUS_TYPE_INT32, &timeout);

        DBusError error;
        dbus_error_init(&error);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            connection_, message, call_timeout_ms, &error);
        dbus_message_unref(message);
        if (reply == nullptr) {
            result.detail = error.message != nullptr ? error.message : "error del bus";
            result.unavailable = is_unavailable_error(error.name);
            if (dbus_error_is_set(&error)) {
                dbus_error_free(&error);
            }
            disconnect();
            return result;
        }

        DBusMessageIter reply_iterator;
        if (dbus_message_iter_init(reply, &reply_iterator) != FALSE &&
            dbus_message_iter_get_arg_type(&reply_iterator) == DBUS_TYPE_UINT32) {
            dbus_uint32_t identifier = 0;
            dbus_message_iter_get_basic(&reply_iterator, &identifier);
            result.notification_id = identifier;
        }
        result.accepted = true;
        dbus_message_unref(reply);
        return result;
    }

    void withdraw(std::uint32_t notification_id) override {
        if (!ensureConnected()) {
            return;
        }
        DBusMessage* message = dbus_message_new_method_call(
            service_name, object_path, interface_name, "CloseNotification");
        if (message == nullptr) {
            return;
        }
        DBusMessageIter iterator;
        dbus_message_iter_init_append(message, &iterator);
        dbus_uint32_t identifier = notification_id;
        dbus_message_iter_append_basic(&iterator, DBUS_TYPE_UINT32, &identifier);
        dbus_connection_send(connection_, message, nullptr);
        dbus_connection_flush(connection_);
        dbus_message_unref(message);
    }

private:
    static void append_string(DBusMessageIter& iterator, const std::string& value) {
        const char* pointer = value.c_str();
        dbus_message_iter_append_basic(&iterator, DBUS_TYPE_STRING, &pointer);
    }

    static void append_markup_hint(DBusMessageIter* hints) {
        DBusMessageIter entry;
        DBusMessageIter variant;
        const char* key = "body-markup";
        dbus_message_iter_open_container(hints, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_bool_t markup = FALSE;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &markup);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(hints, &entry);
    }

    static bool is_unavailable_error(const char* name) {
        if (name == nullptr) {
            return false;
        }
        const std::string error_name(name);
        return error_name.find("ServiceUnknown") != std::string::npos ||
               error_name.find("NameHasNoOwner") != std::string::npos ||
               error_name.find("Disconnected") != std::string::npos;
    }

    bool ensureConnected() {
        if (connection_ != nullptr && dbus_connection_get_is_connected(connection_) != FALSE) {
            return true;
        }
        disconnect();
        DBusError error;
        dbus_error_init(&error);
        DBusConnection* connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
        if (connection == nullptr) {
            last_error_ = error.message != nullptr ? error.message : "sin bus de sesión";
            if (dbus_error_is_set(&error)) {
                dbus_error_free(&error);
            }
            return false;
        }
        dbus_connection_set_exit_on_disconnect(connection, FALSE);
        connection_ = connection;
        connected_.store(true, std::memory_order_relaxed);
        return true;
    }

    void disconnect() {
        connected_.store(false, std::memory_order_relaxed);
        if (connection_ == nullptr) {
            return;
        }
        dbus_connection_close(connection_);
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }

    DBusConnection* connection_ = nullptr;
    std::atomic<bool> connected_{false};
    std::string last_error_;
};

} // namespace

bool session_bus_configured() {
    return has_session_bus();
}

std::unique_ptr<Service> makeSessionService() {
    static std::once_flag threads_once;
    std::call_once(threads_once, [] { dbus_threads_init_default(); });
    return std::make_unique<SessionService>();
}

} // namespace oneintwo::linux_notifications
