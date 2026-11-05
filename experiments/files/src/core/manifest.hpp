// Responsabilidad: construir la oferta a partir de rutas autorizadas, aplicar
// límites y conservar el mapa interno de ruta relativa a ruta absoluta. Ese mapa
// nunca se transmite; el receptor solo recibe rutas relativas e identificadores.
#pragma once

#include "core/model.hpp"

#include <map>
#include <string>
#include <vector>

namespace o2f {

struct BuiltOffer {
    Offer offer;
    std::map<std::string, std::string> absoluteByRel;
};

BuiltOffer buildOffer(const std::vector<std::string>& rootsUtf8, const Limits& limits);

// Formato de estado de la caché (una línea de cabecera y una por elemento).
std::string serializeManifestText(const Offer& offer);
Offer parseManifestText(const std::string& text, const Limits& limits);

// Resumen sin rutas absolutas ni nombres de archivo: apto para logs por defecto.
std::string describeOffer(const Offer& offer);

}  // namespace o2f
