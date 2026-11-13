#include "application.hpp"

#include "coordinator.hpp"
#include "main_window.hpp"
#include "qt_runtime.hpp"

#include "../features/notifications.hpp"
#include "../net/session_link.hpp"
#include "../net/tls_transport.hpp"
#include "../storage/local_store.hpp"

#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <QtGlobal>

#include <utility>

namespace oneintwo {
namespace {

// El coordinador se construye antes de que termine el handshake TLS. Este
// proxy conserva perfil/permisos y los reenvía cuando aparece la sesión.
class SessionPortProxy final : public SessionPort {
public:
    void setDelegate(SessionPort* delegate) {
        delegate_ = delegate;
        if (delegate_ != nullptr) {
            delegate_->applyProfileLimits(limits_);
            delegate_->setGrantedFeatures(granted_send_, granted_receive_);
        }
    }

    SendResult send(FeatureMessage message) override {
        return delegate_ ? delegate_->send(std::move(message)) : SendResult::disconnected;
    }
    void applyProfileLimits(FeatureSet limits) override {
        limits_ = limits;
        if (delegate_) {
            delegate_->applyProfileLimits(limits);
        }
    }
    void setGrantedFeatures(FeatureSet send, FeatureSet receive) override {
        granted_send_ = send;
        granted_receive_ = receive;
        if (delegate_) {
            delegate_->setGrantedFeatures(send, receive);
        }
    }
    FeatureSet effectiveSendFeatures() const override {
        return delegate_ ? delegate_->effectiveSendFeatures() : FeatureSet{};
    }
    FeatureSet effectiveReceiveFeatures() const override {
        return delegate_ ? delegate_->effectiveReceiveFeatures() : FeatureSet{};
    }
    SessionGeneration generation() const override {
        return delegate_ ? delegate_->generation() : 0;
    }
    bool established() const override { return delegate_ && delegate_->established(); }

private:
    SessionPort* delegate_ = nullptr;
    FeatureSet limits_{true, true, true, true, true, true, true, true, true};
    FeatureSet granted_send_{};
    FeatureSet granted_receive_{};
};

class AuthenticatedSessionPort final : public SessionPort {
public:
    explicit AuthenticatedSessionPort(AuthenticatedSession& session) : session_(session) {}

    SendResult send(FeatureMessage message) override { return session_.send(std::move(message)); }
    void applyProfileLimits(FeatureSet limits) override { session_.applyProfileLimits(limits); }
    void setGrantedFeatures(FeatureSet send, FeatureSet receive) override {
        session_.setGrantedFeatures(send, receive);
    }
    FeatureSet effectiveSendFeatures() const override {
        return session_.effectiveSendFeatures();
    }
    FeatureSet effectiveReceiveFeatures() const override {
        return session_.effectiveReceiveFeatures();
    }
    SessionGeneration generation() const override { return session_.generation(); }
    bool established() const override {
        return session_.phase() == SessionLinkPhase::established;
    }

private:
    AuthenticatedSession& session_;
};

class NotificationModulePort final : public NotificationPort {
public:
    explicit NotificationModulePort(SharedNotifications& module) : module_(module) {}

    void setAuthorization(SessionGeneration generation, FeatureSet send,
                          FeatureSet receive) override {
        module_.setAuthorization(generation, send, receive);
    }
    void setAllowedApplications(const std::set<std::string>& app_ids) override {
        module_.setAllowedApplications(app_ids);
    }
    void tick() override { module_.tick(); }
    void sendManual(SessionGeneration generation, const NotificationContent& content) override {
        module_.sendManual(generation, content);
    }
    void receiveNotice(const RemoteNotification& notice) override {
        module_.receiveNotice(notice);
    }

private:
    SharedNotifications& module_;
};

class InactiveNotificationPort final : public NotificationPort {
public:
    void setAuthorization(SessionGeneration, FeatureSet, FeatureSet) override {}
    void setAllowedApplications(const std::set<std::string>&) override {}
    void tick() override {}
    void sendManual(SessionGeneration, const NotificationContent&) override {}
    void receiveNotice(const RemoteNotification&) override {}
};

class UnavailableCapture final : public DesktopVisualCapture {
public:
    VisualCaptureCapabilities visualCapabilities() const override { return {}; }
    void selectSource(VisualShareRef offer, VisualSourceKind,
                      std::function<void(VisualShareRef, DesktopResult)> selected) override {
        if (selected) {
            selected(offer, DesktopResult::unavailable);
        }
    }
    void startCapture(VisualShareRef, VisualCaptureCallbacks) override {}
    void stopCapture(VisualShareRef) override {}
};

std::string defaultStoragePath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        return "oneintwo-preferences.txt";
    }
    return (base + QStringLiteral("/preferences.txt")).toStdString();
}

} // namespace

