#include "features/visual/visual_codec.hpp"
#include "features/visual_share.hpp"
#include "integration/feature_contracts.hpp"

#include "../session/test_support.hpp"

#include <QGuiApplication>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;

namespace {

FeatureSet allFeatures() {
    return FeatureSet{true, true, true, true, true, true, true, true, true};
}

FeatureSet visualFeatures(bool monitor, bool window, bool manual, bool forwarded) {
    FeatureSet features{};
    features.share_monitor = monitor;
    features.share_window = window;
    features.manual_notifications = manual;
    features.forwarded_notifications = forwarded;
    return features;
}

struct FakeClock {
    MonotonicTime value{};
};

struct FakeCapture : DesktopVisualCapture {
    VisualCaptureCapabilities caps{true, true};
    int select_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    VisualShareRef selected_ref{};
    VisualShareRef started_ref{};
    VisualShareRef stopped_ref{};
    VisualSourceKind selected_kind = VisualSourceKind::monitor;
    VisualCaptureCallbacks callbacks;
    std::function<void(VisualShareRef, DesktopResult)> selection;

    VisualCaptureCapabilities visualCapabilities() const override {
        return caps;
    }

    void selectSource(VisualShareRef offer, VisualSourceKind kind,
                      std::function<void(VisualShareRef, DesktopResult)> selected) override {
        ++select_calls;
        selected_ref = offer;
        selected_kind = kind;
        selection = std::move(selected);
    }

    void startCapture(VisualShareRef offer, VisualCaptureCallbacks value) override {
        ++start_calls;
        started_ref = offer;
        callbacks = std::move(value);
    }

    void stopCapture(VisualShareRef offer) override {
        ++stop_calls;
        stopped_ref = offer;
        callbacks = {};
        selection = nullptr;
    }

    void completeSelection(VisualShareRef offer, DesktopResult result) {
        auto callback = std::move(selection);
        selection = nullptr;
        if (callback) {
            callback(offer, result);
        }
    }

    void emitFrame(VisualShareRef offer, VisualFrame frame) {
        if (callbacks.frame) {
            callbacks.frame(offer, std::move(frame));
        }
    }

    void emitState(VisualShareRef offer, VisualSourceState state) {
        if (callbacks.state_changed) {
            callbacks.state_changed(offer, state);
        }
    }
};

struct FakeViewer : VisualViewer {
    int opens = 0;
    int presents = 0;
    int clears = 0;
    int closes = 0;
    VisualShareOffer opened{};
    VisualSourceState last_clear_state = VisualSourceState::available;
    VisualFrame last_frame;

    void open(const VisualShareOffer& offer) override {
        ++opens;
        opened = offer;
    }

    void present(VisualShareRef, const VisualFrame& frame) override {
        ++presents;
        last_frame = frame;
    }

    void clear(VisualShareRef, VisualSourceState state) override {
        ++clears;
        last_clear_state = state;
    }

    void close(VisualShareRef) override {
        ++closes;
    }
};

struct FakeSender {
    std::deque<SendResult> results;
    std::vector<FeatureMessage> messages;

    SendResult send(FeatureMessage message) {
        if (results.empty()) {
            messages.push_back(std::move(message));
            return SendResult::queued;
        }
        const SendResult result = results.front();
        results.pop_front();
        // busy/denied/disconnected no transfieren la propiedad del mensaje.
        if (result == SendResult::queued) {
            messages.push_back(std::move(message));
        }
        return result;
    }
};

struct Harness {
    std::shared_ptr<FakeClock> clock = std::make_shared<FakeClock>();
    std::deque<Task> tasks;
    FakeCapture capture;
    FakeViewer viewer;
    FakeSender sender;
    std::vector<VisualShareOffer> shown;
    std::vector<VisualShareRef> dismissed;
    std::vector<std::string> errors;
    std::unique_ptr<VisualShare> share;

