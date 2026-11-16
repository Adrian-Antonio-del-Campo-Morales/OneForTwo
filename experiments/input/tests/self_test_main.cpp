// Responsibility: Test executable isolated from the core, without backends.
#include "self_test.hpp"

int main() {
  return oit::runSelfTests() == 0 ? 0 : 1;
}
