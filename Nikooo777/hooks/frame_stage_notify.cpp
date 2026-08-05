#include "hooks/hooks.h"

#include "features/config.h"
#include "game/entity_list.h"
#include "math/vector.h"

namespace hooks {

void __fastcall hkFrameStageNotify(void *thisPtr, void * /*edx*/, ClientFrameStage_t curStage) {
    static Vector3 oldPunch, *pPunch = nullptr;

    if (curStage == FRAME_RENDER_START) {
        auto *localPlayer = game::GetLocalPlayer();
        if (features::GetConfig().visualNoRecoil && localPlayer) {
            pPunch = &localPlayer->m_Local().m_vecPunchAngle();
            if (pPunch && (pPunch->x != 0 || pPunch->y != 0 || pPunch->z != 0)) {
                oldPunch = *pPunch;
                pPunch->Zero();
            }
        } else {
            pPunch = nullptr;
        }
    }

    originalFrameStageNotify(thisPtr, curStage);

    if (pPunch) {
        *pPunch = oldPunch;
    }
}

} // namespace hooks
