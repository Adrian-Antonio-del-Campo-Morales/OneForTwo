#include "fake_pipewire.hpp"
#include "mock_portal.hpp"
#include "test_presentation.hpp"
#include "test_support.hpp"

#include "platform/linux/visual/portal_screencast.hpp"
#include "platform/linux/visual/video_frame_convert.hpp"
#include "platform/linux/visual/wayland_visual_capture.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace oneintwo;
using namespace oneintwo::wayland;
using namespace oneintwo::test;

namespace {

struct Fixture {
    explicit Fixture(MockPortal& portal_reference)
        : portal(portal_reference)
    {
    }

    TestRuntime runtime;
    MockPortal& portal;
    FakePipeWireSource* pipewire = nullptr;
    std::unique_ptr<DesktopVisualCapture> capture;
    CaptureHarness harness;

    bool initialise()
    {
        WaylandCaptureDependencies dependencies;
        dependencies.portal = [] { return makePortalScreencastClient(); };
        dependencies.pipewire = [this] {
            auto source = std::make_unique<FakePipeWireSource>();
            pipewire = source.get();
            return source;
        };
        PlatformContext context;
        context.runtime = runtime.module;
        context.parent = nullptr;
        context.events.capabilities_changed = [this] { ++capability_notifications; };
        capture = makeWaylandVisualCaptureWithDependencies(std::move(context),
                                                          std::move(dependencies));
        return capture != nullptr;
    }

    ~Fixture()
    {
        capture.reset();
        runtime.drain();
    }

    DesktopResult select(VisualShareRef ref, VisualSourceKind kind, int timeout_ms = 6000)
    {
        harness.results.clear();
        harness.result_refs.clear();
        capture->selectSource(ref, kind, harness.selectionCallback());
        if (!runtime.pump([this] { return !harness.results.empty(); }, timeout_ms)) {
            return DesktopResult::error;
        }
        return harness.results.back();
    }

    bool requestPending(VisualShareRef ref, VisualSourceKind kind, int timeout_ms = 6000)
    {
        capture->selectSource(ref, kind, harness.selectionCallback());
        return runtime.pump([this] { return portal.startPending(); }, timeout_ms);
    }

    int capability_notifications = 0;
};

VisualCaptureCapabilities waitCapabilities(Fixture& fixture, bool monitor, bool window)
{
    fixture.runtime.pump(
        [&fixture, monitor, window] {
            const VisualCaptureCapabilities capabilities = fixture.capture->visualCapabilities();
            return capabilities.monitor == monitor && capabilities.window == window;
        },
        3000);
    return fixture.capture->visualCapabilities();
}

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

struct EncodedBuffer {
    std::vector<std::uint8_t> bytes;
    int stride = 0;
};

EncodedBuffer encode(const TestSource& source, VideoPixelFormat format, int padding)
{
    EncodedBuffer encoded;
    encoded.stride = static_cast<int>(source.width * 4u) + padding;
    encoded.bytes.assign(static_cast<std::size_t>(encoded.stride) * source.height, 0xCD);
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            const std::size_t from = (static_cast<std::size_t>(y) * source.width + x) * 4ull;
            const std::size_t to = static_cast<std::size_t>(y) * encoded.stride + x * 4ull;
            const std::uint8_t blue = source.bgra[from + 0];
            const std::uint8_t green = source.bgra[from + 1];
            const std::uint8_t red = source.bgra[from + 2];
            if (format == VideoPixelFormat::bgra || format == VideoPixelFormat::bgrx) {
                encoded.bytes[to + 0] = blue;
                encoded.bytes[to + 1] = green;
                encoded.bytes[to + 2] = red;
            } else {
                encoded.bytes[to + 0] = red;
                encoded.bytes[to + 1] = green;
                encoded.bytes[to + 2] = blue;
            }
            encoded.bytes[to + 3] = format == VideoPixelFormat::bgrx
                    || format == VideoPixelFormat::rgbx
                ? 0x22
                : 0x00;
        }
    }
    return encoded;
}

