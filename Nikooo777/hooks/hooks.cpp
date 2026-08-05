#include "hooks/hooks.h"

#include <cstdio>
#include <iostream>

#include "MinHook.h"
#include "features/debug_info.h"
#include "game/interfaces.h"
#include "hooks/d3d9_device.h"

namespace hooks {

CreateMoveFn originalCreateMove = nullptr;
FrameStageNotifyFn originalFrameStageNotify = nullptr;
EndSceneFn originalEndScene = nullptr;

DWORD __stdcall MainThread(void *pModule) {
    FILE *pFile = nullptr;
    AllocConsole();
    SetConsoleTitleA("Nikooo777's H4X!337");
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    std::cout << "injected Nikooo777!" << std::endl;

    auto *baseClient = game::GetBaseClient();
    if (baseClient == nullptr) {
        std::cout << "BaseClient is null!" << std::endl;
        return 1;
    }
    std::cout << "BaseClient: 0x" << std::hex << baseClient << std::endl;

    void ***vtable = (void ***)baseClient;
    void *frameStageNotifyAddress = (*vtable)[35];
    std::cout << "FrameStageNotify: 0x" << std::hex << frameStageNotifyAddress << std::endl;

    auto clientMode = game::GetClientMode();
    auto *addrOfCreateMove = (DWORD *)(((*(DWORD **)(*(DWORD ***)clientMode))[21]));

    if (MH_Initialize() != MH_OK) {
        return 1;
    }

    void *d3d9Device[119] = {};
    if (GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
        std::cout << "d3d9Device vtable captured" << std::endl;
    } else {
        std::cout << "d3d9Device is null!" << std::endl;
        return 1;
    }

    if (MH_CreateHook((LPVOID)addrOfCreateMove, (LPVOID)&hkCreateMove,
                      reinterpret_cast<LPVOID *>(&originalCreateMove)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(frameStageNotifyAddress, (LPVOID)&hkFrameStageNotify,
                      reinterpret_cast<LPVOID *>(&originalFrameStageNotify)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook((LPVOID)d3d9Device[42], (LPVOID)&hkEndScene,
                      reinterpret_cast<LPVOID *>(&originalEndScene)) != MH_OK) {
        return 1;
    }

    if (MH_EnableHook((LPVOID)addrOfCreateMove) != MH_OK) {
        return 1;
    }
    if (MH_EnableHook(frameStageNotifyAddress) != MH_OK) {
        return 1;
    }
    if (MH_EnableHook((LPVOID)d3d9Device[42]) != MH_OK) {
        return 1;
    }

    features::PrintDebugInfo();
    std::cout << "Hooks enabled. INSERT=menu, F1=debug, END=unload" << std::endl;

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(10);
    }

    std::cout << "Exiting!" << std::endl;

    MH_DisableHook((LPVOID)addrOfCreateMove);
    MH_DisableHook(frameStageNotifyAddress);
    MH_DisableHook((LPVOID)d3d9Device[42]);
    MH_Uninitialize();

    ShutdownEndScene();

    Sleep(200);
    if (pFile) {
        fclose(pFile);
    }
    FreeConsole();
    FreeLibraryAndExitThread(static_cast<HMODULE>(pModule), 0);
    return 0;
}

} // namespace hooks
