#pragma once

#include <cstdint>

#include "math/vector.h"

class CUserCmd;

namespace features {

// One diagnostic snapshot of the shared aim/recoil/spread composition for the
// most recent CreateMove call on the game thread.
struct ShotAngleTrace {
    Vector3 inputAngles{};
    Vector3 desiredAngles{};
    Vector3 fireBaseAngles{};
    Vector3 spreadAngles{};
    Vector3 commandAngles{};
    Vector3 currentPunchAngles{};
    Vector3 punchAngles{};
    float intervalPerTick = 0.0f;
    float currentInaccuracy = 0.0f;
    float fireInaccuracy = 0.0f;
    float spreadRadius = 0.0f;
    float accuracyPenalty = 0.0f;
    float fireAccuracyPenalty = 0.0f;
    float spreadX = 0.0f;
    float spreadY = 0.0f;
    float spreadResidualDeg = 0.0f;
    int spreadIterations = 0;
    std::uint32_t seed = 0;
    int shotsFired = 0;
    bool attack = false;
    bool aimbotApplied = false;
    bool noRecoilRequested = false;
    bool noRecoilApplied = false;
    bool noSpreadRequested = false;
    bool noSpreadApplied = false;
    bool recoilStateReadable = false;
    bool firePunchPredicted = false;
    bool spreadStateUsable = false;
    bool fireAccuracyAvailable = false;
    bool seedUsable = false;
};

// Compose the simulation angle once. `inputAngles` is the post-original
// command/camera intent; `desiredAimAngles` is that intent after target
// selection. Recoil and spread are then applied in fire space according to
// the runtime feature configuration.
void ApplyAimAndFireCorrections(CUserCmd *userCmd,
                                const Vector3 &inputAngles,
                                const Vector3 &desiredAimAngles,
                                bool aimbotApplied);

const ShotAngleTrace &GetLastShotAngleTrace();

bool CommandAnglesChanged(const CUserCmd *userCmd,
                          const Vector3 &intendedCamera);

} // namespace features