void testFrameConversion()
{
    const TestSource source = makePresentation("conversion", 96, 64, 40, 90, 140, 77);
    const VideoPixelFormat formats[] = {VideoPixelFormat::bgra, VideoPixelFormat::bgrx,
                                        VideoPixelFormat::rgba, VideoPixelFormat::rgbx};
    for (const VideoPixelFormat format : formats) {
        for (const int padding : {0, 128}) {
            const EncodedBuffer encoded = encode(source, format, padding);
            VideoBufferView view;
            view.format = format;
            view.width = source.width;
            view.height = source.height;
            view.stride = encoded.stride;
            view.data = encoded.bytes.data();
            view.bytes = encoded.bytes.size();
            VisualFrame frame;
            CHECK(convertToBgra(view, frame) == FrameConvertResult::ok);
            CHECK(frameMatches(frame, source));
            CHECK(frameContractHolds(frame));
        }
    }

    {
        const TestSource large = makePresentation("large", 160, 90, 10, 20, 30, 44);
        const EncodedBuffer encoded = encode(large, VideoPixelFormat::bgra, 64);
        VideoBufferView view{VideoPixelFormat::bgra, large.width, large.height, encoded.stride,
                             encoded.bytes.data(), encoded.bytes.size()};
        VisualFrame frame;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::ok);
        CHECK(frame.bgra.size() == static_cast<std::size_t>(large.width) * large.height * 4ull);
        bool opaque = true;
        for (std::size_t index = 3; index < frame.bgra.size(); index += 4) {
            opaque = opaque && frame.bgra[index] == 0xFF;
        }
        CHECK(opaque);
    }

    // Stride negativo: la primera fila de la imagen está al final del búfer.
    {
        const EncodedBuffer encoded = encode(source, VideoPixelFormat::bgra, 0);
        const std::size_t row_bytes = static_cast<std::size_t>(source.width) * 4ull;
        std::vector<std::uint8_t> flipped(encoded.bytes.size());
        for (std::uint32_t y = 0; y < source.height; ++y) {
            std::memcpy(flipped.data() + (source.height - 1u - y) * row_bytes,
                        encoded.bytes.data() + static_cast<std::size_t>(y) * row_bytes, row_bytes);
        }
        VideoBufferView view{VideoPixelFormat::bgra, source.width, source.height,
                             -static_cast<int>(row_bytes), flipped.data(), flipped.size()};
        VisualFrame frame;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::ok);
        CHECK(frameMatches(frame, source));
    }

    {
        std::vector<std::uint8_t> short_buffer(300, 0);
        VideoBufferView view{VideoPixelFormat::bgra, 96, 64, 96 * 4, short_buffer.data(),
                             short_buffer.size()};
        VisualFrame frame;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::invalid);

        view.format = VideoPixelFormat::unknown;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::unsupported_format);

        view.format = VideoPixelFormat::bgra;
        view.width = 0;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::invalid);

        view.width = 20000;
        view.height = 1;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::invalid);

        view.width = 96;
        view.height = 64;
        view.stride = 128;
        CHECK(convertToBgra(view, frame) == FrameConvertResult::invalid);
    }
}

void testCapabilities(MockPortal& portal)
{
    struct Case {
        std::uint32_t types;
        bool monitor;
        bool window;
    };
    const Case cases[] = {{3, true, true}, {1, true, false}, {2, false, true},
                          {0, false, false}};
    for (const Case& item : cases) {
        portal.config = MockPortalConfig{};
        portal.config.available_types = item.types;
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        waitCapabilities(fixture, item.monitor, item.window);
        const VisualCaptureCapabilities capabilities = fixture.capture->visualCapabilities();
        CHECK(capabilities.monitor == item.monitor);
        CHECK(capabilities.window == item.window);
    }
}

void testNoPortal(MockPortal& portal)
{
    portal.unownName();
    g_usleep(30000);
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    fixture.runtime.pumpFor(300);
    const VisualCaptureCapabilities capabilities = fixture.capture->visualCapabilities();
    CHECK(!capabilities.monitor);
    CHECK(!capabilities.window);
    CHECK(fixture.select(makeVisualRef(90), VisualSourceKind::monitor)
          == DesktopResult::unavailable);
    CHECK(portal.calls.empty());
    CHECK(portal.ownName());
}

