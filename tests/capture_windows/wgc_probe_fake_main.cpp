#include "wgc_probe_fake_runtime.hpp"

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr,
                     "usage: wgc_probe_fake <missing-dll|missing-entry-point|unsupported>\n");
        return 64;
    }

    oneintwo::test::FakeProbeRuntime runtime;
    const std::string mode = argv[1];
    if (mode == "missing-dll") {
        runtime.missing_module = L"d3d11.dll";
    } else if (mode == "missing-entry-point") {
        runtime.missing_entry = "D3D11CreateDevice";
    } else if (mode != "unsupported") {
        std::fprintf(stderr, "unknown probe mode: %s\n", argv[1]);
        return 64;
    }

    return oneintwo::wgc_probe::run(runtime);
}
