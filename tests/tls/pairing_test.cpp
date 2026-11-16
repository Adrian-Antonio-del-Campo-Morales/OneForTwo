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
#include <fstream>
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
#ifndef _WIN32
    const auto identity_directory =
        std::filesystem::path(tempPath(directory, "identity"));
    CHECK((std::filesystem::status(identity_directory).permissions() &
           std::filesystem::perms::group_all) ==
          std::filesystem::perms::none);
    CHECK((std::filesystem::status(identity_directory / "identity.key")
               .permissions() &
           std::filesystem::perms::group_all) ==
          std::filesystem::perms::none);
    CHECK((std::filesystem::status(identity_directory / "identity.key")
               .permissions() &
           std::filesystem::perms::others_all) ==
          std::filesystem::perms::none);
#endif

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

TEST_CASE(libcrypto_generated_identity_opens_pinned_tls) {
    QTemporaryDir directory_a;
    QTemporaryDir directory_b;
    CHECK(directory_a.isValid());
    CHECK(directory_b.isValid());
    DeviceIdentityStore store_a(tempPath(directory_a, "identity"));
    DeviceIdentityStore store_b(tempPath(directory_b, "identity"));
    std::string error;
    auto identity_a = store_a.loadOrCreate(&error);
    auto identity_b = store_b.loadOrCreate(&error);
    CHECK(identity_a.has_value());
    CHECK(identity_b.has_value());
    if (!identity_a.has_value() || !identity_b.has_value()) {
        return;
    }

    TlsTransportConfig server_config;
    server_config.mode = TlsTransportMode::listen;
    server_config.certificate_pem = identity_b->certificate_pem;
    server_config.private_key_pem = identity_b->private_key_pem;
    server_config.peer_fingerprint = identity_a->fingerprint;
    server_config.bind_address = "127.0.0.1";
    server_config.listen_port = 0;
    QtTlsTransport server(std::move(server_config));
    bool server_ready = false;
    server.setReadyHandler([&] { server_ready = true; });
    CHECK(server.start(&error));

    TlsTransportConfig client_config;
    client_config.mode = TlsTransportMode::connect;
    client_config.certificate_pem = identity_a->certificate_pem;
    client_config.private_key_pem = identity_a->private_key_pem;
    client_config.peer_fingerprint = identity_b->fingerprint;
    client_config.peer_host = "127.0.0.1";
    client_config.peer_port = server.listeningPort();
    QtTlsTransport client(std::move(client_config));
    bool client_ready = false;
    client.setReadyHandler([&] { client_ready = true; });
    CHECK(client.start(&error));

    CHECK(waitUntil([&] { return server_ready && client_ready; }));
    CHECK(server.encrypted());
    CHECK(client.encrypted());
    CHECK_EQ(server.authenticated_peer_fingerprint(), identity_a->fingerprint);
    CHECK_EQ(client.authenticated_peer_fingerprint(), identity_b->fingerprint);
    client.close();
    server.close();
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
    device.device_id = "equipment-b";
    device.name = "Equipment B";
    device.host = "127.0.0.1";
    device.port = 45871;
    device.pins = {pin_a};
    CHECK(store.save(device));
    CHECK(store.pinTrusted("equipment-b", pin_a));
    CHECK(!store.pinTrusted("equipment-b", pin_b));

    const std::string wrong(64, '0');
    CHECK(!store.pinTrusted("equipment-b", wrong));
    CHECK(store.rotatePin("equipment-b", pin_b));
    CHECK(store.pinTrusted("equipment-b", pin_b));
    CHECK(store.pinTrusted("equipment-b", pin_a));
    CHECK(!store.pinTrusted("equipment-b", wrong));

    PairingStore reopened(path);
    CHECK(reopened.lastError().empty());
    const auto reloaded = reopened.find("equipment-b");
    CHECK(reloaded.has_value());
    if (reloaded.has_value()) {
        CHECK_EQ(reloaded->name, std::string("Equipment B"));
        CHECK_EQ(reloaded->host, std::string("127.0.0.1"));
        CHECK_EQ(reloaded->port, static_cast<std::uint16_t>(45871));
        CHECK_EQ(reloaded->pins.size(), static_cast<std::size_t>(2));
        CHECK_EQ(reloaded->pins.front(), pin_b);
    }

    TrustedDevice listener;
    listener.device_id = "c-team";
    listener.name = "Equipment C";
    listener.port = 45454;
    listener.pins = {pin_b};
    CHECK(store.save(listener));
    const auto listener_reloaded = PairingStore(path).find("c-team");
    CHECK(listener_reloaded.has_value());
    if (listener_reloaded.has_value()) {
        CHECK(listener_reloaded->host.empty());
        CHECK_EQ(listener_reloaded->port, static_cast<std::uint16_t>(45454));
    }
}

