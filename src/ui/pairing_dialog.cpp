#include "pairing_dialog.hpp"

#include "net/pairing_session.hpp"
#include "net/tls_transport.hpp"

#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace oneintwo {

PairingDialog::PairingDialog(DeviceIdentity identity, PairingStore& store,
                             QWidget* parent)
    : QDialog(parent),
      identity_(std::move(identity)),
      store_(store) {
    setWindowTitle(QStringLiteral("Emparejar equipos"));
    setModal(true);
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        QStringLiteral(
            "Crea una invitación en un equipo y únete desde el otro. Después "
            "compara el código de seis cifras en ambas pantallas antes de "
            "confirmar. El certificado no se guarda hasta que los dos "
            "confirméis."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* identity_box = new QGroupBox(QStringLiteral("Identidad de este equipo"), this);
    auto* identity_layout = new QVBoxLayout(identity_box);
    local_info_ = new QLabel(identity_box);
    local_info_->setObjectName(QStringLiteral("pairingLocalIdentity"));
    local_info_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    local_info_->setWordWrap(true);
    local_info_->setText(QStringLiteral("ID: %1\nHuella: %2")
                             .arg(QString::fromStdString(identity_.device_id))
                             .arg(QString::fromStdString(identity_.fingerprint)));
    identity_layout->addWidget(local_info_);
    rotate_ = new QPushButton(QStringLiteral("Rotar certificado…"), identity_box);
    rotate_->setObjectName(QStringLiteral("pairingRotate"));
    rotate_->setToolTip(QStringLiteral(
        "Genera un certificado nuevo; el otro equipo tendrá que volver a "
        "emparejarse con la huella nueva."));
    identity_layout->addWidget(rotate_);
    layout->addWidget(identity_box);

    auto* form = new QFormLayout();
    mode_ = new QComboBox(this);
    mode_->setObjectName(QStringLiteral("pairingMode"));
    mode_->addItem(QStringLiteral("Crear invitación (este equipo escucha)"));
    mode_->addItem(QStringLiteral("Unirse a una invitación"));
    form->addRow(QStringLiteral("Modo:"), mode_);

    name_ = new QLineEdit(this);
    name_->setObjectName(QStringLiteral("pairingName"));
    name_->setText(QStringLiteral("Mi equipo"));
    form->addRow(QStringLiteral("Nombre visible:"), name_);

    host_ = new QLineEdit(this);
    host_->setObjectName(QStringLiteral("pairingHost"));
    host_->setText(QStringLiteral("127.0.0.1"));
    host_->setPlaceholderText(QStringLiteral("IP o nombre del otro equipo"));
    form->addRow(QStringLiteral("Equipo remoto:"), host_);

    port_ = new QSpinBox(this);
    port_->setObjectName(QStringLiteral("pairingPort"));
    port_->setRange(0, 65535);
    port_->setValue(45454);
    port_->setSpecialValueText(QStringLiteral("Automático (solo al escuchar)"));
    form->addRow(QStringLiteral("Puerto:"), port_);

    expected_peer_ = new QLineEdit(this);
    expected_peer_->setObjectName(QStringLiteral("pairingExpectedPeer"));
    expected_peer_->setPlaceholderText(
        QStringLiteral("Opcional: ID del equipo que esperas"));
    form->addRow(QStringLiteral("Equipo esperado:"), expected_peer_);
    layout->addLayout(form);

    auto* sas_box = new QGroupBox(QStringLiteral("Código de confirmación SAS"), this);
    auto* sas_layout = new QVBoxLayout(sas_box);
    sas_ = new QLabel(QStringLiteral("—"), sas_box);
    sas_->setObjectName(QStringLiteral("pairingSas"));
    sas_->setTextFormat(Qt::PlainText);
    sas_->setAlignment(Qt::AlignCenter);
    sas_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    sas_->setStyleSheet(QStringLiteral("font-size: 30px; letter-spacing: 8px;"));
    sas_layout->addWidget(sas_);
    status_ = new QLabel(QStringLiteral("Sin iniciar"), sas_box);
    status_->setObjectName(QStringLiteral("pairingStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    status_->setAlignment(Qt::AlignCenter);
    sas_layout->addWidget(status_);
    layout->addWidget(sas_box);

    auto* buttons = new QHBoxLayout();
    start_ = new QPushButton(QStringLiteral("Iniciar emparejamiento"), this);
    start_->setObjectName(QStringLiteral("pairingStart"));
    confirm_ = new QPushButton(QStringLiteral("Confirmar código"), this);
    confirm_->setObjectName(QStringLiteral("pairingConfirm"));
    confirm_->setEnabled(false);
    cancel_ = new QPushButton(QStringLiteral("Cancelar"), this);
    cancel_->setObjectName(QStringLiteral("pairingCancel"));
    buttons->addWidget(start_);
    buttons->addWidget(confirm_);
    buttons->addStretch(1);
    buttons->addWidget(cancel_);
    layout->addLayout(buttons);

    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateMode(); });
    connect(start_, &QPushButton::clicked, this, [this] { startPairing(); });
    connect(confirm_, &QPushButton::clicked, this, [this] {
        if (!pairing_) {
            return;
        }
        confirm_->setEnabled(false);
        status_->setText(QStringLiteral("Esperando la confirmación del otro equipo…"));
        pairing_->confirm();
    });
    connect(cancel_, &QPushButton::clicked, this, [this] { reject(); });
    connect(rotate_, &QPushButton::clicked, this, [this] {
        if (!rotation_handler_ || transport_) {
            return;
        }
        if (QMessageBox::question(
                this, QStringLiteral("Rotar certificado"),
                QStringLiteral(
                    "Se generará una identidad TLS nueva. Los equipos ya "
                    "emparejados tendrán que confirmar de nuevo la SAS. "
                    "¿Continuar?")) != QMessageBox::Yes) {
            return;
        }
        auto rotated = rotation_handler_();
        if (!rotated.has_value()) {
            status_->setText(QStringLiteral("No se pudo rotar el certificado"));
            return;
        }
        identity_ = std::move(*rotated);
        local_info_->setText(QStringLiteral("ID: %1\nHuella: %2")
                                 .arg(QString::fromStdString(identity_.device_id))
                                 .arg(QString::fromStdString(identity_.fingerprint)));
        status_->setText(QStringLiteral(
            "Certificado rotado. Vuelve a emparejar con el otro equipo."));
    });
    updateMode();
}

PairingDialog::~PairingDialog() {
    pairing_.reset();
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

std::optional<PairingResult> PairingDialog::takeResult() {
    if (!result_) {
        return std::nullopt;
    }
    // El diálogo ya terminó; ahora sí se puede destruir la sesión SAS sin que
    // su destructor borre los handlers del transporte que Application usará.
    pairing_.reset();
    std::optional<PairingResult> result(std::move(*result_));
    result_.reset();
    return result;
}

void PairingDialog::updateMode() {
    const bool connect = mode_->currentIndex() == 1;
    host_->setEnabled(connect);
    host_->setVisible(connect);
    if (connect) {
        port_->setSpecialValueText(QString());
    } else {
        port_->setSpecialValueText(QStringLiteral("Automático (solo al escuchar)"));
        if (port_->value() == 0) {
            port_->setValue(45454);
        }
    }
}

void PairingDialog::setRotationHandler(
    std::function<std::optional<DeviceIdentity>()> handler) {
    rotation_handler_ = std::move(handler);
}

void PairingDialog::setFieldsEnabled(bool enabled) {
    mode_->setEnabled(enabled);
    name_->setEnabled(enabled);
    expected_peer_->setEnabled(enabled);
    port_->setEnabled(enabled);
    if (enabled) {
        updateMode();
    } else {
        host_->setEnabled(false);
    }
}

void PairingDialog::startPairing() {
    if (pairing_ || transport_) {
        return;
    }
    const bool listen = mode_->currentIndex() == 0;
    const QString name = name_->text().trimmed();
    if (name.isEmpty()) {
        fail(QStringLiteral("Escribe un nombre para este equipo"));
        return;
    }
    QString host;
    if (!listen) {
        host = host_->text().trimmed();
        if (host.isEmpty() || port_->value() == 0) {
            fail(QStringLiteral("Indica la dirección y el puerto del otro equipo"));
            return;
        }
    }

    TlsTransportConfig config;
    config.mode = listen ? TlsTransportMode::listen : TlsTransportMode::connect;
    config.certificate_pem = identity_.certificate_pem;
    config.private_key_pem = identity_.private_key_pem;
    // Solo durante esta ceremonia: la SAS sustituye a la huella fijada hasta
    // que las dos personas confirman el mismo código.
    config.allow_unverified = true;
    if (listen) {
        config.bind_address = "0.0.0.0";
        config.listen_port = static_cast<std::uint16_t>(port_->value());
    } else {
        config.peer_host = host.toStdString();
        config.peer_port = static_cast<std::uint16_t>(port_->value());
    }

    transport_ = std::make_unique<QtTlsTransport>(std::move(config));
    transport_->setReadyHandler([this] { beginSession(); });
    transport_->setFailureHandler(
        [this](std::string message) { fail(QString::fromStdString(message)); });

    std::string error;
    if (!transport_->start(&error)) {
        fail(QString::fromStdString(
            error.empty() ? std::string("No se pudo iniciar el emparejamiento")
                          : error));
        return;
    }
    setFieldsEnabled(false);
    start_->setEnabled(false);
    rotate_->setEnabled(false);
    sas_->setText(QStringLiteral("—"));
    if (listen) {
        listen_port_ = transport_->listeningPort();
        port_->setValue(listen_port_);
        status_->setText(QStringLiteral("Escuchando en el puerto %1. Esperando "
                                        "al otro equipo…")
                             .arg(listen_port_));
    } else {
        status_->setText(QStringLiteral("Conectando con %1:%2…")
                             .arg(host)
                             .arg(port_->value()));
    }
}

void PairingDialog::beginSession() {
    if (!transport_) {
        return;
    }
    const bool listen = mode_->currentIndex() == 0;
    if (listen) {
        listen_port_ = transport_->listeningPort();
    }
    PairingLocal local;
    local.device_id = identity_.device_id;
    local.name = name_->text().trimmed().toStdString();
    local.fingerprint = identity_.fingerprint;
    if (local.name.empty()) {
        local.name = "Equipo " + identity_.device_id;
    }

    PairingCallbacks callbacks;
    callbacks.sas_ready = [this](std::string code, PairingPeer) {
        showSas(std::move(code));
    };
    callbacks.completed = [this](PairingPeer peer) { complete(std::move(peer)); };
    callbacks.failed = [this](std::string message) {
        fail(QString::fromStdString(message));
    };
    pairing_ = std::make_unique<PairingSession>(
        *transport_, std::move(local),
        expected_peer_->text().trimmed().toStdString(), std::move(callbacks));
    pairing_->start();
    if (pairing_->state() == PairingState::failed) {
        return;
    }
    status_->setText(QStringLiteral("Esperando la identidad del otro equipo…"));
}

void PairingDialog::showSas(std::string code) {
    sas_->setText(QString::fromStdString(code));
    status_->setText(
        QStringLiteral("Comprueba que este código coincide en los dos equipos. "
                       "Si no coincide, cancela."));
    confirm_->setEnabled(true);
}

void PairingDialog::complete(PairingPeer peer) {
    if (!transport_) {
        return;
    }
    const bool listen = mode_->currentIndex() == 0;
    TrustedDevice device;
    device.device_id = peer.device_id;
    device.name = peer.name;
    // Un host vacío indica que este equipo escucha en `port`; el otro extremo
    // guarda la dirección concreta a la que conectarse.
    device.host = listen ? std::string() : host_->text().trimmed().toStdString();
    device.port = listen ? listen_port_
                         : static_cast<std::uint16_t>(port_->value());
    device.pins = {peer.fingerprint};
    std::string error;
    const bool re_pairing = store_.find(device.device_id).has_value();
    if (re_pairing) {
        // Si el certificado del peer cambió, conserva el pin anterior como
        // respaldo temporal y deja el observado ahora como huella principal.
        if (!store_.rotatePin(device.device_id, device.pins.front(), &error)) {
            fail(QString::fromStdString(
                error.empty()
                    ? std::string("No se pudo rotar el pin del equipo emparejado")
                    : error));
            return;
        }
        auto updated = store_.find(device.device_id);
        if (!updated.has_value()) {
            fail(QStringLiteral("No se pudo leer el equipo reemparejado"));
            return;
        }
        // rotatePin conserva el pin anterior; save actualiza el resto de los
        // datos sin perder el orden nuevo/antiguo.
        updated->name = device.name;
        updated->host = device.host;
        updated->port = device.port;
        if (!store_.save(*updated, &error)) {
            fail(QString::fromStdString(
                error.empty()
                    ? std::string("No se pudo actualizar el equipo reemparejado")
                    : error));
            return;
        }
        device = std::move(*updated);
    } else if (!store_.save(device, &error)) {
        fail(QString::fromStdString(
            error.empty() ? std::string("No se pudo guardar el equipo emparejado")
                          : error));
        return;
    }

    transport_->setReadyHandler({});
    transport_->setFailureHandler({});
    // No destruyas PairingSession aquí: complete() puede ejecutarse dentro de
    // su propio confirm(). El diálogo la libera al cerrarse.
    auto result = std::make_unique<PairingResult>();
    result->peer = std::move(device);
    result->transport = std::move(transport_);
    result->local_was_listener = listen;
    result_ = std::move(result);
    accept();
}

void PairingDialog::fail(const QString& message) {
    if (transport_) {
        transport_->close();
    }
    // La limpieza se difiere: fail() puede venir de un callback ejecutado
    // dentro de PairingSession::start(), confirm() o receive().
    if (pairing_ || transport_) {
        QTimer::singleShot(0, this, [this] {
            pairing_.reset();
            if (transport_) {
                transport_->close();
                transport_.reset();
            }
            if (start_) {
                start_->setEnabled(true);
            }
        });
        start_->setEnabled(false);
    } else {
        start_->setEnabled(true);
    }
    sas_->setText(QStringLiteral("—"));
    confirm_->setEnabled(false);
    status_->clear();
    status_->setText(message);
    setFieldsEnabled(true);
    rotate_->setEnabled(true);
}

void PairingDialog::reject() {
    if (pairing_) {
        pairing_->reject("cancelado");
        pairing_.reset();
    }
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
    QDialog::reject();
}

}  // namespace oneintwo
