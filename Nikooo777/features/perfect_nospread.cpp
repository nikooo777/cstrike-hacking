#include "features/perfect_nospread.h"

#include <cmath>

#include "core/constants.h"
#include "features/config.h"
#include "game/entity_list.h"
#include "game/weapon.h"
#include "sdk/user_cmd.h"

namespace features {

void PerfectNoSpread(CUserCmd *userCmd) {
    if (userCmd == nullptr || !GetConfig().perfectNoSpread) {
        return;
    }
    if ((userCmd->buttons & IN_ATTACK) == 0) {
        return;
    }

    auto *local = game::GetLocalPlayer();
    if (local == nullptr) {
        return;
    }

    game::WeaponSpreadState state{};
    if (!game::ReadWeaponSpreadState(local, state) ||
        !state.usableForCompensation) {
        return;
    }

    std::uint32_t randomSeed = 0;
    bool fromStoredSeed = false;
    if (!game::ResolveCommandRandomSeed(userCmd, randomSeed,
                                        fromStoredSeed)) {
        // A zero-sequence CUserCmd is the temporary ExtraMouseSample command,
        // not a networked/fired command. Do not compensate it with the
        // deterministic seed for command zero.
        return;
    }
    (void)fromStoredSeed;

    game::ConeOffsets cone{};
    if (!game::PredictConeOffsets(static_cast<int>(randomSeed),
                                  state.inaccuracy, state.spread, cone) ||
        !cone.ok) {
        return;
    }

    // Near-zero cone: nothing meaningful to compensate.
    if (fabsf(cone.sx) < 1e-8f && fabsf(cone.sy) < 1e-8f) {
        return;
    }

    game::CompensationResult compensated{};
    if (!game::CompensateAngles(userCmd->viewangles, cone.sx, cone.sy,
                                compensated) ||
        !compensated.ok) {
        return;
    }

    // Fail closed if residual after iteration is still large (bad state).
    if (compensated.forwardErrorDeg > 1.0f) {
        return;
    }

    userCmd->viewangles = compensated.angles;
}

bool ShouldSuppressCameraUpdate(const CUserCmd *userCmd,
                                const Vector3 &intendedCamera) {
    if (userCmd == nullptr || !GetConfig().silentAngles) {
        return false;
    }

    constexpr float kEpsilon = 1e-4f;
    return fabsf(userCmd->viewangles.x - intendedCamera.x) > kEpsilon ||
           fabsf(userCmd->viewangles.y - intendedCamera.y) > kEpsilon ||
           fabsf(userCmd->viewangles.z - intendedCamera.z) > kEpsilon;
}

} // namespace features
