//
// Created by Niko on 7/22/2021.
//

#include "Helper.h"

Helper *Helper::getInstance() {
    static Helper instance;

    return &instance;
}

Helper::Helper() = default;


ClientState *Helper::GetClientState() {
    if (this->clientState) {
        return this->clientState;
    }
    DWORD ApplicationPID = GetProcessId(GetCurrentProcess());
    DWORD engineModuleSize = GetModuleSize(ApplicationPID, (char *) "engine.dll");

    this->clientStateAddr = (DWORD) ScanModCombo((char *) "B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83", (char *) (this->GetModule("engine.dll")), (intptr_t) engineModuleSize) + 1;
    this->clientState = (ClientState *) (*(DWORD **) (this->clientStateAddr));
    return this->clientState;
}

DWORD Helper::GetClientStateAddress() {
    if (!this->clientState) {
        GetClientState();
    }
    return this->clientStateAddr;
}

CBasePlayer *Helper::GetLocalPlayer() {
    CBasePlayer *localPlayer = nullptr;

    while (localPlayer == nullptr) {
        localPlayer = *(CBasePlayer **) (this->GetModule("client.dll") + dwEntityList);
    }
    return localPlayer;
}

DWORD Helper::GetModule(const char *module) {
    auto iter = this->modules.find(module);
    if (iter != this->modules.end()) {
        return iter->second;
    }

    auto m = reinterpret_cast<DWORD>(GetModuleHandleA(module));
    this->modules.insert({module, m});
    return m;
}

CBasePlayer *Helper::GetPlayer(int index) {
    return *(CBasePlayer **) (this->GetModule("client.dll") + dwEntityList + ENTGAP * index);
}