#pragma once

#include "sdk/entity/c_cs_player.h"

namespace game {

CCSPlayer *GetLocalPlayer();
CCSPlayer *GetPlayer(int index);
int GetPlayerCount();
int GetMaxPlayerCount();

} // namespace game
