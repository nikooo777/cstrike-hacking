#include "game/entity_list.h"

#include "core/constants.h"
#include "game/interfaces.h"

namespace game {

CCSPlayer *GetLocalPlayer() {
    return GetPlayer(0);
}

CCSPlayer *GetPlayer(int index) {
    if (index < 0 || index >= MAXPLAYERS) {
        return nullptr;
    }

    auto *entityList = GetClientEntityList();
    if (entityList == nullptr) {
        return nullptr;
    }

    // Player slot 0 corresponds to Source entity index 1; entity index 0 is
    // the world entity in this client build.
    constexpr int kFirstPlayerEntityIndex = 1;
    return reinterpret_cast<CCSPlayer *>(
        entityList->GetClientEntity(kFirstPlayerEntityIndex + index));
}

} // namespace game
