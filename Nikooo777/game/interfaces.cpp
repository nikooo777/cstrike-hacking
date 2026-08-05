#include "game/interfaces.h"

#include <iostream>

#include "core/modules.h"
#include "memory/mem.h"
#include "sdk/create_interface.h"

namespace game {

namespace {

ClientState *g_clientState = nullptr;
ClientMode *g_clientMode = nullptr;
BaseClient *g_baseClient = nullptr;
std::uintptr_t g_clientStateAddr = 0;

} // namespace

ClientState *GetClientState() {
    if (g_clientState) {
        return g_clientState;
    }

    DWORD pid = GetProcessId(GetCurrentProcess());
    DWORD engineSize = mem::GetModuleSize(pid, (char *)"engine.dll");
    auto engineBase = core::GetModule("engine.dll");

    auto scan = mem::ScanModCombo((char *)"B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83",
                                  (char *)engineBase, (intptr_t)engineSize);
    g_clientStateAddr = reinterpret_cast<std::uintptr_t>(scan) + 1;
    g_clientState = *reinterpret_cast<ClientState **>(g_clientStateAddr);
    return g_clientState;
}

ClientMode *GetClientMode() {
    if (g_clientMode) {
        return g_clientMode;
    }

    DWORD pid = GetProcessId(GetCurrentProcess());
    DWORD clientSize = mem::GetModuleSize(pid, (char *)"client.dll");
    auto clientBase = core::GetModule("client.dll");

    auto scan = mem::ScanModCombo((char *)"8B 0D ? ? ? ? 8B 01 5D FF 60 28 CC", (char *)clientBase,
                                  (intptr_t)clientSize);
    auto clientModeAddr = reinterpret_cast<std::uintptr_t>(scan) + 2;
    std::cout << "clientMode addr: 0x" << std::hex << **reinterpret_cast<DWORD **>(clientModeAddr)
              << std::endl;
    g_clientMode = *reinterpret_cast<ClientMode **>(clientModeAddr);
    std::cout << "clientMode2 addr: 0x" << std::hex << *reinterpret_cast<uintptr_t *>(g_clientMode)
              << std::endl;
    return g_clientMode;
}

BaseClient *GetBaseClient() {
    if (g_baseClient) {
        return g_baseClient;
    }
    g_baseClient = (BaseClient *)GetInterface("client.dll", "VClient017");
    return g_baseClient;
}

std::uintptr_t GetClientStateAddress() {
    if (!g_clientState) {
        GetClientState();
    }
    return g_clientStateAddr;
}

} // namespace game
