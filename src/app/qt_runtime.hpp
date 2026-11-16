#pragma once

#include "../integration/feature_contracts.hpp"

namespace oneintwo {

// Monotonic watch and main thread executor Qt. post never executes the
// task inline when there is a QCoreApplication.
ModuleRuntime makeQtModuleRuntime();

} // namespace oneintwo
