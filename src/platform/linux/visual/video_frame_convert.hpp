#pragma once

#include "../../../features/visual_share.hpp"

#include <cstddef>
#include <cstdint>

/*
 * Conversion of PipeWire's native buffer to the common visual contract.
 *
 * The capture adapter always delivers eight-bit BGRA, contiguous rows from top to bottom, opaque
 * alpha and exactly wide × high × 4 bytes. Here they validate dimensions and stride before booking
 * the image.
 */

namespace oneintwo::wayland {

inline constexpr std::uint32_t max_capture_dimension = 16384;
inline constexpr std::uint64_t max_capture_pixels = 64ull * 1024ull * 1024ull;

enum class VideoPixelFormat {
    bgra,
    bgrx,
    rgba,
    rgbx,
    unknown,
};

// View of a 32-bit SPA_VIDEO_FORMAT_RAW buffer per pixel.
struct VideoBufferView {
    VideoPixelFormat format = VideoPixelFormat::unknown;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int32_t stride = 0; // bytes per row; negative if image goes down
    const std::uint8_t* data = nullptr;
    std::size_t bytes = 0; // bytes accessible from data
};

enum class FrameConvertResult {
    ok,
    unsupported_format,
    invalid,
};

// Check the exact contract of an image already converted.
bool frameContractHolds(const VisualFrame& frame);

// It goes through an image BGRA putting alpha at 255.
void makeOpaque(VisualFrame& frame);

FrameConvertResult convertToBgra(const VideoBufferView& view, VisualFrame& out);

} // namespace oneintwo::wayland
