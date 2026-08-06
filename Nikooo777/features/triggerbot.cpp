#include "features/triggerbot.h"

#include <Windows.h>

#include "core/constants.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "game/player.h"
#include "sdk/user_cmd.h"

namespace features {

namespace {

void Attack1(CUserCmd *userCmd, bool active) {
    if (userCmd == nullptr) {
        return;
    }
    if (active) {
        userCmd->buttons |= IN_ATTACK;
    } else {
        userCmd->buttons &= ~IN_ATTACK;
    }
}

} // namespace

void Triggerbot(CUserCmd *userCmd) {
    if (userCmd == nullptr) {
        return;
    }
    if (!GetConfig().triggerbot) {
        return;
    }

    auto localPlayer = game::GetLocalPlayer();
    if (!localPlayer) {
        return;
    }

    if (GetAsyncKeyState(VK_SHIFT) & BUTTON_DOWN) {
        auto aimedTarget = localPlayer->m_iCrosshairID();
        if (aimedTarget > 0 && aimedTarget <= MAXPLAYERS) {
            auto target = game::GetPlayer(aimedTarget - 1);
            if (target == nullptr) {
                Attack1(userCmd, false);
                return;
            }
            auto shouldShoot = game::IsValidTarget(localPlayer, target) &&
                               (localPlayer->m_fFlags() & FL_ONGROUND);
            Attack1(userCmd, shouldShoot);
        } else {
            Attack1(userCmd, false);
        }
    }
}

} // namespace features
