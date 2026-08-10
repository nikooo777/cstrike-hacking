#include "features/norecoil.h"

#include <cmath>

#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/entity/c_cs_player.h"

namespace features {

bool ReadRecoilState(const CCSPlayer *player, RecoilState &state) {
    state = {};
    if (player == nullptr) {
        return false;
    }

    const int localOffset =
        netvars::GetOffset("DT_LocalPlayerExclusive", "m_Local");
    const int punchOffset = netvars::GetOffset("DT_Local", "m_vecPunchAngle");
    if (localOffset < 0 || punchOffset < 0) {
        return false;
    }

    const auto *playerBytes = reinterpret_cast<const char *>(player);
    const auto *localBytes = playerBytes + localOffset;
    if (!mem::ReadValue(localBytes + punchOffset, state.punchAngles)) {
        state = {};
        return false;
    }

    state.punchReadable = std::isfinite(state.punchAngles.x) &&
                           std::isfinite(state.punchAngles.y) &&
                           std::isfinite(state.punchAngles.z);
    return state.punchReadable;
}

} // namespace features
