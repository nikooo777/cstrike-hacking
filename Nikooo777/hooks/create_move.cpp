#include "hooks/hooks.h"

#include <Windows.h>

#include "core/constants.h"
#include "features/aimbot.h"
#include "features/bhop.h"
#include "features/config.h"
#include "features/debug_info.h"
#include "features/norecoil.h"
#include "features/triggerbot.h"

namespace hooks {

bool __fastcall hkCreateMove(void *thisPtr, void * /*edx*/, float flInputSampleTime, CUserCmd *userCmd) {
    features::Bhop();
    features::Triggerbot();

    if ((GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) && features::GetConfig().aimbot) {
        features::Aimbot(userCmd);
    }

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        features::GetConfig().menuOpen = !features::GetConfig().menuOpen;
    }

    if (GetAsyncKeyState(VK_F1) & 1) {
        features::PrintDebugInfo();
    }

    features::NoRecoil(userCmd);
    return originalCreateMove(thisPtr, flInputSampleTime, userCmd);
}

} // namespace hooks
