#include "net/session_link.hpp"
#include "net/tls_transport.hpp"
#include "storage/pairing_store.hpp"
#include "ui/pairing_dialog.hpp"

#include "../session/test_support.hpp"
#include "../tls/test_certificates.hpp"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QSpinBox>
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

bool waitUntil(const std::function<bool()>& condition, int timeout_ms = 8000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    return condition();
}

std::string pathUnder(const QTemporaryDir& directory, const char* name) {
    return (std::filesystem::path(directory.path().toStdString()) / name)
        .string();
}

DeviceIdentity identityFor(const char* device_id, const char* certificate,
                           const char* key) {
    return DeviceIdentity{device_id, certificate, key, pinFor(certificate)};
}

ModuleRuntime directRuntime() {
    ModuleRuntime runtime;
    runtime.now = [] { return std::chrono::steady_clock::now(); };
    runtime.post = [](Task task) { task(); };
    return runtime;
}

FeatureSet allFeatures() {
    return FeatureSet{true, true, true, true, true, true, true, true, true};
}

SessionLinkOptions linkOptions(const std::string& local,
                               const std::string& peer,
                               const std::string& peer_pin,
                               SessionGeneration generation) {
    SessionLinkOptions options;
    options.local_device_id = local;
    options.peer_device_id = peer;
    options.peer_fingerprint = peer_pin;
    options.granted_send = allFeatures();
    options.granted_receive = allFeatures();
    options.allocate_generation = [generation] { return generation; };
    return options;
}

struct SessionEndpoint {
    std::unique_ptr<AuthenticatedSession> session;
    bool established = false;
    bool failed = false;
    std::string failure;
};

template <typename T>
T* field(PairingDialog& dialog, const char* name) {
    T* value = dialog.findChild<T*>(QString::fromLatin1(name));
    CHECK(value != nullptr);
    return value;
}

}  // namespace

