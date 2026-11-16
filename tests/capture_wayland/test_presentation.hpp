#pragma once

#include "features/visual_share.hpp"
#include "platform/desktop.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/*
 * Local test presentation: distinguishable patterns for two monitors and for two windows with the
 * same title, plus a temporary consumer who writes down what the adapter delivers. This harness
 * lives only in the tests.
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

/*
 * Temporary consumer of the DesktopVisualCapture contract. Saves the order of states and frames to
 * check that a change of availability is announced before the accompanying image.
 */
class CaptureHarness {
public:
    std::vector<CaptureEvent> events;
    std::vector<VisualFrame> frames;
    std::vector<VisualSourceState> states;
    std::vector<DesktopResult> results;
    std::vector<VisualShareRef> result_refs;

    std::function<void(VisualShareRef, DesktopResult)> selectionCallback();
    VisualCaptureCallbacks captureCallbacks();

    std::size_t frameCount() const { return frames.size(); }
    bool sawState(VisualSourceState state) const;
    std::string writeSnapshot(const std::string& path) const;
};

} // namespace oneintwo::test