    Harness() {
        ModuleRuntime runtime;
        runtime.now = [this] { return clock->value; };
        runtime.post = [this](Task task) { tasks.push_back(std::move(task)); };
        VisualDependencies dependencies{
            runtime,
            capture,
            viewer,
            [this](FeatureMessage message) { return sender.send(std::move(message)); },
            [this](VisualShareOffer offer) { shown.push_back(offer); },
            [this](VisualShareRef ref) { dismissed.push_back(ref); },
            [this](std::string error) { errors.push_back(std::move(error)); },
        };
        share = std::make_unique<VisualShare>(std::move(dependencies));
    }

    void drain() {
        std::size_t guard = 0;
        while (!tasks.empty() && guard++ < 10000) {
            Task task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
    }

    void advance(std::chrono::milliseconds duration) {
        clock->value += duration;
    }

    void authorize(SessionGeneration generation, FeatureSet send, FeatureSet receive) {
        share->setAuthorization(generation, send, receive);
    }

    template <typename T>
    std::size_t countSent() const {
        std::size_t count = 0;
        for (const FeatureMessage& message : sender.messages) {
            if (std::get_if<T>(&message) != nullptr) {
                ++count;
            }
        }
        return count;
    }

    template <typename T>
    std::vector<T> sentOf() const {
        std::vector<T> out;
        for (const FeatureMessage& message : sender.messages) {
            if (const auto* value = std::get_if<T>(&message)) {
                out.push_back(*value);
            }
        }
        return out;
    }

    VisualShareRef prepareOriginOffer(VisualSourceKind kind = VisualSourceKind::monitor) {
        share->offerSource(7, kind);
        drain();
        capture.completeSelection(capture.selected_ref, DesktopResult::ready);
        drain();
        return capture.selected_ref;
    }

    VisualShareRef prepareViewerLive(VisualSourceKind kind = VisualSourceKind::monitor,
                                     std::uint64_t id = 1) {
        const VisualShareOffer offer{VisualShareRef{7, VisualInitiator::peer, id}, kind};
        share->receiveOffer(offer);
        drain();
        share->decideOffer(offer.ref, VisualDecision::accepted);
        drain();
        share->receiveSourceState(offer.ref, 1, VisualSourceState::available);
        return offer.ref;
    }
};

VisualFrame makeBgra(std::uint32_t width, std::uint32_t height) {
    VisualFrame frame;
    frame.size = VisualSize{width, height};
    frame.bgra.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            frame.bgra[index + 0] = static_cast<std::uint8_t>(x * 3);
            frame.bgra[index + 1] = static_cast<std::uint8_t>(y * 5);
            frame.bgra[index + 2] = static_cast<std::uint8_t>(128);
            frame.bgra[index + 3] = 255;
        }
    }
    return frame;
}

VisualEncodedFrame encodedFrame(const VisualShareRef& ref, VisualSourceRevision revision,
                                std::uint64_t sequence, std::uint32_t width = 64,
                                std::uint32_t height = 32) {
    std::string error;
    const auto encoded = encodeVisualFrame(ref, revision, sequence, makeBgra(width, height), &error);
    CHECK(encoded.has_value());
    return *encoded;
}

bool isJpeg(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xD8;
}

} // namespace

TEST_CASE(offer_requires_permission_capability_and_single_operation) {
    Harness harness;
    harness.authorize(7, FeatureSet{}, FeatureSet{});
    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 0);
    CHECK(!harness.errors.empty());

    harness.authorize(7, allFeatures(), allFeatures());
    harness.capture.caps.monitor = false;
    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 0);
    CHECK(!harness.errors.empty());

    harness.capture.caps.monitor = true;
    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 1);
    CHECK_EQ(harness.countSent<VisualShareOffer>(), static_cast<std::size_t>(0));

    harness.share->offerSource(7, VisualSourceKind::window);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 1); // sigue ocupada

    harness.capture.completeSelection(harness.capture.selected_ref, DesktopResult::ready);
    harness.drain();
    CHECK_EQ(harness.countSent<VisualShareOffer>(), static_cast<std::size_t>(1));
    const auto offers = harness.sentOf<VisualShareOffer>();
    CHECK_EQ(offers.front().source, VisualSourceKind::monitor);
    CHECK_EQ(offers.front().ref.offer_id, static_cast<std::uint64_t>(1));
}

