#include "app/coordinator.hpp"
#include "app/main_window.hpp"
#include "features/visual/visual_codec.hpp"
#include "integration/feature_contracts.hpp"

#include "../session/test_support.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;

namespace {

FeatureSet allFeatures() {
    return FeatureSet{true, true, true, true, true, true, true, true, true};
}

FeatureSet intersect(FeatureSet left, FeatureSet right) {
    return FeatureSet{
        left.text_clipboard && right.text_clipboard,
        left.file_clipboard && right.file_clipboard,
        left.input && right.input,
        left.direct_files && right.direct_files,
        left.links && right.links,
        left.share_monitor && right.share_monitor,
        left.share_window && right.share_window,
        left.manual_notifications && right.manual_notifications,
        left.forwarded_notifications && right.forwarded_notifications,
    };
}

struct FakeSession : SessionPort {
    bool is_established = false;
    SessionGeneration current_generation = 7;
    FeatureSet granted_send{};
    FeatureSet granted_receive{};
    FeatureSet limits = allFeatures();
    std::vector<FeatureMessage> sent;
    SendResult next_result = SendResult::queued;

    SendResult send(FeatureMessage message) override {
        if (next_result == SendResult::queued) {
            sent.push_back(std::move(message));
        }
        return next_result;
    }

    void applyProfileLimits(FeatureSet value) override {
        limits = value;
    }

    void setGrantedFeatures(FeatureSet send_features, FeatureSet receive_features) override {
        granted_send = send_features;
        granted_receive = receive_features;
    }

    FeatureSet effectiveSendFeatures() const override {
        return intersect(granted_send, limits);
    }

    FeatureSet effectiveReceiveFeatures() const override {
        return intersect(granted_receive, limits);
    }

    SessionGeneration generation() const override {
        return is_established ? current_generation : 0;
    }

    bool established() const override {
        return is_established;
    }

    template <typename T>
    std::vector<T> sentOf() const {
        std::vector<T> out;
        for (const FeatureMessage& message : sent) {
            if (const auto* value = std::get_if<T>(&message)) {
                out.push_back(*value);
            }
        }
        return out;
    }
};

struct FakeCapture : DesktopVisualCapture {
    VisualCaptureCapabilities caps{true, true};
    int select_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    VisualShareRef selected_ref{};
    VisualShareRef started_ref{};
    VisualCaptureCallbacks callbacks;
    std::function<void(VisualShareRef, DesktopResult)> selection;

    VisualCaptureCapabilities visualCapabilities() const override {
        return caps;
    }

    void selectSource(VisualShareRef offer, VisualSourceKind,
                      std::function<void(VisualShareRef, DesktopResult)> selected) override {
        ++select_calls;
        selected_ref = offer;
        selection = std::move(selected);
    }

    void startCapture(VisualShareRef offer, VisualCaptureCallbacks value) override {
        ++start_calls;
        started_ref = offer;
        callbacks = std::move(value);
    }

    void stopCapture(VisualShareRef) override {
        ++stop_calls;
        selection = nullptr;
        callbacks = {};
    }

    void completeSelection(DesktopResult result) {
        auto callback = std::move(selection);
        selection = nullptr;
        if (callback) {
            callback(selected_ref, result);
        }
    }

    void emitFrame(VisualShareRef ref, VisualFrame frame) {
        if (callbacks.frame) {
            callbacks.frame(ref, std::move(frame));
        }
    }
};

struct FakeViewer : VisualViewer {
    int opens = 0;
    int presents = 0;
    int clears = 0;
    int closes = 0;
    VisualShareOffer opened{};
    VisualFrame last_frame;
    std::function<void(VisualShareRef)> user_closed;

    void open(const VisualShareOffer& offer) override {
        ++opens;
        opened = offer;
    }

    void present(VisualShareRef, const VisualFrame& frame) override {
        ++presents;
        last_frame = frame;
    }

    void clear(VisualShareRef, VisualSourceState) override {
        ++clears;
    }

    void close(VisualShareRef) override {
        ++closes;
    }

    void triggerUserClose(VisualShareRef ref) {
        if (user_closed) {
            user_closed(ref);
        }
    }
};

struct FakeNotifications : NotificationPort {
    SessionGeneration generation = 0;
    FeatureSet send{};
    FeatureSet receive{};
    std::set<std::string> applications;
    std::vector<NotificationContent> manual;
    std::vector<RemoteNotification> notices;
    int ticks = 0;
    int authorization_updates = 0;

    void setAuthorization(SessionGeneration value, FeatureSet send_features,
                          FeatureSet receive_features) override {
        generation = value;
        send = send_features;
        receive = receive_features;
        ++authorization_updates;
    }

