#include "features/debug_info.h"

#include <iostream>

#include "core/modules.h"
#include "core/offsets.h"
#include "game/entity_list.h"
#include "game/interfaces.h"
#include "netvars/netvars.h"

namespace features {

namespace {

void PrintNetvar(const char *label, const char *table, const char *property) {
    const int offset = netvars::GetOffset(table, property);
    std::cout << label << ": ";
    if (offset >= 0) {
        std::cout << "0x" << std::hex << offset << std::dec;
    } else {
        std::cout << "missing";
    }
    std::cout << " (" << table << "->" << property << ")" << std::endl;
}

} // namespace

void PrintDebugInfo() {
    std::cout << "clientModuleBase: 0x" << std::hex << core::GetModule("client.dll") << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << core::GetModule("server.dll") << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << core::GetModule("engine.dll") << std::endl;

    std::cout << "global/client-only offsets:" << std::endl;
    std::cout << "  entityList: 0x" << std::hex << dwEntityList << std::endl;
    std::cout << "  forceJump: 0x" << std::hex << dwForceJump << std::endl;
    std::cout << "  numPlayers: 0x" << std::hex << dwNumPlayers << std::endl;
    std::cout << "  forceAttack1: 0x" << std::hex << dwForceAttack1 << std::endl;
    std::cout << "  forceAttack2: 0x" << std::hex << dwForceAttack2 << std::dec << std::endl;
    PrintNetvar("entity m_lifeState", "DT_BasePlayer", "m_lifeState");
    PrintNetvar("entity m_iHealth", "DT_BasePlayer", "m_iHealth");
    PrintNetvar("entity m_iTeamNum", "DT_BaseEntity", "m_iTeamNum");
    PrintNetvar("entity m_vecOrigin", "DT_BaseEntity", "m_vecOrigin");
    PrintNetvar("entity m_vecViewOffset", "DT_LocalPlayerExclusive", "m_vecViewOffset");
    PrintNetvar("entity m_vecVelocity", "DT_LocalPlayerExclusive", "m_vecVelocity");
    PrintNetvar("entity m_vecBaseVelocity", "DT_LocalPlayerExclusive", "m_vecBaseVelocity");
    PrintNetvar("entity m_angRotation", "DT_BaseEntity", "m_angRotation");
    PrintNetvar("entity m_fFlags", "DT_BasePlayer", "m_fFlags");
    PrintNetvar("entity m_Local", "DT_LocalPlayerExclusive", "m_Local");
    PrintNetvar("local m_vecPunchAngle", "DT_Local", "m_vecPunchAngle");
    PrintNetvar("local m_vecPunchAngleVel", "DT_Local", "m_vecPunchAngleVel");
    PrintNetvar("player m_iShotsFired", "DT_CSLocalPlayerExclusive", "m_iShotsFired");
    std::cout << "client-only m_iCrosshairID: 0x" << std::hex << 0x14F0 << std::dec << std::endl;
    const auto clientStateAddress = game::GetClientStateAddress();
    const auto engineBase = core::GetModule("engine.dll");
    std::cout << "clientState addr: 0x" << std::hex << clientStateAddress << std::endl;
    if (clientStateAddress != 0 && engineBase != 0 && clientStateAddress >= engineBase) {
        std::cout << "clientState offset: engine.dll + 0x" << std::hex
                  << clientStateAddress - engineBase << std::endl;
    } else {
        std::cout << "clientState offset: unavailable" << std::endl;
    }
    std::cout << "ViewAngles: clientState + 0x4b84" << std::endl;

    if (auto *local = game::GetLocalPlayer()) {
        std::cout << "my position:" << local->m_vecOrigin() << std::endl;
    } else {
        std::cout << "local player: null (not in game?)" << std::endl;
    }
}

} // namespace features
