//
// Created by Niko on 7/22/2021.
//

#include "triggerbot.h"
#include "common.h"
#include "CCSPlayer.h"


void attack1(bool active) {
    auto helper = Helper::getInstance();
    static uintptr_t buffer = 4;
    if (!active && buffer == 4) {
        return;
    }
    buffer = buffer == 4 ? 5 : 4;
    *(DWORD *) (helper->GetModule("client.dll") + dwForceAttack1) = buffer;
}

void triggerBot() {
    auto helper = Helper::getInstance();
    auto localPlayer = helper->GetLocalPlayer();
    if (GetAsyncKeyState(VK_SHIFT) & BUTTON_DOWN) {
        auto aimedTarget = ((CCSPlayer *) localPlayer)->m_iCrosshair;
        if (aimedTarget > 0 && aimedTarget < MAXPLAYERS) {
            auto target = helper->GetPlayer(aimedTarget - 1);
            if (target == nullptr) {
                std::cout << "target is null!" << std::endl;
                return;
            }
            auto shouldShoot = (!target->m_bIsDormant && target->m_iTeamNum != localPlayer->m_iTeamNum &&
                                target->m_iLifeState == ALIVE && localPlayer->m_iFlags & FL_ONGROUND);
            if (shouldShoot) {
                attack1(true);
            } else {
                attack1(false);
            }
        } else {
            attack1(false);
        }
    } else {
        attack1(false);
    }
}