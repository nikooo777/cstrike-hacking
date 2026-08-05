#include "features/norecoil.h"

#include "features/config.h"
#include "game/entity_list.h"
#include "math/vector.h"

namespace features {

void NoRecoil(CUserCmd *userCmd) {
    if (!GetConfig().norecoil) {
        return;
    }

    static Vector3 oldPunch = {0, 0, 0};
    auto localPlayer = game::GetLocalPlayer();
    auto shotsFired = localPlayer->m_iShotsFired();
    auto punchAngle = localPlayer->m_Local().m_vecPunchAngle;

    Vector3 tempAngle = {0, 0, 0};
    if (shotsFired > 0) {
        tempAngle.x = (userCmd->viewangles.x + oldPunch.x) - (punchAngle.x * 2);
        tempAngle.y = (userCmd->viewangles.y + oldPunch.y) - (punchAngle.y * 2);
        tempAngle.NormalizeAngles();
        tempAngle.ClampAngles();
        oldPunch.x = punchAngle.x * 2;
        oldPunch.y = punchAngle.y * 2;
        userCmd->viewangles = tempAngle;
    } else {
        oldPunch = {0, 0, 0};
    }
}

} // namespace features
