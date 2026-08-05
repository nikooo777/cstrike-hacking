#include "features/bhop.h"

#include <Windows.h>

#include "core/constants.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "sdk/user_cmd.h"

namespace features {

void Bhop(CUserCmd *userCmd) {
    if (userCmd == nullptr || !GetConfig().bhop) {
        return;
    }

    auto *local = game::GetLocalPlayer();
    if (!local) {
        return;
    }

    if (GetAsyncKeyState(VK_SPACE) & BUTTON_DOWN) {
        if (local->m_fFlags() & FL_ONGROUND) {
            userCmd->buttons |= IN_JUMP;
        } else {
            userCmd->buttons &= ~IN_JUMP;
        }
    }
}

} // namespace features
