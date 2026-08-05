#include "game/player.h"

#include "core/constants.h"

namespace game {

bool IsAlive(const CBasePlayer *player) {
    return player && player->m_lifeState() == ALIVE;
}

bool IsEnemy(const CCSPlayer *local, const CCSPlayer *other) {
    if (!local || !other) {
        return false;
    }
    int team = other->m_iTeamNum();
    if (team == TEAM_UNASSIGNED || team == TEAM_SPEC) {
        return false;
    }
    return team != local->m_iTeamNum();
}

bool IsValidTarget(const CCSPlayer *local, const CCSPlayer *other) {
    return other && IsAlive(other) && !other->m_bDormant() && IsEnemy(local, other);
}

Vector3 EyePosition(const CBasePlayer *player) {
    return player->m_vecOrigin() + player->m_vecViewOffset();
}

} // namespace game
