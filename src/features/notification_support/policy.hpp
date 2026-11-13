#pragma once

#include "../notifications.hpp"
#include "../../integration/feature_contracts.hpp"

#include <chrono>
#include <cstddef>

namespace oneintwo::notification_policy {

// docs/IMPLEMENTACION_PARALELA.md fija un máximo de tres avisos por ventana de
// un segundo monotónico en cada sentido. feature_contracts.hpp declara además
// notification_rate_per_second como cota del canal interactivo. La política
// local no puede superar esa cota.
inline constexpr unsigned rate_allowance = 3;
inline constexpr auto rate_window = std::chrono::seconds{1};

inline constexpr std::size_t queue_capacity = notification_queue_capacity;
inline constexpr auto queue_lifetime = notification_queue_lifetime;

// Un evento observado no trae identificador nativo al módulo. Se deduplica por
// identidad de aplicación y contenido dentro de una ventana acotada.
inline constexpr std::size_t observed_dedup_capacity = 64;
inline constexpr auto observed_dedup_lifetime = std::chrono::seconds{10};

static_assert(rate_allowance <= notification_rate_per_second,
              "la política local de frecuencia no puede superar la cota del canal");

} // namespace oneintwo::notification_policy
