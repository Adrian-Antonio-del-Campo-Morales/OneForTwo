#pragma once
// Responsabilidad: permitir que el ejecutable del experimento invoque los
// self-tests sin inicializar el escritorio ni la red.

namespace oit {
// Devuelve el número de comprobaciones fallidas (0 = todo correcto).
int runSelfTests();
}  // namespace oit
