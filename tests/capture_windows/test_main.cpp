#include "fake_port.hpp"
#include "test_presentation.hpp"
#include "test_support.hpp"

#include "platform/windows/visual/capture_model.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;
using namespace oneintwo::test;

namespace {

windows::NativeSource monitorA()
{
    return {101, "MONITOR-A - 640x360", 640, 360};
}

windows::NativeSource monitorB()
{
    return {202, "MONITOR-B - 800x600", 800, 600};
}

windows::NativeSource windowA()
{
    return {301, "Editor - notas.txt - 480x300", 480, 300};
}

windows::NativeSource windowB()
{
    return {302, "Editor - notas.txt - 480x300", 480, 300};
}

windows::NativeSource occluderWindow()
{
    return {303, "Superposicion - 480x300", 480, 300};
}

struct Fixture {
    TestRuntime runtime;
    FakeCapturePort* port = nullptr;
    std::unique_ptr<DesktopVisualCapture> capture;
    CaptureHarness harness;

    bool initialise(VisualCaptureCapabilities caps = {true, true})
    {
        auto owned = std::make_unique<FakeCapturePort>();
        port = owned.get();
        port->caps = caps;
        port->monitors = {monitorA(), monitorB()};
        port->windows = {windowA(), windowB(), occluderWindow()};
        PlatformContext context;
        context.runtime = runtime.module;
        context.parent = nullptr;
        capture = std::make_unique<windows::CaptureModel>(std::move(context), std::move(owned));
        return capture != nullptr;
    }

    DesktopResult select(VisualShareRef ref, VisualSourceKind kind)
    {
        capture->selectSource(ref, kind, harness.selectionCallback());
        runtime.drain();
        return harness.results.empty() ? DesktopResult::error : harness.results.back();
    }
};

bool stateBeforeFrame(const CaptureHarness& harness, VisualSourceState state, VisualSize size)
{
    for (std::size_t index = 0; index < harness.events.size(); ++index) {
        const CaptureEvent& event = harness.events[index];
        if (event.kind != CaptureEvent::Kind::state || event.state != state) {
            continue;
        }
        if (index + 1 >= harness.events.size()) {
            return false;
        }
        const CaptureEvent& next = harness.events[index + 1];
        return next.kind == CaptureEvent::Kind::frame && next.frame.size.width == size.width
            && next.frame.size.height == size.height;
    }
    return false;
}

void testCapabilities()
{
    {
        Fixture fixture;
        CHECK(fixture.initialise({false, false}));
        CHECK(!fixture.capture->visualCapabilities().monitor);
        CHECK(!fixture.capture->visualCapabilities().window);
        CHECK(fixture.select(makeVisualRef(1), VisualSourceKind::monitor)
              == DesktopResult::unavailable);
        CHECK(fixture.port->prompt_calls == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise({true, false}));
        CHECK(fixture.capture->visualCapabilities().monitor);
        CHECK(!fixture.capture->visualCapabilities().window);
        CHECK(fixture.select(makeVisualRef(2), VisualSourceKind::window)
              == DesktopResult::unavailable);
        CHECK(fixture.port->prompt_calls == 0);
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->monitors.clear();
        CHECK(fixture.select(makeVisualRef(3), VisualSourceKind::monitor)
              == DesktopResult::unavailable);
        CHECK(fixture.port->prompt_calls == 0);
    }
}

void testMonitorSelection()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(10);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    CHECK(fixture.port->prompt_calls == 1);
    CHECK(fixture.port->last_prompt_kind == VisualSourceKind::monitor);
    CHECK(fixture.port->last_prompt_sources.size() == 2);
    CHECK(fixture.port->adopted_handles.size() == 1);
    CHECK(fixture.port->adopted_handles[0] == 101);
    CHECK(fixture.port->adopted_kinds[0] == VisualSourceKind::monitor);
    CHECK(fixture.harness.results.size() == 1);

