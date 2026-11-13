#include "net/session_link.hpp"
#include "net/tls_transport.hpp"

#include "../session/test_support.hpp"
#include "test_certificates.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QList>
#include <QSslCertificate>
#include <QSsl>
#include <QThread>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

using namespace oneintwo;

namespace {

ModuleRuntime directRuntime() {
    ModuleRuntime runtime;
    runtime.now = [] { return std::chrono::steady_clock::now(); };
    runtime.post = [](Task task) { task(); };
    return runtime;
}

std::string pinFor(const char* certificate_pem) {
    const QList<QSslCertificate> certificates =
        QSslCertificate::fromData(QByteArray(certificate_pem), QSsl::Pem);
    if (certificates.isEmpty() || certificates.front().isNull()) {
        return {};
    }
    return certificates.front().digest(QCryptographicHash::Sha256).toHex().toStdString();
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

SessionLinkOptions optionsFor(const std::string& local, const std::string& peer,
                              const std::string& peer_pin, SessionGeneration generation) {
    SessionLinkOptions options;
    options.local_device_id = local;
    options.peer_device_id = peer;
    options.peer_fingerprint = peer_pin;
    options.granted_send = FeatureSet{true, true, true, true, true, true, true, true, true};
    options.granted_receive = FeatureSet{true, true, true, true, true, true, true, true, true};
    options.allocate_generation = [generation] { return generation; };
    return options;
}

struct Endpoint {
    std::unique_ptr<QtTlsTransport> transport;
    std::unique_ptr<AuthenticatedSession> session;
    bool ready = false;
    bool established = false;
    bool failed = false;
    std::string failure;

    void bindSession(SessionLinkOptions options) {
        SessionLinkHandlers handlers;
        handlers.established = [this](SessionGeneration, SessionGeneration) {
            established = true;
        };
        handlers.failed = [this](SessionLinkError, std::string detail) {
            failed = true;
            failure = std::move(detail);
        };
        session = std::make_unique<AuthenticatedSession>(
            std::move(options), *transport, directRuntime(), std::move(handlers));
    }

    void armReady() {
        transport->setReadyHandler([this] {
            ready = true;
            if (session) {
                session->start();
            }
        });
        transport->setFailureHandler([this](std::string detail) {
            failed = true;
            failure = std::move(detail);
        });
    }
};

TlsTransportConfig serverConfig(const std::string& certificate, const std::string& key,
                                const std::string& peer_pin) {
    TlsTransportConfig config;
    config.mode = TlsTransportMode::listen;
    config.certificate_pem = certificate;
    config.private_key_pem = key;
    config.peer_fingerprint = peer_pin;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0;
    return config;
}

TlsTransportConfig clientConfig(const std::string& certificate, const std::string& key,
                                const std::string& peer_pin, std::uint16_t port) {
    TlsTransportConfig config;
    config.mode = TlsTransportMode::connect;
    config.certificate_pem = certificate;
    config.private_key_pem = key;
    config.peer_fingerprint = peer_pin;
    config.peer_host = "127.0.0.1";
    config.peer_port = port;
    return config;
}

} // namespace

TEST_CASE(loopback_tls_handshake_establishes_two_pinned_sessions) {
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string pin_b = pinFor(oneintwo_test::certificate_b);
    CHECK(!pin_a.empty());
    CHECK(!pin_b.empty());

    Endpoint server;
    server.transport = std::make_unique<QtTlsTransport>(
        serverConfig(oneintwo_test::certificate_b, oneintwo_test::private_key_b, pin_a));
    server.bindSession(optionsFor("B", "A", pin_a, 20));
    server.armReady();
    std::string error;
    CHECK(server.transport->start(&error));
    CHECK(error.empty());
    const std::uint16_t port = server.transport->listeningPort();
    CHECK(port != 0);

    Endpoint client;
    client.transport = std::make_unique<QtTlsTransport>(clientConfig(
        oneintwo_test::certificate_a, oneintwo_test::private_key_a, pin_b, port));
    client.bindSession(optionsFor("A", "B", pin_b, 10));
    client.armReady();
    CHECK(client.transport->start(&error));
    CHECK(error.empty());

    CHECK(waitUntil([&] { return client.established && server.established; }));
    CHECK(client.ready);
    CHECK(server.ready);
    CHECK(client.transport->encrypted());
    CHECK(server.transport->encrypted());
    CHECK_EQ(client.transport->authenticated_peer_fingerprint(), pin_b);
    CHECK_EQ(server.transport->authenticated_peer_fingerprint(), pin_a);
    CHECK_EQ(client.session->phase(), SessionLinkPhase::established);
    CHECK_EQ(server.session->phase(), SessionLinkPhase::established);

    client.session->close();
    server.session->close();
}

TEST_CASE(wrong_pinned_fingerprint_is_rejected_before_session_handshake) {
    const std::string pin_a = pinFor(oneintwo_test::certificate_a);
    const std::string wrong_pin(64, '0');

    Endpoint server;
    server.transport = std::make_unique<QtTlsTransport>(
        serverConfig(oneintwo_test::certificate_b, oneintwo_test::private_key_b, pin_a));
    server.bindSession(optionsFor("B", "A", pin_a, 20));
    server.armReady();
    std::string error;
    CHECK(server.transport->start(&error));

    Endpoint client;
    client.transport = std::make_unique<QtTlsTransport>(clientConfig(
        oneintwo_test::certificate_a, oneintwo_test::private_key_a, wrong_pin,
        server.transport->listeningPort()));
    client.bindSession(optionsFor("A", "B", wrong_pin, 10));
    client.armReady();
    CHECK(client.transport->start(&error));

    CHECK(waitUntil([&] { return client.failed || client.ready; }));
    CHECK(client.failed);
    CHECK(!client.ready);
    CHECK(!client.established);
    CHECK(client.failure.find("huella") != std::string::npos);
    CHECK(client.transport->authenticated_peer_fingerprint().empty());
    CHECK(!client.transport->encrypted());
    CHECK(!server.established);
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    return oneintwo_test::run();
}