void testMonitorSelection(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(1);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    CHECK(portal.observed_types == 1u);
    CHECK(!portal.observed_multiple);
    CHECK(portal.observed_parent.empty());
    CHECK(portal.calls.size() == 4);
    CHECK(portal.calls[0] == "CreateSession");
    CHECK(portal.calls[1] == "SelectSources");
    CHECK(portal.calls[2] == "Start");
    CHECK(portal.calls[3] == "OpenPipeWireRemote");
    CHECK(fixture.harness.results.size() == 1);
    CHECK(fixture.harness.result_refs.size() == 1);
    CHECK(fixture.harness.result_refs[0].offer_id == 1);
    CHECK(fixture.pipewire == nullptr);

    const TestSource monitor_a = makePresentation("monitor-A", 640, 360, 20, 60, 100, 11);
    const TestSource monitor_b = makePresentation("monitor-B", 640, 360, 200, 60, 30, 200);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.pipewire != nullptr);
    if (fixture.pipewire != nullptr) {
        CHECK(fixture.pipewire->node_received == portal.config.monitor_node);
        CHECK(fixture.pipewire->fd_received >= 0);
        fixture.pipewire->pushFrame(toVisualFrame(monitor_a));
        CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 1; }));
        if (!fixture.harness.frames.empty()) {
            CHECK(frameMatches(fixture.harness.frames[0], monitor_a));
            CHECK(!frameMatches(fixture.harness.frames[0], monitor_b));
        }

        const std::string snapshot
            = fixture.harness.writeSnapshot("capture_wayland_monitor.ppm");
        CHECK(!snapshot.empty());
        const auto saved_callbacks = fixture.pipewire->saved;
        fixture.capture->stopCapture(reference);
        fixture.pipewire = nullptr;
        fixture.runtime.pumpFor(150);
        CHECK(portal.session_close_count >= 1);
        const std::size_t frames_after_stop = fixture.harness.frameCount();
        saved_callbacks.frame(toVisualFrame(monitor_a));
        fixture.runtime.pumpFor(100);
        CHECK(fixture.harness.frameCount() == frames_after_stop);
    }
}

void testWindowSelection(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(2);
    CHECK(fixture.select(reference, VisualSourceKind::window) == DesktopResult::ready);
    CHECK(portal.observed_types == 2u);
    CHECK(!portal.observed_multiple);

    const TestSource window = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 15, 120,
                                               70, 33);
    const TestSource occluder = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 240,
                                                 0, 240, 200);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.pipewire != nullptr);
    if (fixture.pipewire != nullptr) {
        CHECK(fixture.pipewire->node_received == portal.config.window_node);
        fixture.pipewire->pushFrame(toVisualFrame(window));
        CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 1; }));
        if (!fixture.harness.frames.empty()) {
            CHECK(frameMatches(fixture.harness.frames[0], window));
            CHECK(!frameContainsColor(fixture.harness.frames[0], 240, 0, 240));
            CHECK(!frameMatches(fixture.harness.frames[0], occluder));
        }
        fixture.capture->stopCapture(reference);
    }
}

void testUnsupportedKind(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.config.available_types = 1;
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    CHECK(fixture.select(makeVisualRef(3), VisualSourceKind::window)
          == DesktopResult::unavailable);
    CHECK(portal.calls.empty());
}

void testCancelAndDenied(MockPortal& portal)
{
    {
        portal.config = MockPortalConfig{};
        portal.config.defer_start_response = true;
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        const VisualShareRef reference = makeVisualRef(4);
        CHECK(fixture.requestPending(reference, VisualSourceKind::monitor));
        portal.respondStart(1);
        CHECK(fixture.runtime.pump([&] { return !fixture.harness.results.empty(); }));
        CHECK(fixture.harness.results.back() == DesktopResult::cancelled);
        fixture.runtime.pumpFor(150);
        CHECK(portal.session_close_count >= 1);
    }
    {
        portal.config = MockPortalConfig{};
        portal.config.start_response = 2;
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        CHECK(fixture.select(makeVisualRef(5), VisualSourceKind::monitor)
              == DesktopResult::denied);
        fixture.runtime.pumpFor(150);
        CHECK(portal.session_close_count >= 1);
    }
    {
        portal.config = MockPortalConfig{};
        portal.config.start_response = 1;
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        CHECK(fixture.select(makeVisualRef(6), VisualSourceKind::monitor)
              == DesktopResult::cancelled);
    }
}

void testStopDuringSelection(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.config.defer_start_response = true;
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(7);
    CHECK(fixture.requestPending(reference, VisualSourceKind::monitor));
    fixture.capture->stopCapture(reference);
    CHECK(fixture.runtime.pump([&] { return !fixture.harness.results.empty(); }));
    CHECK(fixture.harness.results.size() == 1);
    CHECK(fixture.harness.results.back() == DesktopResult::cancelled);
    fixture.runtime.pumpFor(200);
    CHECK(portal.request_close_count >= 1);
    CHECK(fixture.pipewire == nullptr);

    portal.respondStart(0);
    fixture.runtime.pumpFor(200);
    CHECK(fixture.harness.results.size() == 1);
}

