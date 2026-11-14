#include "net/device_identity.hpp"
#include "net/pairing_session.hpp"
#include "net/tls_transport.hpp"
#include "storage/pairing_store.hpp"

#include "test_certificates.hpp"
#include "../session/test_support.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QList>
#include <QSsl>
#include <QSslCertificate>
#include <QTemporaryDir>
#include <QThread>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>

using namespace oneintwo;

namespace {

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

bool waitUntil(const std::function<bool()>& condition, int timeout_ms = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

struct PairingEndpoint {
    std::unique_ptr<QtTlsTransport> transport;
    std::unique_ptr<PairingSession> pairing;
    bool ready = false;
    bool completed = false;
    bool failed = false;
    PairingPeer peer;
    std::string sas;
    std::string error;

    void arm(PairingLocal local, std::string expected_peer) {
        transport->setReadyHandler([this, local = std::move(local),
                                    expected_peer = std::move(expected_peer)] {
            ready = true;
            PairingCallbacks callbacks;
            callbacks.sas_ready = [this](std::string value, PairingPeer other) {
                sas = std::move(value);
                peer = std::move(other);
            };
            callbacks.completed = [this](PairingPeer other) {
                completed = true;
                peer = std::move(other);
            };
            callbacks.failed = [this](std::string message) {
                failed = true;
                error = std::move(message);
            };
            pairing = std::make_unique<PairingSession>(
                *transport, local, expected_peer, std::move(callbacks));
            pairing->start();
        });
        transport->setFailureHandler([this](std::string message) {
            failed = true;
            error = std::move(message);
        });
    }
};

TlsTransportConfig pairingServerConfig(const std::string& certificate,
                                       const std::string& key) {
    TlsTransportConfig config;
    config.mode = TlsTransportMode::listen;
    config.certificate_pem = certificate;
    config.private_key_pem = key;
    config.allow_unverified = true;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0;
    return config;
}

TlsTransportConfig pairingClientConfig(const std::string& certificate,
                                       const std::string& key,
                                       std::uint16_t port) {
    TlsTransportConfig config;
    config.mode = TlsTransportMode::connect;
    config.certificate_pem = certificate;
    config.private_key_pem = key;
    config.allow_unverified = true;
    config.peer_host = "127.0.0.1";
    config.peer_port = port;
    return config;
}

std::string tempPath(const QTemporaryDir& directory, const char* name) {
    return (std::filesystem::path(directory.path().toStdString()) / name)
        .string();
}

}  // namespace

TEST_CASE(identity_is_persisted_and_can_be_rotated) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    DeviceIdentityStore store(tempPath(directory, "identity"));
    std::string error;
    auto identity = store.loadOrCreate(&error);
    CHECK(identity.has_value());
    CHECK(error.empty());
    if (!identity.has_value()) {
        return;
    }
    CHECK_EQ(identity->device_id.size(), static_cast<std::size_t>(32));
    CHECK_EQ(identity->fingerprint.size(), static_cast<std::size_t>(64));
    CHECK(!identity->certificate_pem.empty());
    CHECK(!identity->private_key_pem.empty());

    DeviceIdentityStore reopened(tempPath(directory, "identity"));
    auto loaded = reopened.load(&error);
    CHECK(loaded.has_value());
    CHECK_EQ(loaded->device_id, identity->device_id);
    CHECK_EQ(loaded->fingerprint, identity->fingerprint);
    CHECK_EQ(loaded->certificate_pem, identity->certificate_pem);

    auto rotation = reopened.rotate(&error);
    CHECK(rotation.has_value());
    if (!rotation.has_value()) {
        return;
    }
    CHECK_EQ(rotation->identity.device_id, identity->device_id);
    CHECK_EQ(rotation->previous_fingerprint, identity->fingerprint);
    CHECK(rotation->identity.fingerprint != identity->fingerprint);
    CHECK(rotation->identity.certificate_pem != identity->certificate_pem);