TEST_CASE(window_offer_uses_its_own_permission_and_kind) {
    Harness harness;
    harness.authorize(7, visualFeatures(false, true, false, false),
                      visualFeatures(false, true, false, false));
    harness.share->offerSource(7, VisualSourceKind::window);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 1);
    CHECK_EQ(harness.capture.selected_kind, VisualSourceKind::window);
    harness.capture.completeSelection(harness.capture.selected_ref, DesktopResult::ready);
    harness.drain();
    const auto offers = harness.sentOf<VisualShareOffer>();
    CHECK_EQ(offers.size(), static_cast<std::size_t>(1));
    CHECK_EQ(offers.front().source, VisualSourceKind::window);
}

TEST_CASE(receive_offer_checks_receive_permission) {
    Harness harness;
    harness.authorize(7, allFeatures(), visualFeatures(false, true, false, false));
    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 1}, VisualSourceKind::monitor});
    harness.drain();
    CHECK(harness.shown.empty());

    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 2}, VisualSourceKind::window});
    harness.drain();
    CHECK_EQ(harness.shown.size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.shown.front().source, VisualSourceKind::window);
}

TEST_CASE(simultaneous_and_duplicate_invitations_are_rejected) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 1}, VisualSourceKind::monitor});
    harness.drain();
    CHECK_EQ(harness.shown.size(), static_cast<std::size_t>(1));

    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 2}, VisualSourceKind::window});
    harness.drain();
    CHECK_EQ(harness.shown.size(), static_cast<std::size_t>(1));
    auto decisions = harness.sentOf<VisualDecisionMessage>();
    CHECK_EQ(decisions.size(), static_cast<std::size_t>(1));
    CHECK_EQ(decisions.front().decision, VisualDecision::busy);

    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 2}, VisualSourceKind::window});
    harness.drain();
    CHECK_EQ(harness.sentOf<VisualDecisionMessage>().size(), static_cast<std::size_t>(1));

    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 1}, VisualSourceKind::monitor});
    harness.drain();
    CHECK_EQ(harness.sentOf<VisualDecisionMessage>().size(), static_cast<std::size_t>(1));
}

TEST_CASE(viewer_acceptance_presents_ordered_frames) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareOffer offer{
        VisualShareRef{7, VisualInitiator::peer, 1}, VisualSourceKind::monitor};
    harness.share->receiveOffer(offer);
    harness.drain();
    harness.share->decideOffer(offer.ref, VisualDecision::accepted);
    harness.drain();
    const VisualShareRef ref = offer.ref;
    CHECK_EQ(harness.viewer.opens, 1);
    CHECK_EQ(harness.viewer.opened.source, VisualSourceKind::monitor);
    CHECK_EQ(harness.dismissed.size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.sentOf<VisualDecisionMessage>().front().decision, VisualDecision::accepted);
    CHECK_EQ(harness.viewer.clears, 0);

    // Imagen antes de su anuncio de revisión: se descarta.
    VisualEncodedFrame before = encodedFrame(ref, 1, 1);
    harness.share->receiveFrame(before);
    CHECK_EQ(harness.viewer.presents, 0);

    harness.share->receiveSourceState(ref, 1, VisualSourceState::available);
    CHECK_EQ(harness.viewer.clears, 1);
    CHECK_EQ(harness.viewer.last_clear_state, VisualSourceState::available);
    harness.share->receiveFrame(before);
    CHECK_EQ(harness.viewer.presents, 1);
    CHECK_EQ(harness.viewer.last_frame.size.width, static_cast<std::uint32_t>(64));
    CHECK_EQ(harness.viewer.last_frame.size.height, static_cast<std::uint32_t>(32));

    harness.share->receiveFrame(before);
    CHECK_EQ(harness.viewer.presents, 1); // secuencia repetida

    harness.share->receiveSourceState(ref, 2, VisualSourceState::available);
    harness.share->receiveFrame(encodedFrame(ref, 1, 1));
    CHECK_EQ(harness.viewer.presents, 1); // revisión antigua
    harness.share->receiveFrame(encodedFrame(ref, 3, 3));
    CHECK_EQ(harness.viewer.presents, 1); // revisión adelantada
    harness.share->receiveFrame(encodedFrame(ref, 2, 2));
    CHECK_EQ(harness.viewer.presents, 2);
}

