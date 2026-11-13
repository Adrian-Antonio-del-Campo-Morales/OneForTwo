#pragma once

#include "../visual_share.hpp"

#include <optional>
#include <string>

namespace oneintwo {

// Comprueba que la imagen local tiene exactamente ancho × alto × 4 bytes BGRA.
bool validateVisualFrameGeometry(const VisualFrame& frame, std::string* error = nullptr);

// El soporte de JPEG se comprueba al arrancar; sin él, la función visual debe
// declararse no disponible.
bool jpegCodecAvailable();

// Escala si hace falta, codifica JPEG con calidad inicial 75 y reduce calidad
// y después resolución hasta respetar max_visual_encoded_bytes. Devuelve vacío
// si no puede respetar los límites.
std::optional<VisualEncodedFrame> encodeVisualFrame(const VisualShareRef& ref,
                                                    VisualSourceRevision revision,
                                                    std::uint64_t sequence,
                                                    const VisualFrame& frame,
                                                    std::string* error = nullptr);

// Valida los límites antes de reservar y comprueba las dimensiones del JPEG
// antes de decodificarlo. Devuelve vacío si no es una imagen admisible.
std::optional<VisualFrame> decodeVisualFrame(const VisualEncodedFrame& frame,
                                             std::string* error = nullptr);

} // namespace oneintwo
