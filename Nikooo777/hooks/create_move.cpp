#include "hooks/hooks.h"

#include <Windows.h>

#include "core/constants.h"
#include "features/aimbot.h"
#include "features/bhop.h"
#include "features/config.h"
#include "features/debug_info.h"
#include "features/perfect_nospread.h"
#include "features/triggerbot.h"
#include "math/vector.h"
#include "sdk/user_cmd.h"

namespace hooks {

namespace {

// Source calls ClientMode::CreateMove for an extra mouse sample as well as
// for the command that will be submitted. The extra sample is a temporary
// zeroed CUserCmd; it must not receive aim, recoil, button, or spread-angle
// mutations.
bool g_debugPendingForRealCommand = false;

} // namespace

#if defined(_M_IX86) || defined(__i386__)
bool __fastcall hkCreateMove(void *thisPtr, void * /*edx*/, float flInputSampleTime, CUserCmd *userCmd) {
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd) {
#endif
    const bool result = originalCreateMove(thisPtr, flInputSampleTime, userCmd);
    if (userCmd == nullptr) {
        return result;
    }

    const bool debugPressed = (GetAsyncKeyState(VK_F1) & 1) != 0;
#if !defined(_M_IX86) && !defined(__i386__)
    if (debugPressed) {
        features::ArmClientFireDiagnostic();
    }
#endif
    if (userCmd->command_number <= 0) {
        // Keep the temporary extra-input command untouched. If F1 happened to
        // edge during this call, defer the dump until the next real command so
        // the diagnostic reports the seed that can actually be fired.
        if (debugPressed) {
            g_debugPendingForRealCommand = true;
        }
        return result;
    }

    // Camera intent after the game filled the command; sim features may diverge.
    const Vector3 intendedCamera = userCmd->viewangles;

    features::Bhop(userCmd);
    features::Triggerbot(userCmd);

    bool aimbotApplied = false;
    if ((GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) &&
        features::GetConfig().aimbot) {
        aimbotApplied = features::Aimbot(userCmd);
    }

    features::ApplyAimAndFireCorrections(userCmd, intendedCamera,
                                          userCmd->viewangles,
                                          aimbotApplied);
#if !defined(_M_IX86) && !defined(__i386__)
    features::CaptureClientFireCommand(userCmd);
#endif

    // F1 after mutation so the dump shows final cmd angles / deltas for this tick.
    // Cone prediction uses a local RNG stream and does not reseed vstdlib.
    if (debugPressed || g_debugPendingForRealCommand) {
        features::PrintDebugInfo(userCmd);
        g_debugPendingForRealCommand = false;
    }

    // Source's input caller copies cmd->viewangles into the render camera
    // only when CreateMove returns true. Returning false for a mutated command
    // keeps the compensated simulation angles off the local camera without a
    // later SetViewAngles race against extra input samples/render stages.
    if (features::CommandAnglesChanged(userCmd, intendedCamera)) {
        const auto &shotTrace = features::GetLastShotAngleTrace();
        if (shotTrace.noRecoilApplied ||
            features::GetConfig().silentAngles) {
            return false;
        }
        return true;
    }

    return result;
}

} // namespace hooks