void testStopIgnoresOtherReference(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.config.defer_start_response = true;
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(8);
    CHECK(fixture.requestPending(reference, VisualSourceKind::monitor));
    fixture.capture->stopCapture(makeVisualRef(800));
    CHECK(fixture.harness.results.empty());
    portal.respondStart(0);
    CHECK(fixture.runtime.pump([&] { return !fixture.harness.results.empty(); }));
    CHECK(fixture.harness.results.back() == DesktopResult::ready);
    fixture.capture->stopCapture(reference);
}

void testTypeAttestation(MockPortal& portal)
{
    {
        portal.config = MockPortalConfig{};
        portal.config.window_source_type = 1; // el portal devuelve un monitor
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        CHECK(fixture.select(makeVisualRef(9), VisualSourceKind::window)
              == DesktopResult::unavailable);
        CHECK(fixture.pipewire == nullptr);
        fixture.runtime.pumpFor(150);
        CHECK(portal.session_close_count >= 1);
    }
    {
        portal.config = MockPortalConfig{};
        portal.config.include_source_type = false;
        portal.resetObservations();
        Fixture fixture(portal);
        CHECK(fixture.initialise());
        CHECK(fixture.select(makeVisualRef(10), VisualSourceKind::window)
              == DesktopResult::unavailable);
    }
}

void testPipeWireRemoteFailure(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.config.fail_open_pipewire = true;
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    CHECK(fixture.select(makeVisualRef(11), VisualSourceKind::monitor)
          == DesktopResult::error);
    CHECK(fixture.pipewire == nullptr);
}

void testFrameStatesAndLateCallbacks(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(12);
    CHECK(fixture.select(reference, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.pipewire != nullptr);
    if (fixture.pipewire == nullptr) {
        return;
    }
    const TestSource initial = makePresentation("monitor-A", 640, 360, 20, 60, 100, 11);
    fixture.pipewire->pushFrame(toVisualFrame(initial));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 1; }));

    const TestSource resized = makePresentation("monitor-A", 800, 600, 20, 60, 100, 11);
    fixture.pipewire->pushSize({resized.width, resized.height});
    fixture.pipewire->pushFrame(toVisualFrame(resized));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 2; }));
    CHECK(fixture.harness.sawState(VisualSourceState::available));
    CHECK(stateBeforeFrame(fixture.harness, VisualSourceState::available,
                           {resized.width, resized.height}));
    CHECK(frameMatches(fixture.harness.frames[1], resized));

    const std::size_t before_stop = fixture.harness.frameCount();
    const auto late = fixture.pipewire->saved;
    fixture.capture->stopCapture(reference);
    fixture.pipewire = nullptr;
    late.frame(toVisualFrame(initial));
    late.size_changed({resized.width, resized.height});
    fixture.runtime.pumpFor(150);
    CHECK(fixture.harness.frameCount() == before_stop);
}

void testSessionClosed(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(13);
    CHECK(fixture.select(reference, VisualSourceKind::window) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.pipewire != nullptr);
    if (fixture.pipewire == nullptr) {
        return;
    }
    const TestSource window = makePresentation("Editor", 480, 300, 15, 120, 70, 33);
    fixture.pipewire->pushFrame(toVisualFrame(window));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 1; }));

    const auto late = fixture.pipewire->saved;
    portal.emitSessionClosed();
    CHECK(fixture.runtime.pump([&] {
        return fixture.harness.sawState(VisualSourceState::closed);
    }));
    fixture.pipewire = nullptr;
    const std::size_t before = fixture.harness.frameCount();
    late.frame(toVisualFrame(window));
    fixture.runtime.pumpFor(150);
    CHECK(fixture.harness.frameCount() == before);
    fixture.capture->stopCapture(reference);
    fixture.capture->stopCapture(reference);
}

