#include "features/triggerbot.h"

#include <Windows.h>
#include <iostream>

#include "core/constants.h"
#include "core/modules.h"
#include "core/offsets.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "game/player.h"

namespace features {

namespace {

void Attack1(bool active) {
    static uintptr_t buffer = 4;
    if (!active && buffer == 4) {
        return;
    }
    buffer = buffer == 4 ? 5 : 4;
    *reinterpret_cast<DWORD *>(core::GetModule("client.dll") + dwForceAttack1) = buffer;
}

} // namespace

void Triggerbot() {
    if (!GetConfig().triggerbot) {
        Attack1(false);
        return;
    }

    auto localPlayer = game::GetLocalPlayer();
    if (GetAsyncKeyState(VK_SHIFT) & BUTTON_DOWN) {
        auto aimedTarget = localPlayer->m_iCrosshairID();
        if (aimedTarget > 0 && aimedTarget < MAXPLAYERS) {
            auto target = game::GetPlayer(aimedTarget - 1);
            if (target == nullptr) {
                std::cout << "target is null!" << std::endl;
                return;
            }
            auto shouldShoot = game::IsValidTarget(localPlayer, target) &&
                               (localPlayer->m_fFlags() & FL_ONGROUND);
            Attack1(shouldShoot);
        } else {
            Attack1(false);
        }
    } else {
        Attack1(false);
    }
}

} // namespace features
