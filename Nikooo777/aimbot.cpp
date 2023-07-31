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

void aimbot(CUserCmd *userCmd) {
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
		auto p = helper->GetPlayer(i);
		if (!p || p->m_iTeamNum == localPlayer->m_iTeamNum || p->m_iLifeState != ALIVE || p->m_bIsDormant) {
			continue;
		}
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
	//std::cout << "closest player is Player " << closestPlayerIndex << " with a distance of " << closestDistance << " units and HP:" << std::dec << playerToAim->m_iHealth << std::endl;
	//get their head position
	auto boneMatrixBase = playerToAim->m_pBoneMatrixBase;
	auto headMatrix = boneMatrixBase + 0x30 * 14;
	Vector3 headPos{*(float *) (headMatrix + 0xC), *(float *) (headMatrix + 0x1C), *(float *) (headMatrix + 0x2C)};
	auto viewPos = localPlayer->m_vecPos + localPlayer->m_vecViewOffset;
	//calculate view angles
	auto newAngles = viewPos.CalcAngle(headPos);

	//aim at viewAngles
	userCmd->viewangles = newAngles;
}