TEST_CASE(trust_store_rotates_pin_and_peer_metadata_together) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string old_pin = pinFor(oneintwo_test::certificate_a);
    const std::string new_pin = pinFor(oneintwo_test::certificate_b);
    CHECK(!old_pin.empty());
    CHECK(!new_pin.empty());
    CHECK(old_pin != new_pin);

    PairingStore store(path);
    CHECK(store.lastError().empty());
    TrustedDevice original;
    original.device_id = "equipment-b";
    original.name = "Equipment B";
    original.host = "127.0.0.1";
    original.port = 45871;
    original.pins = {old_pin};
    CHECK(store.save(original));

    TrustedDevice updated = original;
    updated.name = "Renamed Team B";
    updated.host = "192.0.2.10";
    updated.port = 47123;
    updated.pins = {new_pin};
    const auto rotated = store.rotatePin(updated);
    CHECK(rotated.has_value());
    if (!rotated.has_value()) {
        return;
    }
    CHECK_EQ(rotated->name, std::string("Renamed Team B"));
    CHECK_EQ(rotated->host, std::string("192.0.2.10"));
    CHECK_EQ(rotated->port, static_cast<std::uint16_t>(47123));
    CHECK_EQ(rotated->pins.size(), static_cast<std::size_t>(2));
    CHECK_EQ(rotated->pins.front(), new_pin);
    CHECK_EQ(rotated->pins.back(), old_pin);

    PairingStore reopened(path);
    CHECK(reopened.lastError().empty());
    const auto persisted = reopened.find("equipment-b");
    CHECK(persisted.has_value());
    if (!persisted.has_value()) {
        return;
    }
    CHECK_EQ(persisted->name, std::string("Renamed Team B"));
    CHECK_EQ(persisted->host, std::string("192.0.2.10"));
    CHECK_EQ(persisted->port, static_cast<std::uint16_t>(47123));
    CHECK_EQ(persisted->pins.size(), static_cast<std::size_t>(2));
    CHECK_EQ(persisted->pins.front(), new_pin);
    CHECK_EQ(persisted->pins.back(), old_pin);
}

TEST_CASE(trust_store_exposes_primary_and_fallback_and_retires_backup) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string old_pin = pinFor(oneintwo_test::certificate_a);
    const std::string new_pin = pinFor(oneintwo_test::certificate_b);
    CHECK(!old_pin.empty());
    CHECK(!new_pin.empty());
    CHECK(old_pin != new_pin);

    PairingStore store(path);
    TrustedDevice device;
    device.device_id = "equipment-b";
    device.name = "Equipment B";
    device.host = "127.0.0.1";
    device.port = 45871;
    device.pins = {old_pin};
    CHECK(store.save(device));
    device.pins = {new_pin};
    CHECK(store.rotatePin(device).has_value());

    CHECK_EQ(store.primaryPin("equipment-b").value_or(""), new_pin);
    CHECK_EQ(store.fallbackPin("equipment-b").value_or(""), old_pin);
    CHECK(store.pinRole("equipment-b", new_pin) ==
          PairingStore::PinRole::primary);
    CHECK(store.pinRole("equipment-b", old_pin) ==
          PairingStore::PinRole::fallback);
    CHECK(store.primaryPinTrusted("equipment-b", new_pin));
    CHECK(!store.primaryPinTrusted("equipment-b", old_pin));
    // PinTrusted compatibility retains the backup as confidence.
    CHECK(store.pinTrusted("equipment-b", old_pin));

    std::string error;
    CHECK(store.retireFallback("equipment-b", &error));
    CHECK(error.empty());
    CHECK(store.primaryPin("equipment-b").has_value());
    CHECK(!store.fallbackPin("equipment-b").has_value());
    CHECK(!store.pinTrusted("equipment-b", old_pin));
    CHECK(store.primaryPinTrusted("equipment-b", new_pin));
    CHECK(store.retireFallback("equipment-b", &error));

    PairingStore reopened(path);
    const auto persisted = reopened.find("equipment-b");
    CHECK(persisted.has_value());
    if (persisted.has_value()) {
        CHECK_EQ(persisted->pins.size(), static_cast<std::size_t>(1));
        CHECK_EQ(persisted->pins.front(), new_pin);
    }
}

