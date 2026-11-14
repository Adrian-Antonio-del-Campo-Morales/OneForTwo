#include "app/application.hpp"

#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"
#include "net/device_identity.hpp"
#include "net/tls_transport.hpp"
#include "storage/pairing_store.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QList>
#include <QSsl>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QThread>
#include <QtGlobal>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "../session/test_support.hpp"
#include "../tls/test_certificates.hpp"

namespace oneintwo {

// El producto todavía no contiene el módulo de avisos ni las fábricas nativas.
// Este target comprueba el enlace real de Application; los dobles mantienen
// fuera del núcleo esas dependencias pendientes.
struct SharedNotifications::Impl {};

SharedNotifications::SharedNotifications(NotificationDependencies)
    : impl_(std::make_unique<Impl>()) {}

SharedNotifications::~SharedNotifications() = default;

void SharedNotifications::tick() {}

void SharedNotifications::setAllowedApplications(std::set<std::string>) {}

void SharedNotifications::setAuthorization(SessionGeneration, FeatureSet, FeatureSet) {}

void SharedNotifications::sendManual(SessionGeneration, const NotificationContent&) {}

void SharedNotifications::forwardObserved(SessionGeneration, const ObservedNotification&) {}

void SharedNotifications::receiveNotice(const RemoteNotification&) {}

#if defined(Q_OS_WIN)
std::unique_ptr<DesktopVisualCapture> makeWindowsVisualCapture(PlatformContext) {
    return nullptr;
}

std::unique_ptr<DesktopNotifications> makeWindowsNotifications(PlatformContext) {
    return nullptr;
}
#elif defined(Q_OS_LINUX)
std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCapture(PlatformContext) {
    return nullptr;
}

std::unique_ptr<DesktopNotifications> makeLinuxNotifications(PlatformContext) {
    return nullptr;
}
#endif

}  // namespace oneintwo

namespace {

std::string pathUnder(const QTemporaryDir& directory, const char* name) {
    return (std::filesystem::path(directory.path().toStdString()) / name)
        .string();
}

std::string pinFor(const char* certificate_pem) {
    const QList<QSslCertificate> certificates =
        QSslCertificate::fromData(QByteArray(certificate_pem), QSsl::Pem);
    if (certificates.isEmpty() || certificates.front().isNull()) {
        return {};
    }
    return certificates.front()
        .digest(QCryptographicHash::Sha256)
        .toHex()
        .toStdString();
}

bool waitUntil(const std::function<bool()>& condition, int timeout_ms = 8000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

}  // namespace

TEST_CASE(application_starts_with_pairing_dialog_linked) {
    QTemporaryDir directory;
    CHECK(directory.isValid());

    oneintwo::ApplicationOptions options;
    options.storage_path = pathUnder(directory, "preferences.txt");
    options.identity_directory = pathUnder(directory, "identity");
    options.pairing_store_path = pathUnder(directory, "paired-devices.tsv");
    options.peer_name = "Equipo de prueba";

    oneintwo::Application application(std::move(options));
    application.start();
    application.start();
    QCoreApplication::processEvents();
}

TEST_CASE(application_reuses_saved_pin_and_endpoint_on_start) {
    QTemporaryDir directory;
    CHECK(directory.isValid());

    const std::string identity_directory = pathUnder(directory, "identity");
    oneintwo::DeviceIdentityStore identity_store(identity_directory);
    std::string error;
    auto identity = identity_store.loadOrCreate(&error);
    CHECK(identity.has_value());
    CHECK(error.empty());
    if (!identity.has_value()) {
        return;
    }

    const std::string pin_b = pinFor(oneintwo_test::certificate_b);
    CHECK(!pin_b.empty());

    // El extremo B escucha en un puerto efímero que solo conoce el par guardado.
    oneintwo::TlsTransportConfig server_config;
    server_config.mode = oneintwo::TlsTransportMode::listen;
    server_config.certificate_pem = oneintwo_test::certificate_b;
    server_config.private_key_pem = oneintwo_test::private_key_b;
    server_config.peer_fingerprint = identity->fingerprint;
    server_config.bind_address = "127.0.0.1";
    server_config.listen_port = 0;
    oneintwo::QtTlsTransport server(std::move(server_config));
    bool server_ready = false;
    std::string server_error;
    server.setReadyHandler([&] { server_ready = true; });
    server.setFailureHandler([&](std::string message) {
        server_error = std::move(message);
    });
    CHECK(server.start(&error));
    CHECK(server.listeningPort() != 0);

    const std::string pairing_store_path =
        pathUnder(directory, "paired-devices.tsv");
    oneintwo::PairingStore pairing_store(pairing_store_path);
    CHECK(pairing_store.lastError().empty());
    oneintwo::TrustedDevice peer;
    peer.device_id = "device-b";
    peer.name = "Equipo B";
    peer.host = "127.0.0.1";
    peer.port = server.listeningPort();
    peer.pins = {pin_b};
    CHECK(pairing_store.save(peer));

    // Sin TLS explícito ni emparejamiento: Application debe leer este par y
    // conectar al endpoint guardado con el pin guardado.
    oneintwo::ApplicationOptions options;
    options.storage_path = pathUnder(directory, "preferences.txt");
    options.identity_directory = identity_directory;
    options.pairing_store_path = pairing_store_path;
    oneintwo::Application application(std::move(options));
    application.start();

    CHECK(waitUntil([&] { return server_ready || !server_error.empty(); }));
    CHECK(server_ready);
    CHECK(server_error.empty());
    CHECK(server.encrypted());
    CHECK_EQ(server.authenticated_peer_fingerprint(), identity->fingerprint);
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return oneintwo_test::run();
}