    void setAllowedApplications(const std::set<std::string>& app_ids) override {
        applications = app_ids;
    }

    void tick() override {
        ++ticks;
    }

    void sendManual(SessionGeneration, const NotificationContent& content) override {
        manual.push_back(content);
    }

    void receiveNotice(const RemoteNotification& notice) override {
        notices.push_back(notice);
    }
};

VisualFrame makeBgra(std::uint32_t width, std::uint32_t height) {
    VisualFrame frame;
    frame.size = VisualSize{width, height};
    frame.bgra.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t index = 0; index < frame.bgra.size(); index += 4) {
        frame.bgra[index + 0] = 20;
        frame.bgra[index + 1] = 120;
        frame.bgra[index + 2] = 200;
        frame.bgra[index + 3] = 255;
    }
    return frame;
}

struct Harness {
    std::deque<Task> tasks;
    FakeSession session;
    FakeCapture capture;
    FakeNotifications notifications;
    MainWindow window;
    FakeViewer* viewer = nullptr;
    std::unique_ptr<Coordinator> coordinator;

    Harness() {
        ModuleRuntime runtime;
        runtime.now = [] { return std::chrono::steady_clock::now(); };
        runtime.post = [this](Task task) { tasks.push_back(std::move(task)); };
        CoordinatorDependencies dependencies{
            runtime,
            session,
            capture,
            notifications,
            window,
            [this](ModuleRuntime, std::function<void(VisualShareRef)> closed) {
                auto fake = std::make_unique<FakeViewer>();
                fake->user_closed = std::move(closed);
                viewer = fake.get();
                return fake;
            },
            "Equipo B",
        };
        coordinator = std::make_unique<Coordinator>(std::move(dependencies));
    }

    void establish() {
        session.is_established = true;
        coordinator->sessionEstablished(session.current_generation);
    }

    void drain() {
        std::size_t guard = 0;
        while (!tasks.empty() && guard++ < 10000) {
            Task task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
    }

    QPushButton* button(const char* name) {
        return window.findChild<QPushButton*>(QString::fromLatin1(name));
    }

    QCheckBox* checkBox(const char* name) {
        return window.findChild<QCheckBox*>(QString::fromLatin1(name));
    }

    QLabel* label(const char* name) {
        return window.findChild<QLabel*>(QString::fromLatin1(name));
    }

    void setVisualPermissions(bool send_monitor, bool receive_monitor) {
        checkBox("permissionSendMonitor")->setChecked(send_monitor);
        checkBox("permissionReceiveMonitor")->setChecked(receive_monitor);
    }

    void setNoticePermissions(bool send_manual) {
        checkBox("permissionSendManual")->setChecked(send_manual);
    }
};

bool sameRef(const VisualShareRef& left, const VisualShareRef& right) {
    return left.generation == right.generation && left.initiator == right.initiator &&
           left.offer_id == right.offer_id;
}

} // namespace

TEST_CASE(main_window_is_wired_to_session_and_profile) {
    Harness harness;
    CHECK_EQ(harness.session.limits.share_monitor, true); // Habitual por defecto
    CHECK_EQ(harness.session.granted_send.share_monitor, false);
    CHECK_EQ(harness.notifications.generation, static_cast<SessionGeneration>(0));
    CHECK(!harness.button("shareMonitorButton")->isEnabled());

    harness.establish();
    CHECK(harness.button("shareMonitorButton")->isEnabled());
    CHECK_EQ(harness.notifications.generation, static_cast<SessionGeneration>(7));
    CHECK_EQ(harness.label("statusLabel")->text().toStdString(), std::string("Sesión establecida"));

    auto* combo = harness.window.findChild<QComboBox*>(QStringLiteral("profileCombo"));
    CHECK(combo != nullptr);
    combo->setCurrentIndex(2); // Pausa
    CHECK_EQ(harness.coordinator->profile(), ProfileId::paused);
    CHECK_EQ(harness.session.limits.share_monitor, false);
    CHECK_EQ(harness.session.limits.manual_notifications, false);

    combo->setCurrentIndex(0); // Habitual
    CHECK_EQ(harness.session.limits.share_monitor, true);
}

TEST_CASE(permission_checks_reach_session_and_visual_module) {
    Harness harness;
    harness.establish();
    harness.button("shareMonitorButton")->click();
    CHECK_EQ(harness.capture.select_calls, 0);
    CHECK(!harness.label("errorLabel")->text().isEmpty());

    harness.setVisualPermissions(true, true);
    CHECK(harness.session.granted_send.share_monitor);
    CHECK(harness.session.granted_receive.share_monitor);
    harness.button("shareMonitorButton")->click();
    CHECK_EQ(harness.capture.select_calls, 1);
    CHECK(harness.coordinator->hasActiveVisual() == false); // aún seleccionando
}