    const TestSource pattern = makePresentation("monitor-A", 640, 360, 20, 60, 100, 11);
    const TestSource other = makePresentation("monitor-B", 640, 360, 200, 60, 30, 200);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.port->last_capture != nullptr);
    CHECK(fixture.port->last_stats->started);
    fixture.port->last_capture->pushFrame(toVisualFrame(pattern));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);
    CHECK(frameMatches(fixture.harness.frames[0], pattern));
    CHECK(!frameMatches(fixture.harness.frames[0], other));
    CHECK(!fixture.harness.writeSnapshot("capture_windows_monitor.ppm").empty());
    fixture.capture->stopCapture(reference);
    CHECK(fixture.port->last_stats->stopped);
}

void testPromptSelectionAndCancel()
{
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->prompt_mode = FakeCapturePort::PromptMode::immediate_second;
        CHECK(fixture.select(makeVisualRef(11), VisualSourceKind::monitor)
              == DesktopResult::ready);
        CHECK(fixture.port->adopted_handles.size() == 1);
        CHECK(fixture.port->adopted_handles[0] == 202);
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->prompt_mode = FakeCapturePort::PromptMode::cancel;
        CHECK(fixture.select(makeVisualRef(12), VisualSourceKind::monitor)
              == DesktopResult::cancelled);
        CHECK(fixture.port->adopted_handles.empty());
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->adopt_succeeds = false;
        CHECK(fixture.select(makeVisualRef(13), VisualSourceKind::monitor)
              == DesktopResult::unavailable);
    }
}

void testWindowTypeIsPreserved()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    CHECK(fixture.select(makeVisualRef(14), VisualSourceKind::window) == DesktopResult::ready);
    CHECK(fixture.port->last_prompt_kind == VisualSourceKind::window);
    CHECK(fixture.port->adopted_kinds.size() == 1);
    CHECK(fixture.port->adopted_kinds[0] == VisualSourceKind::window);
    CHECK(fixture.port->adopted_handles[0] == 301);

    const TestSource window = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 15, 120,
                                               70, 33);
    const TestSource occluder = makePresentation("Superposici\xC3\xB3n", 480, 300, 240, 0, 240,
                                                 200);
    fixture.capture->startCapture(makeVisualRef(14), fixture.harness.captureCallbacks());
    fixture.port->last_capture->pushFrame(toVisualFrame(window));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);
    CHECK(frameMatches(fixture.harness.frames[0], window));
    CHECK(!frameContainsColor(fixture.harness.frames[0], 240, 0, 240));
    CHECK(!frameMatches(fixture.harness.frames[0], occluder));

    // Moving the window to another monitor does not change the retained identity (HWND).
    const auto saved_monitors = fixture.port->monitors;
    fixture.port->monitors.clear();
    fixture.port->last_capture->pushFrame(toVisualFrame(window));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 2);
    CHECK(fixture.port->adopted_handles.size() == 1);
    fixture.port->monitors = saved_monitors;

    fixture.capture->stopCapture(makeVisualRef(14));
}

void testDeferredPromptAndCancellation()
{
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->prompt_mode = FakeCapturePort::PromptMode::deferred;
        const VisualShareRef reference = makeVisualRef(15);
        fixture.capture->selectSource(reference, VisualSourceKind::monitor,
                                      fixture.harness.selectionCallback());
        fixture.runtime.drain();
        CHECK(fixture.harness.results.empty());
        fixture.port->completePrompt(0);
        fixture.runtime.drain();
        CHECK(fixture.harness.results.size() == 1);
        CHECK(fixture.harness.results.back() == DesktopResult::ready);
        fixture.capture->stopCapture(reference);
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->prompt_mode = FakeCapturePort::PromptMode::deferred;
        const VisualShareRef reference = makeVisualRef(16);
        fixture.capture->selectSource(reference, VisualSourceKind::monitor,
                                      fixture.harness.selectionCallback());
        fixture.runtime.drain();
        fixture.capture->stopCapture(reference);
        fixture.runtime.drain();
        CHECK(fixture.harness.results.size() == 1);
        CHECK(fixture.harness.results.back() == DesktopResult::cancelled);
        CHECK(fixture.port->cancel_prompt_calls >= 1);
        CHECK(fixture.port->adopted_handles.empty());

        // Late native response of the selector closed.
        fixture.port->completePrompt(0);
        fixture.runtime.drain();
        CHECK(fixture.harness.results.size() == 1);
        CHECK(fixture.port->adopted_handles.empty());
    }
    {
        Fixture fixture;
        CHECK(fixture.initialise());
        fixture.port->prompt_mode = FakeCapturePort::PromptMode::deferred;
        const VisualShareRef reference = makeVisualRef(17);
        fixture.capture->selectSource(reference, VisualSourceKind::monitor,
                                      fixture.harness.selectionCallback());
        fixture.runtime.drain();
        fixture.capture->stopCapture(makeVisualRef(170));
        fixture.runtime.drain();
        CHECK(fixture.harness.results.empty());
        fixture.port->completePrompt(1);
        fixture.runtime.drain();
        CHECK(fixture.harness.results.size() == 1);
        CHECK(fixture.harness.results.back() == DesktopResult::ready);
        fixture.capture->stopCapture(reference);
    }
}

