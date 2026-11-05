#pragma once
// Responsabilidad: cola acotada y sin asignaciones para comunicar el hilo de
// captura con el bucle principal. Un productor y un consumidor.
//
// Si la cola se llena se descarta el evento nuevo; el hook no debe bloquearse.

#include <atomic>
#include <cstddef>

namespace oit {

template <typename T, size_t N>
class SpscRing {
 public:
  static_assert(N >= 2, "La cola necesita al menos dos posiciones");

  bool push(const T& value) noexcept {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t next = (head + 1) % N;
    if (next == tail_.load(std::memory_order_acquire)) return false;
    buffer_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T& value) noexcept {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false;
    value = buffer_[tail];
    tail_.store((tail + 1) % N, std::memory_order_release);
    return true;
  }

 private:
  T buffer_[N]{};
  std::atomic<size_t> head_{0};
  std::atomic<size_t> tail_{0};
};

}  // namespace oit
