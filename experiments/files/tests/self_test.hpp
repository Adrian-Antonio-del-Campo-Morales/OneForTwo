// Responsabilidad: exponer las comprobaciones aisladas al ejecutable principal
// (--self-test). No inicializa escritorio, portapapeles ni red.
#pragma once

namespace o2f {
int runSelfTests(bool verbose);
}