struct Application::Impl {
    explicit Impl(ApplicationOptions options_in) : options(std::move(options_in)) {
        if (options.storage_path.empty()) {
            options.storage_path = defaultStoragePath();
        }
        store = std::make_unique<LocalStore>(options.storage_path);
        runtime = makeQtModuleRuntime();
    }

    ~Impl() { shutdown(); }

    void start() {
        if (window) {
            window->show();
            window->raise();
            return;
        }

        window = std::make_unique<MainWindow>();
        window->setPeerName(options.peer_name);

        PlatformContext platform{runtime, window.get(), DesktopEvents{}};
        platform.events.locked = [this](bool locked) {
            if (coordinator) {
                coordinator->setLocked(locked);
            }
        };
        platform.events.capabilities_changed = [this] {
            if (window && capture && !capture->visualCapabilities().monitor &&
                !capture->visualCapabilities().window) {
                window->setError("La captura de escritorio no está disponible");
            }
        };

#if defined(Q_OS_WIN)
        capture = makeWindowsVisualCapture(platform);
        desktop_notifications = makeWindowsNotifications(platform);
#elif defined(Q_OS_LINUX)
        capture = makeWaylandVisualCapture(platform);
        desktop_notifications = makeLinuxNotifications(platform);
#endif
        if (!capture) {
            capture = std::make_unique<UnavailableCapture>();
            window->setError("La captura de escritorio no está disponible");
        }
        if (desktop_notifications) {
            NotificationDependencies dependencies{
                runtime,
                *desktop_notifications,
                [this](FeatureMessage message) {
                    return session_proxy.send(std::move(message));
                },
                [this] { return options.peer_name; },
                [this](std::string error) { window->setError(std::move(error)); },
            };
            notifications = std::make_unique<SharedNotifications>(std::move(dependencies));
            notifications_port = std::make_unique<NotificationModulePort>(*notifications);
        } else {
            notifications_port = std::make_unique<InactiveNotificationPort>();
        }

        buildSession();
        buildCoordinator();
        wirePreferences();
        wireEvents();
        if (session) {
            session->start();
        }
        window->show();
        window->raise();
    }

    void buildSession() {
        if (options.create_transport) {
            transport = options.create_transport();
            if (!transport) {
                window->setError("No se pudo crear el transporte de sesión");
                return;
            }
            if (options.peer_fingerprint.empty()) {
                window->setError("Falta la huella autenticada del par");
                return;
            }
            createSession(*transport, options.peer_fingerprint);
            return;
        }
        if (options.tls.certificate_pem.empty() && options.tls.private_key_pem.empty()) {
            return;
        }
        TlsTransportConfig config = options.tls;
        if (config.peer_fingerprint.empty()) {
            config.peer_fingerprint = options.peer_fingerprint;
        }
        if (config.peer_fingerprint.empty()) {
            window->setError("Falta la huella autenticada del par");
            return;
        }
        const std::string peer_pin = config.peer_fingerprint;
        tls_transport = std::make_unique<QtTlsTransport>(std::move(config));
        tls_transport->setReadyHandler([this, peer_pin] {
            if (session) {
                return;
            }
            createSession(*tls_transport, peer_pin);
            if (session) {
                session->start();
            }
        });
        tls_transport->setFailureHandler([this](std::string detail) {
            if (window) {
                window->setError(std::move(detail));
            }
        });
        std::string error;
        if (!tls_transport->start(&error)) {
            window->setError(error.empty() ? "No se pudo iniciar el transporte TLS" : error);
            tls_transport.reset();
        }
    }

