#include "portal_screencast.hpp"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace oneintwo::wayland {
namespace {

constexpr char kPortalName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalPath[] = "/org/freedesktop/portal/desktop";
constexpr char kScreenCastInterface[] = "org.freedesktop.portal.ScreenCast";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kSessionInterface[] = "org.freedesktop.portal.Session";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

constexpr std::uint32_t kSourceMonitor = 1;
constexpr std::uint32_t kSourceWindow = 2;

// The composer's dialogue may take time; internal steps may not.
constexpr int kInteractiveTimeoutMs = 120000;
constexpr int kInternalTimeoutMs = 10000;

ScreenCastStatus mapRemoteError(const GError* error)
{
    if (error == nullptr) {
        return ScreenCastStatus::error;
    }
    if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)
        || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER)
        || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)
        || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT)
        || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE)) {
        return ScreenCastStatus::unavailable;
    }

    gchar* remote = g_dbus_error_get_remote_error(error);
    const ScreenCastStatus mapped = [&] {
        if (remote == nullptr) {
            return ScreenCastStatus::error;
        }
        if (g_strcmp0(remote, "org.freedesktop.portal.Error.Cancelled") == 0) {
            return ScreenCastStatus::cancelled;
        }
        if (g_strcmp0(remote, "org.freedesktop.portal.Error.NotAllowed") == 0
            || g_strcmp0(remote, "org.freedesktop.portal.Error.PermissionDenied") == 0) {
            return ScreenCastStatus::denied;
        }
        if (g_strcmp0(remote, "org.freedesktop.portal.Error.NotFound") == 0
            || g_strcmp0(remote, "org.freedesktop.portal.Error.NotSupported") == 0) {
            return ScreenCastStatus::unavailable;
        }
        return ScreenCastStatus::error;
    }();
    g_free(remote);
    return mapped;
}

struct CallData {
    std::function<void(GVariant*, GError*)> complete;
};

void onCallFinished(GObject* source, GAsyncResult* result, gpointer user)
{
    auto* data = static_cast<CallData*>(user);
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
    data->complete(reply, error);
    if (reply != nullptr) {
        g_variant_unref(reply);
    }
    if (error != nullptr) {
        g_error_free(error);
    }
    delete data;
}

struct FdCallData {
    std::function<void(GVariant*, GUnixFDList*, GError*)> complete;
};

void onFdCallFinished(GObject* source, GAsyncResult* result, gpointer user)
{
    auto* data = static_cast<FdCallData*>(user);
    GError* error = nullptr;
    GUnixFDList* fd_list = nullptr;
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_finish(
        G_DBUS_CONNECTION(source), &fd_list, result, &error);
    data->complete(reply, fd_list, error);
    if (reply != nullptr) {
        g_variant_unref(reply);
    }
    if (fd_list != nullptr) {
        g_object_unref(fd_list);
    }
    if (error != nullptr) {
        g_error_free(error);
    }
    delete data;
}

bool typeMatchesKind(std::uint32_t source_type, VisualSourceKind kind)
{
    return source_type == (kind == VisualSourceKind::monitor ? kSourceMonitor : kSourceWindow);
}

} // namespace

class PortalScreencastClientImpl final : public PortalScreencastClient {
public:
    PortalScreencastClientImpl()
    {
        context_ = g_main_context_new();
        std::promise<void> ready;
        auto ready_signal = ready.get_future();
        thread_ = std::thread([this, &ready] {
            worker_thread_ = std::this_thread::get_id();
            g_main_context_push_thread_default(context_);
            loop_ = g_main_loop_new(context_, FALSE);
            ready.set_value();
            g_main_loop_run(loop_);
            g_main_loop_unref(loop_);
            loop_ = nullptr;
            g_main_context_pop_thread_default(context_);
            worker_thread_ = std::thread::id{};
        });
        ready_signal.wait();
    }

