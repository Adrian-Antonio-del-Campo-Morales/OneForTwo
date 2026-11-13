#include "test_harness.hpp"

#include <exception>
#include <iostream>

namespace oneintwo::tests {
namespace {
int failures = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

Registrar::Registrar(const char* name, void (*function)()) {
    registry().push_back(TestCase{name, function});
}

void report_failure(const char* file, int line, const std::string& message) {
    ++failures;
    std::cerr << "FALLO " << file << ":" << line << " | " << message << '\n';
}

int failure_count() {
    return failures;
}

int run_all() {
    int executed = 0;
    for (const auto& test : registry()) {
        const int before = failures;
        try {
            test.function();
        } catch (const RequirementFailure& failure) {
            std::cerr << "  " << test.name << ": " << failure.what() << '\n';
        } catch (const std::exception& error) {
            report_failure(test.name, 0, std::string("excepción no controlada: ") + error.what());
        } catch (...) {
            report_failure(test.name, 0, "excepción no controlada");
        }
        ++executed;
        std::cout << (failures == before ? "[OK]   " : "[FALLO]") << ' ' << test.name << '\n';
    }
    std::cout << executed << " casos, " << failures << " fallos\n";
    return failures == 0 ? 0 : 1;
}

} // namespace oneintwo::tests
