// Resolve delayed imports before the first SDL/DXC call, independently of cwd.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <delayimp.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {
[[noreturn]] void missing_library(const char* name) {
    std::fprintf(stderr, "Tetrisphere: cannot load lib/%s (Windows error %lu).\n",
                 name, GetLastError());
    MessageBoxW(nullptr,
        L"A required library could not be loaded from the game's lib folder. "
        L"Extract the complete package and keep lib beside the executables.",
        L"Tetrisphere library error", MB_OK | MB_ICONERROR);
    ExitProcess(ERROR_MOD_NOT_FOUND);
}

FARPROC WINAPI load_package_library(unsigned event, PDelayLoadInfo info) {
    if (event != dliNotePreLoadLibrary ||
        (_stricmp(info->szDll, "SDL2.dll") != 0 &&
         _stricmp(info->szDll, "dxcompiler.dll") != 0)) return nullptr;
    wchar_t executable[32768];
    const DWORD count = GetModuleFileNameW(nullptr, executable, 32768);
    if (count == 0 || count >= 32768) missing_library(info->szDll);
    const auto path = std::filesystem::path(executable).parent_path() /
                      L"lib" / info->szDll;
    const HMODULE module = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (module == nullptr) missing_library(info->szDll);
    return reinterpret_cast<FARPROC>(module);
}
} // namespace

extern "C" const PfnDliHook __pfnDliNotifyHook2 = load_package_library;
