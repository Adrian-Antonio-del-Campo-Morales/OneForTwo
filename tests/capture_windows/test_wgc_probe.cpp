#include "test_support.hpp"
#include "wgc_probe_core.hpp"

#include <cstdio>
#include <string>

using namespace oneintwo;
using namespace oneintwo::test;

namespace {

namespace probe = oneintwo::wgc_probe;

class FakeRuntime final : public probe::Runtime {
public:
    std::wstring missing_module;
    std::string missing_entry;
    bool supported = false;
    int support_calls = 0;

    probe::LoadResult loadModule(const wchar_t* name) override
    {
        if (missing_module == name) {
            return {nullptr, true, 2};
        }
        return {&handle_token, false, 0};
    }

    probe::EntryPoint resolve(void* handle, const char* name) override
    {
        if (!handle || missing_entry == name) {
            return nullptr;
        }
        return &fake_entry;
    }

    void unloadModule(void*) override {}

    bool wgcSupported() override
    {
        ++support_calls;
        return supported;
    }

    static void fake_entry() {}

private:
    static int handle_token;
};

int FakeRuntime::handle_token = 0;

void testMissingRequiredDllExitsTwo()
{
    FakeRuntime runtime;
    runtime.missing_module = L"d3d11.dll";

    CHECK(probe::run(runtime) == 2);
    CHECK(runtime.support_calls == 0);
}

void testMissingEntryPointExitsThree()
{
    FakeRuntime runtime;
    runtime.missing_entry = "D3D11CreateDevice";

    CHECK(probe::run(runtime) == 3);
    CHECK(runtime.support_calls == 0);
}

void testUnsupportedWgcExitsZero()
{
    FakeRuntime runtime;
    runtime.supported = false;

    CHECK(probe::run(runtime) == 0);
    CHECK(runtime.support_calls == 1);
}

} // namespace

int main()
{
    testMissingRequiredDllExitsTwo();
    testMissingEntryPointExitsThree();
    testUnsupportedWgcExitsZero();

    std::printf("wgc_probe: %d comprobaciones, %d fallos\n", check_count(),
                failure_count());
    return failure_count() == 0 ? 0 : 1;
}
