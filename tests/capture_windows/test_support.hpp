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
 * Ejecutor de prueba: ModuleRuntime.post encola y el hilo de prueba drena en
 * orden FIFO. No hay hilos nativos en el modelo Windows, así que esto reproduce
 * la serialización de callbacks que exige el contrato.
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
