#pragma once

#include "coordinator.hpp"

#include <QMainWindow>

#include <optional>
#include <set>
#include <string>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QWidget;
class QCloseEvent;

namespace oneintwo {

class MainWindow final : public QMainWindow, public CoordinatorView {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void bindActions(UiActions actions) override;
    void showVisualOffer(const VisualShareOffer& offer, const std::string& peer_name) override;
    void dismissVisualOffer() override;
    void setVisualActive(bool active, const std::string& description) override;
    void setSessionAvailable(bool available) override;
    void setEffectiveFeatures(FeatureSet send, FeatureSet receive) override;
    void setStatus(const std::string& status) override;
    void setError(const std::string& error) override;

    void setPeerName(const std::string& peer_name);
    void setProfileSelection(ProfileId profile);
    void setAllowedApplications(const std::set<std::string>& app_ids);
    void setPermissionChecks(FeatureSet send, FeatureSet receive);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void emitPermissions();
    void emitAllowedApplications();
    void updateEffectiveLabel();
    std::set<std::string> checkedApplications() const;

    UiActions actions_;
    std::optional<VisualShareOffer> current_offer_;

    QLabel* status_label_ = nullptr;
    QLabel* error_label_ = nullptr;
    QLabel* peer_label_ = nullptr;
    QLabel* effective_label_ = nullptr;
    QLabel* visual_state_label_ = nullptr;
    QComboBox* profile_combo_ = nullptr;
    QPushButton* share_monitor_ = nullptr;
    QPushButton* share_window_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QWidget* offer_panel_ = nullptr;
    QLabel* offer_label_ = nullptr;
    QCheckBox* send_monitor_ = nullptr;
    QCheckBox* receive_monitor_ = nullptr;
    QCheckBox* send_window_ = nullptr;
    QCheckBox* receive_window_ = nullptr;
    QCheckBox* send_manual_ = nullptr;
    QCheckBox* receive_manual_ = nullptr;
    QCheckBox* send_forwarded_ = nullptr;
    QCheckBox* receive_forwarded_ = nullptr;
    QLineEdit* notice_title_ = nullptr;
    QPlainTextEdit* notice_body_ = nullptr;
    QListWidget* app_list_ = nullptr;
    QLineEdit* app_input_ = nullptr;
    FeatureSet effective_send_{};
    FeatureSet effective_receive_{};
    bool visual_active_ = false;
    bool session_available_ = false;
};

} // namespace oneintwo
