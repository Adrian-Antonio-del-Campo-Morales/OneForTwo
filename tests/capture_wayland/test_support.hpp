#pragma once

#include "integration/feature_contracts.hpp"

#include <glib.h>

#include <chrono>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
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
 * Runtime de prueba: ModuleRuntime.post encola en orden FIFO y el hilo de
 * prueba drena la cola mientras avanza el bucle GLib del portal simulado.
 */
struct TestRuntime {
    std::mutex mutex;
    std::deque<Task> tasks;
    ModuleRuntime module;

    TestRuntime()
    {
        module.now = [] { return std::chrono::steady_clock::now(); };
        module.post = [this](Task task) {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push_back(std::move(task));
        };
    }

    void drain()
    {
        for (;;) {
            Task task;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (tasks.empty()) {
                    return;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
        }
    }

    bool pump(const std::function<bool()>& done, int timeout_ms = 5000)
    {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            drain();
            if (done()) {
                return true;
            }
            while (g_main_context_iteration(nullptr, FALSE) != FALSE) {
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                drain();
                return done();
            }
            g_usleep(2000);
        }
    }

    bool pumpFor(int milliseconds)
    {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(milliseconds);
        while (std::chrono::steady_clock::now() < deadline) {
            drain();
            while (g_main_context_iteration(nullptr, FALSE) != FALSE) {
            }
            g_usleep(2000);
        }
        drain();
        return true;
    }
};

} // namespace oneintwo::test