TEST_CASE(viewer_handles_suspension_recovery_and_closure) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareViewerLive();
    harness.share->receiveFrame(encodedFrame(ref, 1, 1));
    CHECK_EQ(harness.viewer.presents, 1);

    harness.share->receiveSourceState(ref, 2, VisualSourceState::suspended);
    CHECK_EQ(harness.viewer.last_clear_state, VisualSourceState::suspended);
    harness.share->receiveFrame(encodedFrame(ref, 2, 2));
    CHECK_EQ(harness.viewer.presents, 1); // suspendida

    harness.share->receiveActivity(ref, 3);
    CHECK_EQ(harness.viewer.last_clear_state, VisualSourceState::available);
    harness.share->receiveFrame(encodedFrame(ref, 3, 2));
    CHECK_EQ(harness.viewer.presents, 2);

    harness.share->receiveSourceState(ref, 4, VisualSourceState::closed);
    CHECK_EQ(harness.viewer.last_clear_state, VisualSourceState::closed);
    CHECK_EQ(harness.viewer.closes, 1);
    harness.share->receiveFrame(encodedFrame(ref, 3, 3));
    CHECK_EQ(harness.viewer.presents, 2);
}

TEST_CASE(viewer_times_out_without_signals_and_is_kept_alive_by_activity) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareViewerLive();
    harness.advance(std::chrono::seconds(2));
    harness.share->receiveActivity(ref, 1);
    harness.share->tick();
    CHECK_EQ(harness.viewer.closes, 0);

    harness.advance(std::chrono::seconds(4));
    harness.share->tick();
    CHECK_EQ(harness.viewer.closes, 1);
    CHECK_EQ(harness.countSent<VisualStopMessage>(), static_cast<std::size_t>(1));
}

TEST_CASE(origin_encodes_frames_with_revision_sequence_and_jpeg) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();
    CHECK_EQ(harness.capture.start_calls, 1);
    CHECK_EQ(harness.countSent<VisualStateMessage>(), static_cast<std::size_t>(1));
    const auto states = harness.sentOf<VisualStateMessage>();
    CHECK_EQ(states.front().revision, static_cast<VisualSourceRevision>(1));
    CHECK_EQ(states.front().state, VisualSourceState::available);

    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();
    CHECK_EQ(harness.capture.start_calls, 1); // no se inicia dos veces

    harness.capture.emitFrame(ref, makeBgra(64, 32));
    harness.drain();
    auto frames = harness.sentOf<VisualEncodedFrame>();
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    CHECK_EQ(frames.front().revision, static_cast<VisualSourceRevision>(1));
    CHECK_EQ(frames.front().sequence, static_cast<std::uint64_t>(1));
    CHECK(frames.front().key_frame);
    CHECK(isJpeg(frames.front().bytes));
    CHECK(frames.front().bytes.size() <= max_visual_encoded_bytes);

    // Mismo instante: el límite de 30 fps descarta la captura.
    harness.capture.emitFrame(ref, makeBgra(64, 32));
    harness.drain();
    CHECK_EQ(harness.sentOf<VisualEncodedFrame>().size(), static_cast<std::size_t>(1));

    harness.advance(std::chrono::milliseconds(34));
    harness.capture.emitFrame(ref, makeBgra(64, 32));
    harness.drain();
    frames = harness.sentOf<VisualEncodedFrame>();
    CHECK_EQ(frames.size(), static_cast<std::size_t>(2));
    CHECK_EQ(frames.back().sequence, static_cast<std::uint64_t>(2));
}

