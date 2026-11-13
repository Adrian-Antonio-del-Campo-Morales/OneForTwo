#include "test_presentation.hpp"

#include <fstream>
#include <utility>

namespace oneintwo::test {
namespace {

std::uint8_t boosted(std::uint8_t value, int delta)
{
    const int result = static_cast<int>(value) + delta;
    return static_cast<std::uint8_t>(result < 0 ? 0 : (result > 255 ? 255 : result));
}

void writePixel(TestSource& source, std::uint32_t x, std::uint32_t y, std::uint8_t red,
                std::uint8_t green, std::uint8_t blue)
{
    const std::size_t index
        = (static_cast<std::size_t>(y) * source.width + x) * 4ull;
    source.bgra[index + 0] = blue;
    source.bgra[index + 1] = green;
    source.bgra[index + 2] = red;
    source.bgra[index + 3] = 0xFF;
}

} // namespace

TestSource makePresentation(const std::string& label, std::uint32_t width,
                            std::uint32_t height, std::uint8_t red, std::uint8_t green,
                            std::uint8_t blue, std::uint8_t marker)
{
    TestSource source;
    source.label = label;
    source.width = width;
    source.height = height;
    source.bgra.assign(static_cast<std::size_t>(width) * height * 4ull, 0);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint8_t pixel_red = red;
            std::uint8_t pixel_green = green;
            std::uint8_t pixel_blue = blue;
            if (((x / 64u) + (y / 64u)) % 2u == 0u) {
                pixel_red = boosted(red, 18);
                pixel_green = boosted(green, 18);
                pixel_blue = boosted(blue, 18);
            }
            if (x < 16u && y < 16u) {
                pixel_red = marker;
                pixel_green = static_cast<std::uint8_t>(255u - marker);
                pixel_blue = static_cast<std::uint8_t>(marker / 2u);
            }
            if (x < 4u || y < 4u || x >= width - 4u || y >= height - 4u) {
                pixel_red = 255;
                pixel_green = 255;
                pixel_blue = 255;
            }
            writePixel(source, x, y, pixel_red, pixel_green, pixel_blue);
        }
    }
    return source;
}

TestSource shiftPresentation(const TestSource& source, std::uint32_t dx, std::uint32_t dy)
{
    TestSource shifted = source;
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            const std::uint32_t target_x = (x + dx) % source.width;
            const std::uint32_t target_y = (y + dy) % source.height;
            const std::size_t from = (static_cast<std::size_t>(y) * source.width + x) * 4ull;
            const std::size_t to
                = (static_cast<std::size_t>(target_y) * source.width + target_x) * 4ull;
            for (int channel = 0; channel < 4; ++channel) {
                shifted.bgra[to + channel] = source.bgra[from + channel];
            }
        }
    }
    return shifted;
}

TestSource resizePresentation(const TestSource& source, std::uint32_t width,
                              std::uint32_t height)
{
    const std::size_t middle
        = (static_cast<std::size_t>(source.height / 2) * source.width + source.width / 2) * 4ull;
    const std::size_t marker = 0;
    return makePresentation(source.label, width, height, source.bgra[middle + 2],
                            source.bgra[middle + 1], source.bgra[middle + 0],
                            source.bgra[marker + 2]);
}

bool frameMatches(const VisualFrame& frame, const TestSource& source)
{
    if (frame.size.width != source.width || frame.size.height != source.height) {
        return false;
    }
    return frame.bgra == source.bgra;
}

bool frameContainsColor(const VisualFrame& frame, std::uint8_t red, std::uint8_t green,
                        std::uint8_t blue)
{
    const std::size_t pixels = frame.bgra.size() / 4;
    for (std::size_t index = 0; index < pixels; ++index) {
        if (frame.bgra[index * 4 + 2] == red && frame.bgra[index * 4 + 1] == green
            && frame.bgra[index * 4 + 0] == blue) {
            return true;
        }
    }
    return false;
}

VisualFrame toVisualFrame(const TestSource& source)
{
    VisualFrame frame;
    frame.size = {source.width, source.height};
    frame.bgra = source.bgra;
    return frame;
}

VisualShareRef makeVisualRef(std::uint64_t offer_id)
{
    VisualShareRef ref;
    ref.generation = 7;
    ref.initiator = VisualInitiator::local;
    ref.offer_id = offer_id;
    return ref;
}

std::function<void(VisualShareRef, DesktopResult)> CaptureHarness::selectionCallback()
{
    return [this](VisualShareRef ref, DesktopResult result) {
        result_refs.push_back(ref);
        results.push_back(result);
    };
}

VisualCaptureCallbacks CaptureHarness::captureCallbacks()
{
    VisualCaptureCallbacks callbacks;
    callbacks.frame = [this](VisualShareRef, VisualFrame frame) {
        CaptureEvent event;
        event.kind = CaptureEvent::Kind::frame;
        event.frame = std::move(frame);
        frames.push_back(event.frame);
        events.push_back(std::move(event));
    };
    callbacks.state_changed = [this](VisualShareRef, VisualSourceState state) {
        CaptureEvent event;
        event.kind = CaptureEvent::Kind::state;
        event.state = state;
        states.push_back(state);
        events.push_back(std::move(event));
    };
    return callbacks;
}

bool CaptureHarness::sawState(VisualSourceState state) const
{
    for (const VisualSourceState observed : states) {
        if (observed == state) {
            return true;
        }
    }
    return false;
}

std::string CaptureHarness::writeSnapshot(const std::string& path) const
{
    if (frames.empty() || path.empty()) {
        return {};
    }
    const VisualFrame& frame = frames.front();
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    file << "P6\n" << frame.size.width << " " << frame.size.height << "\n255\n";
    for (std::size_t index = 0; index < frame.bgra.size(); index += 4) {
        const char pixel[3] = {static_cast<char>(frame.bgra[index + 2]),
                               static_cast<char>(frame.bgra[index + 1]),
                               static_cast<char>(frame.bgra[index + 0])};
        file.write(pixel, 3);
    }
    return path;
}

} // namespace oneintwo::test
