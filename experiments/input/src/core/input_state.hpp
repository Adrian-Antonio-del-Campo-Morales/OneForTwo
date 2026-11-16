#pragma once
// Responsibility: orderly tracking of keys and buttons pressed to be able
// release them in reverse order before any exit.

#include <cstdint>
#include <vector>

namespace oit {

struct HeldInput {
  bool button = false;
  uint16_t code = 0;
};

class InputState {
 public:
  // Return true only if state changes (avoids duplicates and releases)
  // of elements that were not pressed).
  bool keyDown(uint16_t usage);
  bool keyUp(uint16_t usage);
  bool buttonDown(uint16_t button);
  bool buttonUp(uint16_t button);

  // Returns the items in release order (inverse to the pulsation order) and
  // Empty the state.
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