    DeviceIdentityStore after(tempPath(directory, "identity"));
    auto reloaded = after.load(&error);
    CHECK(reloaded.has_value());
    CHECK_EQ(reloaded->fingerprint, rotation->identity.fingerprint);
}

TEST_CASE(trust_store_keeps_one_pin_per_device_and_rotates_it) {
    QTemporaryDir directory;
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string pin_b = pinFor(oneintwo_test::certificate_b);
    CHECK(!pin_a.empty());
    CHECK(!pin_b.empty());

    PairingStore store(path);
    CHECK(store.lastError().empty());
    TrustedDevice device;
    device.device_id = "equipo-b";
    device.name = "Equipo B";
    device.host = "127.0.0.1";
    device.port = 45871;
    device.pins = {pin_a};
    CHECK(store.save(device));
    CHECK(store.pinTrusted("equipo-b", pin_a));
    CHECK(!store.pinTrusted("equipo-b", pin_b));

    const std::string wrong(64, '0');
    CHECK(!store.pinTrusted("equipo-b", wrong));
    CHECK(store.rotatePin("equipo-b", pin_b));
    CHECK(store.pinTrusted("equipo-b", pin_b));
    CHECK(store.pinTrusted("equipo-b", pin_a));
    CHECK(!store.pinTrusted("equipo-b", wrong));

    PairingStore reopened(path);
    CHECK(reopened.lastError().empty());
    const auto reloaded = reopened.find("equipo-b");
    CHECK(reloaded.has_value());
    if (reloaded.has_value()) {
        CHECK_EQ(reloaded->name, std::string("Equipo B"));
        CHECK_EQ(reloaded->host, std::string("127.0.0.1"));
        CHECK_EQ(reloaded->port, static_cast<std::uint16_t>(45871));
        CHECK_EQ(reloaded->pins.size(), static_cast<std::size_t>(2));
        CHECK_EQ(reloaded->pins.front(), pin_b);
    }

    TrustedDevice listener;
    listener.device_id = "equipo-c";
    listener.name = "Equipo C";
    listener.port = 45454;
    listener.pins = {pin_b};
    CHECK(store.save(listener));
    const auto listener_reloaded = PairingStore(path).find("equipo-c");
    CHECK(listener_reloaded.has_value());
    if (listener_reloaded.has_value()) {
        CHECK(listener_reloaded->host.empty());
        CHECK_EQ(listener_reloaded->port, static_cast<std::uint16_t>(45454));
    }
}

