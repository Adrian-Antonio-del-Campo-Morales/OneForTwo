// Comprueba la carga real de las dependencias del backend WGC sin crear una
// sesión gráfica. Un WGC no soportado es un resultado válido; una DLL o un
// entry point ausente es un fallo.
#include "platform/windows/visual/win32_port.hpp"

#include <windows.h>

#include <cstdio>

namespace {

struct RequiredModule {
    const wchar_t* name;
    const char* entry_point;
};

bool checkRequiredModule(const RequiredModule& module, bool& missing_dll, bool& missing_entry)
{
    HMODULE handle = LoadLibraryExW(module.name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!handle) {
        std::printf("probe: %ls missing (dll, error %lu)\n", module.name,
                    static_cast<unsigned long>(GetLastError()));
        missing_dll = true;
        return false;
    }
    const FARPROC entry = GetProcAddress(handle, module.entry_point);
    if (!entry) {
        std::printf("probe: %ls missing entry point %s\n", module.name, module.entry_point);
        missing_entry = true;
        FreeLibrary(handle);
        return false;
    }
    std::printf("probe: %ls loaded (%s)\n", module.name, module.entry_point);
    FreeLibrary(handle);
    return true;
}

bool checkOptionalWindowsApp(bool& missing_dll)
{
    HMODULE handle = LoadLibraryExW(L"windowsapp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (handle) {
        std::printf("probe: windowsapp.dll loaded\n");
        FreeLibrary(handle);
        return true;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_MOD_NOT_FOUND) {
        // WindowsApp.lib es una umbrella library; no hay un windowsapp.dll
        // independiente en todas las instalaciones.
        std::printf("probe: windowsapp.dll not present (umbrella library; checked via "
                    "GraphicsCapture.dll)\n");
        return true;
    }
    std::printf("probe: windowsapp.dll missing (dll, error %lu)\n",
                static_cast<unsigned long>(error));
    missing_dll = true;
    return false;
}

bool checkGraphicsCaptureRuntime(bool& missing_entry)
{
    HMODULE handle = LoadLibraryExW(L"GraphicsCapture.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!handle) {
        // Si falta el componente de captura, el sistema puede ser anterior a WGC
        // o no exponerlo; deja que la comprobación de soporte dé el veredicto.
        std::printf("probe: GraphicsCapture.dll not present; WGC will report unsupported\n");
        return true;
    }
    const FARPROC activation = GetProcAddress(handle, "DllGetActivationFactory");
    if (!activation) {
        std::printf("probe: GraphicsCapture.dll missing entry point DllGetActivationFactory\n");
        missing_entry = true;
        FreeLibrary(handle);
        return false;
    }
    std::printf("probe: GraphicsCapture.dll loaded (DllGetActivationFactory)\n");
    FreeLibrary(handle);
    return true;
}

} // namespace

int main()
{
    const RequiredModule required[] = {
        {L"d3d11.dll", "D3D11CreateDevice"},
        {L"dxgi.dll", "CreateDXGIFactory1"},
        {L"dwmapi.dll", "DwmGetWindowAttribute"},
        {L"runtimeobject.dll", "RoGetActivationFactory"},
    };

    bool missing_dll = false;
    bool missing_entry = false;
    bool ok = true;
    for (const RequiredModule& module : required) {
        ok = checkRequiredModule(module, missing_dll, missing_entry) && ok;
    }
    ok = checkOptionalWindowsApp(missing_dll) && ok;
    ok = checkGraphicsCaptureRuntime(missing_entry) && ok;

    if (missing_dll) {
        std::printf("probe: result=missing-dll\n");
        return 2;
    }
    if (missing_entry) {
        std::printf("probe: result=missing-entry-point\n");
        return 3;
    }
    if (!ok) {
        std::printf("probe: result=missing-runtime\n");
        return 2;
    }

    const bool supported = oneintwo::windows::windowsGraphicsCaptureSupported();
    std::printf("probe: result=%s\n", supported ? "supported" : "unsupported");
    return 0;
}
