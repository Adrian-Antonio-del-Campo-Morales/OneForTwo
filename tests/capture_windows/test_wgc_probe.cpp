#include "test_support.hpp"
#include "wgc_probe_core.hpp"
#include "wgc_probe_fake_runtime.hpp"

#include <cstdio>
#include <string>

using namespace oneintwo;
using namespace oneintwo::test;

namespace {

namespace probe = oneintwo::wgc_probe;
using FakeRuntime = oneintwo::test::FakeProbeRuntime;

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
