#include "hooks/hooks.h"

#include <cstdio>
#include <iostream>
#include <string>

#include "MinHook.h"
#include "config/config.h"
#include "features/debug_info.h"
#include "features/config.h"
#include "game/interfaces.h"
#include "hooks/d3d9_device.h"
#include "netvars/netvars.h"
#include "sdk/client_mode.h"

namespace hooks {

CreateMoveFn originalCreateMove = nullptr;
FrameStageNotifyFn originalFrameStageNotify = nullptr;
EndSceneFn originalEndScene = nullptr;

namespace {

// IClientMode::CreateMove is vtable index 21 on this CS:S client build.
constexpr int kCreateMoveVtableIndex = 21;
// IBaseClientDLL::FrameStageNotify is vtable index 35.
constexpr int kFrameStageNotifyVtableIndex = 35;
// IDirect3DDevice9::EndScene is vtable index 42.
constexpr int kEndSceneVtableIndex = 42;

template <typename T>
T GetVFunc(void *instance, int index) {
    auto **vtable = *reinterpret_cast<void ***>(instance);
    return reinterpret_cast<T>(vtable[index]);
}

void *GetCreateMoveAddress(ClientMode *clientMode) {
    // ClientMode pointer → vtable → CreateMove slot.
    return reinterpret_cast<void *>(GetVFunc<CreateMoveFn>(clientMode, kCreateMoveVtableIndex));
}

void *GetFrameStageNotifyAddress(BaseClient *baseClient) {
    return reinterpret_cast<void *>(
        GetVFunc<FrameStageNotifyFn>(baseClient, kFrameStageNotifyVtableIndex));
}

} // namespace

DWORD __stdcall MainThread(void *pModule) {
    FILE *pFile = nullptr;
    AllocConsole();
    SetConsoleTitleA("Nikooo777's H4X!337");
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    std::cout << "injected Nikooo777!" << std::endl;

    std::string configError;
    if (!config::Load(reinterpret_cast<HMODULE>(pModule), configError)) {
        std::cout << "Failed to load signatures.ini: " << configError << std::endl;
        return 1;
    }
    features::ApplyConfig(config::Get());
    std::cout << "Loaded config: " << config::Get().path << std::endl;

    auto *baseClient = game::GetBaseClient();
    if (baseClient == nullptr) {
        std::cout << "BaseClient is null!" << std::endl;
        return 1;
    }
    std::cout << "BaseClient: 0x" << std::hex << baseClient << std::endl;

    std::string netvarError;
    if (!netvars::Initialize(baseClient->GetAllClasses(), netvarError)) {
        std::cout << "Failed to initialize netvars: " << netvarError << std::endl;
        return 1;
    }
    std::cout << "Netvars initialized" << std::endl;

    auto *clientEntityList = game::GetClientEntityList();
    if (clientEntityList == nullptr) {
        std::cout << "ClientEntityList is null!" << std::endl;
        return 1;
    }

    void *frameStageNotifyAddress = GetFrameStageNotifyAddress(baseClient);
    std::cout << "FrameStageNotify: 0x" << std::hex << frameStageNotifyAddress << std::endl;

    auto *clientMode = game::GetClientMode();
    if (!clientMode) {
        std::cout << "ClientMode is null!" << std::endl;
        return 1;
    }
    void *createMoveAddress = GetCreateMoveAddress(clientMode);
    std::cout << "CreateMove: 0x" << std::hex << createMoveAddress << std::endl;

    if (MH_Initialize() != MH_OK) {
        return 1;
    }

    void *d3d9Device[119] = {};
    if (!GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
        std::cout << "Failed to capture D3D9 device vtable" << std::endl;
        MH_Uninitialize();
        return 1;
    }
    void *endSceneAddress = d3d9Device[kEndSceneVtableIndex];
    std::cout << "EndScene: 0x" << std::hex << endSceneAddress << std::endl;

    if (MH_CreateHook(createMoveAddress, (LPVOID)&hkCreateMove,
                      reinterpret_cast<LPVOID *>(&originalCreateMove)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(frameStageNotifyAddress, (LPVOID)&hkFrameStageNotify,
                      reinterpret_cast<LPVOID *>(&originalFrameStageNotify)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(endSceneAddress, (LPVOID)&hkEndScene,
                      reinterpret_cast<LPVOID *>(&originalEndScene)) != MH_OK) {
        return 1;
    }

    if (MH_EnableHook(createMoveAddress) != MH_OK ||
        MH_EnableHook(frameStageNotifyAddress) != MH_OK ||
        MH_EnableHook(endSceneAddress) != MH_OK) {
        return 1;
    }

    features::PrintDebugInfo();
    std::cout << "Hooks enabled. INSERT=menu, F1=debug, END=unload" << std::endl;

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(10);
    }

    std::cout << "Exiting!" << std::endl;

    MH_DisableHook(createMoveAddress);
    MH_DisableHook(frameStageNotifyAddress);
    MH_DisableHook(endSceneAddress);
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
