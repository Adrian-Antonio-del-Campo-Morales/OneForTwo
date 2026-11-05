#pragma once
// Responsabilidad: geometría de un monitor, conversión de coordenadas y
// detección del cruce de borde con anti-rebote.

#include "types.hpp"
#include <cstdint>

namespace oit {

// Convierte la coordenada vertical absoluta en una proporción 0..65535.
uint16_t ratioFromY(const Geo& geo, int32_t y);

// Convierte una proporción en la coordenada vertical del rectángulo indicado.
int32_t yFromRatio(const Geo& geo, uint16_t ratio);

// El borde de retorno del destino es el opuesto al borde compartido del origen.
Edge returnEdge(Edge sourceEdge);

class EdgeGate {
 public:
  EdgeGate(const Geo& geo, Edge edge, int antiBouncePx, int cooldownMs);

  // Alimenta una posición local (relativa al rectángulo). Devuelve true una
  // sola vez por cruce y escribe la proporción vertical de entrada.
  bool observe(int32_t localX, int32_t localY, int64_t nowMs, uint16_t& ratioY);

  // Debe llamarse al volver al control local: exige alejarse del borde antes de
  // permitir otro cruce y aplica el enfriamiento.
  void noteReturn(int64_t nowMs);

 private:
  Geo geo_;
  Edge edge_;
  int antiBouncePx_;
  int cooldownMs_;
  bool armed_ = true;
  bool haveLast_ = false;
  int32_t lastX_ = 0;
  int64_t cooldownUntilMs_ = 0;
};

}  // namespace oit
