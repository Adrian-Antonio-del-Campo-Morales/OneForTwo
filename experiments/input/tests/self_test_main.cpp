// Responsabilidad: ejecutable de pruebas aisladas del núcleo, sin backends.
#include "self_test.hpp"

int main() {
  return oit::runSelfTests() == 0 ? 0 : 1;
}
