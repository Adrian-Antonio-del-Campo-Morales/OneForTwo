#include "tls_transport.hpp"

#include <QAbstractSocket>
#include <QByteArray>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace oneintwo {
namespace {

constexpr qint64 max_transport_buffer = 4 * 1024 * 1024;
constexpr std::uint32_t max_frame_bytes = 256 * 1024;
constexpr int handshake_timeout_ms = 10000;

QString normalizePin(const QString& value) {
    QString normalized;
    normalized.reserve(value.size());
    for (const QChar character : value) {
        if (character == ':' || character == '-' || character == ' ') {
            continue;
        }
        if (!character.isDigit() &&
            !(character >= 'a' && character <= 'f') &&
            !(character >= 'A' && character <= 'F')) {
            return {};
        }
        normalized.append(character.toLower());
    }
    return normalized.size() == 64 ? normalized : QString{};
}

QString certificatePin(const QSslCertificate& certificate) {
    if (certificate.isNull()) {
        return {};
    }
    return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex());
}

void setError(std::string* error, const QString& message) {
    if (error != nullptr) {
        *error = message.toStdString();
    }
}

} // namespace

struct QtTlsTransport::Impl final : QObject {
    explicit Impl(TlsTransportConfig config_in) : config(std::move(config_in)) {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, this, [this] {
            fail(QStringLiteral("tiempo de handshake TLS agotado"));
        });
    }

    ~Impl() override { close(); }

    enum class State {
        idle,
        listening,
        handshaking,
        ready,
        closed,
        failed,
    };

    bool start(std::string* error) {
        if (state != State::idle) {
            setError(error, QStringLiteral("el transporte TLS ya se ha iniciado"));
            return false;
        }
        if (!prepare(error)) {
            return false;
        }
        if (config.mode == TlsTransportMode::listen) {
            server = std::make_unique<QSslServer>();
            server->setSslConfiguration(ssl_config);
            server->setHandshakeTimeout(handshake_timeout_ms);
            QObject::connect(server.get(), &QSslServer::sslErrors, this,
                             [this](QSslSocket* socket, const QList<QSslError>& errors) {
                                 handleSslErrors(socket, errors);
                             });
            QObject::connect(server.get(), &QSslServer::startedEncryptionHandshake, this,
                             [this](QSslSocket* socket) { attachSocket(socket); });
            QObject::connect(server.get(), &QSslServer::newConnection, this,
                             [this] { acceptPending(); });
            const QHostAddress address(QString::fromStdString(config.bind_address));
            if (!server->listen(address, static_cast<quint16>(config.listen_port))) {
                setError(error, server->errorString());
                server.reset();
                return false;
            }
            state = State::listening;
            return true;
        }

        if (config.peer_port == 0) {
            setError(error, QStringLiteral("puerto TLS del par inválido"));
            return false;
        }
        socket = std::make_unique<QSslSocket>();
        active = socket.get();
        attachSocket(active);
        QObject::connect(active, &QSslSocket::sslErrors, this,
                         [this, socket = active](const QList<QSslError>& errors) {
                             handleSslErrors(socket, errors);
                         });
        active->setSslConfiguration(ssl_config);
        active->connectToHostEncrypted(QString::fromStdString(config.peer_host),
                                       static_cast<quint16>(config.peer_port));
        state = State::handshaking;
        timer.start(handshake_timeout_ms);
        return true;
    }

    bool prepare(std::string* error) {
        if (config.certificate_pem.empty() || config.private_key_pem.empty()) {
            setError(error, QStringLiteral("faltan el certificado o la clave TLS"));
            return false;
        }
        expected_pin = normalizePin(QString::fromStdString(config.peer_fingerprint));
        if (expected_pin.isEmpty()) {
            setError(error, QStringLiteral("la huella TLS fijada no es un SHA-256 válido"));
            return false;
        }
        const QList<QSslCertificate> certificates =
            QSslCertificate::fromData(QByteArray::fromStdString(config.certificate_pem), QSsl::Pem);
        if (certificates.isEmpty() || certificates.front().isNull()) {
            setError(error, QStringLiteral("el certificado TLS no es válido"));
            return false;
        }
        const QByteArray key_data = QByteArray::fromStdString(config.private_key_pem);
        QSslKey key(key_data, QSsl::Rsa, QSsl::Pem);
        if (key.isNull()) {
            key = QSslKey(key_data, QSsl::Ec, QSsl::Pem);
        }
        if (key.isNull()) {
            setError(error, QStringLiteral("la clave TLS no es válida"));
            return false;
        }
        local_certificate = certificates.front();
        local_key = key;
        ssl_config = QSslConfiguration::defaultConfiguration();
        ssl_config.setLocalCertificate(local_certificate);
        ssl_config.setPrivateKey(local_key);
        ssl_config.setPeerVerifyMode(QSslSocket::QueryPeer);
        ssl_config.setProtocol(QSsl::TlsV1_2OrLater);
        return true;
    }

    void attachSocket(QSslSocket* socket_to_attach) {
        if (socket_to_attach == nullptr || state == State::closed || state == State::failed) {
            return;
        }
        if (active != nullptr && active != socket_to_attach) {
            socket_to_attach->abort();
            return;
        }
        if (active == socket_to_attach && attached) {
            return;
        }
        active = socket_to_attach;
        attached = true;
        QObject::connect(socket_to_attach, &QSslSocket::encrypted, this,
                         [this, socket_to_attach] { onEncrypted(socket_to_attach); });
        QObject::connect(socket_to_attach, &QSslSocket::readyRead, this,
                         [this, socket_to_attach] { readFrames(socket_to_attach); });
        QObject::connect(socket_to_attach, &QSslSocket::errorOccurred, this,
                         [this, socket_to_attach](QAbstractSocket::SocketError) {
                             if (socket_to_attach != active || state == State::closed ||
                                 state == State::failed) {
                                 return;
                             }
                             fail(QStringLiteral("error TLS: %1")
                                      .arg(socket_to_attach->errorString()));
                         });
        QObject::connect(socket_to_attach, &QSslSocket::disconnected, this,
                         [this, socket_to_attach] {
                             if (socket_to_attach != active || state == State::closed ||
                                 state == State::failed || closing) {
                                 return;
                             }
                             if (state == State::ready) {
                                 closeAfterRemoteDisconnect();
                             } else {
                                 fail(QStringLiteral("conexión TLS cerrada durante el handshake"));
                             }
                         });
        if (socket_to_attach->isEncrypted()) {
            onEncrypted(socket_to_attach);
        }
    }

    void acceptPending() {
        if (!server) {
            return;
        }
        while (server->hasPendingConnections()) {
            QTcpSocket* pending = server->nextPendingConnection();
            auto* ssl_socket = qobject_cast<QSslSocket*>(pending);
            if (ssl_socket == nullptr) {
                pending->abort();
                pending->deleteLater();
                continue;
            }
            if (active != nullptr && active != ssl_socket) {
                ssl_socket->abort();
                continue;
            }
            attachSocket(ssl_socket);
        }
        // Una sesión usa un único par; no se aceptan conexiones adicionales.
        server->close();
    }

    void handleSslErrors(QSslSocket* socket_with_errors, const QList<QSslError>& errors) {
        if (socket_with_errors == nullptr || state == State::closed || state == State::failed) {
            return;
        }
        const QString actual = certificatePin(socket_with_errors->peerCertificate());
        if (!actual.isEmpty() && actual == expected_pin) {
            socket_with_errors->ignoreSslErrors(errors);
            return;
        }
        fail(QStringLiteral("huella TLS del par no coincide con la fijada"));
    }

    void onEncrypted(QSslSocket* socket_ready) {
        if (socket_ready == nullptr || socket_ready != active || state == State::closed ||
            state == State::failed || state == State::ready) {
            return;
        }
        const QString actual = certificatePin(socket_ready->peerCertificate());
        if (actual.isEmpty() || actual != expected_pin) {
            fail(QStringLiteral("huella TLS del par no coincide con la fijada"));
            return;
        }
        verified_pin = actual;
        state = State::ready;
        timer.stop();
        if (ready_handler) {
            ready_handler();
        }
    }

    void readFrames(QSslSocket* socket_reading) {
        if (socket_reading == nullptr || socket_reading != active || state != State::ready ||
            closing) {
            return;
        }
        incoming.append(socket_reading->readAll());
        if (incoming.size() > max_transport_buffer) {
            fail(QStringLiteral("búfer TLS de entrada desbordado"));
            return;
        }
        while (incoming.size() >= 5) {
            const quint8 channel = static_cast<quint8>(incoming.at(0));
            const quint32 length =
                (static_cast<quint32>(static_cast<quint8>(incoming.at(1))) << 24) |
                (static_cast<quint32>(static_cast<quint8>(incoming.at(2))) << 16) |
                (static_cast<quint32>(static_cast<quint8>(incoming.at(3))) << 8) |
                static_cast<quint32>(static_cast<quint8>(incoming.at(4)));
            if (channel > 1 || length > max_frame_bytes) {
                fail(QStringLiteral("trama TLS inválida"));
                return;
            }
            const qsizetype frame_size = static_cast<qsizetype>(length) + 5;
            if (incoming.size() < frame_size) {
                return;
            }
            const QByteArray payload = incoming.mid(5, static_cast<qsizetype>(length));
            incoming.remove(0, frame_size);
            if (receiver) {
                std::vector<std::uint8_t> bytes;
                bytes.reserve(static_cast<std::size_t>(payload.size()));
                for (const char byte : payload) {
                    bytes.push_back(static_cast<std::uint8_t>(byte));
                }
                receiver(channel == 0 ? SessionChannelKind::interactive
                                      : SessionChannelKind::data,
                         std::move(bytes));
            }
        }
    }

    TransportSendStatus send(SessionChannelKind channel,
                             const std::vector<std::uint8_t>& frame) {
        if (state != State::ready || active == nullptr || !active->isEncrypted() || closing) {
            return TransportSendStatus::closed;
        }
        if (frame.size() > max_frame_bytes) {
            return TransportSendStatus::busy;
        }
        const qint64 frame_size = static_cast<qint64>(frame.size()) + 5;
        if (active->bytesToWrite() + frame_size > max_transport_buffer) {
            return TransportSendStatus::busy;
        }
        QByteArray bytes;
        bytes.reserve(static_cast<qsizetype>(frame_size));
        bytes.append(static_cast<char>(channel == SessionChannelKind::interactive ? 0 : 1));
        const quint32 length = static_cast<quint32>(frame.size());
        bytes.append(static_cast<char>((length >> 24) & 0xFFu));
        bytes.append(static_cast<char>((length >> 16) & 0xFFu));
        bytes.append(static_cast<char>((length >> 8) & 0xFFu));
        bytes.append(static_cast<char>(length & 0xFFu));
        bytes.append(reinterpret_cast<const char*>(frame.data()),
                     static_cast<qsizetype>(frame.size()));
        if (active->write(bytes) != frame_size) {
            fail(QStringLiteral("no se pudo escribir la trama TLS"));
            return TransportSendStatus::closed;
        }
        return TransportSendStatus::sent;
    }

    void closeAfterRemoteDisconnect() {
        if (state == State::closed || state == State::failed) {
            return;
        }
        state = State::closed;
        timer.stop();
        active = nullptr;
        attached = false;
        incoming.clear();
        if (!closed_notified && closed_handler) {
            closed_notified = true;
            closed_handler();
        }
    }

    void fail(const QString& message) {
        if (state == State::failed || state == State::closed || closing || failure_pending) {
            return;
        }
        failure_pending = true;
        const QString detail = message;
        QMetaObject::invokeMethod(
            this,
            [this, detail] {
                failure_pending = false;
                failNow(detail);
            },
            Qt::QueuedConnection);
    }

    void failNow(const QString& message) {
        if (state == State::failed || state == State::closed || closing) {
            return;
        }
        const bool was_ready = state == State::ready;
        state = State::failed;
        timer.stop();
        if (active != nullptr) {
            QObject::disconnect(active.data(), nullptr, this, nullptr);
            active->abort();
            active = nullptr;
        }
        if (socket) {
            socket->abort();
        }
        if (server) {
            server->close();
        }
        attached = false;
        incoming.clear();
        const auto failure = failure_handler;
        const auto closed = closed_handler;
        if (failure) {
            failure(message.toStdString());
        }
        if (was_ready && !closed_notified && closed) {
            closed_notified = true;
            closed();
        }
    }

    void close() {
        if (state == State::closed) {
            return;
        }
        closing = true;
        timer.stop();
        if (active != nullptr) {
            QObject::disconnect(active.data(), nullptr, this, nullptr);
            active->abort();
            active = nullptr;
        }
        if (socket) {
            socket->abort();
            socket.reset();
        }
        server.reset();
        attached = false;
        incoming.clear();
        verified_pin.clear();
        state = State::closed;
    }

    TlsTransportConfig config;
    QSslCertificate local_certificate;
    QSslKey local_key;
    QSslConfiguration ssl_config;
    QString expected_pin;
    QString verified_pin;
    std::unique_ptr<QSslServer> server;
    std::unique_ptr<QSslSocket> socket;
    QPointer<QSslSocket> active;
    bool attached = false;
    bool closing = false;
    bool closed_notified = false;
    bool failure_pending = false;
    State state = State::idle;
    QTimer timer;
    QByteArray incoming;
    TransportReceiver receiver;
    TransportClosed closed_handler;
    std::function<void()> ready_handler;
    std::function<void(std::string)> failure_handler;
};

