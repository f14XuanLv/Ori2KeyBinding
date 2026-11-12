#include "runtime.h"
#include <array>

namespace {
std::atomic<HMODULE> original_version{};
constexpr std::array<const char*, 17> exports{
    "GetFileVersionInfoA", "GetFileVersionInfoByHandle", "GetFileVersionInfoExA", "GetFileVersionInfoExW",
    "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA", "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",
    "GetFileVersionInfoW", "VerFindFileA", "VerFindFileW", "VerInstallFileA", "VerInstallFileW",
    "VerLanguageNameA", "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW"};
}
extern "C" FARPROC resolve_version(size_t index) {
    // No C++ local-static guard here: a second thread may call us while holding the loader lock.
    // Waiting for another thread's guard in that situation can deadlock LoadLibrary on the first thread.
    auto original = original_version.load();
    if (!original) {
        // Search flags alone do not override Windows' already-loaded basename cache.
        // An absolute path is necessary to avoid resolving this proxy back to itself.
        wchar_t system[MAX_PATH]{};
        GetSystemDirectoryW(system, MAX_PATH);
        wcscat_s(system, L"\\version.dll");
        original = LoadLibraryExW(system, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (original) original_version.store(original);
    }
    const auto fn = original && index < exports.size() ? GetProcAddress(original, exports[index]) : nullptr;
    if (!fn) RaiseFailFastException(nullptr, nullptr, 0);
    return fn;
}
extern "C" __declspec(dllexport) unsigned long long OriPad_GetStatus() {
    return (ori::overlay_ready ? 1ull : 0ull) | (ori::panel_open ? 2ull : 0ull) |
        (ori::input_calls.load() << 2);
}
extern "C" __declspec(dllexport) void OriPad_SetPanelVisible(BOOL visible) { ori::panel_open = visible != FALSE; }
extern "C" __declspec(dllexport) BOOL OriPad_ReloadConfig() { return ori::reload_config(); }
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH);
        const auto* name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
        if (_wcsicmp(name, L"oriwotw.exe") == 0 || _wcsicmp(name, L"OriPadHarness.exe") == 0) ori::start(self);
    }
    return TRUE;
}
