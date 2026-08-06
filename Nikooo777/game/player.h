#pragma once

#include "sdk/entity/c_base_player.h"
#include "sdk/entity/c_cs_player.h"
#include "math/vector.h"

namespace game {

struct BoneCacheInfo {
    std::uintptr_t entityAddress = 0;
    std::uintptr_t matrix = 0;
    int count = 0;
    bool matrixReadable = false;
    bool countReadable = false;
};

struct DormancyInfo {
    bool dormant = true;
    bool resolved = false;
};

bool IsAlive(const CBasePlayer *player);
bool IsEnemy(const CCSPlayer *local, const CCSPlayer *other);
bool IsValidTarget(const CCSPlayer *local, const CCSPlayer *other);
bool GetDormancyInfo(const CBasePlayer *player, DormancyInfo &info);
Vector3 EyePosition(const CBasePlayer *player);
bool GetBoneCacheInfo(const CBasePlayer *player, BoneCacheInfo &cache);
bool GetBonePosition(const CBasePlayer *player, int bone, Vector3 &position);

} // namespace game
