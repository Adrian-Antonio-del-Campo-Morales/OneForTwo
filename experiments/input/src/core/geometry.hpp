#pragma once
// Responsibility: Monitor geometry, coordinate conversion and
// detection of border crossing with anti-rebound.

#include "types.hpp"
#include <cstdint>

namespace oit {

// Converts the absolute vertical coordinate into a ratio 0.65535.
uint16_t ratioFromY(const Geo& geo, int32_t y);

// Converts a proportion into the vertical coordinate of the indicated rectangle.
int32_t yFromRatio(const Geo& geo, uint16_t ratio);

// The return edge of the destination is the opposite of the shared border of the origin.
Edge returnEdge(Edge sourceEdge);

class EdgeGate {
 public:
  EdgeGate(const Geo& geo, Edge edge, int antiBouncePx, int cooldownMs);

  // Feeds a local position (relative to the rectangle).
  // just once per crossing and write down the vertical input ratio.
  bool observe(int32_t localX, int32_t localY, int64_t nowMs, uint16_t& ratioY);

  // It should be called when returning to local control: it requires moving away from the edge before
  // allow another crossing and apply cooling.
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
