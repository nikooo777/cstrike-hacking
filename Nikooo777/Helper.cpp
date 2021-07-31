//
// Created by Niko on 7/22/2021.
//

#include "common.h"
#include "Helper.h"

Helper *Helper::getInstance() {
    static Helper instance;

    return &instance;
}

Helper::Helper() = default;


ClientState *Helper::GetClientState() {
    if (clientState) {
        return clientState;
    }
    DWORD ApplicationPID = GetProcessId(GetCurrentProcess());
    DWORD engineModuleSize = GetModuleSize(ApplicationPID, (char *) "engine.dll");

    clientStateAddr = (DWORD) ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83", (char *) (GetModule("engine.dll")), (intptr_t) engineModuleSize) + 1;
    clientState = (ClientState *) (*(DWORD **) (clientStateAddr));
    return clientState;
}

DWORD Helper::GetClientStateAddress() {
    if (!clientState) {
        GetClientState();
    }
    return this->clientStateAddr;
}

CBasePlayer *Helper::GetLocalPlayer() {
    CBasePlayer *localPlayer = nullptr;

    while (localPlayer == nullptr) {
        localPlayer = *(CBasePlayer **) (GetModule("client.dll") + dwEntityList);
    }
    return localPlayer;
}

DWORD Helper::GetModule(const std::string &module) {
    auto iter = modules.find(module);
    if (iter != modules.end()) {
        return iter->second;
    }

    auto m = reinterpret_cast<DWORD>(GetModuleHandleA(module.c_str()));
    this->modules.insert({module, m});
    return m;
}

CBasePlayer *Helper::GetPlayer(int index) {
    return *(CBasePlayer **) (GetModule("client.dll") + dwEntityList + ENTGAP * index);
}

int Helper::GetPlayerCount() {
    return (int) *(DWORD **) (GetModule("server.dll") + dwNumPlayers);
}

int Helper::GetMaxPlayerCount() {
    return (int) *(DWORD **) (GetModule("server.dll") + dwMaxPlayers);
}