#include "features/aimbot.h"

#include "features/config.h"
#include "game/entity_list.h"
#include "game/player.h"

namespace features {

void Aimbot(CUserCmd *userCmd) {
    if (!GetConfig().aimbot) {
        return;
    }

    auto localPlayer = game::GetLocalPlayer();
    if (!localPlayer) {
        return;
    }

    auto maxPlayers = 32;
    auto closestDistance = 999999.f;
    auto closestPlayerIndex = -1;

    for (int i = 1; i <= maxPlayers; i++) {
        auto p = game::GetPlayer(i);
        if (!game::IsValidTarget(localPlayer, p)) {
            continue;
        }

        auto distance = localPlayer->m_vecOrigin().Distance(p->m_vecOrigin());
        if (distance < closestDistance) {
            closestDistance = distance;
            closestPlayerIndex = i;
        }
    }

    if (closestPlayerIndex == -1) {
        return;
    }

    auto playerToAim = game::GetPlayer(closestPlayerIndex);
    auto boneMatrixBase = playerToAim->m_dwBoneMatrix();
    auto headMatrix = boneMatrixBase + 0x30 * 14;
    Vector3 headPos{*(float *)(headMatrix + 0xC), *(float *)(headMatrix + 0x1C),
                    *(float *)(headMatrix + 0x2C)};
    auto viewPos = game::EyePosition(localPlayer);
    userCmd->viewangles = viewPos.CalcAngle(headPos);
}

} // namespace features
