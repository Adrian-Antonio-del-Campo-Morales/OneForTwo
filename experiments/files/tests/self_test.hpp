// Liability: expose isolated checks to the main executable
// (--self-test) does not initialize desktop, clipboard or network.
#pragma once

namespace o2f {
int runSelfTests(bool verbose);
}
