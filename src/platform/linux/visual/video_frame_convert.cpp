#include "video_frame_convert.hpp"

#include <cstring>
#include <limits>

namespace oneintwo::wayland {
namespace {

bool dimensionsAreSane(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0 || width > max_capture_dimension
        || height > max_capture_dimension) {
        return false;
    }
    const std::uint64_t pixels
        = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    return pixels <= max_capture_pixels;
}

} // namespace

bool frameContractHolds(const VisualFrame& frame)
{
    if (!dimensionsAreSane(frame.size.width, frame.size.height)) {
        return false;
    }
    const std::uint64_t expected = static_cast<std::uint64_t>(frame.size.width)
        * static_cast<std::uint64_t>(frame.size.height) * 4ull;
    return expected == frame.bgra.size();
}

void makeOpaque(VisualFrame& frame)
{
    const std::size_t pixels
        = static_cast<std::size_t>(frame.size.width) * frame.size.height;
    for (std::size_t index = 3; index < pixels * 4; index += 4) {
        frame.bgra[index] = 0xFF;
    }
}

FrameConvertResult convertToBgra(const VideoBufferView& view, VisualFrame& out)
{
    if (view.format == VideoPixelFormat::unknown) {
        return FrameConvertResult::unsupported_format;
    }
    if (view.data == nullptr || !dimensionsAreSane(view.width, view.height)) {
        return FrameConvertResult::invalid;
    }

    const std::uint32_t row_bytes = view.width * 4u;
    const std::int32_t stride = view.stride;
    if (stride == 0) {
        return FrameConvertResult::invalid;
    }

    const std::uint64_t stride_abs = static_cast<std::uint64_t>(
        stride < 0 ? -static_cast<std::int64_t>(stride) : stride);
    if (stride_abs < row_bytes) {
        return FrameConvertResult::invalid;
    }
    const std::uint64_t required
        = (static_cast<std::uint64_t>(view.height) - 1ull) * stride_abs + row_bytes;
    if (required > view.bytes || required > std::numeric_limits<std::size_t>::max()) {
        return FrameConvertResult::invalid;
    }

    VisualFrame frame;
    frame.size = {view.width, view.height};
    // The output is contiguous: without row filling, exactly width × height × 4.
    frame.bgra.resize(static_cast<std::size_t>(row_bytes) * view.height);

    const std::int64_t step = stride < 0 ? -static_cast<std::int64_t>(stride_abs)
                                         : static_cast<std::int64_t>(stride_abs);
    // data points to the beginning of the buffer; with negative stride the first row
    // of the image is the last area of the buffer.
    const std::uint8_t* first = stride < 0
        ? view.data + static_cast<std::size_t>(view.height - 1u) * stride_abs
        : view.data;

    for (std::uint32_t row = 0; row < view.height; ++row) {
        const std::uint8_t* source = first + static_cast<std::int64_t>(row) * step;
        std::uint8_t* target = frame.bgra.data() + static_cast<std::size_t>(row) * row_bytes;
        switch (view.format) {
        case VideoPixelFormat::bgra:
            std::memcpy(target, source, row_bytes);
            for (std::uint32_t x = 0; x < view.width; ++x) {
                target[x * 4u + 3u] = 0xFF;
            }
            break;
        case VideoPixelFormat::bgrx:
            std::memcpy(target, source, row_bytes);
            for (std::uint32_t x = 0; x < view.width; ++x) {
                target[x * 4u + 3u] = 0xFF;
            }
            break;
        case VideoPixelFormat::rgba:
        case VideoPixelFormat::rgbx:
            for (std::uint32_t x = 0; x < view.width; ++x) {
                const std::uint8_t* pixel = source + x * 4u;
                std::uint8_t* destination = target + x * 4u;
                destination[0] = pixel[2];
                destination[1] = pixel[1];
                destination[2] = pixel[0];
                destination[3] = 0xFF;
            }
            break;
        case VideoPixelFormat::unknown:
            return FrameConvertResult::unsupported_format;
        }
    }

    out = std::move(frame);
    return FrameConvertResult::ok;
}

} // namespace oneintwo::wayland
