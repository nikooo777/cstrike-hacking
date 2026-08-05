#include "features/bhop.h"

#include <Windows.h>

#include "core/constants.h"
#include "core/modules.h"
#include "core/offsets.h"
#include "features/config.h"
#include "game/entity_list.h"

namespace features {

void Bhop() {
    if (!GetConfig().bhop) {
        return;
    }

    if (GetAsyncKeyState(VK_SPACE) & BUTTON_DOWN) {
        uintptr_t buffer = 4;
        if (game::GetLocalPlayer()->m_fFlags() & FL_ONGROUND) {
            buffer = 5;
        }
        *reinterpret_cast<DWORD *>(core::GetModule("client.dll") + dwForceJump) = buffer;
    }
}

} // namespace features
