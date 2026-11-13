#include "features/visual_share.hpp"
#include "integration/feature_contracts.hpp"

#include "../session/test_support.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QPixmap>
#include <QWidget>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace oneintwo;

namespace {

struct ViewerHarness {
    std::deque<Task> tasks;
    std::vector<VisualShareRef> user_closed;
    std::unique_ptr<VisualViewer> viewer;

    ViewerHarness() {
        ModuleRuntime runtime;
        runtime.now = [] { return std::chrono::steady_clock::now(); };
        runtime.post = [this](Task task) { tasks.push_back(std::move(task)); };
        viewer = makeVisualViewer(runtime, [this](VisualShareRef ref) {
            user_closed.push_back(ref);
        });
    }

    QWidget* widget() {
        return dynamic_cast<QWidget*>(viewer.get());
    }

    void drain() {
        while (!tasks.empty()) {
            Task task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
        QCoreApplication::processEvents();
    }
};

VisualShareOffer offerFor(VisualSourceKind source, std::uint64_t id = 1) {
    return VisualShareOffer{VisualShareRef{5, VisualInitiator::peer, id}, source};
}

VisualFrame gradient(std::uint32_t width, std::uint32_t height) {
    VisualFrame frame;
    frame.size = VisualSize{width, height};
    frame.bgra.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            frame.bgra[index + 0] = static_cast<std::uint8_t>(x);
            frame.bgra[index + 1] = static_cast<std::uint8_t>(y);
            frame.bgra[index + 2] = 200;
            frame.bgra[index + 3] = 255;
        }
    }
    return frame;
}

} // namespace

TEST_CASE(viewer_opens_in_an_independent_window_with_modality_title) {
    ViewerHarness harness;
    const VisualShareOffer monitor = offerFor(VisualSourceKind::monitor);
    harness.viewer->open(monitor);
    harness.drain();
    QWidget* window = harness.widget();
    CHECK(window != nullptr);
    CHECK(window->isVisible());
    CHECK(window->windowTitle().contains(QStringLiteral("Pantalla completa")));

    harness.viewer->close(monitor.ref);
    harness.drain();
    CHECK(!window->isVisible());

    const VisualShareOffer single = offerFor(VisualSourceKind::window, 2);
    harness.viewer->open(single);
    harness.drain();
    CHECK(window->windowTitle().contains(QStringLiteral("Una ventana")));
    harness.viewer->close(single.ref);
}

TEST_CASE(present_keeps_aspect_ratio_and_clear_updates_state) {
    ViewerHarness harness;
    const VisualShareOffer offer = offerFor(VisualSourceKind::monitor);
    harness.viewer->open(offer);
    harness.drain();
    QWidget* window = harness.widget();
    window->resize(400, 400);
    harness.viewer->present(offer.ref, gradient(200, 100));
    harness.drain();

    QLabel* frame_label = window->findChild<QLabel*>(QStringLiteral("visualFrame"));
    QLabel* state_label = window->findChild<QLabel*>(QStringLiteral("visualStateLabel"));
    CHECK(frame_label != nullptr);
    CHECK(state_label != nullptr);
    const QPixmap pixmap = frame_label->pixmap();
    CHECK(!pixmap.isNull());
    CHECK(pixmap.height() > 0);
    const double ratio = static_cast<double>(pixmap.width()) / pixmap.height();
    CHECK_NEAR(ratio, 2.0, 0.08);
    CHECK_EQ(state_label->text().toStdString(), std::string("En directo"));

    harness.viewer->clear(offer.ref, VisualSourceState::suspended);
    harness.drain();
    CHECK_EQ(state_label->text().toStdString(), std::string("Fuente en pausa"));
    CHECK(frame_label->pixmap().isNull());

    harness.viewer->clear(offer.ref, VisualSourceState::available);
    CHECK_EQ(state_label->text().toStdString(), std::string("Sin imagen"));
    harness.viewer->close(offer.ref);
}

TEST_CASE(present_ignores_other_operations_and_bad_buffers) {
    ViewerHarness harness;
    const VisualShareOffer offer = offerFor(VisualSourceKind::window);
    harness.viewer->open(offer);
    harness.drain();

    QLabel* state = harness.widget()->findChild<QLabel*>(QStringLiteral("visualStateLabel"));
    harness.viewer->present(VisualShareRef{5, VisualInitiator::peer, 99}, gradient(20, 10));
    CHECK_EQ(state->text().toStdString(), std::string("Esperando imagen…"));

    VisualFrame bad = gradient(20, 10);
    bad.bgra.resize(8);
    harness.viewer->present(offer.ref, bad);
    CHECK_EQ(state->text().toStdString(), std::string("Esperando imagen…"));
    harness.viewer->close(offer.ref);
}

TEST_CASE(programmatic_close_does_not_report_a_user_close) {
    ViewerHarness harness;
    const VisualShareOffer offer = offerFor(VisualSourceKind::monitor);
    harness.viewer->open(offer);
    harness.drain();
    harness.viewer->close(offer.ref);
    harness.drain();
    CHECK(harness.user_closed.empty());
}

TEST_CASE(user_close_reports_once_through_the_executor) {
    ViewerHarness harness;
    const VisualShareOffer offer = offerFor(VisualSourceKind::monitor, 7);
    harness.viewer->open(offer);
    harness.drain();
    CHECK_EQ(harness.user_closed.size(), static_cast<std::size_t>(0));

    QWidget* window = harness.widget();
    CHECK(window->close());
    harness.drain();
    CHECK_EQ(harness.user_closed.size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.user_closed.front().offer_id, static_cast<std::uint64_t>(7));
    CHECK_EQ(harness.user_closed.front().initiator, VisualInitiator::peer);

    window->close();
    harness.drain();
    CHECK_EQ(harness.user_closed.size(), static_cast<std::size_t>(1));
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return oneintwo_test::run();
}