TEST_CASE(sas_pairing_over_temporary_tls_accepts_matching_confirmation) {
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string pin_b = pinFor(oneintwo_test::certificate_b);
    CHECK(!pin_a.empty());
    CHECK(!pin_b.empty());

    PairingEndpoint server;
    server.transport = std::make_unique<QtTlsTransport>(
        pairingServerConfig(oneintwo_test::certificate_b,
                            oneintwo_test::private_key_b));
    server.arm(PairingLocal{"device-b", "Equipo B", pin_b}, "device-a");
    std::string error;
    CHECK(server.transport->start(&error));
    CHECK(error.empty());
    CHECK(server.transport->listeningPort() != 0);

    PairingEndpoint client;
    client.transport = std::make_unique<QtTlsTransport>(
        pairingClientConfig(oneintwo_test::certificate_a,
                            oneintwo_test::private_key_a,
                            server.transport->listeningPort()));
    client.arm(PairingLocal{"device-a", "Equipo A", pin_a}, "device-b");
    CHECK(client.transport->start(&error));
    CHECK(error.empty());

    CHECK(waitUntil([&] {
        return !server.sas.empty() && !client.sas.empty();
    }));
    CHECK(!server.failed);
    CHECK(!client.failed);
    CHECK_EQ(server.sas, client.sas);
    CHECK_EQ(server.sas.size(), static_cast<std::size_t>(6));
    CHECK_EQ(server.peer.fingerprint, pin_a);
    CHECK_EQ(client.peer.fingerprint, pin_b);

    server.pairing->confirm();
    CHECK(!server.completed);
    client.pairing->confirm();
    CHECK(waitUntil([&] { return server.completed && client.completed; }));
    CHECK(server.pairing->state() == PairingState::complete);
    CHECK(client.pairing->state() == PairingState::complete);

    QTemporaryDir directory;
    PairingStore store(tempPath(directory, "devices.tsv"));
    TrustedDevice peer_a;
    peer_a.device_id = "device-a";
    peer_a.name = "Equipo A";
    peer_a.pins = {server.peer.fingerprint};
    TrustedDevice peer_b;
    peer_b.device_id = "device-b";
    peer_b.name = "Equipo B";
    peer_b.pins = {client.peer.fingerprint};
    CHECK(store.save(peer_a));
    CHECK(store.save(peer_b));
    CHECK(store.pinTrusted("device-a", pin_a));
    CHECK(store.pinTrusted("device-b", pin_b));

    client.transport->close();
    server.transport->close();

    // El pin guardado debe servir para una conexión normal, sin el modo de
    // emparejamiento ni una huella sin autenticar.
    TlsTransportConfig normal_server;
    normal_server.mode = TlsTransportMode::listen;
    normal_server.certificate_pem = oneintwo_test::certificate_b;
    normal_server.private_key_pem = oneintwo_test::private_key_b;
    normal_server.peer_fingerprint = pin_a;
    normal_server.bind_address = "127.0.0.1";
    normal_server.listen_port = 0;
    QtTlsTransport pinned_server(std::move(normal_server));
    bool server_ready = false;
    pinned_server.setReadyHandler([&] { server_ready = true; });
    CHECK(pinned_server.start(&error));

    TlsTransportConfig normal_client;
    normal_client.mode = TlsTransportMode::connect;
    normal_client.certificate_pem = oneintwo_test::certificate_a;
    normal_client.private_key_pem = oneintwo_test::private_key_a;
    normal_client.peer_fingerprint = pin_b;
    normal_client.peer_host = "127.0.0.1";
    normal_client.peer_port = pinned_server.listeningPort();
    QtTlsTransport pinned_client(std::move(normal_client));
    bool client_ready = false;
    pinned_client.setReadyHandler([&] { client_ready = true; });
    CHECK(pinned_client.start(&error));
    CHECK(waitUntil([&] { return server_ready && client_ready; }));
    CHECK_EQ(pinned_client.authenticated_peer_fingerprint(), pin_b);
    CHECK_EQ(pinned_server.authenticated_peer_fingerprint(), pin_a);
    pinned_client.close();
    pinned_server.close();
}

TEST_CASE(sas_pairing_rejects_when_one_side_refuses) {
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string pin_b = pinFor(oneintwo_test::certificate_b);

    PairingEndpoint server;
    server.transport = std::make_unique<QtTlsTransport>(
        pairingServerConfig(oneintwo_test::certificate_b,
                            oneintwo_test::private_key_b));
    server.arm(PairingLocal{"device-b", "Equipo B", pin_b}, "device-a");
    std::string error;
    CHECK(server.transport->start(&error));

    PairingEndpoint client;
    client.transport = std::make_unique<QtTlsTransport>(
        pairingClientConfig(oneintwo_test::certificate_a,
                            oneintwo_test::private_key_a,
                            server.transport->listeningPort()));
    client.arm(PairingLocal{"device-a", "Equipo A", pin_a}, "device-b");
    CHECK(client.transport->start(&error));

    CHECK(waitUntil([&] {
        return !server.sas.empty() && !client.sas.empty();
    }));
    CHECK_EQ(server.sas, client.sas);

    server.pairing->reject("no coincide la pantalla");
    CHECK(server.pairing->state() == PairingState::rejected);
    CHECK(waitUntil([&] { return client.failed; }));
    CHECK(!client.completed);
    CHECK(client.pairing->state() == PairingState::rejected ||
          client.pairing->state() == PairingState::failed);
    CHECK(!server.completed);

    client.transport->close();
    server.transport->close();
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    return oneintwo_test::run();
}
