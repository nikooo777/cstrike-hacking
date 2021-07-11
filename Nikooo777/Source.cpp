#include <iostream>
#include <Windows.h>
#include <thread>
#include "playerbase.h"

#define BUTTON_DOWN 0x8000
#define ENTGAP 0x10
#define MAXPLAYERS 64

struct gameOffsets {
    DWORD dwEntityList = 0x4D5AE4;
    DWORD dwForceJump = 0x4F5D24;
    DWORD dwForceAttack1 = 0x4F5D30;
    DWORD dwForceAttack2 = 0x4F5D3C;
    DWORD dwNumPlayers = 0x4F2150;
} offsets;

struct values {
    PlayerBase *localPlayer;
    DWORD clientModuleBase;
    DWORD serverModuleBase;
} shared;


PlayerBase *getPlayer(int index) {
    return *(PlayerBase **) (shared.clientModuleBase + offsets.dwEntityList + ENTGAP * index);
}

void handleBhop() {
    if (GetAsyncKeyState(VK_SPACE) & BUTTON_DOWN) {
        uintptr_t buffer = 4;
        if (shared.localPlayer->m_iFlags & FL_ONGROUND) {
            buffer = 5;
        }
        *(DWORD *) (shared.clientModuleBase + offsets.dwForceJump) = buffer;
    }
}

void attack1(bool active) {
    static uintptr_t buffer = 4;
    if (!active && buffer == 4) {
        return;
    }
    buffer = buffer == 4 && active ? 5 : 4;
    *(DWORD *) (shared.clientModuleBase + offsets.dwForceAttack1) = buffer;
}

void handleTriggerbot() {
    if (GetAsyncKeyState(VK_SHIFT) & BUTTON_DOWN) {
        auto aimedTarget = shared.localPlayer->m_iCrosshair;
        if (aimedTarget > 0 && aimedTarget < MAXPLAYERS) {
            auto target = getPlayer(aimedTarget - 1);
            if (target == nullptr) {
                std::cout << "target is null??!" << std::endl;
                return;
            }
            auto shouldShoot = (!target->m_bIsDormant && target->m_iTeamNum != shared.localPlayer->m_iTeamNum &&
                                target->m_iLifeState == ALIVE && shared.localPlayer->m_iFlags & FL_ONGROUND);
            if (shouldShoot) {
                attack1(true);
            }else{
                attack1(false);
            }
        }else{
            attack1(false);
        }
    } else {
        attack1(false);
    }
}

void printInfo(){
    std::cout << "clientModuleBase: 0x" << std::hex << shared.clientModuleBase << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << shared.serverModuleBase << std::endl;

    std::cout << "entityListOffset: 0x" << std::hex << offsets.dwEntityList << std::endl;
    std::cout << "forceJumpOffset: 0x" << std::hex << offsets.dwForceJump << std::endl;
    std::cout << "numPlayersOffset: 0x" << std::hex << offsets.dwNumPlayers << std::endl;
    std::cout << "forceAttack1Offset: 0x" << std::hex << offsets.dwForceAttack1 << std::endl;
    std::cout << "forceAttack2Offset: 0x" << std::hex << offsets.dwForceAttack2 << std::endl;

    std::cout << "localPlayer/EntityList: 0x" << std::hex << shared.localPlayer << std::endl;
    std::cout << "ingame players: " << std::dec << (int)*(PlayerBase **) (shared.serverModuleBase + offsets.dwNumPlayers) << std::endl;
}

DWORD __stdcall doAllTheThings111(void *pParam) {
    //init a console for debug output
    FILE *pFile = nullptr;
    AllocConsole();
    SetConsoleTitle("Nikooo777's H4X!337");
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    std::cout << "injected Nikooo777!" << std::endl;


    shared.clientModuleBase = reinterpret_cast<DWORD>(GetModuleHandleA("client.dll"));
    shared.serverModuleBase = reinterpret_cast<DWORD>(GetModuleHandleA("server.dll"));

    //get local player (which is also the first element of the entityList that we can then iterate on)
    while (shared.localPlayer == nullptr) {
        shared.localPlayer = *(PlayerBase **) (shared.clientModuleBase + offsets.dwEntityList);
    }
    printInfo();

    while (!GetAsyncKeyState(VK_END)) {
        handleBhop();
        handleTriggerbot();
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            printInfo();
            Sleep(10); //pressing insert will prevent the rest of the cheats from executing for 10ms...
        }
    }
    std::cout << "Exiting!" << std::endl;
    Sleep(2000);

    //release the debug console handles
    fclose(pFile);
    FreeConsole();

    //detach the DLL from the main process
    FreeLibraryAndExitThread(static_cast<HMODULE>(pParam), NULL);
}


BOOL WINAPI DllMain(
        HINSTANCE hModule,
        DWORD fdwReason,
        LPVOID lpReserved) {
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, doAllTheThings111, hModule, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
