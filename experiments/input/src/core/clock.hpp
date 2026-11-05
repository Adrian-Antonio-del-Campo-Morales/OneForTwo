#pragma once
// Responsabilidad: reloj monótono usado por los bucles de sesión. Los
// controladores reciben el tiempo como parámetro para poder probarse con un
// reloj falso.

#include <chrono>
#include <cstdint>

namespace oit {

inline int64_t steadyNowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace oit
