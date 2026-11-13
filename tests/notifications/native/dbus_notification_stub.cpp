#include <dbus/dbus.h>

#include <cstdio>
#include <string>

/*
 * Servicio mínimo de org.freedesktop.Notifications para el recorrido nativo en
 * un entorno sin escritorio. Recibe Notify y CloseNotification por un bus de
 * sesión privado, imprime los campos y responde con un identificador.
 *
 * Se usa con dbus-run-session; no forma parte del producto.
 */

namespace {

std::string visible(const char* text) {
    std::string result = text != nullptr ? text : "";
    std::string escaped;
    escaped.reserve(result.size());
    for (const char character : result) {
        if (character == '\n') {
            escaped.append("\\n");
        } else {
            escaped.push_back(character);
        }
    }
    return escaped;
}

bool read_string(DBusMessageIter& iterator, const char*& value) {
    if (dbus_message_iter_get_arg_type(&iterator) != DBUS_TYPE_STRING) {
        return false;
    }
    dbus_message_iter_get_basic(&iterator, &value);
    dbus_message_iter_next(&iterator);
    return true;
}

void print_notify(DBusMessage* message) {
    DBusMessageIter iterator;
    if (!dbus_message_iter_init(message, &iterator)) {
        return;
    }
    const char* application = "";
    const char* icon = "";
    const char* summary = "";
    const char* body = "";
    dbus_uint32_t replaces = 0;
    dbus_int32_t timeout = 0;

    read_string(iterator, application);
    if (dbus_message_iter_get_arg_type(&iterator) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&iterator, &replaces);
        dbus_message_iter_next(&iterator);
    }
    read_string(iterator, icon);
    read_string(iterator, summary);
    read_string(iterator, body);
    // actions, hints y timeout se omiten en el resumen.
    if (dbus_message_iter_get_arg_type(&iterator) == DBUS_TYPE_ARRAY) {
        dbus_message_iter_next(&iterator);
        if (dbus_message_iter_get_arg_type(&iterator) == DBUS_TYPE_ARRAY) {
            dbus_message_iter_next(&iterator);
        }
        if (dbus_message_iter_get_arg_type(&iterator) == DBUS_TYPE_INT32) {
            dbus_message_iter_get_basic(&iterator, &timeout);
        }
    }

    std::printf("Notify app=%s replaces=%u timeout=%d summary=%s body=%s\n",
                application, replaces, timeout, visible(summary).c_str(),
                visible(body).c_str());
    std::fflush(stdout);
}

void print_close(DBusMessage* message) {
    DBusMessageIter iterator;
    if (!dbus_message_iter_init(message, &iterator)) {
        return;
    }
    dbus_uint32_t identifier = 0;
    if (dbus_message_iter_get_arg_type(&iterator) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(&iterator, &identifier);
    }
    std::printf("CloseNotification id=%u\n", identifier);
    std::fflush(stdout);
}

void reply_empty(DBusConnection* connection, DBusMessage* request) {
    DBusMessage* reply = dbus_message_new_method_return(request);
    if (reply != nullptr) {
        dbus_connection_send(connection, reply, nullptr);
        dbus_connection_flush(connection);
        dbus_message_unref(reply);
    }
}

} // namespace

int main() {
    DBusError error;
    dbus_error_init(&error);
    DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (connection == nullptr) {
        std::fprintf(stderr, "stub: sin bus de sesión: %s\n",
                     error.message != nullptr ? error.message : "desconocido");
        dbus_error_free(&error);
        return 1;
    }
    dbus_connection_set_exit_on_disconnect(connection, FALSE);

    if (dbus_bus_request_name(connection, "org.freedesktop.Notifications",
                              DBUS_NAME_FLAG_REPLACE_EXISTING,
                              &error) < 0) {
        std::fprintf(stderr, "stub: no se pudo tomar el nombre: %s\n",
                     error.message != nullptr ? error.message : "desconocido");
        dbus_error_free(&error);
        return 1;
    }
    std::printf("stub listo\n");
    std::fflush(stdout);

    for (;;) {
        if (!dbus_connection_read_write(connection, 1000)) {
            break;
        }
        DBusMessage* message = nullptr;
        while ((message = dbus_connection_pop_message(connection)) != nullptr) {
            if (dbus_message_is_method_call(message, "org.freedesktop.Notifications",
                                            "Notify")) {
                print_notify(message);
                DBusMessage* reply = dbus_message_new_method_return(message);
                if (reply != nullptr) {
                    DBusMessageIter iterator;
                    dbus_message_iter_init_append(reply, &iterator);
                    dbus_uint32_t identifier = 41;
                    dbus_message_iter_append_basic(&iterator, DBUS_TYPE_UINT32, &identifier);
                    dbus_connection_send(connection, reply, nullptr);
                    dbus_connection_flush(connection);
                    dbus_message_unref(reply);
                }
            } else if (dbus_message_is_method_call(message, "org.freedesktop.Notifications",
                                                   "CloseNotification")) {
                print_close(message);
                reply_empty(connection, message);
            }
            dbus_message_unref(message);
        }
    }

    dbus_connection_unref(connection);
    return 0;
}