TEST_CASE(trust_store_rolls_back_save_remove_and_rotation_on_write_failure) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string old_pin = pinFor(oneintwo_test::certificate_a);
    const std::string new_pin = pinFor(oneintwo_test::certificate_b);

    PairingStore store(path);
    TrustedDevice original;
    original.device_id = "equipment-b";
    original.name = "Original name";
    original.host = "127.0.0.1";
    original.port = 45871;
    original.pins = {old_pin};
    CHECK(store.save(original));

    // A directory in the destination path fails to replace
    // QSaveFile without relying on special platform permissions.
    std::error_code filesystem_error;
    CHECK(std::filesystem::remove(path, filesystem_error));
    CHECK(!filesystem_error);
    CHECK(std::filesystem::create_directory(path, filesystem_error));
    CHECK(!filesystem_error);

    TrustedDevice replacement = original;
    replacement.name = "Name that should not persist";
    replacement.pins = {new_pin};
    std::string error;
    CHECK(!store.save(replacement, &error));
    CHECK(!error.empty());
    const auto after_save = store.find("equipment-b");
    CHECK(after_save.has_value());
    if (after_save.has_value()) {
        CHECK_EQ(after_save->name, std::string("Original name"));
        CHECK_EQ(after_save->pins.front(), old_pin);
    }

    error.clear();
    CHECK(!store.remove("equipment-b", &error));
    CHECK(!error.empty());
    CHECK(store.find("equipment-b").has_value());

    error.clear();
    CHECK(!store.rotatePin(replacement, &error));
    CHECK(!error.empty());
    const auto after_rotation = store.find("equipment-b");
    CHECK(after_rotation.has_value());
    if (after_rotation.has_value()) {
        CHECK_EQ(after_rotation->name, std::string("Original name"));
        CHECK_EQ(after_rotation->pins.size(), static_cast<std::size_t>(1));
        CHECK_EQ(after_rotation->pins.front(), old_pin);
    }
}

TEST_CASE(trust_store_rolls_back_fallback_retirement_on_write_failure) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string old_pin = pinFor(oneintwo_test::certificate_a);
    const std::string new_pin = pinFor(oneintwo_test::certificate_b);

    PairingStore store(path);
    TrustedDevice device;
    device.device_id = "equipment-b";
    device.name = "Equipment B";
    device.host = "127.0.0.1";
    device.port = 45871;
    device.pins = {old_pin};
    CHECK(store.save(device));
    device.pins = {new_pin};
    CHECK(store.rotatePin(device).has_value());

    std::error_code filesystem_error;
    CHECK(std::filesystem::remove(path, filesystem_error));
    CHECK(!filesystem_error);
    CHECK(std::filesystem::create_directory(path, filesystem_error));
    CHECK(!filesystem_error);

    std::string error;
    CHECK(!store.retireFallback("equipment-b", &error));
    CHECK(!error.empty());
    const auto current = store.find("equipment-b");
    CHECK(current.has_value());
    if (current.has_value()) {
        CHECK_EQ(current->pins.size(), static_cast<std::size_t>(2));
        CHECK_EQ(current->pins.front(), new_pin);
        CHECK_EQ(current->pins.back(), old_pin);
    }
}

TEST_CASE(trust_store_reload_preserves_last_good_state_on_corruption) {
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string path = tempPath(directory, "devices.tsv");
    const std::string pin = pinFor(oneintwo_test::certificate_a);

    PairingStore store(path);
    TrustedDevice device;
    device.device_id = "equipment-b";
    device.name = "Equipment B";
    device.host = "127.0.0.1";
    device.port = 45871;
    device.pins = {pin};
    CHECK(store.save(device));

    {
        std::ofstream corrupt(path, std::ios::trunc);
        CHECK(corrupt.good());
        corrupt << "corrupt file\n";
    }
    std::string error;
    CHECK(!store.reload(&error));
    CHECK(!error.empty());
    CHECK(!store.lastError().empty());
    CHECK(store.find("equipment-b").has_value());
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
    server.arm(PairingLocal{"device-b", "Equipment B", pin_b}, "device-a");
    std::string error;
    CHECK(server.transport->start(&error));
    CHECK(error.empty());
    CHECK(server.transport->listeningPort() != 0);

    PairingEndpoint client;
    client.transport = std::make_unique<QtTlsTransport>(
        pairingClientConfig(oneintwo_test::certificate_a,
                            oneintwo_test::private_key_a,
                            server.transport->listeningPort()));
    client.arm(PairingLocal{"device-a", "Equipment A", pin_a}, "device-b");
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
    peer_a.name = "Equipment A";
    peer_a.pins = {server.peer.fingerprint};
    TrustedDevice peer_b;
    peer_b.device_id = "device-b";
    peer_b.name = "Equipment B";
    peer_b.pins = {client.peer.fingerprint};
    CHECK(store.save(peer_a));
    CHECK(store.save(peer_b));
    CHECK(store.pinTrusted("device-a", pin_a));
    CHECK(store.pinTrusted("device-b", pin_b));

    client.transport->close();
    server.transport->close();

    // The saved pin must serve for a normal connection, without the mode of
    // pairing or an unauthenticated print.
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
    server.arm(PairingLocal{"device-b", "Equipment B", pin_b}, "device-a");
    std::string error;
    CHECK(server.transport->start(&error));

    PairingEndpoint client;
    client.transport = std::make_unique<QtTlsTransport>(
        pairingClientConfig(oneintwo_test::certificate_a,
                            oneintwo_test::private_key_a,
                            server.transport->listeningPort()));
    client.arm(PairingLocal{"device-a", "Equipment A", pin_a}, "device-b");
    CHECK(client.transport->start(&error));

    CHECK(waitUntil([&] {
        return !server.sas.empty() && !client.sas.empty();
    }));
    CHECK_EQ(server.sas, client.sas);

    server.pairing->reject("does not match the screen");
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