TEST_CASE(origin_share_stop_and_encoded_frames_cross_the_coordinator) {
    Harness harness;
    harness.establish();
    harness.setVisualPermissions(true, true);
    harness.button("shareMonitorButton")->click();
    CHECK_EQ(harness.capture.select_calls, 1);
    harness.capture.completeSelection(DesktopResult::ready);
    harness.drain();

    auto offers = harness.session.sentOf<VisualShareOffer>();
    CHECK_EQ(offers.size(), static_cast<std::size_t>(1));
    CHECK_EQ(offers.front().source, VisualSourceKind::monitor);
    const VisualShareRef ref = offers.front().ref;
    CHECK(harness.coordinator->hasActiveVisual());

    harness.coordinator->receive(VisualDecisionMessage{ref, VisualDecision::accepted});
    harness.drain();
    CHECK_EQ(harness.capture.start_calls, 1);

    harness.capture.emitFrame(ref, makeBgra(64, 32));
    harness.drain();
    const auto frames = harness.session.sentOf<VisualEncodedFrame>();
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    CHECK(frames.front().key_frame);

    harness.button("stopButton")->click();
    CHECK_EQ(harness.session.sentOf<VisualStopMessage>().size(), static_cast<std::size_t>(1));
    CHECK(harness.capture.stop_calls >= 1);
    CHECK(!harness.coordinator->hasActiveVisual());
    CHECK(!harness.button("stopButton")->isEnabled());
}

TEST_CASE(stale_messages_do_not_clear_a_newer_visual_operation) {
    Harness harness;
    harness.establish();
    harness.setVisualPermissions(true, true);
    harness.button("shareMonitorButton")->click();
    harness.capture.completeSelection(DesktopResult::ready);
    harness.drain();
    CHECK(harness.coordinator->hasActiveVisual());

    const VisualShareRef stale{7, VisualInitiator::local, 999};
    harness.coordinator->receive(VisualDecisionMessage{stale, VisualDecision::rejected});
    harness.coordinator->receive(
        VisualStateMessage{stale, 1, VisualSourceState::closed});
    harness.coordinator->receive(VisualStopMessage{stale});
    CHECK(harness.coordinator->hasActiveVisual());
    CHECK(harness.button("stopButton")->isEnabled());
    CHECK(harness.session.sentOf<VisualStopMessage>().empty());

    harness.button("stopButton")->click();
    CHECK(!harness.coordinator->hasActiveVisual());
}

TEST_CASE(incoming_offer_is_presented_and_acceptance_opens_viewer) {
    Harness harness;
    harness.establish();
    harness.setVisualPermissions(true, true);
    const VisualShareOffer offer{VisualShareRef{7, VisualInitiator::peer, 1},
                                 VisualSourceKind::monitor};
    harness.coordinator->receive(FeatureMessage{offer});
    auto* panel = harness.window.findChild<QWidget*>(QStringLiteral("offerPanel"));
    CHECK(panel != nullptr);
    CHECK(!panel->isHidden());
    CHECK(harness.label("offerLabel")->text().contains(QStringLiteral("Equipo B")));

    harness.button("acceptOfferButton")->click();
    CHECK_EQ(harness.session.sentOf<VisualDecisionMessage>().size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.session.sentOf<VisualDecisionMessage>().front().decision,
             VisualDecision::accepted);
    CHECK_EQ(harness.viewer->opens, 1);
    CHECK(harness.coordinator->hasActiveVisual());

    harness.coordinator->receive(
        VisualStateMessage{offer.ref, 1, VisualSourceState::available});
    CHECK_EQ(harness.viewer->clears, 1);

    VisualEncodedFrame encoded;
    encoded.ref = offer.ref;
    encoded.revision = 1;
    encoded.sequence = 1;
    encoded.size = VisualSize{64, 32};
    encoded.key_frame = true;
    std::string codec_error;
    const auto jpeg = encodeVisualFrame(offer.ref, 1, 1, makeBgra(64, 32), &codec_error);
    CHECK(jpeg.has_value());
    harness.coordinator->receive(FeatureMessage{*jpeg});
    CHECK_EQ(harness.viewer->presents, 1);

    harness.button("stopButton")->click();
    CHECK_EQ(harness.session.sentOf<VisualStopMessage>().size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.viewer->closes, 1);
}

TEST_CASE(viewer_user_close_requests_visual_stop) {
    Harness harness;
    harness.establish();
    harness.setVisualPermissions(true, true);
    const VisualShareOffer offer{VisualShareRef{7, VisualInitiator::peer, 3},
                                 VisualSourceKind::monitor};
    harness.coordinator->receive(FeatureMessage{offer});
    harness.button("acceptOfferButton")->click();
    CHECK_EQ(harness.viewer->opens, 1);

    harness.viewer->triggerUserClose(offer.ref);
    CHECK_EQ(harness.session.sentOf<VisualStopMessage>().size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.viewer->closes, 1);
    CHECK(!harness.coordinator->hasActiveVisual());
}

