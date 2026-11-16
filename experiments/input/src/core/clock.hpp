#pragma once
// Responsibility: monotonous clock used by session loops.
// controllers receive time as a parameter to be tested with a
// reloj falso.

#include <chrono>
#include <cstdint>

namespace oit {

inline int64_t steadyNowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace oit
