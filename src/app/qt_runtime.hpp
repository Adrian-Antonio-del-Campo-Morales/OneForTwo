#pragma once

#include "../integration/feature_contracts.hpp"

namespace oneintwo {

// Reloj monotónico y ejecutor del hilo principal Qt. post nunca ejecuta la
// tarea inline cuando existe un QCoreApplication.
ModuleRuntime makeQtModuleRuntime();

} // namespace oneintwo
