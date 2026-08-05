#include "game/entity_list.h"

#include "core/modules.h"
#include "core/offsets.h"
#include "core/constants.h"

namespace game {

CCSPlayer *GetLocalPlayer() {
    // Entity list slot 0 is the local player. May be null before fully in-game — callers must check.
    return *reinterpret_cast<CCSPlayer **>(core::GetModule("client.dll") + dwEntityList);
}

CCSPlayer *GetPlayer(int index) {
    if (index < 0) {
        return nullptr;
    }
    return *reinterpret_cast<CCSPlayer **>(core::GetModule("client.dll") + dwEntityList + ENTGAP * index);
}

int GetPlayerCount() {
    return *reinterpret_cast<int *>(core::GetModule("server.dll") + dwNumPlayers);
}

int GetMaxPlayerCount() {
    return *reinterpret_cast<int *>(core::GetModule("server.dll") + dwMaxPlayers);
}

} // namespace game
