#pragma once
// Responsabilidad: seguimiento ordenado de teclas y botones pulsados para poder
// liberarlos en orden inverso ante cualquier salida.

#include <cstdint>
#include <vector>

namespace oit {

struct HeldInput {
  bool button = false;
  uint16_t code = 0;
};

class InputState {
 public:
  // Devuelven true solo si el estado cambia (evita duplicados y liberaciones
  // de elementos que no estaban pulsados).
  bool keyDown(uint16_t usage);
  bool keyUp(uint16_t usage);
  bool buttonDown(uint16_t button);
  bool buttonUp(uint16_t button);

  // Devuelve los elementos en orden de liberación (inverso al de pulsación) y
  // vacía el estado.
  std::vector<HeldInput> releaseAll();

  void clear();
  bool empty() const { return held_.empty(); }
  size_t size() const { return held_.size(); }

 private:
  bool add(bool button, uint16_t code);
  bool remove(bool button, uint16_t code);
  std::vector<HeldInput> held_;
};

}  // namespace oit
