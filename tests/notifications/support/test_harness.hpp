#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace oneintwo::tests {

struct TestCase {
    const char* name;
    void (*function)();
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& message);
int failure_count();
int run_all();

struct Registrar {
    Registrar(const char* name, void (*function)());
};

struct RequirementFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <typename T>
std::string describe(const T& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

} // namespace oneintwo::tests

#define TEST_CASE(name)                                                               \
    static void name();                                                               \
    static ::oneintwo::tests::Registrar registrar_##name(#name, &name);               \
    static void name()

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            ::oneintwo::tests::report_failure(__FILE__, __LINE__,                     \
                                              "CHECK falló: " #condition);            \
        }                                                                             \
    } while (false)

#define CHECK_MESSAGE(condition, message)                                             \
    do {                                                                              \
        if (!(condition)) {                                                           \
            ::oneintwo::tests::report_failure(                                        \
                __FILE__, __LINE__, "CHECK falló: " #condition " | " +                \
                                        ::std::string(message));                       \
        }                                                                             \
    } while (false)

#define REQUIRE(condition)                                                            \
    do {                                                                              \
        if (!(condition)) {                                                           \
            ::oneintwo::tests::report_failure(__FILE__, __LINE__,                     \
                                              "REQUIRE falló: " #condition);          \
            throw ::oneintwo::tests::RequirementFailure("REQUIRE falló: " #condition);\
        }                                                                             \
    } while (false)

#define CHECK_EQUAL(actual, expected)                                                 \
    do {                                                                              \
        const auto& actual_value = (actual);                                          \
        const auto& expected_value = (expected);                                      \
        if (!(actual_value == expected_value)) {                                      \
            ::oneintwo::tests::report_failure(                                        \
                __FILE__, __LINE__,                                                   \
                "CHECK_EQUAL falló: " #actual " != " #expected " (" +                 \
                    ::oneintwo::tests::describe(actual_value) + " vs " +              \
                    ::oneintwo::tests::describe(expected_value) + ")");               \
        }                                                                             \
    } while (false)
