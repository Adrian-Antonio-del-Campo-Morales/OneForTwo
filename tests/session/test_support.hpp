#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace oneintwo_test {

struct Failure {
    std::string message;
};

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        cases().push_back(Case{name, std::move(body)});
    }
};

template <typename T,
          typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value,
                                    int> = 0>
std::string describe(T value) {
    return std::to_string(value);
}
inline std::string describe(const std::string& value) {
    return "\"" + value + "\"";
}
inline std::string describe(bool value) {
    return value ? "true" : "false";
}

template <typename T, typename std::enable_if_t<std::is_enum<T>::value, int> = 0>
std::string describe(T value) {
    return std::to_string(static_cast<long long>(value));
}

inline int run() {
    int failed = 0;
    for (const Case& test : cases()) {
        std::printf("[ RUN  ] %s\n", test.name.c_str());
        std::fflush(stdout);
        try {
            test.body();
            std::printf("[ OK   ] %s\n", test.name.c_str());
        } catch (const Failure& failure) {
            ++failed;
            std::printf("[ FAIL ] %s\n         %s\n", test.name.c_str(),
                        failure.message.c_str());
        } catch (const std::exception& exception) {
            ++failed;
            std::printf("[ FAIL ] %s\n         excepción: %s\n", test.name.c_str(),
                        exception.what());
        }
        std::fflush(stdout);
    }
    std::printf("%zu casos, %d fallos\n", cases().size(), failed);
    return failed == 0 ? 0 : 1;
}

} // namespace oneintwo_test

#define TEST_CASE(name)                                                                    \
    static void name();                                                                    \
    static ::oneintwo_test::Registrar registrar_##name(#name, name);                       \
    static void name()

#define CHECK(expression)                                                                  \
    do {                                                                                   \
        if (!(expression)) {                                                               \
            throw ::oneintwo_test::Failure{std::string(__FILE__) + ":" +                   \
                                           std::to_string(__LINE__) +                      \
                                           ": CHECK(" #expression ")"};                    \
        }                                                                                  \
    } while (false)

#define CHECK_EQ(actual, expected)                                                         \
    do {                                                                                   \
        const auto actual_value = (actual);                                                \
        const auto expected_value = (expected);                                            \
        if (!(actual_value == expected_value)) {                                           \
            throw ::oneintwo_test::Failure{                                                \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +                   \
                ": CHECK_EQ(" #actual ", " #expected ") esperado " +                       \
                ::oneintwo_test::describe(expected_value) + ", obtenido " +                \
                ::oneintwo_test::describe(actual_value)};                                  \
        }                                                                                  \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                            \
    do {                                                                                   \
        const double actual_value = (actual);                                              \
        const double expected_value = (expected);                                          \
        if (!((actual_value) >= (expected_value) - (tolerance) &&                          \
              (actual_value) <= (expected_value) + (tolerance))) {                         \
            throw ::oneintwo_test::Failure{                                                \
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +                   \
                ": CHECK_NEAR(" #actual ")"};                                             \
        }                                                                                  \
    } while (false)
