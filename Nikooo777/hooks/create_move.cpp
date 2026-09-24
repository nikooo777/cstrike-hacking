#include "hooks/hooks.h"

#include <Windows.h>

#include "core/arch.h"
#include "core/constants.h"
#include "features/aimbot.h"
#include "features/bhop.h"
#include "features/config.h"
#include "features/debug_info.h"
#include "features/fire_capture.h"
#include "features/perfect_nospread.h"
#include "features/telemetry.h"
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

#if ARCH_X86()
bool __fastcall hkCreateMove(void *thisPtr, void * /*edx*/, float flInputSampleTime, CUserCmd *userCmd) {
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd) {
#endif
    const bool result = originalCreateMove(thisPtr, flInputSampleTime, userCmd);
    if (userCmd == nullptr) {
        return result;
    }

    const bool debugPressed = (GetAsyncKeyState(VK_F1) & 1) != 0;
#if ARCH_X64()
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

    features::telemetry::CountCall(features::telemetry::Hook::CreateMove);

    // Camera intent after the game filled the command; sim features may diverge.
    const Vector3 intendedCamera = userCmd->viewangles;

    // These read physical keys, which the menu's message filter cannot hide.
    const bool menuOpen = features::GetConfig().menuOpen;
    if (!menuOpen) {
        features::Bhop(userCmd);
        features::Triggerbot(userCmd);
    }

    bool aimbotApplied = false;
    if (!menuOpen && (GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) &&
        features::GetConfig().aimbot) {
        aimbotApplied = features::Aimbot(userCmd);
    }

    const auto shotTrace = features::ApplyAimAndFireCorrections(
        userCmd, intendedCamera, userCmd->viewangles, aimbotApplied);
    features::telemetry::PublishShotTrace(shotTrace);
#if ARCH_X64()
    features::CaptureClientFireCommand(userCmd, shotTrace);
#endif

    // F1 after mutation so the dump shows final cmd angles / deltas for this tick.
    // Cone prediction uses a local RNG stream and does not reseed vstdlib.
    const bool dumpRequested = features::telemetry::ConsumeDumpRequest();
    if (debugPressed || g_debugPendingForRealCommand || dumpRequested) {
        features::PrintDebugInfo(userCmd, &shotTrace);
        g_debugPendingForRealCommand = false;
    }

    // Source's input caller copies cmd->viewangles into the render camera
    // only when CreateMove returns true. Returning false for a mutated command
    // keeps the compensated simulation angles off the local camera without a
    // later SetViewAngles race against extra input samples/render stages.
    if (features::CommandAnglesChanged(userCmd, intendedCamera)) {
        if (shotTrace.noRecoilApplied ||
            features::GetConfig().silentAngles) {
            return false;
        }
        return true;
    }

    return result;
}

} // namespace hooks
