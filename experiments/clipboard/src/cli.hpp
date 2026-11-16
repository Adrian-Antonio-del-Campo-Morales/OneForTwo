#pragma once

// Responsibility: declare the console interface and the executioner of
// Insulated auto-tests. It does not access the clipboard or the network itself.

#include <string>
#include <vector>

namespace oit {

int run_cli(const std::vector<std::string>& args);
int run_self_tests();

}  // namespace oit