TEST_CASE(origin_offer_expiry_ignores_late_acceptance) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    CHECK_EQ(harness.countSent<VisualShareOffer>(), static_cast<std::size_t>(1));

    harness.advance(std::chrono::seconds(29));
    harness.share->tick();
    CHECK_EQ(harness.capture.stop_calls, 0);
    harness.advance(std::chrono::seconds(1));
    harness.share->tick();
    CHECK_EQ(harness.capture.stop_calls, 1);

    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();
    CHECK_EQ(harness.capture.start_calls, 0);
    CHECK_EQ(harness.countSent<VisualStateMessage>(), static_cast<std::size_t>(0));
}

TEST_CASE(viewer_offer_expiry_dismisses_without_decision) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    harness.share->receiveOffer(
        VisualShareOffer{VisualShareRef{7, VisualInitiator::peer, 1}, VisualSourceKind::monitor});
    harness.drain();
    CHECK_EQ(harness.shown.size(), static_cast<std::size_t>(1));
    harness.advance(std::chrono::seconds(30));
    harness.share->tick();
    CHECK_EQ(harness.dismissed.size(), static_cast<std::size_t>(1));
    CHECK_EQ(harness.countSent<VisualDecisionMessage>(), static_cast<std::size_t>(0));
    CHECK_EQ(harness.viewer.opens, 0);
}

TEST_CASE(stop_is_idempotent_and_remote_stop_does_not_reply) {
    Harness origin;
    origin.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = origin.prepareOriginOffer();
    origin.share->receiveDecision(ref, VisualDecision::accepted);
    origin.drain();

    origin.share->stop(ref);
    CHECK_EQ(origin.countSent<VisualStopMessage>(), static_cast<std::size_t>(1));
    CHECK_EQ(origin.capture.stop_calls, 1);
    origin.share->stop(ref);
    CHECK_EQ(origin.countSent<VisualStopMessage>(), static_cast<std::size_t>(1));
    CHECK_EQ(origin.capture.stop_calls, 1);

    Harness viewer;
    viewer.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef viewer_ref = viewer.prepareViewerLive();
    const std::size_t before = viewer.sender.messages.size();
    viewer.share->receiveStop(viewer_ref);
    CHECK_EQ(viewer.viewer.closes, 1);
    CHECK_EQ(viewer.sender.messages.size(), before); // no reenvía cierre
    viewer.share->receiveStop(viewer_ref);
    CHECK_EQ(viewer.viewer.closes, 1);
}

TEST_CASE(authorization_revocation_and_generation_change_invalidate_operations) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();

    harness.authorize(7, visualFeatures(false, true, true, true), allFeatures());
    CHECK_EQ(harness.capture.stop_calls, 1);
    CHECK_EQ(harness.countSent<VisualStopMessage>(), static_cast<std::size_t>(1));
    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 1); // rechazada por permiso

    harness.authorize(8, allFeatures(), allFeatures());
    harness.share->offerSource(8, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 2);
    CHECK_EQ(harness.capture.selected_ref.offer_id, static_cast<std::uint64_t>(1));
    CHECK_EQ(harness.capture.selected_ref.generation, static_cast<SessionGeneration>(8));
}

TEST_CASE(source_state_changes_increment_revision_and_drop_stale_work) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();

    harness.capture.emitFrame(ref, makeBgra(48, 24));
    harness.capture.emitState(ref, VisualSourceState::suspended);
    harness.drain();
    const auto states = harness.sentOf<VisualStateMessage>();
    CHECK_EQ(states.size(), static_cast<std::size_t>(2));
    CHECK_EQ(states.back().revision, static_cast<VisualSourceRevision>(2));
    CHECK_EQ(states.back().state, VisualSourceState::suspended);
    CHECK_EQ(harness.sentOf<VisualEncodedFrame>().size(), static_cast<std::size_t>(0));

    harness.advance(std::chrono::milliseconds(100));
    harness.capture.emitFrame(ref, makeBgra(48, 24));
    harness.drain();
    CHECK_EQ(harness.sentOf<VisualEncodedFrame>().size(), static_cast<std::size_t>(0));

    harness.capture.emitState(ref, VisualSourceState::available);
    harness.drain();
    harness.advance(std::chrono::milliseconds(100));
    harness.capture.emitFrame(ref, makeBgra(48, 24));
    harness.drain();
    const auto frames = harness.sentOf<VisualEncodedFrame>();
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    CHECK_EQ(frames.front().revision, static_cast<VisualSourceRevision>(3));
}

