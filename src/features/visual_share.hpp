#pragma once

#include "../core/session.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>

namespace oneintwo {

inline constexpr auto visual_offer_lifetime = std::chrono::seconds{30};
inline constexpr std::uint32_t max_visual_frame_rate = 30;
inline constexpr std::uint32_t max_visual_dimension = 1920;
inline constexpr std::uint32_t max_visual_pixels = 1920 * 1080;
inline constexpr std::size_t max_visual_encoded_bytes = 1024 * 1024;

enum class VisualSourceKind {
    monitor,
    window,
};

enum class VisualInitiator {
    local,
    peer,
};

// El iniciador es relativo a este equipo. Protocolo lo deduce del sentido.
struct VisualShareRef {
    SessionGeneration generation;
    VisualInitiator initiator;
    std::uint64_t offer_id;
};

struct VisualShareOffer {
    VisualShareRef ref;
    VisualSourceKind source;
};

enum class VisualDecision {
    accepted,
    rejected,
    busy,
};

enum class VisualSourceState {
    available,
    suspended,
    closed,
};

// Identifica una etapa de la fuente dentro de una operación visual.
using VisualSourceRevision = std::uint64_t;

struct VisualSize {
    std::uint32_t width;
    std::uint32_t height;
};

// Imagen local en memoria con filas BGRA contiguas. No es una trama de red.
struct VisualFrame {
    VisualSize size;
    std::vector<std::uint8_t> bgra;
};

// Unidad completa tras el reensamblado acotado del canal de datos.
struct VisualEncodedFrame {
    VisualShareRef ref;
    VisualSourceRevision revision;
    std::uint64_t sequence;
    VisualSize size;
    bool key_frame;
    std::vector<std::uint8_t> bytes;
};

// Entradas del coordinador y de la sesión. Una operación visual por par.
struct VisualDependencies;

class VisualShare {
public:
    explicit VisualShare(VisualDependencies dependencies);
    ~VisualShare();
    VisualShare(const VisualShare&) = delete;
    VisualShare& operator=(const VisualShare&) = delete;
    void tick();
    void setAuthorization(SessionGeneration generation, FeatureSet send, FeatureSet receive);
    void offerSource(SessionGeneration generation, VisualSourceKind source);
    void receiveOffer(const VisualShareOffer& offer);
    void decideOffer(VisualShareRef offer, VisualDecision decision);
    void receiveDecision(VisualShareRef offer, VisualDecision decision);
    void receiveSourceState(VisualShareRef offer, VisualSourceRevision revision,
                            VisualSourceState state);
    void receiveActivity(VisualShareRef offer, VisualSourceRevision revision);
    void receiveFrame(VisualEncodedFrame frame);
    void stop(VisualShareRef offer);
    void receiveStop(VisualShareRef offer);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Puerto de presentación implementado por la UI. El coordinador conecta el cierre.
class VisualViewer {
public:
    virtual ~VisualViewer() = default;
    virtual void open(const VisualShareOffer& offer) = 0;
    virtual void present(VisualShareRef offer, const VisualFrame& frame) = 0;
    virtual void clear(VisualShareRef offer, VisualSourceState state) = 0;
    virtual void close(VisualShareRef offer) = 0;
};

} // namespace oneintwo
