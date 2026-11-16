#pragma once

#include "../notifications.hpp"
#include "../../integration/feature_contracts.hpp"

#include <chrono>
#include <cstddef>

namespace oneintwo::notification_policy {

// docs/PARALLEL_IMPLEMENTATION.md sets a maximum of three notices by window of
// a second monotonic in each direction. feature_contracts.hpp is the only
// source of that figure; local politics uses the same channel size.
inline constexpr unsigned rate_allowance = notification_rate_per_second;
inline constexpr auto rate_window = std::chrono::seconds{1};

inline constexpr std::size_t queue_capacity = notification_queue_capacity;
inline constexpr auto queue_lifetime = notification_queue_lifetime;

// An observed event does not bring native identifier to the module. It is doubled by
// application identity and content within a narrowed window.
inline constexpr std::size_t observed_dedup_capacity = 64;
inline constexpr auto observed_dedup_lifetime = std::chrono::seconds{10};

static_assert(rate_allowance > 0, "the frequency policy should allow warnings");

} // namespace oneintwo::notification_policy
