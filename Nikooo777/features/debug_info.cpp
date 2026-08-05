#include "features/debug_info.h"

#include <iostream>

#include "core/modules.h"
#include "core/offsets.h"
#include "game/entity_list.h"
#include "game/interfaces.h"

namespace features {

void PrintDebugInfo() {
    std::cout << "clientModuleBase: 0x" << std::hex << core::GetModule("client.dll") << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << core::GetModule("server.dll") << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << core::GetModule("engine.dll") << std::endl;

    std::cout << "entityListOffset: 0x" << std::hex << dwEntityList << std::endl;
    std::cout << "forceJumpOffset: 0x" << std::hex << dwForceJump << std::endl;
    std::cout << "numPlayersOffset: 0x" << std::hex << dwNumPlayers << std::endl;
    std::cout << "forceAttack1Offset: 0x" << std::hex << dwForceAttack1 << std::endl;
    std::cout << "forceAttack2Offset: 0x" << std::hex << dwForceAttack2 << std::endl;
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