    ~PortalScreencastClientImpl() override
    {
        shutting_down_.store(true);
        invokeSync([this] { shutdownOnWorker(); });
        if (loop_ != nullptr) {
            g_main_loop_quit(loop_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        g_main_context_unref(context_);
        context_ = nullptr;
    }

    void probe(CapabilitiesResult complete) override
    {
        post([this, complete = std::move(complete)] {
            if (shutting_down_.load()) {
                return;
            }
            probeOnWorker(std::move(complete));
        });
    }

    void open(VisualSourceKind kind, std::string parent_window, SessionClosed on_closed,
              SessionResult complete) override
    {
        post([this, kind, parent_window = std::move(parent_window),
              on_closed = std::move(on_closed), complete = std::move(complete)] {
            if (shutting_down_.load()) {
                return;
            }
            openOnWorker(kind, parent_window, std::move(on_closed), std::move(complete));
        });
    }

    void cancel() override
    {
        post([this] {
            if (shutting_down_.load() || finished_) {
                return;
            }
            closePendingRequest();
            finish(ScreenCastStatus::cancelled, {});
        });
    }

    void close() override
    {
        post([this] {
            if (shutting_down_.load()) {
                return;
            }
            if (!finished_) {
                closePendingRequest();
                finish(ScreenCastStatus::cancelled, {});
            }
            closeSessionOnWorker();
        });
    }

    int duplicatePipeWireFd() const override
    {
        const int fd = pipewire_fd_.load();
        if (fd < 0) {
            return -1;
        }
        return ::dup(fd);
    }

private:
    enum class Stage {
        idle,
        creating_session,
        selecting_sources,
        starting,
        opening_remote,
    };

    void post(std::function<void()> task)
    {
        auto* holder = new std::function<void()>(std::move(task));
        g_main_context_invoke(context_, [](gpointer data) -> gboolean {
            auto* callable = static_cast<std::function<void()>*>(data);
            (*callable)();
            delete callable;
            return G_SOURCE_REMOVE;
        }, holder);
    }

    void invokeSync(const std::function<void()>& task)
    {
        if (std::this_thread::get_id() == worker_thread_) {
            task();
            return;
        }
        std::promise<void> done;
        auto finished = done.get_future();
        post([&done, &task] {
            task();
            done.set_value();
        });
        finished.wait();
    }

    GDBusConnection* ensureConnection()
    {
        if (connection_ != nullptr) {
            return connection_;
        }
        GError* error = nullptr;
        connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (connection_ == nullptr) {
            if (error != nullptr) {
                g_error_free(error);
            }
            return nullptr;
        }
        return connection_;
    }

    static void onVoidCall(GObject* source, GAsyncResult* result, gpointer)
    {
        GError* error = nullptr;
        GVariant* reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);
        if (reply != nullptr) {
            g_variant_unref(reply);
        }
        if (error != nullptr) {
            g_error_free(error);
        }
    }

    void fireAndForget(const std::string& path, const char* interface_name, const char* method)
    {
        if (connection_ == nullptr || path.empty()) {
            return;
        }
        g_dbus_connection_call(connection_, kPortalName, path.c_str(), interface_name, method,
                               nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr,
                               &PortalScreencastClientImpl::onVoidCall, nullptr);
    }

    void callPortal(const char* interface_name, const char* method, GVariant* parameters,
                    const char* reply_type, int timeout_ms,
                    std::function<void(GVariant*, GError*)> complete)
    {
        auto* data = new CallData{std::move(complete)};
        g_dbus_connection_call(connection_, kPortalName, kPortalPath, interface_name, method,
                               parameters, reply_type == nullptr ? nullptr
                                                                : G_VARIANT_TYPE(reply_type),
                               G_DBUS_CALL_FLAGS_NONE, timeout_ms, nullptr,
                               &PortalScreencastClientImpl::onCallFinished, data);
    }

    static void onCallFinished(GObject* source, GAsyncResult* result, gpointer user)
    {
        ::oneintwo::wayland::onCallFinished(source, result, user);
    }

    static void onFdCallFinished(GObject* source, GAsyncResult* result, gpointer user)
    {
        ::oneintwo::wayland::onFdCallFinished(source, result, user);
    }

    std::string uniqueSenderToken() const
    {
        const gchar* unique = g_dbus_connection_get_unique_name(connection_);
        if (unique == nullptr) {
            return "unknown";
        }
        std::string token = unique[0] == ':' ? unique + 1 : unique;
        for (char& character : token) {
            if (character == '.') {
                character = '_';
            }
        }
        return token;
    }

    std::string nextHandleToken()
    {
        return "oneintwo" + std::to_string(++token_counter_);
    }

    std::string requestPathFor(const std::string& token) const
    {
        return std::string(kPortalPath) + "/request/" + uniqueSenderToken() + "/" + token;
    }

    void subscribeRequest(const std::string& path)
    {
        if (request_subscription_ != 0) {
            g_dbus_connection_signal_unsubscribe(connection_, request_subscription_);
        }
        request_path_ = path;
        request_subscription_ = g_dbus_connection_signal_subscribe(
            connection_, kPortalName, kRequestInterface, "Response", request_path_.c_str(), nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE, &PortalScreencastClientImpl::onRequestResponse, this,
            nullptr);
    }

    void unsubscribeRequest()
    {
        if (request_subscription_ != 0 && connection_ != nullptr) {
            g_dbus_connection_signal_unsubscribe(connection_, request_subscription_);
        }
        request_subscription_ = 0;
        request_path_.clear();
    }

    static void onRequestResponse(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                  const gchar*, GVariant* parameters, gpointer user)
    {
        auto* self = static_cast<PortalScreencastClientImpl*>(user);
        guint32 response = 2;
        GVariant* results = nullptr;
        g_variant_get(parameters, "(u@a{sv})", &response, &results);
        self->handleRequestResponse(response, results);
        g_variant_unref(results);
    }

    void handleRequestResponse(guint32 response, GVariant* results)
    {
        if (finished_) {
            return;
        }
        unsubscribeRequest();
        cancelTimeout();

        if (response == 1) {
            finish(ScreenCastStatus::cancelled, {});
            return;
        }
        if (response != 0) {
            finish(ScreenCastStatus::denied, {});
            return;
        }

        switch (stage_) {
        case Stage::creating_session: {
            GVariant* handle = g_variant_lookup_value(results, "session_handle",
                                                      G_VARIANT_TYPE_STRING);
            if (handle == nullptr) {
                finish(ScreenCastStatus::error, {});
                return;
            }
            session_handle_ = g_variant_get_string(handle, nullptr);
            g_variant_unref(handle);
            selectSources();
            break;
        }
        case Stage::selecting_sources:
            startSession();
            break;
        case Stage::starting:
            handleStreams(results);
            break;
        case Stage::opening_remote:
        case Stage::idle:
            finish(ScreenCastStatus::error, {});
            break;
        }
    }

    void cancelTimeout()
    {
        if (timeout_source_ != 0) {
            g_source_remove(timeout_source_);
            timeout_source_ = 0;
        }
    }

    void armTimeout(int timeout_ms)
    {
        cancelTimeout();
        const guint seconds = static_cast<guint>((timeout_ms + 999) / 1000);
        timeout_source_ = g_timeout_add_seconds(seconds, &PortalScreencastClientImpl::onTimeout,
                                                this);
    }

    static gboolean onTimeout(gpointer user)
    {
        auto* self = static_cast<PortalScreencastClientImpl*>(user);
        self->timeout_source_ = 0;
        if (!self->finished_) {
            self->closePendingRequest();
            self->finish(ScreenCastStatus::error, {});
        }
        return G_SOURCE_REMOVE;
    }

    void closePendingRequest()
    {
        if (!request_path_.empty()) {
            const std::string path = request_path_;
            unsubscribeRequest();
            fireAndForget(path, kRequestInterface, "Close");
        }
    }

    void closeSessionOnWorker()
    {
        if (!session_handle_.empty()) {
            const std::string handle = session_handle_;
            session_handle_.clear();
            fireAndForget(handle, kSessionInterface, "Close");
        }
        if (session_subscription_ != 0 && connection_ != nullptr) {
            g_dbus_connection_signal_unsubscribe(connection_, session_subscription_);
            session_subscription_ = 0;
        }
        closePipeWireFd();
    }

    void closePipeWireFd()
    {
        const int fd = pipewire_fd_.exchange(-1);
        if (fd >= 0) {
            ::close(fd);
        }
    }

    void finish(ScreenCastStatus status, ScreenCastSource source)
    {
        if (finished_) {
            return;
        }
        finished_ = true;
        stage_ = Stage::idle;
        cancelTimeout();
        unsubscribeRequest();
        auto complete = std::move(complete_);
        complete_ = nullptr;
        if (status != ScreenCastStatus::ready) {
            closed_ = nullptr;
            closeSessionOnWorker();
        }
        if (complete) {
            complete(status, std::move(source));
        }
    }

    void probeOnWorker(CapabilitiesResult complete)
    {
        if (complete == nullptr) {
            return;
        }
        if (ensureConnection() == nullptr) {
            complete({}, false);
            return;
        }
        callPortal(kPropertiesInterface, "Get",
                   g_variant_new("(ss)", kScreenCastInterface, "AvailableSourceTypes"), "(v)",
                   kInternalTimeoutMs, [this, complete = std::move(complete)](GVariant* reply,
                                                                             GError* error) {
                       if (reply == nullptr) {
                           if (error != nullptr
                               && g_error_matches(error, G_DBUS_ERROR,
                                                  G_DBUS_ERROR_UNKNOWN_PROPERTY)) {
                               probeLegacyVersion(std::move(complete));
                               return;
                           }
                           (void)error;
                           complete({}, false);
                           return;
                       }
                       GVariant* value = nullptr;
                       g_variant_get(reply, "(v)", &value);
                       ScreenCastSupport support;
                       if (value != nullptr && g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
                           const std::uint32_t types = g_variant_get_uint32(value);
                           support.monitor = (types & kSourceMonitor) != 0;
                           support.window = (types & kSourceWindow) != 0;
                       }
                       if (value != nullptr) {
                           g_variant_unref(value);
                       }
                       complete(support, true);
                   });
    }

    void probeLegacyVersion(CapabilitiesResult complete)
    {
        callPortal(kPropertiesInterface, "Get",
                   g_variant_new("(ss)", kScreenCastInterface, "version"), "(v)",
                   kInternalTimeoutMs, [complete = std::move(complete)](GVariant* reply,
                                                                        GError* error) {
                       if (reply == nullptr) {
                           complete({}, false);
                           (void)error;
                           return;
                       }
                       // Portals prior to AvailableSourceTypes only document monitors.
                       ScreenCastSupport support;
                       support.monitor = true;
                       complete(support, true);
                   });
    }

    void openOnWorker(VisualSourceKind kind, const std::string& parent_window,
                      SessionClosed on_closed, SessionResult complete)
    {
        parent_window_ = parent_window;
        if (ensureConnection() == nullptr) {
            if (complete) {
                complete(ScreenCastStatus::unavailable, {});
            }
            return;
        }
        if (!finished_) {
            closePendingRequest();
            finish(ScreenCastStatus::cancelled, {});
        }
        closeSessionOnWorker();

        finished_ = false;
        kind_ = kind;
        complete_ = std::move(complete);
        closed_ = std::move(on_closed);
        stage_ = Stage::creating_session;

        const std::string token = nextHandleToken();
        subscribeRequest(requestPathFor(token));

        GVariantBuilder options;
        g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&options, "{sv}", "handle_token",
                              g_variant_new_string(token.c_str()));
        g_variant_builder_add(&options, "{sv}", "session_handle_token",
                              g_variant_new_string(token.c_str()));
        armTimeout(kInternalTimeoutMs);
        callPortal(kScreenCastInterface, "CreateSession", g_variant_new("(a{sv})", &options), "(o)",
                   kInternalTimeoutMs, [this](GVariant* reply, GError* error) {
                       if (reply == nullptr) {
                           if (!finished_) {
                               finish(mapRemoteError(error), {});
                           }
                       }
                   });
    }

