#pragma once

#include "../integration/feature_contracts.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace oneintwo {

// Network format of functions, version 1. View
// docs/PARALLEL_IMPLEMENTATION.md, Function Protocol, version 1.
inline constexpr std::uint16_t feature_protocol_version = 1;
inline constexpr std::size_t max_interactive_payload_bytes = 4096;
inline constexpr std::size_t max_fragment_data_bytes = 65536;
inline constexpr std::size_t visual_fragment_metadata_bytes = 50;
inline constexpr std::size_t feature_frame_header_bytes = 8;
inline constexpr std::size_t visual_fragment_payload_bytes =
    visual_fragment_metadata_bytes + max_fragment_data_bytes;

enum class FeatureWireType : std::uint16_t {
    offer = 1,
    decision = 2,
    state = 3,
    activity = 4,
    stop = 5,
    frame = 6,
    notice = 7,
};

enum class ProtocolChannel {
    interactive,
    data,
};

enum class ProtocolStatus {
    ok,
    needs_more,
    version_unsupported,
    unknown_type,
    malformed,
    trailing_bytes,
    payload_too_large,
    out_of_range,
    generation_mismatch,
    assembly_mismatch,
    channel_mismatch,
};

// Explicit translation between generations. `local` is the generation of this
// and `peer` the one announced by the pair during the establishment.
struct GenerationPair {
    SessionGeneration local = 0;
    SessionGeneration peer = 0;
};

struct WireFrame {
    ProtocolChannel channel = ProtocolChannel::interactive;
    std::vector<std::uint8_t> bytes;
};

struct DecodeResult {
    ProtocolStatus status = ProtocolStatus::needs_more;
    std::optional<FeatureMessage> message;
    ProtocolChannel channel = ProtocolChannel::interactive;
};

// A video image produces several WireFrames; the rest produces one.
// Returns empty and fixes `error` if the object is not representative.
std::vector<WireFrame> encodeFeatureMessage(const FeatureMessage& message,
                                            GenerationPair generations,
                                            ProtocolStatus* error = nullptr);

ProtocolStatus encodeFeatureMessage(const FeatureMessage& message,
                                    GenerationPair generations,
                                    std::vector<WireFrame>& out);

// Incremental decoder with a single video reassembly. An error
// structural is terminal: the caller must close the channel and restart.
class FeatureFrameDecoder {
public:
    explicit FeatureFrameDecoder(GenerationPair generations = {});
    ~FeatureFrameDecoder();
    FeatureFrameDecoder(const FeatureFrameDecoder&) = delete;
    FeatureFrameDecoder& operator=(const FeatureFrameDecoder&) = delete;
    void setGenerations(GenerationPair generations);
    void reset();

    DecodeResult feed(const std::uint8_t* data, std::size_t size);
    std::size_t bufferedBytes() const;
    bool hasAssembly() const;

private:
    ProtocolStatus decodePayload(FeatureWireType type, const std::uint8_t* payload,
                                 std::size_t size, DecodeResult& out);
    ProtocolStatus appendFragment(const std::uint8_t* payload, std::size_t size,
                                  DecodeResult& out);

    GenerationPair generations_;
    std::vector<std::uint8_t> buffer_;
    struct Assembly;
    std::unique_ptr<Assembly> assembly_;
};

} // namespace oneintwo
