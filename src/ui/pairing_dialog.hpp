#pragma once

#include "net/device_identity.hpp"
#include "storage/pairing_store.hpp"

#include <QDialog>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QString;

namespace oneintwo {

class PairingSession;
class QtTlsTransport;
struct PairingPeer;

struct PairingResult {
    TrustedDevice peer;
    std::unique_ptr<QtTlsTransport> transport;
    bool local_was_listener = false;
};

// Diálogo autocontenido: crea un canal TLS temporal, muestra una SAS y solo
// entrega el canal cuando las dos personas han confirmado el mismo código.
class PairingDialog final : public QDialog {
public:
    PairingDialog(DeviceIdentity identity, PairingStore& store,
                  QWidget* parent = nullptr);
    ~PairingDialog() override;

    std::optional<PairingResult> takeResult();
    void setRotationHandler(
        std::function<std::optional<DeviceIdentity>()> handler);

protected:
    void reject() override;

private:
    void startPairing();
    void beginSession();
    void showSas(std::string code);
    void complete(PairingPeer peer);
    void fail(const QString& message);
    void setFieldsEnabled(bool enabled);
    void updateMode();

    DeviceIdentity identity_;
    PairingStore& store_;
    std::unique_ptr<QtTlsTransport> transport_;
    std::unique_ptr<PairingSession> pairing_;
    std::unique_ptr<PairingResult> result_;
    std::uint16_t listen_port_ = 0;

    QComboBox* mode_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* host_ = nullptr;
    QSpinBox* port_ = nullptr;
    QLineEdit* expected_peer_ = nullptr;
    QLabel* local_info_ = nullptr;
    QLabel* sas_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* confirm_ = nullptr;
    QPushButton* rotate_ = nullptr;
    std::function<std::optional<DeviceIdentity>()> rotation_handler_;
    QPushButton* cancel_ = nullptr;
};

}  // namespace oneintwo
