#pragma once

// Responsabilidad: declarar la interfaz de consola y el ejecutor de
// auto-pruebas aisladas. No accede al portapapeles ni a la red por sí mismo.

#include <string>
#include <vector>

namespace oit {

int run_cli(const std::vector<std::string>& args);
int run_self_tests();

}  // namespace oit