void testSecondSelectionWhileActive()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(18);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.harness.results.clear();
    fixture.capture->selectSource(makeVisualRef(19), VisualSourceKind::monitor,
                                  fixture.harness.selectionCallback());
    fixture.runtime.drain();
    CHECK(fixture.harness.results.size() == 1);
    CHECK(fixture.harness.results.back() == DesktopResult::error);
    CHECK(fixture.port->prompt_calls == 1);
    fixture.capture->stopCapture(reference);
}

void testFrameContract()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(20);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    FakeCapture* capture = fixture.port->last_capture;

    VisualFrame truncated;
    truncated.size = {100, 100};
    truncated.bgra.resize(16);
    capture->pushFrame(std::move(truncated));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.empty());

    VisualFrame empty;
    empty.size = {0, 10};
    capture->pushFrame(std::move(empty));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.empty());

    VisualFrame oversized;
    oversized.size = {20000, 2};
    oversized.bgra.assign(20000 * 2 * 4, 0);
    capture->pushFrame(std::move(oversized));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.empty());

    VisualFrame transparent;
    transparent.size = {8, 8};
    transparent.bgra.assign(8 * 8 * 4, 0);
    capture->pushFrame(std::move(transparent));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);
    bool opaque = true;
    for (std::size_t index = 3; index < fixture.harness.frames[0].bgra.size(); index += 4) {
        opaque = opaque && fixture.harness.frames[0].bgra[index] == 0xFF;
    }
    CHECK(opaque);
    fixture.capture->stopCapture(reference);
}

void testStateOrderingAndSuspension()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(21);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    FakeCapture* capture = fixture.port->last_capture;

    const TestSource initial = makePresentation("monitor-A", 640, 360, 20, 60, 100, 11);
    capture->pushFrame(toVisualFrame(initial));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);

    const TestSource resized = resizePresentation(initial, 800, 600);
    capture->pushFrame(toVisualFrame(resized));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 2);
    CHECK(fixture.harness.sawState(VisualSourceState::available));
    CHECK(stateBeforeFrame(fixture.harness, VisualSourceState::available,
                           {resized.width, resized.height}));

    capture->pushState(VisualSourceState::suspended);
    fixture.runtime.drain();
    CHECK(fixture.harness.sawState(VisualSourceState::suspended));
    const std::size_t before = fixture.harness.frames.size();
    capture->pushFrame(toVisualFrame(initial));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == before);

    capture->pushState(VisualSourceState::available);
    fixture.runtime.drain();
    capture->pushFrame(toVisualFrame(initial));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == before + 1);
    fixture.capture->stopCapture(reference);
}