QtTlsTransport::QtTlsTransport(TlsTransportConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

QtTlsTransport::~QtTlsTransport() = default;

bool QtTlsTransport::start(std::string* error) {
    return impl_->start(error);
}

void QtTlsTransport::setReadyHandler(std::function<void()> handler) {
    impl_->ready_handler = std::move(handler);
}

void QtTlsTransport::setFailureHandler(std::function<void(std::string)> handler) {
    impl_->failure_handler = std::move(handler);
}

std::uint16_t QtTlsTransport::listeningPort() const {
    return impl_->server ? impl_->server->serverPort() : 0;
}

bool QtTlsTransport::encrypted() const {
    return impl_->state == Impl::State::ready && impl_->active != nullptr &&
           impl_->active->isEncrypted();
}

std::string QtTlsTransport::authenticated_peer_fingerprint() const {
    return impl_->verified_pin.toStdString();
}

TransportSendStatus QtTlsTransport::send(SessionChannelKind channel,
                                         const std::vector<std::uint8_t>& frame) {
    return impl_->send(channel, frame);
}

void QtTlsTransport::setReceiver(TransportReceiver receiver) {
    impl_->receiver = std::move(receiver);
}

void QtTlsTransport::setClosedHandler(TransportClosed handler) {
    impl_->closed_handler = std::move(handler);
}

void QtTlsTransport::close() {
    impl_->close();
}

} // namespace oneintwo
