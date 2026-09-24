#include "hooks/hooks.h"

#include <cstdio>
#include <iostream>
#include <string>

#include "core/arch.h"
#include "MinHook.h"
#include "config/config.h"
#include "features/debug_info.h"
#include "features/config.h"
#include "features/telemetry.h"
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
ResetFn originalReset = nullptr;
#if ARCH_X64()
ClientFireBulletsFn originalClientFireBullets = nullptr;
UpdateAccuracyPenaltyFn originalUpdateAccuracyPenalty = nullptr;
#endif

namespace {

// IClientMode::CreateMove is vtable index 21 on this CS:S client build.
constexpr std::size_t kCreateMoveVtableIndex = 21;
constexpr std::size_t kOverrideViewVtableIndex = 16;

void *GetCreateMoveAddress(ClientMode *clientMode) {
    return mem::GetVirtual<void *>(clientMode, kCreateMoveVtableIndex);
}

void *GetOverrideViewAddress(ClientMode *clientMode) {
    return mem::GetVirtual<void *>(clientMode, kOverrideViewVtableIndex);
}

void *GetLockCursorAddress(void *surface) {
    return mem::GetVirtual<void *>(surface, sdk::vgui::kSurfaceLockCursor);
}

#if ARCH_X64()
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
    void *createMoveAddress = GetCreateMoveAddress(clientMode);
    if (overrideViewAddress == nullptr || createMoveAddress == nullptr) {
        std::cout << "ClientMode OverrideView/CreateMove slots are not executable"
                  << std::endl;
        return 1;
    }
    std::cout << "OverrideView: 0x" << std::hex << overrideViewAddress
              << std::endl;
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

#if ARCH_X64()
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
    void *resetAddress = d3d9Device[kResetVtableIndex];
    std::cout << "EndScene: 0x" << std::hex << endSceneAddress
              << " Reset: 0x" << resetAddress << std::dec << std::endl;
    const ExFactorySlots exFactory = GetExFactorySlots();
    if (exFactory.reset == nullptr) {
        std::cout << "D3D9Ex factory device unavailable" << std::endl;
    } else if (exFactory.reset != resetAddress ||
               exFactory.endScene != endSceneAddress) {
        std::cout << "D3D9Ex factory device differs: EndScene: 0x" << std::hex
                  << exFactory.endScene << " Reset: 0x" << exFactory.reset
                  << std::dec << "; only the D3D9 entries are hooked"
                  << std::endl;
    } else {
        std::cout << "D3D9Ex factory device shares EndScene and Reset"
                  << std::endl;
    }

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
                      reinterpret_cast<LPVOID *>(&originalEndScene)) != MH_OK ||
        MH_CreateHook(resetAddress, (LPVOID)&hkReset,
                      reinterpret_cast<LPVOID *>(&originalReset)) != MH_OK) {
        return 1;
    }
#if ARCH_X64()
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
        MH_EnableHook(endSceneAddress) != MH_OK ||
        MH_EnableHook(resetAddress) != MH_OK) {
        return 1;
    }
#if ARCH_X64()
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

    features::telemetry::Startup startup;
    startup.renderView = game::GetRenderView() != nullptr;
    startup.modelInfo = game::GetModelInfo() != nullptr;
    startup.engineTrace = game::GetEngineTrace() != nullptr;
#if ARCH_X64()
    startup.fireCapture = clientFireBulletsAddress != nullptr;
    startup.accuracyUpdate = updateAccuracyPenaltyAddress != nullptr;
#endif
    features::telemetry::RecordStartup(startup);

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
    MH_DisableHook(resetAddress);
#if ARCH_X64()
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
