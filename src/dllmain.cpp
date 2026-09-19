#include <Windows.h>

#include "NFSMW/Pursuit.hpp"

PursuitCheats g_Cheats;

namespace {

    DWORD WINAPI Startup(LPVOID) {
        g_Cheats.LoadSettings();
        g_Cheats.ApplyPatches();
        return 0;
    }

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (const HANDLE thread = CreateThread(nullptr, 0, Startup, nullptr, 0, nullptr)) CloseHandle(thread);
    }
    return TRUE;
}