void testCloseAndLateCallbacks()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(22);
    CHECK(fixture.select(reference, VisualSourceKind::window) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    FakeCapture* capture = fixture.port->last_capture;
    const TestSource window = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 15, 120,
                                               70, 33);
    capture->pushFrame(toVisualFrame(window));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);

    const auto late = capture->sink;
    capture->pushState(VisualSourceState::closed);
    fixture.runtime.drain();
    CHECK(fixture.harness.sawState(VisualSourceState::closed));
    CHECK(fixture.port->last_stats->stopped);
    CHECK(fixture.port->adopted_handles.size() == 1);

    const std::size_t before = fixture.harness.frames.size();
    late.frame(toVisualFrame(window));
    late.state(VisualSourceState::closed);
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == before);
    std::size_t closed_states = 0;
    for (const VisualSourceState state : fixture.harness.states) {
        if (state == VisualSourceState::closed) {
            ++closed_states;
        }
    }
    CHECK(closed_states == 1);
    // The new window with the same title is not adopted without a new invitation.
    CHECK(fixture.port->adopted_handles.size() == 1);
    fixture.capture->stopCapture(reference);

    const VisualShareRef second = makeVisualRef(23);
    fixture.port->prompt_mode = FakeCapturePort::PromptMode::immediate_second;
    CHECK(fixture.select(second, VisualSourceKind::window) == DesktopResult::ready);
    CHECK(fixture.port->adopted_handles.size() == 2);
    CHECK(fixture.port->adopted_handles.back() == 302);
    fixture.capture->stopCapture(second);
}

void testStopDuringCapture()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(24);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    FakeCapture* capture = fixture.port->last_capture;
    const TestSource pattern = makePresentation("monitor-A", 640, 360, 20, 60, 100, 11);
    capture->pushFrame(toVisualFrame(pattern));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);

    const auto saved = capture->sink;
    fixture.capture->stopCapture(reference);
    CHECK(fixture.port->last_stats->stopped);
    const std::size_t before = fixture.harness.frames.size();
    saved.frame(toVisualFrame(pattern));
    fixture.capture->stopCapture(reference);
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == before);
}

void testStartWithoutSelection()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    fixture.capture->startCapture(makeVisualRef(25), fixture.harness.captureCallbacks());
    fixture.runtime.drain();
    CHECK(fixture.harness.states.size() == 1);
    CHECK(fixture.harness.states.back() == VisualSourceState::closed);
}

void testTwoOperations()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef first = makeVisualRef(26);
    CHECK(fixture.select(first, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(first, fixture.harness.captureCallbacks());
    fixture.capture->stopCapture(first);

    fixture.harness.results.clear();
    const VisualShareRef second = makeVisualRef(27);
    CHECK(fixture.select(second, VisualSourceKind::window) == DesktopResult::ready);
    CHECK(fixture.port->last_prompt_kind == VisualSourceKind::window);
    fixture.capture->stopCapture(second);
}

void testWindowIsolationHarness()
{
    Fixture fixture;
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(28);
    CHECK(fixture.select(reference, VisualSourceKind::window) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    FakeCapture* capture = fixture.port->last_capture;

    const TestSource window = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 15, 120,
                                               70, 33);
    capture->pushFrame(toVisualFrame(window));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 1);
    CHECK(frameMatches(fixture.harness.frames[0], window));
    CHECK(!frameContainsColor(fixture.harness.frames[0], 240, 0, 240));

    const TestSource moved = shiftPresentation(window, 41, 17);
    capture->pushFrame(toVisualFrame(moved));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 2);
    CHECK(frameMatches(fixture.harness.frames[1], moved));

    const TestSource resized = resizePresentation(moved, 620, 380);
    capture->pushFrame(toVisualFrame(resized));
    fixture.runtime.drain();
    CHECK(fixture.harness.frames.size() == 3);
    CHECK(stateBeforeFrame(fixture.harness, VisualSourceState::available,
                           {resized.width, resized.height}));

    CHECK(!fixture.harness.writeSnapshot("capture_windows_window.ppm").empty());
    capture->pushState(VisualSourceState::closed);
    fixture.runtime.drain();
    CHECK(fixture.harness.sawState(VisualSourceState::closed));
    fixture.capture->stopCapture(reference);
}

} // namespace

int main()
{
    testCapabilities();
    testMonitorSelection();
    testPromptSelectionAndCancel();
    testWindowTypeIsPreserved();
    testDeferredPromptAndCancellation();
    testSecondSelectionWhileActive();
    testFrameContract();
    testStateOrderingAndSuspension();
    testCloseAndLateCallbacks();
    testStopDuringCapture();
    testStartWithoutSelection();
    testTwoOperations();
    testWindowIsolationHarness();

    std::printf("capture_windows: %d comprobaciones, %d fallos\n", check_count(),
                failure_count());
    return failure_count() == 0 ? 0 : 1;
}
