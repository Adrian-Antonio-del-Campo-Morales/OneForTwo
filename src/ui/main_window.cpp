#include "../app/main_window.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace oneintwo {
namespace {

QString modalityName(VisualSourceKind source) {
    return source == VisualSourceKind::monitor ? QStringLiteral("Pantalla completa")
                                               : QStringLiteral("Una ventana");
}

QString joinFeatures(const FeatureSet& features) {
    QStringList parts;
    if (features.share_monitor) {
        parts << QStringLiteral("pantalla");
    }
    if (features.share_window) {
        parts << QStringLiteral("ventana");
    }
    if (features.manual_notifications) {
        parts << QStringLiteral("avisos manuales");
    }
    if (features.forwarded_notifications) {
        parts << QStringLiteral("reenvío");
    }
    if (features.text_clipboard) {
        parts << QStringLiteral("texto");
    }
    if (features.file_clipboard) {
        parts << QStringLiteral("archivos");
    }
    if (features.input) {
        parts << QStringLiteral("entrada");
    }
    if (features.links) {
        parts << QStringLiteral("enlaces");
    }
    return parts.isEmpty() ? QStringLiteral("ninguna") : parts.join(QStringLiteral(", "));
}

QListWidgetItem* makeApplicationItem(const std::string& app_id) {
    auto* item = new QListWidgetItem(QString::fromStdString(app_id));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
    return item;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("OneInTwo"));
    resize(980, 760);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout();
    peer_label_ = new QLabel(QStringLiteral("Par: sin identificar"), central);
    peer_label_->setObjectName(QStringLiteral("peerLabel"));
    status_label_ = new QLabel(QStringLiteral("Sin sesión"), central);
    status_label_->setObjectName(QStringLiteral("statusLabel"));
    header->addWidget(peer_label_);
    header->addStretch(1);
    header->addWidget(status_label_);
    layout->addLayout(header);

    auto* profile_row = new QHBoxLayout();
    profile_row->addWidget(new QLabel(QStringLiteral("Perfil:"), central));
    profile_combo_ = new QComboBox(central);
    profile_combo_->setObjectName(QStringLiteral("profileCombo"));
    profile_combo_->addItem(QStringLiteral("Habitual"), static_cast<int>(ProfileId::usual));
    profile_combo_->addItem(QStringLiteral("Solo envíos"), static_cast<int>(ProfileId::sends_only));
    profile_combo_->addItem(QStringLiteral("Pausa"), static_cast<int>(ProfileId::paused));
    profile_row->addWidget(profile_combo_);
    profile_row->addStretch(1);
    auto* quit = new QPushButton(QStringLiteral("Salir"), central);
    quit->setObjectName(QStringLiteral("quitButton"));
    profile_row->addWidget(quit);
    layout->addLayout(profile_row);

    auto* share_box = new QGroupBox(QStringLiteral("Compartir en directo"), central);
    auto* share_layout = new QHBoxLayout(share_box);
    share_monitor_ = new QPushButton(QStringLiteral("Compartir pantalla"), share_box);
    share_monitor_->setObjectName(QStringLiteral("shareMonitorButton"));
    share_window_ = new QPushButton(QStringLiteral("Compartir ventana"), share_box);
    share_window_->setObjectName(QStringLiteral("shareWindowButton"));
    stop_button_ = new QPushButton(QStringLiteral("Detener"), share_box);
    stop_button_->setObjectName(QStringLiteral("stopButton"));
    visual_state_label_ = new QLabel(QStringLiteral("Sin operación visual"), share_box);
    visual_state_label_->setObjectName(QStringLiteral("visualActiveLabel"));
    share_layout->addWidget(share_monitor_);
    share_layout->addWidget(share_window_);
    share_layout->addWidget(stop_button_);
    share_layout->addWidget(visual_state_label_, 1);
    layout->addWidget(share_box);