void testSecondOperation(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef first = makeVisualRef(14);
    CHECK(fixture.select(first, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.capture->startCapture(first, fixture.harness.captureCallbacks());
    fixture.capture->stopCapture(first);
    fixture.harness.results.clear();
    const VisualShareRef second = makeVisualRef(15);
    CHECK(fixture.select(second, VisualSourceKind::window) == DesktopResult::ready);
    CHECK(portal.observed_types == 2u);
    fixture.capture->stopCapture(second);
}

void testSecondSelectionWhileActive(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef first = makeVisualRef(16);
    CHECK(fixture.select(first, VisualSourceKind::monitor) == DesktopResult::ready);
    fixture.harness.results.clear();
    fixture.capture->selectSource(makeVisualRef(17), VisualSourceKind::monitor,
                                  fixture.harness.selectionCallback());
    CHECK(fixture.runtime.pump([&] { return !fixture.harness.results.empty(); }));
    CHECK(fixture.harness.results.back() == DesktopResult::error);
    fixture.capture->stopCapture(first);
}

void testWindowLifecycle(MockPortal& portal)
{
    portal.config = MockPortalConfig{};
    portal.resetObservations();
    Fixture fixture(portal);
    CHECK(fixture.initialise());
    const VisualShareRef reference = makeVisualRef(18);
    CHECK(fixture.select(reference, VisualSourceKind::window) == DesktopResult::ready);
    fixture.capture->startCapture(reference, fixture.harness.captureCallbacks());
    CHECK(fixture.pipewire != nullptr);
    if (fixture.pipewire == nullptr) {
        return;
    }

    const TestSource window = makePresentation("Editor \xE2\x80\x94 notas.txt", 480, 300, 15, 120,
                                               70, 33);
    fixture.pipewire->pushFrame(toVisualFrame(window));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 1; }));

    // Mover la ventana no cambia la identidad ni el tipo solicitado.
    const TestSource moved = shiftPresentation(window, 37, 21);
    fixture.pipewire->pushFrame(toVisualFrame(moved));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 2; }));
    CHECK(frameMatches(fixture.harness.frames[1], moved));
    CHECK(!fixture.harness.sawState(VisualSourceState::closed));

    // Redimensionar anuncia disponibilidad y conserva la misma operación.
    const TestSource resized = resizePresentation(moved, 620, 380);
    fixture.pipewire->pushSize({resized.width, resized.height});
    fixture.pipewire->pushFrame(toVisualFrame(resized));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == 3; }));
    CHECK(stateBeforeFrame(fixture.harness, VisualSourceState::available,
                           {resized.width, resized.height}));

    // Cerrar la fuente termina la operación; una nueva ventana con el mismo
    // título no reanuda la anterior.
    portal.emitSessionClosed();
    CHECK(fixture.runtime.pump([&] {
        return fixture.harness.sawState(VisualSourceState::closed);
    }));
    fixture.capture->stopCapture(reference);

    const VisualShareRef second = makeVisualRef(19);
    CHECK(fixture.select(second, VisualSourceKind::window) == DesktopResult::ready);
    fixture.capture->startCapture(second, fixture.harness.captureCallbacks());
    const std::size_t before = fixture.harness.frameCount();
    fixture.pipewire->pushFrame(toVisualFrame(window));
    CHECK(fixture.runtime.pump([&] { return fixture.harness.frameCount() == before + 1; }));
    fixture.capture->stopCapture(second);

    CHECK(!fixture.harness.writeSnapshot("capture_wayland_window.ppm").empty());
}

} // namespace

int main()
{
    MockPortal portal;
    if (!portal.start()) {
        std::fprintf(stderr, "No se pudo iniciar el bus D-Bus privado\n");
        return 2;
    }
    if (!portal.ownName()) {
        std::fprintf(stderr, "No se pudo publicar el portal simulado\n");
        portal.stop();
        return 2;
    }

    testFrameConversion();
    testNoPortal(portal);
    testCapabilities(portal);
    testMonitorSelection(portal);
    testWindowSelection(portal);
    testUnsupportedKind(portal);
    testCancelAndDenied(portal);
    testStopDuringSelection(portal);
    testStopIgnoresOtherReference(portal);
    testTypeAttestation(portal);
    testPipeWireRemoteFailure(portal);
    testFrameStatesAndLateCallbacks(portal);
    testSessionClosed(portal);
    testSecondOperation(portal);
    testSecondSelectionWhileActive(portal);
    testWindowLifecycle(portal);

    std::printf("capture_wayland: %d comprobaciones, %d fallos\n", check_count(),
                failure_count());
    portal.stop();
    return failure_count() == 0 ? 0 : 1;
}
