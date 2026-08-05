#pragma once

#include "sdk/entity/c_cs_player.h"

namespace game {

CCSPlayer *GetLocalPlayer();
CCSPlayer *GetPlayer(int index);

} // namespace game
