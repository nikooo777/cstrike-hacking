/*
 * Written by Nikooo777
 */

#include "common.h"
#include "hack.h"

BOOL WINAPI DllMain(
        HINSTANCE hModule,
        DWORD fdwReason,
        LPVOID lpReserved) {
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, mainLoop, hModule, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
