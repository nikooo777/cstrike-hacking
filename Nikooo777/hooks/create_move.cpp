#include "hooks/hooks.h"

#include <Windows.h>

#include "core/constants.h"
#include "features/aimbot.h"
#include "features/bhop.h"
#include "features/config.h"
#include "features/debug_info.h"
#include "features/norecoil.h"
#include "features/perfect_nospread.h"
#include "features/triggerbot.h"
#include "math/vector.h"
#include "sdk/user_cmd.h"

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

    // Camera intent after the game filled the command; sim features may diverge.
    const Vector3 intendedCamera = userCmd->viewangles;

    features::Bhop(userCmd);
    features::Triggerbot(userCmd);

    if ((GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) && features::GetConfig().aimbot) {
        features::Aimbot(userCmd);
    }

    features::NoRecoil(userCmd);
    features::PerfectNoSpread(userCmd);

    // F1 after mutation so the dump shows final cmd angles / deltas for this tick.
    // Cone prediction uses a local RNG stream and does not reseed vstdlib.
    if (GetAsyncKeyState(VK_F1) & 1) {
        features::PrintDebugInfo(userCmd);
    }

    // Source's input caller copies cmd->viewangles into the render camera
    // only when CreateMove returns true. Returning false for a mutated command
    // keeps the compensated simulation angles off the local camera without a
    // later SetViewAngles race against extra input samples/render stages.
    if (features::ShouldSuppressCameraUpdate(userCmd, intendedCamera)) {
        return false;
    }

    return result;
}

} // namespace hooks
