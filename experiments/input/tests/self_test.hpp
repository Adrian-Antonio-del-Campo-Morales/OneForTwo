#pragma once
// Responsibility: to allow the executable of the experiment to invoke the
// self-tests without initializing the desktop or network.

namespace oit {
// Returns the number of failed checks (0 = all correct).
int runSelfTests();
}  // namespace oit
