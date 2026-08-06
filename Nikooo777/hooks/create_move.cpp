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

#if defined(_M_IX86) || defined(__i386__)
bool __fastcall hkCreateMove(void *thisPtr, void * /*edx*/, float flInputSampleTime, CUserCmd *userCmd) {
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd) {
#endif
    const bool result = originalCreateMove(thisPtr, flInputSampleTime, userCmd);
    if (userCmd == nullptr) {
        return result;
    }

    features::Bhop(userCmd);
    features::Triggerbot(userCmd);

    if ((GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) && features::GetConfig().aimbot) {
        features::Aimbot(userCmd);
    }

    if (GetAsyncKeyState(VK_F1) & 1) {
        features::PrintDebugInfo();
    }

    features::NoRecoil(userCmd);
    return result;
}

} // namespace hooks