    offer_panel_ = new QGroupBox(QStringLiteral("Invitación recibida"), central);
    offer_panel_->setObjectName(QStringLiteral("offerPanel"));
    auto* offer_layout = new QHBoxLayout(offer_panel_);
    offer_label_ = new QLabel(QStringLiteral("Sin invitación"), offer_panel_);
    offer_label_->setObjectName(QStringLiteral("offerLabel"));
    auto* accept = new QPushButton(QStringLiteral("Aceptar"), offer_panel_);
    accept->setObjectName(QStringLiteral("acceptOfferButton"));
    auto* reject = new QPushButton(QStringLiteral("Rechazar"), offer_panel_);
    reject->setObjectName(QStringLiteral("rejectOfferButton"));
    offer_layout->addWidget(offer_label_, 1);
    offer_layout->addWidget(accept);
    offer_layout->addWidget(reject);
    offer_panel_->hide();
    layout->addWidget(offer_panel_);

    auto* permissions = new QGroupBox(QStringLiteral("Permisos por sentido"), central);
    auto* grid = new QGridLayout(permissions);
    grid->addWidget(new QLabel(QStringLiteral("Modalidad"), permissions), 0, 0);
    grid->addWidget(new QLabel(QStringLiteral("Enviar"), permissions), 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Recibir"), permissions), 0, 2);
    const auto add_permission_row = [&](int row, const QString& name, QCheckBox*& send,
                                        QCheckBox*& receive, const QString& send_name,
                                        const QString& receive_name) {
        grid->addWidget(new QLabel(name, permissions), row, 0);
        send = new QCheckBox(permissions);
        receive = new QCheckBox(permissions);
        send->setObjectName(send_name);
        receive->setObjectName(receive_name);
        grid->addWidget(send, row, 1);
        grid->addWidget(receive, row, 2);
    };
    add_permission_row(1, QStringLiteral("Pantalla completa"), send_monitor_, receive_monitor_,
                       QStringLiteral("permissionSendMonitor"),
                       QStringLiteral("permissionReceiveMonitor"));
    add_permission_row(2, QStringLiteral("Una ventana"), send_window_, receive_window_,
                       QStringLiteral("permissionSendWindow"),
                       QStringLiteral("permissionReceiveWindow"));
    add_permission_row(3, QStringLiteral("Avisos manuales"), send_manual_, receive_manual_,
                       QStringLiteral("permissionSendManual"),
                       QStringLiteral("permissionReceiveManual"));
    add_permission_row(4, QStringLiteral("Reenvío automático"), send_forwarded_,
                       receive_forwarded_, QStringLiteral("permissionSendForwarded"),
                       QStringLiteral("permissionReceiveForwarded"));
    layout->addWidget(permissions);

    auto* notice_box = new QGroupBox(QStringLiteral("Enviar aviso manual"), central);
    auto* notice_layout = new QGridLayout(notice_box);
    notice_title_ = new QLineEdit(notice_box);
    notice_title_->setObjectName(QStringLiteral("noticeTitle"));
    notice_title_->setPlaceholderText(QStringLiteral("Título"));
    notice_body_ = new QPlainTextEdit(notice_box);
    notice_body_->setObjectName(QStringLiteral("noticeBody"));
    notice_body_->setPlaceholderText(QStringLiteral("Cuerpo del aviso"));
    notice_body_->setMaximumHeight(90);
    auto* send_notice = new QPushButton(QStringLiteral("Enviar aviso"), notice_box);
    send_notice->setObjectName(QStringLiteral("sendNoticeButton"));
    notice_layout->addWidget(notice_title_, 0, 0, 1, 2);
    notice_layout->addWidget(notice_body_, 1, 0, 1, 2);
    notice_layout->addWidget(send_notice, 2, 1);
    layout->addWidget(notice_box);

    auto* apps_box = new QGroupBox(QStringLiteral("Aplicaciones autorizadas para reenvío"), central);
    auto* apps_layout = new QVBoxLayout(apps_box);
    app_list_ = new QListWidget(apps_box);
    app_list_->setObjectName(QStringLiteral("appList"));
    app_list_->setMaximumHeight(110);
    auto* app_row = new QHBoxLayout();
    app_input_ = new QLineEdit(apps_box);
    app_input_->setObjectName(QStringLiteral("appInput"));
    app_input_->setPlaceholderText(QStringLiteral("Identificador de aplicación"));
    auto* add_app = new QPushButton(QStringLiteral("Añadir"), apps_box);
    add_app->setObjectName(QStringLiteral("addAppButton"));
    app_row->addWidget(app_input_, 1);
    app_row->addWidget(add_app);
    apps_layout->addWidget(app_list_);
    apps_layout->addLayout(app_row);
    layout->addWidget(apps_box);

    effective_label_ = new QLabel(central);
    effective_label_->setObjectName(QStringLiteral("effectiveLabel"));
    effective_label_->setWordWrap(true);
    layout->addWidget(effective_label_);

    error_label_ = new QLabel(central);
    error_label_->setObjectName(QStringLiteral("errorLabel"));
    error_label_->setStyleSheet(QStringLiteral("color:#b00020;"));
    error_label_->setWordWrap(true);
    layout->addWidget(error_label_);
    layout->addStretch(1);
    setCentralWidget(central);

    updateEffectiveLabel();
    setVisualActive(false, std::string{});
    setSessionAvailable(false);

    connect(share_monitor_, &QPushButton::clicked, this, [this] {
        if (actions_.share_requested) {
            actions_.share_requested(VisualSourceKind::monitor);
        }
    });
    connect(share_window_, &QPushButton::clicked, this, [this] {
        if (actions_.share_requested) {
            actions_.share_requested(VisualSourceKind::window);
        }
    });
    connect(stop_button_, &QPushButton::clicked, this, [this] {
        if (actions_.stop_requested) {
            actions_.stop_requested();
        }
    });
    connect(profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (!actions_.profile_changed) {
                    return;
                }
                actions_.profile_changed(
                    static_cast<ProfileId>(profile_combo_->itemData(index).toInt()));
            });
    connect(accept, &QPushButton::clicked, this, [this] {
        if (current_offer_.has_value() && actions_.offer_decision) {
            actions_.offer_decision(current_offer_->ref, VisualDecision::accepted);
        }
        dismissVisualOffer();
    });
    connect(reject, &QPushButton::clicked, this, [this] {
        if (current_offer_.has_value() && actions_.offer_decision) {
            actions_.offer_decision(current_offer_->ref, VisualDecision::rejected);
        }
        dismissVisualOffer();
    });
    connect(send_notice, &QPushButton::clicked, this, [this] {
        if (!actions_.notice_requested) {
            return;
        }
        NotificationContent content;
        content.title = notice_title_->text().toStdString();
        content.body = notice_body_->toPlainText().toStdString();
        actions_.notice_requested(std::move(content));
        notice_body_->clear();
    });
    connect(add_app, &QPushButton::clicked, this, [this] {
        const QString text = app_input_->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        app_list_->addItem(makeApplicationItem(text.toStdString()));
        app_input_->clear();
        emitAllowedApplications();
    });
    connect(app_list_, &QListWidget::itemChanged, this,
            [this](QListWidgetItem*) { emitAllowedApplications(); });
    connect(quit, &QPushButton::clicked, this, [] { QCoreApplication::quit(); });

