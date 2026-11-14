#pragma once

#include <cstdio>

namespace oneintwo::wgc_probe {

using EntryPoint = void (*)();

struct LoadResult {
    void* handle = nullptr;
    bool not_found = false;
    unsigned long error = 0;
};

class Runtime {
public:
    virtual ~Runtime() = default;

    virtual LoadResult loadModule(const wchar_t* name) = 0;
    virtual EntryPoint resolve(void* handle, const char* name) = 0;
    virtual void unloadModule(void* handle) = 0;
    virtual bool wgcSupported() = 0;
};

enum class Outcome {
    supported,
    unsupported,
    missing_dll,
    missing_entry_point,
};

struct RequiredModule {
    const wchar_t* name;
    const char* entry_point;
};

inline constexpr RequiredModule kRequiredModules[] = {
    {L"d3d11.dll", "D3D11CreateDevice"},
    {L"dxgi.dll", "CreateDXGIFactory1"},
    {L"dwmapi.dll", "DwmGetWindowAttribute"},
    {L"runtimeobject.dll", "RoGetActivationFactory"},
};

inline bool checkRequired(Runtime& runtime, const RequiredModule& module, bool& missing_dll,
                          bool& missing_entry)
{
    const LoadResult loaded = runtime.loadModule(module.name);
    if (!loaded.handle) {
        std::printf("probe: %ls missing (dll, error %lu)\n", module.name, loaded.error);
        missing_dll = true;
        return false;
    }
    if (!runtime.resolve(loaded.handle, module.entry_point)) {
        std::printf("probe: %ls missing entry point %s\n", module.name, module.entry_point);
        missing_entry = true;
        runtime.unloadModule(loaded.handle);
        return false;
    }
    std::printf("probe: %ls loaded (%s)\n", module.name, module.entry_point);
    runtime.unloadModule(loaded.handle);
    return true;
}

inline bool checkWindowsApp(Runtime& runtime, bool& missing_dll)
{
    const LoadResult loaded = runtime.loadModule(L"windowsapp.dll");
    if (loaded.handle) {
        std::printf("probe: windowsapp.dll loaded\n");
        runtime.unloadModule(loaded.handle);
        return true;
    }
    if (loaded.not_found) {
        // WindowsApp.lib es una umbrella library; no hay un windowsapp.dll
        // independiente en todas las instalaciones.
        std::printf("probe: windowsapp.dll not present (umbrella library; checked via "
                    "GraphicsCapture.dll)\n");
        return true;
    }
    std::printf("probe: windowsapp.dll missing (dll, error %lu)\n", loaded.error);
    missing_dll = true;
    return false;
}

inline bool checkGraphicsCapture(Runtime& runtime, bool& missing_entry)
{
    const LoadResult loaded = runtime.loadModule(L"GraphicsCapture.dll");
    if (!loaded.handle) {
        // Si falta el componente de captura, el sistema puede ser anterior a WGC
        // o no exponerlo; deja que la comprobación de soporte dé el veredicto.
        std::printf("probe: GraphicsCapture.dll not present; WGC will report unsupported\n");
        return true;
    }
    if (!runtime.resolve(loaded.handle, "DllGetActivationFactory")) {
        std::printf("probe: GraphicsCapture.dll missing entry point DllGetActivationFactory\n");
        missing_entry = true;
        runtime.unloadModule(loaded.handle);
        return false;
    }
    std::printf("probe: GraphicsCapture.dll loaded (DllGetActivationFactory)\n");
    runtime.unloadModule(loaded.handle);
    return true;
}

inline Outcome evaluate(Runtime& runtime)
{
    bool missing_dll = false;
    bool missing_entry = false;

    for (const RequiredModule& module : kRequiredModules) {
        checkRequired(runtime, module, missing_dll, missing_entry);
    }
    checkWindowsApp(runtime, missing_dll);
    checkGraphicsCapture(runtime, missing_entry);

    if (missing_dll) {
        return Outcome::missing_dll;
    }
    if (missing_entry) {
        return Outcome::missing_entry_point;
    }
    return runtime.wgcSupported() ? Outcome::supported : Outcome::unsupported;
}

inline const char* outcomeName(Outcome outcome)
{
    switch (outcome) {
    case Outcome::supported:
        return "supported";
    case Outcome::unsupported:
        return "unsupported";
    case Outcome::missing_dll:
        return "missing-dll";
    case Outcome::missing_entry_point:
        return "missing-entry-point";
    }
    return "unknown";
}

inline int exitCode(Outcome outcome)
{
    switch (outcome) {
    case Outcome::missing_dll:
        return 2;
    case Outcome::missing_entry_point:
        return 3;
    case Outcome::supported:
    case Outcome::unsupported:
        return 0;
    }
    return 2;
}

inline int run(Runtime& runtime)
{
    const Outcome outcome = evaluate(runtime);
    std::printf("probe: result=%s\n", outcomeName(outcome));
    return exitCode(outcome);
}

} // namespace oneintwo::wgc_probe
