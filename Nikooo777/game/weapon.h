#pragma once

#include <cstdint>

#include "game/weapon_math.h"

class CCSPlayer;

namespace game {

// Opaque weapon entity pointer from the client entity list.
using WeaponEntity = void *;

// Live values needed by perfect-nospread diagnostics and compensation.
// See aidocs/005_no-spread-and-weapon-accuracy.md.
struct WeaponSpreadState {
    WeaponEntity weapon = nullptr;
    std::uint32_t handle = 0;
    int mode = 0;
    int clip1 = -1;
    float inaccuracy = 0.0f;
    float spread = 0.0f;
    float accuracyPenalty = 0.0f;
    bool handleReadOk = false;
    bool weaponResolved = false;
    bool modeOk = false;
    bool clipOk = false;
    bool penaltyOk = false;
    bool inaccuracyOk = false;
    bool spreadOk = false;
    bool methodsOk = false;
    // True when methods are ok and values sit in soft plausible ranges.
    bool usableForCompensation = false;
};

bool GetActiveWeapon(const CCSPlayer *player, WeaponEntity &weapon,
                     std::uint32_t *handleOut = nullptr,
                     bool *handleReadOut = nullptr);
bool ReadWeaponSpreadState(const CCSPlayer *player, WeaponSpreadState &state);

// Call virtual GetInaccuracy / GetSpread (slots 382 / 383). Fail closed.
bool GetWeaponInaccuracy(WeaponEntity weapon, float &out);
bool GetWeaponSpread(WeaponEntity weapon, float &out);

} // namespace game
