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
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/client_mode.h"
#include "sdk/vgui_surface.h"

namespace hooks {

CreateMoveFn originalCreateMove = nullptr;
OverrideViewFn originalOverrideView = nullptr;
LockCursorFn originalLockCursor = nullptr;
EndSceneFn originalEndScene = nullptr;
#if !defined(_M_IX86) && !defined(__i386__)
ClientFireBulletsFn originalClientFireBullets = nullptr;
UpdateAccuracyPenaltyFn originalUpdateAccuracyPenalty = nullptr;
#endif

namespace {

// IClientMode::CreateMove is vtable index 21 on this CS:S client build.
constexpr int kCreateMoveVtableIndex = 21;
constexpr int kOverrideViewVtableIndex = 16;
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

void *GetOverrideViewAddress(ClientMode *clientMode) {
    return reinterpret_cast<void *>(
        GetVFunc<OverrideViewFn>(clientMode, kOverrideViewVtableIndex));
}

void *GetLockCursorAddress(void *surface) {
    auto *vtable = mem::ReadPointer<void>(surface);
    if (vtable == nullptr) {
        return nullptr;
    }

    void *address = nullptr;
    const auto slotAddress =
        reinterpret_cast<std::uintptr_t>(vtable) +
        sdk::vgui::kSurfaceLockCursor * sizeof(void *);
    if (!mem::ReadValue(reinterpret_cast<const void *>(slotAddress), address) ||
        address == nullptr || !mem::IsExecutable(address)) {
        return nullptr;
    }
    return address;
}

#if !defined(_M_IX86) && !defined(__i386__)
void *GetClientFireBulletsAddress() {
    const auto &signature = config::Get().clientFireBullets;
    std::size_t matchCount = 0;
    auto *match = game::FindConfiguredSignature(signature, matchCount);
    if (match == nullptr) {
        return nullptr;
    }

    std::uintptr_t address = 0;
    if (!mem::DecodeRipRelative32(
            match, signature.operandOffset, signature.instructionOffset,
            signature.instructionLength, address) ||
        address == 0 ||
        !mem::IsExecutable(reinterpret_cast<const void *>(address))) {
        std::cout << "ClientFireBullets target is not executable" << std::endl;
        return nullptr;
    }

    std::cout << "ClientFireBullets: 0x" << std::hex << address << std::dec
              << std::endl;
    return reinterpret_cast<void *>(address);
}

void *GetUpdateAccuracyPenaltyAddress() {
    const auto &signature = config::Get().updateAccuracyPenalty;
    std::size_t matchCount = 0;
    auto *match = game::FindConfiguredSignature(signature, matchCount);
    if (match == nullptr || signature.operand != "match" ||
        !mem::IsExecutable(match)) {
        std::cout << "UpdateAccuracyPenalty target is not executable"
                  << std::endl;
        return nullptr;
    }
    return match;
}
#endif

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

    auto *clientMode = game::GetClientMode();
    if (!clientMode) {
        std::cout << "ClientMode is null!" << std::endl;
        return 1;
    }
    void *overrideViewAddress = GetOverrideViewAddress(clientMode);
    std::cout << "OverrideView: 0x" << std::hex << overrideViewAddress
              << std::endl;
    void *createMoveAddress = GetCreateMoveAddress(clientMode);
    std::cout << "CreateMove: 0x" << std::hex << createMoveAddress << std::endl;

    void *vguiSurface = game::GetVguiSurface();
    if (vguiSurface == nullptr) {
        std::cout << "VGUI_Surface030 is null!" << std::endl;
        return 1;
    }
    void *lockCursorAddress = GetLockCursorAddress(vguiSurface);
    if (lockCursorAddress == nullptr) {
        std::cout << "VGUI_Surface030 LockCursor is not executable"
                  << std::endl;
        return 1;
    }
    std::cout << "LockCursor: 0x" << std::hex << lockCursorAddress
              << std::endl;

#if !defined(_M_IX86) && !defined(__i386__)
    void *clientFireBulletsAddress = GetClientFireBulletsAddress();
    void *updateAccuracyPenaltyAddress =
        GetUpdateAccuracyPenaltyAddress();
#endif

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
    if (MH_CreateHook(overrideViewAddress, (LPVOID)&hkOverrideView,
                      reinterpret_cast<LPVOID *>(&originalOverrideView)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(lockCursorAddress, (LPVOID)&hkLockCursor,
                      reinterpret_cast<LPVOID *>(&originalLockCursor)) != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(endSceneAddress, (LPVOID)&hkEndScene,
                      reinterpret_cast<LPVOID *>(&originalEndScene)) != MH_OK) {
        return 1;
    }
#if !defined(_M_IX86) && !defined(__i386__)
    if (clientFireBulletsAddress != nullptr &&
        MH_CreateHook(clientFireBulletsAddress, (LPVOID)&hkClientFireBullets,
                      reinterpret_cast<LPVOID *>(
                          &originalClientFireBullets)) != MH_OK) {
        std::cout << "ClientFireBullets diagnostic hook failed" << std::endl;
        clientFireBulletsAddress = nullptr;
    }
    if (updateAccuracyPenaltyAddress != nullptr &&
        MH_CreateHook(updateAccuracyPenaltyAddress,
                      (LPVOID)&hkUpdateAccuracyPenalty,
                      reinterpret_cast<LPVOID *>(
                          &originalUpdateAccuracyPenalty)) != MH_OK) {
        std::cout << "UpdateAccuracyPenalty diagnostic hook failed"
                  << std::endl;
        updateAccuracyPenaltyAddress = nullptr;
    }
#endif

    if (MH_EnableHook(createMoveAddress) != MH_OK ||
        MH_EnableHook(overrideViewAddress) != MH_OK ||
        MH_EnableHook(lockCursorAddress) != MH_OK ||
        MH_EnableHook(endSceneAddress) != MH_OK) {
        return 1;
    }
#if !defined(_M_IX86) && !defined(__i386__)
    if (clientFireBulletsAddress != nullptr &&
        MH_EnableHook(clientFireBulletsAddress) != MH_OK) {
        std::cout << "ClientFireBullets diagnostic hook could not be enabled"
                  << std::endl;
        clientFireBulletsAddress = nullptr;
    }
    if (updateAccuracyPenaltyAddress != nullptr &&
        MH_EnableHook(updateAccuracyPenaltyAddress) != MH_OK) {
        std::cout << "UpdateAccuracyPenalty diagnostic hook could not be enabled"
                  << std::endl;
        updateAccuracyPenaltyAddress = nullptr;
    }
#endif

    features::PrintDebugInfo();
    std::cout << "Hooks enabled. INSERT=menu, F1=debug, END=unload" << std::endl;

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(10);
    }

    std::cout << "Exiting!" << std::endl;

    MH_DisableHook(createMoveAddress);
    MH_DisableHook(overrideViewAddress);
    MH_DisableHook(lockCursorAddress);
    MH_DisableHook(endSceneAddress);
#if !defined(_M_IX86) && !defined(__i386__)
    if (clientFireBulletsAddress != nullptr) {
        MH_DisableHook(clientFireBulletsAddress);
    }
    if (updateAccuracyPenaltyAddress != nullptr) {
        MH_DisableHook(updateAccuracyPenaltyAddress);
    }
#endif
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
