#pragma once

#include "wgc_probe_core.hpp"

#include <string>

namespace oneintwo::test {

class FakeProbeRuntime final : public wgc_probe::Runtime {
public:
    std::wstring missing_module;
    std::string missing_entry;
    bool supported = false;
    int support_calls = 0;

    wgc_probe::LoadResult loadModule(const wchar_t* name) override
    {
        if (missing_module == std::wstring(name)) {
            return {nullptr, true, 2};
        }
        return {static_cast<void*>(&handleToken()), false, 0};
    }

    wgc_probe::EntryPoint resolve(void* handle, const char* name) override
    {
        if (!handle || missing_entry == name) {
            return nullptr;
        }
        return &fakeEntry;
    }

    void unloadModule(void*) override {}

    bool wgcSupported() override
    {
        ++support_calls;
        return supported;
    }

    static void fakeEntry() {}

private:
    static int& handleToken()
    {
        static int token = 0;
        return token;
    }
};

} // namespace oneintwo::test
