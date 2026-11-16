#pragma once

#include "../integration/feature_contracts.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace oneintwo {

// Formato de red de las funciones, versión 1. Ver
// docs/IMPLEMENTACION_PARALELA.md, "Protocolo de funciones, versión 1".
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

// Traducción explícita entre generaciones. `local` es la generación de este
// extremo y `peer` la anunciada por el par durante el establecimiento.
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

// Una imagen de vídeo produce varios WireFrame; el resto produce uno.
// Devuelve vacío y fija `error` si el objeto no es representable.
std::vector<WireFrame> encodeFeatureMessage(const FeatureMessage& message,
                                            GenerationPair generations,
                                            ProtocolStatus* error = nullptr);

ProtocolStatus encodeFeatureMessage(const FeatureMessage& message,
                                    GenerationPair generations,
                                    std::vector<WireFrame>& out);

// Decodificador incremental con un único reensamblado de vídeo. Un error
// estructural es terminal: el llamador debe cerrar el canal y reiniciar.
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
