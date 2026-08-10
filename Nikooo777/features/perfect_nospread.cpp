#include "features/perfect_nospread.h"

#include <cmath>

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

namespace {

ShotAngleTrace g_lastShotAngleTrace{};

} // namespace

const ShotAngleTrace &GetLastShotAngleTrace() {
    return g_lastShotAngleTrace;
}

void ApplyAimAndFireCorrections(CUserCmd *userCmd,
                                const Vector3 &inputAngles,
                                const Vector3 &desiredAimAngles,
                                bool aimbotApplied) {
    g_lastShotAngleTrace = {};
    g_lastShotAngleTrace.inputAngles = inputAngles;
    g_lastShotAngleTrace.desiredAngles = desiredAimAngles;
    g_lastShotAngleTrace.aimbotApplied = aimbotApplied;

    if (userCmd == nullptr) {
        return;
    }

    // Aimbot is the first writer; if a later stage cannot read its state,
    // preserving its selected angle is safer than reverting to the camera.
    userCmd->viewangles = desiredAimAngles;
    g_lastShotAngleTrace.commandAngles = desiredAimAngles;
    g_lastShotAngleTrace.fireBaseAngles = desiredAimAngles;
    g_lastShotAngleTrace.spreadAngles = desiredAimAngles;

    const auto &config = GetConfig();
    g_lastShotAngleTrace.attack = (userCmd->buttons & IN_ATTACK) != 0;
    g_lastShotAngleTrace.noRecoilRequested = config.norecoil;
    g_lastShotAngleTrace.noSpreadRequested =
        config.perfectNoSpread && g_lastShotAngleTrace.attack;

    auto *local = game::GetLocalPlayer();
    RecoilState recoilState{};
    if (local != nullptr) {
        ReadRecoilState(local, recoilState);
    }
    Vector3 firePunch = recoilState.punchAngles;
    float intervalPerTick = 0.0f;
#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    if (recoilState.punchReadable &&
        game::GetIntervalPerTick(intervalPerTick) &&
        game::PredictCssPunchDecay(recoilState.punchAngles, intervalPerTick,
                                   firePunch)) {
        g_lastShotAngleTrace.firePunchPredicted = true;
    }
#endif
    g_lastShotAngleTrace.currentPunchAngles = recoilState.punchAngles;
    g_lastShotAngleTrace.punchAngles = firePunch;
    g_lastShotAngleTrace.intervalPerTick = intervalPerTick;
    g_lastShotAngleTrace.recoilStateReadable = recoilState.punchReadable;

    game::ShotAngleRequest request{};
    request.desiredAngles = desiredAimAngles;
    request.punchAngles = firePunch;
    request.punchReadable = recoilState.punchReadable;
    request.noRecoil = config.norecoil;
    request.noSpread = g_lastShotAngleTrace.noSpreadRequested;

    if (g_lastShotAngleTrace.noSpreadRequested && local != nullptr) {
        game::WeaponSpreadState spreadState{};
        const bool spreadRead = game::ReadWeaponSpreadState(local, spreadState);
        if (spreadRead) {
            g_lastShotAngleTrace.currentInaccuracy = spreadState.inaccuracy;
            g_lastShotAngleTrace.fireInaccuracy =
                spreadState.fireInaccuracy;
            g_lastShotAngleTrace.spreadRadius = spreadState.spread;
            g_lastShotAngleTrace.accuracyPenalty =
                spreadState.accuracyPenalty;
            g_lastShotAngleTrace.fireAccuracyPenalty =
                spreadState.fireAccuracyPenalty;
            g_lastShotAngleTrace.shotsFired = spreadState.shotsFired;
            g_lastShotAngleTrace.fireAccuracyAvailable =
                spreadState.fireInaccuracyOk;
        }
        if (spreadRead && spreadState.usableForCompensation) {
            g_lastShotAngleTrace.spreadStateUsable = true;

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
                    g_lastShotAngleTrace.seed = randomSeed;
                    g_lastShotAngleTrace.seedUsable = true;
                    g_lastShotAngleTrace.spreadX = cone.sx;
                    g_lastShotAngleTrace.spreadY = cone.sy;
                }
            }
        }
    }

    game::ShotAngleResult result{};
    if (game::ComposeShotAngles(request, result) && result.ok) {
        userCmd->viewangles = result.commandAngles;
        g_lastShotAngleTrace.fireBaseAngles = result.fireBaseAngles;
        g_lastShotAngleTrace.spreadAngles = result.spreadAngles;
        g_lastShotAngleTrace.commandAngles = result.commandAngles;
        g_lastShotAngleTrace.spreadResidualDeg = result.spreadResidualDeg;
        g_lastShotAngleTrace.spreadIterations = result.spreadIterations;
        g_lastShotAngleTrace.noRecoilApplied = result.noRecoilApplied;
        g_lastShotAngleTrace.noSpreadApplied = result.noSpreadApplied;
    }

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
