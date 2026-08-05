#pragma once

#include "sdk/entity/c_base_player.h"
#include "sdk/entity/c_cs_player.h"
#include "math/vector.h"

namespace game {

bool IsAlive(const CBasePlayer *player);
bool IsEnemy(const CCSPlayer *local, const CCSPlayer *other);
bool IsValidTarget(const CCSPlayer *local, const CCSPlayer *other);
Vector3 EyePosition(const CBasePlayer *player);

} // namespace game
