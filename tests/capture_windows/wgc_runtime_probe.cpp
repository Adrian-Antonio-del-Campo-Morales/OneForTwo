// Check the actual load of the WGC backend dependencies without creating a
// graphic session. An unsupported WGC is a valid result; a DLL or a
// missing entry point is a bug.
#include "platform/windows/visual/win32_port.hpp"
#include "wgc_probe_core.hpp"

#include <windows.h>

namespace {

class WindowsRuntime final : public oneintwo::wgc_probe::Runtime {
public:
    using LoadResult = oneintwo::wgc_probe::LoadResult;
    using EntryPoint = oneintwo::wgc_probe::EntryPoint;

    LoadResult loadModule(const wchar_t* name) override
    {
        HMODULE handle = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (handle) {
            return {reinterpret_cast<void*>(handle), false, 0};
        }
        const DWORD error = GetLastError();
        return {nullptr, error == ERROR_MOD_NOT_FOUND, static_cast<unsigned long>(error)};
    }

    EntryPoint resolve(void* handle, const char* name) override
    {
        return reinterpret_cast<EntryPoint>(
            GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
    }

    void unloadModule(void* handle) override
    {
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
    }

    bool wgcSupported() override
    {
        return oneintwo::windows::windowsGraphicsCaptureSupported();
    }
};

} // namespace

int main()
{
    WindowsRuntime runtime;
    return oneintwo::wgc_probe::run(runtime);
}
