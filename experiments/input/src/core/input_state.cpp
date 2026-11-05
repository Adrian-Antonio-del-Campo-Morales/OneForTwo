// Responsabilidad: mantener y liberar el estado de entradas pulsadas.
#include "input_state.hpp"

#include <algorithm>

namespace oit {

bool InputState::add(bool button, uint16_t code) {
  for (const HeldInput& held : held_) {
    if (held.button == button && held.code == code) return false;
  }
  held_.push_back(HeldInput{button, code});
  return true;
}

bool InputState::remove(bool button, uint16_t code) {
  for (auto it = held_.begin(); it != held_.end(); ++it) {
    if (it->button == button && it->code == code) {
      held_.erase(it);
      return true;
    }
  }
  return false;
}

bool InputState::keyDown(uint16_t usage) { return add(false, usage); }

bool InputState::keyUp(uint16_t usage) { return remove(false, usage); }

bool InputState::buttonDown(uint16_t button) { return add(true, button); }

bool InputState::buttonUp(uint16_t button) { return remove(true, button); }

std::vector<HeldInput> InputState::releaseAll() {
  std::vector<HeldInput> ordered(held_.rbegin(), held_.rend());
  held_.clear();
  return ordered;
}

void InputState::clear() { held_.clear(); }

}  // namespace oit