    for (QCheckBox* box : {send_monitor_, receive_monitor_, send_window_, receive_window_,
                           send_manual_, receive_manual_, send_forwarded_, receive_forwarded_}) {
        connect(box, &QCheckBox::toggled, this, [this](bool) { emitPermissions(); });
    }
}

void MainWindow::bindActions(UiActions actions) {
    actions_ = std::move(actions);
}

void MainWindow::showVisualOffer(const VisualShareOffer& offer, const std::string& peer_name) {
    current_offer_ = offer;
    offer_label_->setText(QStringLiteral("Invitación de %1: %2")
                              .arg(QString::fromStdString(peer_name))
                              .arg(modalityName(offer.source)));
    offer_panel_->show();
    show();
    raise();
}

void MainWindow::dismissVisualOffer() {
    current_offer_.reset();
    offer_panel_->hide();
    offer_label_->setText(QStringLiteral("Sin invitación"));
}

void MainWindow::setVisualActive(bool active, const std::string& description) {
    visual_active_ = active;
    stop_button_->setEnabled(active);
    share_monitor_->setEnabled(session_available_ && !active);
    share_window_->setEnabled(session_available_ && !active);
    visual_state_label_->setText(description.empty() ? QStringLiteral("Sin operación visual")
                                                      : QString::fromStdString(description));
}

