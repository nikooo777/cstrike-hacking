#include "features/aimbot.h"

#include <limits>

#include "core/constants.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "game/player.h"

namespace features {

bool Aimbot(CUserCmd *userCmd) {
    if (userCmd == nullptr) {
        return false;
    }
    if (!GetConfig().aimbot) {
        return false;
    }

    auto localPlayer = game::GetLocalPlayer();
    if (!localPlayer) {
        return false;
    }

    constexpr int kAimBone = 14;
    const auto viewPosition = game::EyePosition(localPlayer);
    auto closestDistance = (std::numeric_limits<float>::max)();
    CCSPlayer *closestPlayer = nullptr;
    Vector3 closestHead{};

    // Rebuild the candidate set every tick. This naturally drops a dead or
    // stale target and allows the next visible target to be selected.
    for (int i = 1; i < MAXPLAYERS; ++i) {
        auto p = game::GetPlayer(i);
        if (!game::IsValidTarget(localPlayer, p)) {
            continue;
        }

        Vector3 headPosition{};
        if (!game::GetBonePosition(p, kAimBone, headPosition) ||
            !game::IsVisible(localPlayer, p, headPosition)) {
            continue;
        }

        const auto distance = viewPosition.Distance(headPosition);
        if (distance < closestDistance) {
            closestDistance = distance;
            closestPlayer = p;
            closestHead = headPosition;
        }
    }

    if (closestPlayer == nullptr) {
        return false;
    }

    userCmd->viewangles = viewPosition.CalcAngle(closestHead);
    return true;
}

} // namespace features
