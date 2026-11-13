#pragma once

#include "features/visual_share.hpp"
#include "platform/desktop.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/*
 * Presentación local de prueba para el modelo Windows: dos monitores y dos
 * ventanas con el mismo título, con un patrón distinguible. El arnés es un
 * consumidor temporal de la interfaz y vive solo en las pruebas.
 */

namespace oneintwo::test {

struct TestSource {
    std::string label;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;
};

TestSource makePresentation(const std::string& label, std::uint32_t width,
                            std::uint32_t height, std::uint8_t red, std::uint8_t green,
                            std::uint8_t blue, std::uint8_t marker);
TestSource shiftPresentation(const TestSource& source, std::uint32_t dx, std::uint32_t dy);
TestSource resizePresentation(const TestSource& source, std::uint32_t width,
                              std::uint32_t height);

bool frameMatches(const VisualFrame& frame, const TestSource& source);
bool frameContainsColor(const VisualFrame& frame, std::uint8_t red, std::uint8_t green,
                        std::uint8_t blue);
VisualFrame toVisualFrame(const TestSource& source);
VisualShareRef makeVisualRef(std::uint64_t offer_id);

struct CaptureEvent {
    enum class Kind {
        frame,
        state,
    };
    Kind kind = Kind::frame;
    VisualSourceState state = VisualSourceState::available;
    VisualFrame frame;
};

class CaptureHarness {
public:
    std::vector<CaptureEvent> events;
    std::vector<VisualFrame> frames;
    std::vector<VisualSourceState> states;
    std::vector<DesktopResult> results;

    std::function<void(VisualShareRef, DesktopResult)> selectionCallback();
    VisualCaptureCallbacks captureCallbacks();
    bool sawState(VisualSourceState state) const;
    std::string writeSnapshot(const std::string& path) const;
};

} // namespace oneintwo::test
