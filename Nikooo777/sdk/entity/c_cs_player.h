#pragma once

#include "core/padding.h"
#include "sdk/client_offsets.h"
#include "sdk/entity/c_base_player.h"

// CS:S-specific player fields. Same client entity pointer as CBasePlayer.
class CCSPlayer : public CBasePlayer {
public:
    DEFINE_NETVAR(int, m_iShotsFired, "DT_CSLocalPlayerExclusive", "m_iShotsFired");
    // Client-only crosshair target (C_CSPlayer::GetIDTarget); no RecvProp
    // exists for it in either architecture.
    DEFINE_MEMBER(int, m_iCrosshairID, sdk::offsets::kCrosshairTarget);
};