TEST_CASE(notice_form_and_application_list_reach_notification_module) {
    Harness harness;
    harness.establish();
    harness.setNoticePermissions(true);

    auto* title = harness.window.findChild<QLineEdit*>(QStringLiteral("noticeTitle"));
    auto* body = harness.window.findChild<QPlainTextEdit*>(QStringLiteral("noticeBody"));
    CHECK(title != nullptr);
    CHECK(body != nullptr);
    title->setText(QStringLiteral("Hola"));
    body->setPlainText(QStringLiteral("Mensaje manual"));
    harness.button("sendNoticeButton")->click();
    CHECK_EQ(harness.notifications.manual.size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.notifications.manual.front().title, std::string("Hola"));
    CHECK_EQ(harness.notifications.manual.front().body, std::string("Mensaje manual"));

    harness.setNoticePermissions(false);
    harness.button("sendNoticeButton")->click();
    CHECK_EQ(harness.notifications.manual.size(), static_cast<std::size_t>(1));

    auto* app_input = harness.window.findChild<QLineEdit*>(QStringLiteral("appInput"));
    CHECK(app_input != nullptr);
    app_input->setText(QStringLiteral("org.example.app"));
    harness.button("addAppButton")->click();
    CHECK_EQ(harness.notifications.applications.count("org.example.app"),
             static_cast<std::size_t>(1));

    auto* list = harness.window.findChild<QListWidget*>(QStringLiteral("appList"));
    CHECK(list != nullptr);
    CHECK_EQ(list->count(), 1);
    list->item(0)->setCheckState(Qt::Unchecked);
    CHECK(harness.notifications.applications.empty());
}

TEST_CASE(incoming_notice_and_lock_are_routed) {
    Harness harness;
    harness.establish();
    harness.setNoticePermissions(true);
    RemoteNotification notice;
    notice.ref = NotificationRef{7, 42};
    notice.mode = NotificationMode::manual;
    notice.content = NotificationContent{"Título", "Cuerpo"};
    harness.coordinator->receive(FeatureMessage{notice});
    CHECK_EQ(harness.notifications.notices.size(), static_cast<std::size_t>(1));

    harness.setVisualPermissions(true, true);
    harness.button("shareMonitorButton")->click();
    harness.capture.completeSelection(DesktopResult::ready);
    harness.drain();
    CHECK(harness.coordinator->hasActiveVisual());

    harness.coordinator->setLocked(true);
    CHECK_EQ(harness.session.sentOf<VisualStopMessage>().size(), static_cast<std::size_t>(1));
    CHECK(!harness.notifications.send.manual_notifications);
    CHECK_EQ(harness.label("statusLabel")->text().toStdString(), std::string("Sesión bloqueada"));

    harness.coordinator->setLocked(false);
    CHECK(harness.notifications.send.manual_notifications);
    CHECK_EQ(harness.label("statusLabel")->text().toStdString(), std::string("Sesión reanudada"));
}

TEST_CASE(session_close_retires_permissions_and_viewer) {
    Harness harness;
    harness.establish();
    harness.setVisualPermissions(true, true);
    CHECK(harness.button("shareMonitorButton")->isEnabled());
    harness.coordinator->sessionClosed();
    CHECK(!harness.button("shareMonitorButton")->isEnabled());
    CHECK_EQ(harness.notifications.generation, static_cast<SessionGeneration>(0));
    CHECK(!harness.notifications.send.share_monitor);
    CHECK_EQ(harness.label("statusLabel")->text().toStdString(), std::string("Sin sesión"));
}

TEST_CASE(share_without_session_reports_error_and_does_not_capture) {
    Harness harness;
    harness.coordinator->shareSource(VisualSourceKind::monitor);
    CHECK_EQ(harness.capture.select_calls, 0);
    CHECK(!harness.label("errorLabel")->text().isEmpty());
}

TEST_CASE(receive_permission_disabled_hides_invitation) {
    Harness harness;
    harness.establish();
    const VisualShareOffer offer{VisualShareRef{7, VisualInitiator::peer, 1},
                                 VisualSourceKind::monitor};
    harness.coordinator->receive(FeatureMessage{offer});
    auto* panel = harness.window.findChild<QWidget*>(QStringLiteral("offerPanel"));
    CHECK(panel->isHidden());
    CHECK(harness.session.sentOf<VisualDecisionMessage>().empty());
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return oneintwo_test::run();
}
