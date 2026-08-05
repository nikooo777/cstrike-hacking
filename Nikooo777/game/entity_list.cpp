#include "game/entity_list.h"

#include "core/modules.h"
#include "core/offsets.h"
#include "core/constants.h"

namespace game {

CCSPlayer *GetLocalPlayer() {
    CCSPlayer *localPlayer = nullptr;
    while (localPlayer == nullptr) {
        localPlayer = *reinterpret_cast<CCSPlayer **>(core::GetModule("client.dll") + dwEntityList);
    }
    return localPlayer;
}

CCSPlayer *GetPlayer(int index) {
    return *reinterpret_cast<CCSPlayer **>(core::GetModule("client.dll") + dwEntityList + ENTGAP * index);
}

int GetPlayerCount() {
    return *reinterpret_cast<int *>(core::GetModule("server.dll") + dwNumPlayers);
}

int GetMaxPlayerCount() {
    return *reinterpret_cast<int *>(core::GetModule("server.dll") + dwMaxPlayers);
}

} // namespace game
