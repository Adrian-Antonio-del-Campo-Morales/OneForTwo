#pragma once

#include <gio/gio.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oneintwo::test {

struct MockPortalConfig {
    std::uint32_t available_types = 3; // 1 monitor, 2 ventana, 4 virtual
    guint32 create_response = 0;
    guint32 select_response = 0;
    guint32 start_response = 0;
    bool defer_start_response = false;
    bool include_source_type = true;
    std::uint32_t monitor_source_type = 1;
    std::uint32_t window_source_type = 2;
    std::uint32_t monitor_node = 101;
    std::uint32_t window_node = 102;
    std::uint32_t stream_width = 640;
    std::uint32_t stream_height = 360;
    bool fail_open_pipewire = false;
};

/*
 * Servicio org.freedesktop.portal.ScreenCast de prueba sobre un bus D-Bus
 * privado. Implementa solicitudes (Request), sesiones (Session), propiedades y
 * el descriptor PipeWire. Registra lo que observa para afirmar el contrato.
 */
class MockPortal {
public:
    MockPortal();
    ~MockPortal();

    bool start(); // inicia el bus privado
    bool ownName();
    void unownName();
    void stop();

    MockPortalConfig config;
    std::vector<std::string> calls;
    guint32 observed_types = 0;
    bool observed_multiple = true;
    std::string observed_parent;
    std::string session_handle;
    guint request_close_count = 0;
    guint session_close_count = 0;
    int created_sessions = 0;

    void resetObservations();

    bool startPending() const { return start_pending_; }
    void respondStart(guint32 code = 0);
    void emitSessionClosed();

    GDBusConnection* connection() const { return connection_; }

    // Trampolín del servicio simulado.
    bool handleMethod(const char* sender, const char* object_path, const char* interface_name,
                      const char* method_name, GVariant* parameters,
                      GDBusMethodInvocation* invocation);
    void emitResponse(const std::string& path, guint32 code, GVariant* results);

    static void onNameAcquired(GDBusConnection*, const gchar*, gpointer user);
    static void onNameLost(GDBusConnection*, const gchar*, gpointer user);

private:
    void registerRequest(const std::string& path);
    void registerSession(const std::string& path);
    bool pumpUntil(const std::function<bool()>& done, int timeout_ms) const;

    GDBusConnection* connection_ = nullptr;
    GPid daemon_pid_ = 0;
    guint portal_registration_ = 0;
    guint properties_registration_ = 0;
    guint owner_id_ = 0;
    bool owns_name_ = false;
    bool start_pending_ = false;
    std::string pending_start_path_;
    std::string selected_session_;
    std::string current_session_;
    std::uint32_t current_source_type_ = 1;
    unsigned session_counter_ = 0;
    std::vector<guint> dynamic_registrations_;
};

} // namespace oneintwo::test
