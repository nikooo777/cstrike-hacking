/*
 * Written by Nikooo777
 */
#include <iostream>
#include <Windows.h>
#include <thread>
#include "CBasePlayer.h"
#include "ClientState.h"
#include "CCSPlayer.h"
#include "mem/mem.h"
#include "misc.h"


struct gameOffsets {
    DWORD dwEntityList = 0x4D5AE4;
    DWORD dwForceJump = 0x4F5D24;
    DWORD dwForceAttack1 = 0x4F5D30;
    DWORD dwForceAttack2 = 0x4F5D3C;
    DWORD dwNumPlayers = 0x4F2150;
} offsets;

struct values {
    CBasePlayer *localPlayer;
    ClientState *clientState;
    DWORD clientModuleBase;
    DWORD serverModuleBase;
    DWORD engineModuleBase;
    DWORD clientStateAddr;
    Vector3 oldPunch;
} shared;


CBasePlayer *getPlayer(int index) {
    return *(CBasePlayer **) (shared.clientModuleBase + offsets.dwEntityList + ENTGAP * index);
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
        auto aimedTarget = ((CCSPlayer *) shared.localPlayer)->m_iCrosshair;
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
            } else {
                attack1(false);
            }
        } else {
            attack1(false);
        }
    } else {
        attack1(false);
    }
}

float clamp(float x, float upper, float lower) {
    return min(upper, max(x, lower));
}

void ClampAngles(Vector3 &angle) {
    if (angle.x > 89.0f) { angle.x = 89.0f; }
    if (angle.x < -89.0f) { angle.x = -89.0f; }
}

void NormalizeAngles(Vector3 &angle) {
    while (angle.y > 180) {
        angle.y -= 360;
    }
    while (angle.y < -180) {
        angle.y += 360;
    }

    angle.y = std::remainderf(angle.y, 360.f);
}

void handleRecoil() {
    auto shotsFired = ((CCSPlayer *) shared.localPlayer)->m_iShotsFired;
    auto punchAngle = shared.localPlayer->m_Local.m_vecPunchAngle;
    auto viewAngles = &shared.clientState->m_vViewAngles;

    Vector3 tempAngle = {0, 0, 0};
    if (shotsFired > 0) {
        tempAngle.x = (viewAngles->x + shared.oldPunch.x) - (punchAngle.x * 2);
        tempAngle.y = (viewAngles->y + shared.oldPunch.y) - (punchAngle.y * 2);
        NormalizeAngles(tempAngle);
        ClampAngles(tempAngle);
        shared.oldPunch.x = punchAngle.x * 2;
        shared.oldPunch.y = punchAngle.y * 2;
        *viewAngles = tempAngle;
    } else {
        shared.oldPunch = {0, 0, 0}; // reset old punch
    }
}

void printInfo() {
    std::cout << "clientModuleBase: 0x" << std::hex << shared.clientModuleBase << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << shared.serverModuleBase << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << shared.engineModuleBase << std::endl;

    std::cout << "entityListOffset: 0x" << std::hex << offsets.dwEntityList << std::endl;
    std::cout << "forceJumpOffset: 0x" << std::hex << offsets.dwForceJump << std::endl;
    std::cout << "numPlayersOffset: 0x" << std::hex << offsets.dwNumPlayers << std::endl;
    std::cout << "forceAttack1Offset: 0x" << std::hex << offsets.dwForceAttack1 << std::endl;
    std::cout << "forceAttack2Offset: 0x" << std::hex << offsets.dwForceAttack2 << std::endl;
    std::cout << "clientState addr: 0x" << std::hex << shared.clientStateAddr << std::endl;
    std::cout << "clientState offset: engine.dll + 0x" << std::hex << shared.clientStateAddr - shared.engineModuleBase << std::endl;
    std::cout << "ViewAngles: clientState + 0x4b84" << std::endl;

    std::cout << "localPlayer/EntityList: 0x" << std::hex << shared.localPlayer << std::endl;
    std::cout << "ingame players: " << std::dec << (int) *(DWORD **) (shared.serverModuleBase + offsets.dwNumPlayers) << std::endl;

    // This is me testing out a few signatures to clientState. More here: https://www.youtube.com/watch?v=J6vO-ANi4Q8
//    auto addr = ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 83 78 1C 00", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize);
//    auto addr2 = ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? 5D C2 08 00", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize);
//    auto addr3 = ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? 83 3D ? ? ? ? ? 75 4A", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize);
//    auto clientStateAddr = ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize) + 1;
//    auto addr5 = ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? 8D 85 ? ? ? ? 50 68 ? ? ? ? FF", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize);
//    std::cout << "clientState: 0x" << std::hex << *(DWORD **) (addr + 1) << std::endl;
//    std::cout << "clientState2: 0x" << std::hex << *(DWORD **) (addr2 + 1) << std::endl; // this one doesn't return a valid address!
//    std::cout << "clientState3: 0x" << std::hex << *(DWORD **) (addr3 + 1) << std::endl;
//    std::cout << "clientState4: 0x" << std::hex << *(DWORD **) (clientStateAddr + 1) << std::endl;
//    std::cout << "clientState5: 0x" << std::hex << *(DWORD **) (addr5 + 1) << std::endl;
//    auto *client = (ClientState *) (*(DWORD **) (addr5 + 1));
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
    shared.engineModuleBase = reinterpret_cast<DWORD>(GetModuleHandleA("engine.dll"));

    DWORD ApplicationPID = GetProcessId(GetCurrentProcess());
    DWORD engineModuleSize = GetModuleSize(ApplicationPID, (char *) "engine.dll");
    shared.clientStateAddr = (DWORD) ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83", (char *) shared.engineModuleBase, (intptr_t) engineModuleSize) + 1;
    shared.clientState = (ClientState *) (*(DWORD **) (shared.clientStateAddr));
    //get local player (which is also the first element of the entityList that we can then iterate on)
    while (shared.localPlayer == nullptr) {
        shared.localPlayer = *(CBasePlayer **) (shared.clientModuleBase + offsets.dwEntityList);
    }
    printInfo();
    shared.oldPunch = {0, 0, 0};
    while (!GetAsyncKeyState(VK_END)) {
        shared.localPlayer = *(CBasePlayer **) (shared.clientModuleBase + offsets.dwEntityList);
        handleBhop();
        handleTriggerbot();
        handleRecoil();
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
