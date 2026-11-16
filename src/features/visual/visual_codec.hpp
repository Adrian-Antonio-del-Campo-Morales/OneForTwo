#pragma once

#include "../visual_share.hpp"

#include <optional>
#include <string>

namespace oneintwo {

// Check that the local image is exactly wide × high × 4 bytes BGRA.
bool validateVisualFrameGeometry(const VisualFrame& frame, std::string* error = nullptr);

// The support of JPEG is checked when booting; without it, the visual function must
// to declare itself unavailable.
bool jpegCodecAvailable();

// Scale if necessary, encodes JPEG with initial quality 75 and reduces quality
// and then resolution to respect max_visual_encoded_bytes.
// if you can't respect the limits.
std::optional<VisualEncodedFrame> encodeVisualFrame(const VisualShareRef& ref,
                                                    VisualSourceRevision revision,
                                                    std::uint64_t sequence,
                                                    const VisualFrame& frame,
                                                    std::string* error = nullptr);

// Validate the limits before booking and check the dimensions of the JPEG
// before decoding. Returns empty if it is not an acceptable image.
std::optional<VisualFrame> decodeVisualFrame(const VisualEncodedFrame& frame,
                                             std::string* error = nullptr);

} // namespace oneintwo
