// Responsabilidad: cálculo de coordenadas y puerta de activación del borde.
#include "geometry.hpp"

#include <algorithm>

namespace oit {

uint16_t ratioFromY(const Geo& geo, int32_t y) {
  if (geo.h <= 1) return 0;
  const int32_t clamped = std::max(geo.y, std::min(geo.y + geo.h - 1, y));
  const int64_t offset = static_cast<int64_t>(clamped - geo.y);
  return static_cast<uint16_t>((offset * 65535) / (geo.h - 1));
}

int32_t yFromRatio(const Geo& geo, uint16_t ratio) {
  if (geo.h <= 1) return geo.y;
  const int64_t offset = (static_cast<int64_t>(ratio) * (geo.h - 1)) / 65535;
  const int32_t y = geo.y + static_cast<int32_t>(offset);
  return std::max(geo.y, std::min(geo.y + geo.h - 1, y));
}

Edge returnEdge(Edge sourceEdge) {
  return sourceEdge == Edge::Right ? Edge::Left : Edge::Right;
}

EdgeGate::EdgeGate(const Geo& geo, Edge edge, int antiBouncePx, int cooldownMs)
    : geo_(geo),
      edge_(edge),
      antiBouncePx_(std::max(0, antiBouncePx)),
      cooldownMs_(std::max(0, cooldownMs)) {}

bool EdgeGate::observe(int32_t localX, int32_t localY, int64_t nowMs, uint16_t& ratioY) {
  const int32_t edgeX = edge_ == Edge::Right ? geo_.w - 1 : 0;
  const bool inner = edge_ == Edge::Right
                         ? localX <= geo_.w - 1 - antiBouncePx_
                         : localX >= antiBouncePx_;

  if (!armed_ && inner) armed_ = true;

  const bool atEdge = edge_ == Edge::Right ? localX >= edgeX : localX <= edgeX;
  const bool outward = haveLast_ && (edge_ == Edge::Right ? localX > lastX_ : localX < lastX_);
  const bool ready = nowMs >= cooldownUntilMs_;
  const bool trigger = armed_ && ready && atEdge && outward;

  lastX_ = localX;
  haveLast_ = true;

  if (!trigger) return false;
  armed_ = false;
  ratioY = ratioFromY(geo_, localY);
  return true;
}

void EdgeGate::noteReturn(int64_t nowMs) {
  armed_ = false;
  cooldownUntilMs_ = nowMs + cooldownMs_;
}

}  // namespace oit
