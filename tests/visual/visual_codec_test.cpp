#include "features/visual/visual_codec.hpp"

#include "../session/test_support.hpp"

#include <QGuiApplication>

#include <cstdint>
#include <cstdlib>
#include <string>

using namespace oneintwo;

namespace {

VisualFrame gradient(std::uint32_t width, std::uint32_t height) {
    VisualFrame frame;
    frame.size = VisualSize{width, height};
    frame.bgra.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            frame.bgra[index + 0] = static_cast<std::uint8_t>(x % 256);
            frame.bgra[index + 1] = static_cast<std::uint8_t>(y % 256);
            frame.bgra[index + 2] = 96;
            frame.bgra[index + 3] = 255;
        }
    }
    return frame;
}

VisualFrame noise(std::uint32_t width, std::uint32_t height) {
    VisualFrame frame;
    frame.size = VisualSize{width, height};
    frame.bgra.resize(static_cast<std::size_t>(width) * height * 4);
    std::uint32_t state = 0x12345678u;
    for (std::size_t index = 0; index < frame.bgra.size(); ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        frame.bgra[index] = static_cast<std::uint8_t>(state & 0xFFu);
    }
    for (std::size_t index = 3; index < frame.bgra.size(); index += 4) {
        frame.bgra[index] = 255;
    }
    return frame;
}

VisualShareRef localRef() {
    return VisualShareRef{1, VisualInitiator::local, 1};
}

} // namespace

TEST_CASE(codec_is_available) {
    CHECK(jpegCodecAvailable());
}

TEST_CASE(roundtrip_preserves_size_and_colours) {
    const VisualFrame source = gradient(200, 100);
    std::string error;
    const auto encoded = encodeVisualFrame(localRef(), 1, 1, source, &error);
    CHECK(encoded.has_value());
    CHECK_EQ(encoded->size.width, static_cast<std::uint32_t>(200));
    CHECK_EQ(encoded->size.height, static_cast<std::uint32_t>(100));
    CHECK(encoded->key_frame);
    CHECK(encoded->bytes.size() <= max_visual_encoded_bytes);
    CHECK(encoded->bytes.size() >= 2);
    CHECK_EQ(static_cast<int>(encoded->bytes[0]), 0xFF);
    CHECK_EQ(static_cast<int>(encoded->bytes[1]), 0xD8);

    const auto decoded = decodeVisualFrame(*encoded, &error);
    CHECK(decoded.has_value());
    CHECK_EQ(decoded->size.width, static_cast<std::uint32_t>(200));
    CHECK_EQ(decoded->size.height, static_cast<std::uint32_t>(100));
    CHECK_EQ(decoded->bgra.size(), source.bgra.size());

    const auto sample = [](const VisualFrame& frame, std::uint32_t x, std::uint32_t y,
                           std::size_t channel) {
        return static_cast<int>(
            frame.bgra[(static_cast<std::size_t>(y) * frame.size.width + x) * 4 + channel]);
    };
    CHECK_NEAR(sample(*decoded, 20, 20, 2), 96, 24);
    CHECK_NEAR(sample(*decoded, 150, 60, 0), sample(source, 150, 60, 0), 24);
    CHECK_NEAR(sample(*decoded, 150, 60, 1), sample(source, 150, 60, 1), 24);
    CHECK_EQ(sample(*decoded, 10, 10, 3), 255);
}

TEST_CASE(large_frames_are_scaled_within_contract_limits) {
    const VisualFrame source = gradient(4000, 3000);
    std::string error;
    const auto encoded = encodeVisualFrame(localRef(), 1, 2, source, &error);
    CHECK(encoded.has_value());
    CHECK(encoded->size.width <= max_visual_dimension);
    CHECK(encoded->size.height <= max_visual_dimension);
    CHECK(static_cast<std::uint64_t>(encoded->size.width) * encoded->size.height <=
          max_visual_pixels);
    CHECK(encoded->bytes.size() <= max_visual_encoded_bytes);
    const auto decoded = decodeVisualFrame(*encoded, &error);
    CHECK(decoded.has_value());
    CHECK_EQ(decoded->size.width, encoded->size.width);
}

TEST_CASE(noise_reduces_quality_then_resolution_to_fit_one_mebibyte) {
    const VisualFrame source = noise(1920, 1080);
    std::string error;
    const auto encoded = encodeVisualFrame(localRef(), 1, 3, source, &error);
    CHECK(encoded.has_value());
    CHECK(encoded->bytes.size() <= max_visual_encoded_bytes);
    CHECK(encoded->size.width <= max_visual_dimension);
    CHECK(encoded->size.height <= max_visual_dimension);
    const auto decoded = decodeVisualFrame(*encoded, &error);
    CHECK(decoded.has_value());
}

TEST_CASE(invalid_local_frames_are_rejected) {
    std::string error;
    VisualFrame frame = gradient(10, 10);
    frame.bgra.pop_back();
    CHECK(!validateVisualFrameGeometry(frame, &error));
    CHECK(!encodeVisualFrame(localRef(), 1, 1, frame, &error).has_value());

    frame = gradient(10, 10);
    frame.size.width = 0;
    CHECK(!encodeVisualFrame(localRef(), 1, 1, frame, &error).has_value());

    VisualFrame valid = gradient(10, 10);
    CHECK(!encodeVisualFrame(VisualShareRef{1, VisualInitiator::local, 0}, 1, 1, valid,
                             &error)
               .has_value());
    CHECK(!encodeVisualFrame(localRef(), 0, 1, valid, &error).has_value());
    CHECK(!encodeVisualFrame(localRef(), 1, 0, valid, &error).has_value());
}

TEST_CASE(decoder_rejects_malformed_or_mismatched_images) {
    std::string error;
    const auto encoded = encodeVisualFrame(localRef(), 1, 1, gradient(100, 50), &error);
    CHECK(encoded.has_value());

    VisualEncodedFrame wrong_size = *encoded;
    wrong_size.size = VisualSize{200, 100};
    CHECK(!decodeVisualFrame(wrong_size, &error).has_value());

    VisualEncodedFrame too_large = *encoded;
    too_large.size = VisualSize{1920, 1081};
    CHECK(!decodeVisualFrame(too_large, &error).has_value());

    VisualEncodedFrame empty = *encoded;
    empty.bytes.clear();
    CHECK(!decodeVisualFrame(empty, &error).has_value());

    VisualEncodedFrame oversized = *encoded;
    oversized.bytes.assign(max_visual_encoded_bytes + 1, 0);
    CHECK(!decodeVisualFrame(oversized, &error).has_value());

    VisualEncodedFrame garbage = *encoded;
    garbage.bytes.assign(256, 0x5A);
    CHECK(!decodeVisualFrame(garbage, &error).has_value());

    VisualEncodedFrame zero = *encoded;
    zero.size = VisualSize{0, 0};
    CHECK(!decodeVisualFrame(zero, &error).has_value());
}

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    return oneintwo_test::run();
}