TEST_CASE(pairing_dialog_pairs_loopback_and_adopts_authenticated_session) {
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string pin_b = pinFor(oneintwo_test::certificate_b);
    CHECK(!pin_a.empty());
    CHECK(!pin_b.empty());

    QTemporaryDir directory;
    CHECK(directory.isValid());
    const std::string store_a_path = pathUnder(directory, "a.tsv");
    const std::string store_b_path = pathUnder(directory, "b.tsv");
    PairingStore store_a(store_a_path);
    PairingStore store_b(store_b_path);
    CHECK(store_a.lastError().empty());
    CHECK(store_b.lastError().empty());

    PairingDialog listener(
        identityFor("device-a", oneintwo_test::certificate_a,
                    oneintwo_test::private_key_a),
        store_a);
    listener.setRotationHandler([] { return std::nullopt; });
    auto* listener_name = field<QLineEdit>(listener, "pairingName");
    auto* listener_port = field<QSpinBox>(listener, "pairingPort");
    listener_name->setText(QStringLiteral("Equipo A"));
    // Puerto automático para no competir con otros procesos de la suite.
    listener_port->setValue(0);
    field<QPushButton>(listener, "pairingStart")->click();
    CHECK(waitUntil([&] { return listener_port->value() != 0; }));
    const int listening_port = listener_port->value();
    CHECK(listening_port > 0);
    CHECK(listening_port <= 65535);

    PairingDialog connector(
        identityFor("device-b", oneintwo_test::certificate_b,
                    oneintwo_test::private_key_b),
        store_b);
    connector.setRotationHandler([] { return std::nullopt; });
    auto* connector_mode = field<QComboBox>(connector, "pairingMode");
    auto* connector_name = field<QLineEdit>(connector, "pairingName");
    auto* connector_host = field<QLineEdit>(connector, "pairingHost");
    auto* connector_port = field<QSpinBox>(connector, "pairingPort");
    connector_mode->setCurrentIndex(1);
    connector_name->setText(QStringLiteral("Equipo B"));
    connector_host->setText(QStringLiteral("127.0.0.1"));
    connector_port->setValue(listening_port);
    field<QPushButton>(connector, "pairingStart")->click();

    auto* listener_sas = field<QLabel>(listener, "pairingSas");
    auto* connector_sas = field<QLabel>(connector, "pairingSas");
    CHECK(waitUntil([&] {
        return listener_sas->text() != QStringLiteral("—") &&
               connector_sas->text() != QStringLiteral("—");
    }));
    CHECK(!listener_sas->text().isEmpty());
    CHECK(!connector_sas->text().isEmpty());
    CHECK(listener_sas->text() == connector_sas->text());

    auto* listener_confirm = field<QPushButton>(listener, "pairingConfirm");
    auto* connector_confirm = field<QPushButton>(connector, "pairingConfirm");
    CHECK(listener_confirm->isEnabled());
    CHECK(connector_confirm->isEnabled());
    listener_confirm->click();
    connector_confirm->click();
    CHECK(waitUntil([&] {
        return listener.result() == QDialog::Accepted &&
               connector.result() == QDialog::Accepted;
    }));

    auto result_a = listener.takeResult();
    auto result_b = connector.takeResult();
    CHECK(result_a.has_value());
    CHECK(result_b.has_value());
    if (!result_a.has_value() || !result_b.has_value()) {
        return;
    }
    CHECK_EQ(result_a->peer.device_id, std::string("device-b"));
    CHECK_EQ(result_b->peer.device_id, std::string("device-a"));
    CHECK_EQ(result_a->peer.pins.front(), pin_b);
    CHECK_EQ(result_b->peer.pins.front(), pin_a);
    CHECK(result_a->transport != nullptr);
    CHECK(result_b->transport != nullptr);
    CHECK(result_a->transport->encrypted());
    CHECK(result_b->transport->encrypted());
    CHECK_EQ(result_a->transport->authenticated_peer_fingerprint(), pin_b);
    CHECK_EQ(result_b->transport->authenticated_peer_fingerprint(), pin_a);

    // El pin que guardó el diálogo, no solo el del resultado en memoria.
    PairingStore persisted_a(store_a_path);
    PairingStore persisted_b(store_b_path);
    CHECK(persisted_a.lastError().empty());
    CHECK(persisted_b.lastError().empty());
    const auto saved_b = persisted_a.find("device-b");
    const auto saved_a = persisted_b.find("device-a");
    CHECK(saved_b.has_value());
    CHECK(saved_a.has_value());
    if (saved_b.has_value()) {
        CHECK(saved_b->trusts(pin_b));
        CHECK(saved_b->name == std::string("Equipo B"));
    }
    if (saved_a.has_value()) {
        CHECK(saved_a->trusts(pin_a));
        CHECK(saved_a->name == std::string("Equipo A"));
    }

    SessionEndpoint session_a;
    SessionEndpoint session_b;
    SessionLinkHandlers handlers_a;
    handlers_a.established = [&session_a](SessionGeneration, SessionGeneration) {
        session_a.established = true;
    };
    handlers_a.failed = [&session_a](SessionLinkError, std::string detail) {
        session_a.failed = true;
        session_a.failure = std::move(detail);
    };
    session_a.session = std::make_unique<AuthenticatedSession>(
        linkOptions("device-a", "device-b", pin_b, 101),
        *result_a->transport, directRuntime(), std::move(handlers_a));

    SessionLinkHandlers handlers_b;
    handlers_b.established = [&session_b](SessionGeneration, SessionGeneration) {
        session_b.established = true;
    };
    handlers_b.failed = [&session_b](SessionLinkError, std::string detail) {
        session_b.failed = true;
        session_b.failure = std::move(detail);
    };
    session_b.session = std::make_unique<AuthenticatedSession>(
        linkOptions("device-b", "device-a", pin_a, 202),
        *result_b->transport, directRuntime(), std::move(handlers_b));

    session_a.session->start();
    session_b.session->start();
    CHECK(waitUntil([&] {
        return session_a.established && session_b.established;
    }));
    CHECK(!session_a.failed);
    CHECK(!session_b.failed);
    CHECK_EQ(session_a.session->phase(), SessionLinkPhase::established);
    CHECK_EQ(session_b.session->phase(), SessionLinkPhase::established);

    session_a.session->close();
    session_b.session->close();
    result_a->transport->close();
    result_b->transport->close();
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return oneintwo_test::run();
}