void MainWindow::setSessionAvailable(bool available) {
    session_available_ = available;
    share_monitor_->setEnabled(available && !visual_active_);
    share_window_->setEnabled(available && !visual_active_);
}

void MainWindow::setEffectiveFeatures(FeatureSet send, FeatureSet receive) {
    effective_send_ = send;
    effective_receive_ = receive;
    updateEffectiveLabel();
}

void MainWindow::setStatus(const std::string& status) {
    status_label_->setText(QString::fromStdString(status));
}

void MainWindow::setError(const std::string& error) {
    error_label_->setText(QString::fromStdString(error));
}

void MainWindow::setPeerName(const std::string& peer_name) {
    peer_label_->setText(QStringLiteral("Par: %1").arg(QString::fromStdString(peer_name)));
}

void MainWindow::setProfileSelection(ProfileId profile) {
    const QSignalBlocker blocker(profile_combo_);
    const int index = profile_combo_->findData(static_cast<int>(profile));
    if (index >= 0) {
        profile_combo_->setCurrentIndex(index);
    }
}

void MainWindow::setAllowedApplications(const std::set<std::string>& app_ids) {
    const QSignalBlocker blocker(app_list_);
    app_list_->clear();
    for (const std::string& app_id : app_ids) {
        app_list_->addItem(makeApplicationItem(app_id));
    }
}

void MainWindow::setPermissionChecks(FeatureSet send, FeatureSet receive) {
    const QSignalBlocker block_send_monitor(send_monitor_);
    const QSignalBlocker block_receive_monitor(receive_monitor_);
    const QSignalBlocker block_send_window(send_window_);
    const QSignalBlocker block_receive_window(receive_window_);
    const QSignalBlocker block_send_manual(send_manual_);
    const QSignalBlocker block_receive_manual(receive_manual_);
    const QSignalBlocker block_send_forwarded(send_forwarded_);
    const QSignalBlocker block_receive_forwarded(receive_forwarded_);
    send_monitor_->setChecked(send.share_monitor);
    receive_monitor_->setChecked(receive.share_monitor);
    send_window_->setChecked(send.share_window);
    receive_window_->setChecked(receive.share_window);
    send_manual_->setChecked(send.manual_notifications);
    receive_manual_->setChecked(receive.manual_notifications);
    send_forwarded_->setChecked(send.forwarded_notifications);
    receive_forwarded_->setChecked(receive.forwarded_notifications);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Cerrar la configuración conserva el agente y el visor independiente.
    hide();
    event->ignore();
}

void MainWindow::emitPermissions() {
    if (!actions_.permissions_changed) {
        return;
    }
    FeatureSet send{};
    FeatureSet receive{};
    send.share_monitor = send_monitor_->isChecked();
    receive.share_monitor = receive_monitor_->isChecked();
    send.share_window = send_window_->isChecked();
    receive.share_window = receive_window_->isChecked();
    send.manual_notifications = send_manual_->isChecked();
    receive.manual_notifications = receive_manual_->isChecked();
    send.forwarded_notifications = send_forwarded_->isChecked();
    receive.forwarded_notifications = receive_forwarded_->isChecked();
    actions_.permissions_changed(send, receive);
}

void MainWindow::emitAllowedApplications() {
    if (actions_.allowed_applications_changed) {
        actions_.allowed_applications_changed(checkedApplications());
    }
}

std::set<std::string> MainWindow::checkedApplications() const {
    std::set<std::string> apps;
    for (int row = 0; row < app_list_->count(); ++row) {
        const QListWidgetItem* item = app_list_->item(row);
        if (item != nullptr && item->checkState() == Qt::Checked) {
            apps.insert(item->text().toStdString());
        }
    }
    return apps;
}

void MainWindow::updateEffectiveLabel() {
    effective_label_->setText(
        QStringLiteral("Efectivo · Envío: %1 | Recepción: %2")
            .arg(joinFeatures(effective_send_))
            .arg(joinFeatures(effective_receive_)));
}

} // namespace oneintwo
