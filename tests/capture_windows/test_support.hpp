#pragma once

#include "integration/feature_contracts.hpp"

#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <utility>

namespace oneintwo::test {

inline int& check_count()
{
    static int count = 0;
    return count;
}

inline int& failure_count()
{
    static int count = 0;
    return count;
}

inline bool check(bool condition, const char* expression, const char* file, int line)
{
    ++check_count();
    if (!condition) {
        ++failure_count();
        std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expression);
    }
    return condition;
}

#define CHECK(condition) ::oneintwo::test::check((condition), #condition, __FILE__, __LINE__)

/*
 * Test Runer: ModuleRuntime.post glues and test thread drains in FIFO order. There are no native
 * threads in the Windows model, so this reproduces the serialization of callbacks required by the
 * contract.
 */
struct TestRuntime {
    std::deque<Task> tasks;
    ModuleRuntime module;

    TestRuntime()
    {
        module.now = [] { return std::chrono::steady_clock::now(); };
        module.post = [this](Task task) { tasks.push_back(std::move(task)); };
    }

    void drain()
    {
        for (;;) {
            if (tasks.empty()) {
                return;
            }
            Task task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
    }
};

} // namespace oneintwo::test
