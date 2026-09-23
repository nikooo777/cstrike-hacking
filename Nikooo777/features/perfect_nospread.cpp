#include "features/perfect_nospread.h"

#include <cmath>

#include "core/arch.h"
#include "core/constants.h"
#include "features/config.h"
#include "features/norecoil.h"
#include "game/entity_list.h"
#include "game/timing.h"
#include "game/weapon.h"
#include "game/weapon_math.h"
#include "sdk/entity/c_cs_player.h"
#include "sdk/user_cmd.h"

namespace features {

ShotAngleTrace ApplyAimAndFireCorrections(CUserCmd *userCmd,
                                          const Vector3 &inputAngles,
                                          const Vector3 &desiredAimAngles,
                                          bool aimbotApplied) {
    ShotAngleTrace trace{};
    trace.inputAngles = inputAngles;
    trace.desiredAngles = desiredAimAngles;
    trace.aimbotApplied = aimbotApplied;

    if (userCmd == nullptr) {
        return trace;
    }

    // Aimbot is the first writer; if a later stage cannot read its state,
    // preserving its selected angle is safer than reverting to the camera.
    userCmd->viewangles = desiredAimAngles;
    trace.commandAngles = desiredAimAngles;
    trace.fireBaseAngles = desiredAimAngles;
    trace.spreadAngles = desiredAimAngles;

    const auto &config = GetConfig();
    trace.attack = (userCmd->buttons & IN_ATTACK) != 0;
    trace.noRecoilRequested = config.norecoil;
    trace.noSpreadRequested =
        config.perfectNoSpread && trace.attack;

    auto *local = game::GetLocalPlayer();
    RecoilState recoilState{};
    if (local != nullptr) {
        ReadRecoilState(local, recoilState);
    }
    Vector3 firePunch = recoilState.punchAngles;
    float intervalPerTick = 0.0f;
#if ARCH_X64()
    if (recoilState.punchReadable &&
        game::GetIntervalPerTick(intervalPerTick) &&
        game::PredictCssPunchDecay(recoilState.punchAngles, intervalPerTick,
                                   firePunch)) {
        trace.firePunchPredicted = true;
    }
#endif
    trace.currentPunchAngles = recoilState.punchAngles;
    trace.punchAngles = firePunch;
    trace.intervalPerTick = intervalPerTick;
    trace.recoilStateReadable = recoilState.punchReadable;

    game::ShotAngleRequest request{};
    request.desiredAngles = desiredAimAngles;
    request.punchAngles = firePunch;
    request.punchReadable = recoilState.punchReadable;
    request.noRecoil = config.norecoil;
    request.noSpread = trace.noSpreadRequested;

    if (trace.noSpreadRequested && local != nullptr) {
        game::WeaponSpreadState spreadState{};
        const bool spreadRead = game::ReadWeaponSpreadState(local, spreadState);
        if (spreadRead) {
            trace.currentInaccuracy = spreadState.inaccuracy;
            trace.fireInaccuracy =
                spreadState.fireInaccuracy;
            trace.spreadRadius = spreadState.spread;
            trace.accuracyPenalty =
                spreadState.accuracyPenalty;
            trace.fireAccuracyPenalty =
                spreadState.fireAccuracyPenalty;
            trace.shotsFired = spreadState.shotsFired;
            trace.fireAccuracyAvailable =
                spreadState.fireInaccuracyOk;
        }
        if (spreadRead && spreadState.usableForCompensation) {
            trace.spreadStateUsable = true;

            std::uint32_t randomSeed = 0;
            bool fromStoredSeed = false;
            if (game::ResolveCommandRandomSeed(userCmd, randomSeed,
                                               fromStoredSeed)) {
                (void)fromStoredSeed;
                game::ConeOffsets cone{};
                if (game::PredictConeOffsets(
                        static_cast<int>(randomSeed),
                        spreadState.fireInaccuracy,
                        spreadState.spread, cone) &&
                    cone.ok) {
                    request.spreadAvailable = true;
                    request.spreadX = cone.sx;
                    request.spreadY = cone.sy;
                    trace.seed = randomSeed;
                    trace.seedUsable = true;
                    trace.spreadX = cone.sx;
                    trace.spreadY = cone.sy;
                }
            }
        }
    }

    game::ShotAngleResult result{};
    if (game::ComposeShotAngles(request, result) && result.ok) {
        userCmd->viewangles = result.commandAngles;
        trace.fireBaseAngles = result.fireBaseAngles;
        trace.spreadAngles = result.spreadAngles;
        trace.commandAngles = result.commandAngles;
        trace.spreadResidualDeg = result.spreadResidualDeg;
        trace.spreadIterations = result.spreadIterations;
        trace.noRecoilApplied = result.noRecoilApplied;
        trace.noSpreadApplied = result.noSpreadApplied;
    }
    return trace;
}

bool CommandAnglesChanged(const CUserCmd *userCmd,
                          const Vector3 &intendedCamera) {
    if (userCmd == nullptr) {
        return false;
    }

    constexpr float kEpsilon = 1e-4f;
    return fabsf(userCmd->viewangles.x - intendedCamera.x) > kEpsilon ||
           fabsf(userCmd->viewangles.y - intendedCamera.y) > kEpsilon ||
           fabsf(userCmd->viewangles.z - intendedCamera.z) > kEpsilon;
}

} // namespace features