    void selectSources()
    {
        stage_ = Stage::selecting_sources;
        const std::string token = nextHandleToken();
        subscribeRequest(requestPathFor(token));
        armTimeout(kInternalTimeoutMs);

        GVariantBuilder options;
        g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&options, "{sv}", "handle_token",
                              g_variant_new_string(token.c_str()));
        g_variant_builder_add(&options, "{sv}", "types",
                              g_variant_new_uint32(kind_ == VisualSourceKind::monitor
                                                       ? kSourceMonitor
                                                       : kSourceWindow));
        g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(FALSE));
        // Hidden Cursor: the shared image does not incorporate the local pointer.
        g_variant_builder_add(&options, "{sv}", "cursor_mode", g_variant_new_uint32(1));

        callPortal(kScreenCastInterface, "SelectSources",
                   g_variant_new("(oa{sv})", session_handle_.c_str(), &options), "(o)",
                   kInternalTimeoutMs, [this](GVariant* reply, GError* error) {
                       if (reply == nullptr && !finished_) {
                           finish(mapRemoteError(error), {});
                       }
                   });
    }

    void startSession()
    {
        stage_ = Stage::starting;
        const std::string token = nextHandleToken();
        subscribeRequest(requestPathFor(token));
        armTimeout(kInteractiveTimeoutMs);

        GVariantBuilder options;
        g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&options, "{sv}", "handle_token",
                              g_variant_new_string(token.c_str()));

        callPortal(kScreenCastInterface, "Start",
                   g_variant_new("(osa{sv})", session_handle_.c_str(), parent_window_.c_str(),
                                 &options),
                   "(o)", kInteractiveTimeoutMs, [this](GVariant* reply, GError* error) {
                       if (reply == nullptr && !finished_) {
                           finish(mapRemoteError(error), {});
                       }
                   });
    }

    void handleStreams(GVariant* results)
    {
        GVariant* streams = g_variant_lookup_value(results, "streams",
                                                   G_VARIANT_TYPE("a(ua{sv})"));
        if (streams == nullptr) {
            finish(ScreenCastStatus::unavailable, {});
            return;
        }
        std::vector<ScreenCastSource> sources;
        GVariantIter iterator;
        g_variant_iter_init(&iterator, streams);
        guint32 node_id = 0;
        GVariant* properties = nullptr;
        while (g_variant_iter_next(&iterator, "(u@a{sv})", &node_id, &properties)) {
            ScreenCastSource source;
            source.node_id = node_id;
            GVariant* size = g_variant_lookup_value(properties, "size", G_VARIANT_TYPE("(ii)"));
            if (size != nullptr) {
                gint32 width = 0;
                gint32 height = 0;
                g_variant_get(size, "(ii)", &width, &height);
                g_variant_unref(size);
                if (width > 0 && height > 0) {
                    source.width = static_cast<std::uint32_t>(width);
                    source.height = static_cast<std::uint32_t>(height);
                }
            }
            GVariant* type = g_variant_lookup_value(properties, "source_type",
                                                    G_VARIANT_TYPE_UINT32);
            if (type != nullptr) {
                source.has_source_type = true;
                source.source_type = g_variant_get_uint32(type);
                g_variant_unref(type);
            }
            GVariant* id = g_variant_lookup_value(properties, "id", G_VARIANT_TYPE_STRING);
            if (id != nullptr) {
                source.stream_id = g_variant_get_string(id, nullptr);
                g_variant_unref(id);
            }
            GVariant* mapping = g_variant_lookup_value(properties, "mapping_id",
                                                       G_VARIANT_TYPE_STRING);
            if (mapping != nullptr) {
                source.mapping_id = g_variant_get_string(mapping, nullptr);
                g_variant_unref(mapping);
            }
            sources.push_back(std::move(source));
            g_variant_unref(properties);
        }
        g_variant_unref(streams);

        if (sources.size() != 1) {
            finish(ScreenCastStatus::unavailable, {});
            return;
        }
        if (sources.front().node_id == 0 || sources.front().width == 0
            || sources.front().height == 0) {
            finish(ScreenCastStatus::error, {});
            return;
        }
        if (!sources.front().has_source_type
            || !typeMatchesKind(sources.front().source_type, kind_)) {
            // Without effective rate attestation, isolation cannot be guaranteed.
            finish(ScreenCastStatus::unavailable, {});
            return;
        }

        pending_source_ = sources.front();
        openPipeWireRemote();
    }

    void openPipeWireRemote()
    {
        stage_ = Stage::opening_remote;
        armTimeout(kInternalTimeoutMs);

        GVariantBuilder options;
        g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

        auto* data = new FdCallData{
            [this](GVariant* reply, GUnixFDList* fd_list, GError* error) {
                cancelTimeout();
                if (reply == nullptr || fd_list == nullptr) {
                    if (!finished_) {
                        finish(mapRemoteError(error), {});
                    }
                    return;
                }
                gint32 handle = 0;
                g_variant_get(reply, "(h)", &handle);
                GError* fd_error = nullptr;
                const int fd = g_unix_fd_list_get(fd_list, handle, &fd_error);
                if (fd < 0) {
                    if (fd_error != nullptr) {
                        g_error_free(fd_error);
                    }
                    if (!finished_) {
                        finish(ScreenCastStatus::error, {});
                    }
                    return;
                }
                pipewire_fd_.store(fd);
                if (!finished_) {
                    subscribeSessionClosed();
                    finish(ScreenCastStatus::ready, pending_source_);
                }
            }};
        g_dbus_connection_call_with_unix_fd_list(
            connection_, kPortalName, kPortalPath, kScreenCastInterface, "OpenPipeWireRemote",
            g_variant_new("(oa{sv})", session_handle_.c_str(), &options), G_VARIANT_TYPE("(h)"),
            G_DBUS_CALL_FLAGS_NONE, kInternalTimeoutMs, nullptr, nullptr, &onFdCallFinished, data);
    }

    void subscribeSessionClosed()
    {
        if (session_subscription_ != 0) {
            g_dbus_connection_signal_unsubscribe(connection_, session_subscription_);
        }
        session_subscription_ = g_dbus_connection_signal_subscribe(
            connection_, kPortalName, kSessionInterface, "Closed", session_handle_.c_str(),
            nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
            &PortalScreencastClientImpl::onSessionClosedSignal, this, nullptr);
    }

    static void onSessionClosedSignal(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                      const gchar*, GVariant*, gpointer user)
    {
        auto* self = static_cast<PortalScreencastClientImpl*>(user);
        self->handleSessionClosed();
    }

    void handleSessionClosed()
    {
        if (session_subscription_ != 0 && connection_ != nullptr) {
            g_dbus_connection_signal_unsubscribe(connection_, session_subscription_);
            session_subscription_ = 0;
        }
        session_handle_.clear();
        closePipeWireFd();
        auto closed = std::move(closed_);
        closed_ = nullptr;
        if (closed) {
            closed();
        }
    }

    void shutdownOnWorker()
    {
        if (!finished_) {
            closePendingRequest();
            finished_ = true;
            stage_ = Stage::idle;
            cancelTimeout();
            complete_ = nullptr;
        }
        closeSessionOnWorker();
        if (connection_ != nullptr) {
            g_object_unref(connection_);
            connection_ = nullptr;
        }
    }

    GMainContext* context_ = nullptr;
    GMainLoop* loop_ = nullptr;
    std::thread thread_;
    std::thread::id worker_thread_;
    GDBusConnection* connection_ = nullptr;

    Stage stage_ = Stage::idle;
    bool finished_ = true;
    std::atomic<bool> shutting_down_{false};
    VisualSourceKind kind_ = VisualSourceKind::monitor;
    SessionResult complete_;
    SessionClosed closed_;
    std::string session_handle_;
    std::string parent_window_;
    std::string request_path_;
    guint request_subscription_ = 0;
    guint session_subscription_ = 0;
    guint timeout_source_ = 0;
    unsigned token_counter_ = 0;
    ScreenCastSource pending_source_;
    std::atomic<int> pipewire_fd_{-1};
};

std::unique_ptr<PortalScreencastClient> makePortalScreencastClient()
{
    return std::make_unique<PortalScreencastClientImpl>();
}

} // namespace oneintwo::wayland