    void createSession(SessionTransport& active_transport, const std::string& peer_pin) {
        SessionLinkOptions link;
        link.local_device_id = options.local_device_id;
        link.peer_device_id = options.peer_device_id;
        link.peer_fingerprint = peer_pin;
        SessionLinkHandlers handlers;
        handlers.established = [this](SessionGeneration local, SessionGeneration) {
            if (coordinator) {
                coordinator->sessionEstablished(local);
                window->setStatus("Sesión establecida");
            }
        };
        handlers.failed = [this](SessionLinkError, std::string detail) {
            if (window) {
                window->setError(detail);
            }
            if (coordinator) {
                coordinator->sessionClosed();
            }
        };
        handlers.closed = [this] {
            if (coordinator) {
                coordinator->sessionClosed();
            }
        };
        handlers.message = [this](FeatureMessage message) {
            if (coordinator) {
                coordinator->receive(std::move(message));
            }
        };
        session = std::make_unique<AuthenticatedSession>(std::move(link), active_transport,
                                                         runtime, std::move(handlers));
        session_adapter = std::make_unique<AuthenticatedSessionPort>(*session);
        session_proxy.setDelegate(session_adapter.get());
    }

    void buildCoordinator() {
        if (notifications_port == nullptr) {
            notifications_port = std::make_unique<InactiveNotificationPort>();
        }
        CoordinatorDependencies dependencies{
            runtime,
            session_proxy,
            *capture,
            *notifications_port,
            *window,
            [](ModuleRuntime module_runtime, std::function<void(VisualShareRef)> closed) {
                return makeVisualViewer(std::move(module_runtime), std::move(closed));
            },
            options.peer_name,
            [this](ProfileId profile) {
                if (store) {
                    store->saveProfile(options.peer_device_id, profile);
                }
            },
            [this](std::set<std::string> app_ids) {
                if (store) {
                    store->saveAllowedApplications(app_ids);
                }
            },
        };
        coordinator = std::make_unique<Coordinator>(std::move(dependencies));
        if (coordinator->profile() != ProfileId::usual) {
            window->setProfileSelection(coordinator->profile());
        }
    }

    void wirePreferences() {
        const ProfileId profile = store->loadProfile(options.peer_device_id);
        coordinator->setProfile(profile);
        window->setProfileSelection(profile);
        const std::set<std::string> apps = store->loadAllowedApplications();
        coordinator->setAllowedApplications(apps);
        window->setAllowedApplications(apps);
    }

    void wireEvents() {
        timer = new QTimer(window.get());
        timer->setInterval(static_cast<int>(feature_tick_interval.count()));
        QObject::connect(timer, &QTimer::timeout, window.get(), [this] {
            if (coordinator) {
                coordinator->tick();
            }
        });
        timer->start();
    }

    void shutdown() {
        if (timer != nullptr) {
            timer->stop();
            timer = nullptr;
        }
        if (session) {
            session->close();
        }
        coordinator.reset();
        notifications_port.reset();
        session_proxy.setDelegate(nullptr);
        session_adapter.reset();
        session.reset();
        transport.reset();
        if (tls_transport) {
            tls_transport->close();
            tls_transport.reset();
        }
        notifications.reset();
        desktop_notifications.reset();
        capture.reset();
        window.reset();
    }

    ApplicationOptions options;
    ModuleRuntime runtime;
    std::unique_ptr<LocalStore> store;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<DesktopVisualCapture> capture;
    std::unique_ptr<DesktopNotifications> desktop_notifications;
    std::unique_ptr<SharedNotifications> notifications;
    std::unique_ptr<NotificationPort> notifications_port;
    std::unique_ptr<SessionTransport> transport;
    std::unique_ptr<QtTlsTransport> tls_transport;
    std::unique_ptr<AuthenticatedSession> session;
    std::unique_ptr<AuthenticatedSessionPort> session_adapter;
    SessionPortProxy session_proxy;
    std::unique_ptr<Coordinator> coordinator;
    QTimer* timer = nullptr;
};

Application::Application(ApplicationOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

Application::~Application() = default;

void Application::start() {
    impl_->start();
}

} // namespace oneintwo
