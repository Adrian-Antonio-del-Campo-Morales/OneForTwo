#pragma once

#include "../../../features/visual_share.hpp"

#include <cstddef>
#include <cstdint>

/*
 * Conversión del búfer nativo de PipeWire al contrato visual común.
 *
 * El adaptador de captura entrega siempre BGRA de ocho bits, filas contiguas
 * de arriba hacia abajo, alpha opaco y exactamente ancho × alto × 4 bytes.
 * Aquí se validan dimensiones y stride antes de reservar la imagen.
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

// Vista de un búfer SPA_VIDEO_FORMAT_RAW de 32 bits por píxel.
struct VideoBufferView {
    VideoPixelFormat format = VideoPixelFormat::unknown;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int32_t stride = 0; // bytes por fila; negativo si la imagen va de abajo arriba
    const std::uint8_t* data = nullptr;
    std::size_t bytes = 0; // bytes accesibles desde data
};

enum class FrameConvertResult {
    ok,
    unsupported_format,
    invalid,
};

// Comprueba el contrato exacto de una imagen ya convertida.
bool frameContractHolds(const VisualFrame& frame);

// Atraviesa una imagen BGRA poniendo alpha a 255.
void makeOpaque(VisualFrame& frame);

FrameConvertResult convertToBgra(const VideoBufferView& view, VisualFrame& out);

} // namespace oneintwo::wayland
