//
// Created by Niko on 7/25/2021.
//

#include "common.h"
#include "aimbot.h"
#include "vector.h"
#include "Helper.h"

std::ostream &operator<<(std::ostream &os, const Vector3 &v) {
    os << "x: " << v.x << ", y: " << v.y << ", z: " << v.z << " - drawcross " << v.x << " " << v.y << " " << v.z;
    return os;
}

void aimbot() {
    auto helper = Helper::getInstance();
    auto localPlayer = helper->GetLocalPlayer();
    if (!localPlayer) {
        return;
    }
    //get number of players
    auto maxPlayers = 32;//helper->GetMaxPlayerCount();
    //loop through all players
    auto closestDistance = 999999.f;
    auto closestPlayerIndex = -1;
    for (int i = 0; i < maxPlayers; i++) {
        //print team, hp, position
        auto p = helper->GetPlayer(i);
        if (!p || p->m_iTeamNum == localPlayer->m_iTeamNum || p->m_iLifeState != ALIVE || p->m_bIsDormant) {
            continue;
        }
//        std::cout << "Player " << i << " team: " << p->m_iTeamNum << " health: " << p->m_iHealth << " pos: " << p->m_vecPos << std::endl;
        //find closest enemy
        auto distance = localPlayer->m_vecPos.Distance(p->m_vecPos);
        if (distance < closestDistance) {
            closestDistance = distance;
            closestPlayerIndex = i;
        }
    }
    if (closestPlayerIndex == -1) {
        return;
    }
    auto playerToAim = helper->GetPlayer(closestPlayerIndex);
    std::cout << "closest player is Player " << closestPlayerIndex << " with a distance of " << closestDistance << " units and HP:" << playerToAim->m_iHealth << std::endl;
    //get their head position
    auto boneMatrixBase = playerToAim->m_pBoneMatrixBase;
    auto headMatrix = boneMatrixBase + 0x30 * 14;
    Vector3 headPos{*(float *) (headMatrix + 0xC), *(float *) (headMatrix + 0x1C), *(float *) (headMatrix + 0x2C)};
    std::cout << "player vec: " << playerToAim->m_vecPos << " head vec: " << headPos << std::endl;
    auto viewPos = localPlayer->m_vecPos + localPlayer->m_vecViewOffset;
    auto newAngles = viewPos.CalcAngle(headPos);
//    newAngles.NormalizeAngles();
//    newAngles.ClampAngles();
    std::cout << "new angles: " << newAngles << std::endl;
    helper->GetClientState()->m_vViewAngles = newAngles;
    //calculate view angles
    //aim at viewAngles
    //fire

}