TEST_CASE(encoded_queue_is_bounded_and_retries_when_busy) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();

    for (int index = 0; index < 6; ++index) {
        harness.advance(std::chrono::milliseconds(34));
        harness.capture.emitFrame(ref, makeBgra(48, 24));
    }
    harness.drain();
    const std::size_t bounded = harness.sentOf<VisualEncodedFrame>().size();
    CHECK(bounded > 0);
    CHECK(bounded <= 2);

    Harness retry;
    retry.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef retry_ref = retry.prepareOriginOffer();
    retry.share->receiveDecision(retry_ref, VisualDecision::accepted);
    retry.drain();
    retry.sender.results = {SendResult::busy, SendResult::queued, SendResult::queued};
    retry.capture.emitFrame(retry_ref, makeBgra(32, 16));
    retry.drain();
    CHECK_EQ(retry.sentOf<VisualEncodedFrame>().size(), static_cast<std::size_t>(0));
    retry.share->tick();
    CHECK_EQ(retry.sentOf<VisualEncodedFrame>().size(), static_cast<std::size_t>(1));
}

TEST_CASE(data_send_failure_terminates_operation) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareOriginOffer();
    harness.share->receiveDecision(ref, VisualDecision::accepted);
    harness.drain();
    harness.sender.results = {SendResult::disconnected};
    harness.capture.emitFrame(ref, makeBgra(32, 16));
    harness.drain();
    CHECK_EQ(harness.capture.stop_calls, 1);
}

TEST_CASE(cancelled_or_stale_selection_frees_the_operation) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    const VisualShareRef first = harness.capture.selected_ref;
    harness.capture.completeSelection(first, DesktopResult::cancelled);
    harness.drain();
    CHECK_EQ(harness.countSent<VisualShareOffer>(), static_cast<std::size_t>(0));

    harness.share->offerSource(7, VisualSourceKind::monitor);
    harness.drain();
    CHECK_EQ(harness.capture.select_calls, 2);
    const VisualShareRef second = harness.capture.selected_ref;
    harness.capture.completeSelection(
        VisualShareRef{7, VisualInitiator::local, second.offer_id + 99}, DesktopResult::ready);
    harness.drain();
    CHECK_EQ(harness.countSent<VisualShareOffer>(), static_cast<std::size_t>(0));
    CHECK_EQ(harness.capture.stop_calls, 3);
}

TEST_CASE(viewer_rejects_invalid_encoded_images) {
    Harness harness;
    harness.authorize(7, allFeatures(), allFeatures());
    const VisualShareRef ref = harness.prepareViewerLive();

    VisualEncodedFrame oversized = encodedFrame(ref, 1, 1);
    oversized.bytes.assign(max_visual_encoded_bytes + 1, 0x11);
    harness.share->receiveFrame(oversized);
    CHECK_EQ(harness.viewer.presents, 0);

    VisualEncodedFrame garbage;
    garbage.ref = ref;
    garbage.revision = 1;
    garbage.sequence = 1;
    garbage.size = VisualSize{64, 32};
    garbage.key_frame = true;
    garbage.bytes.assign(128, 0x5A);
    harness.share->receiveFrame(garbage);
    CHECK_EQ(harness.viewer.presents, 0);
    CHECK(!harness.errors.empty());
}

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    return oneintwo_test::run();
}
