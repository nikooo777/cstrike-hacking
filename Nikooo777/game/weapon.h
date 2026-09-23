#pragma once

#include <cstddef>
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
    std::uint16_t weaponInfoIndex = 0;
    int weaponId = -1;
    int mode = 0;
    int shotsFired = 0;
    int clip1 = -1;
    float inaccuracy = 0.0f;
    float fireInaccuracy = 0.0f;
    float spread = 0.0f;
    float accuracyPenalty = 0.0f;
    float fireAccuracyPenalty = 0.0f;
    float decayedAccuracyPenalty = 0.0f;
    float accuracyBaseline = 0.0f;
    float accuracyRecoveryTime = 0.0f;
    float intervalPerTick = 0.0f;
    std::uintptr_t inaccuracyMethod = 0;
    std::uintptr_t spreadMethod = 0;
    std::uintptr_t accuracyModelParentSlot = 0;
    std::uintptr_t accuracyModelConVar = 0;
    int accuracyModel = -1;
    float accuracyState = 0.0f;
    bool handleReadOk = false;
    bool weaponResolved = false;
    bool weaponIdOk = false;
    bool weaponInfoIndexOk = false;
    bool modeOk = false;
    bool shotsFiredOk = false;
    bool clipOk = false;
    bool penaltyOk = false;
    bool nextPenaltyOk = false;
    bool preFireDecayOk = false;
    bool inaccuracyOk = false;
    bool fireInaccuracyOk = false;
    bool spreadOk = false;
    bool methodsOk = false;
    bool accuracyModelOk = false;
    bool accuracyStateOk = false;
    // The current CS fire path consumes the two getter values as separate
    // polar radii: inaccuracy once per shot, spread once per pellet.
    // True when both methods and the live clip state are usable.
    bool usableForCompensation = false;
};

bool GetActiveWeapon(const CCSPlayer *player, WeaponEntity &weapon,
                     std::uint32_t *handleOut = nullptr,
                     bool *handleReadOut = nullptr);
bool ReadWeaponSpreadState(const CCSPlayer *player, WeaponSpreadState &state);

// Call virtual GetInaccuracy / GetSpread (slots 382 / 383). Fail closed.
bool GetWeaponInaccuracy(WeaponEntity weapon, float &out);
bool GetWeaponSpread(WeaponEntity weapon, float &out);
bool GetWeaponMethodAddress(WeaponEntity weapon, std::size_t slot,
                            std::uintptr_t &address);
bool GetWeaponId(WeaponEntity weapon, int &out);

} // namespace game
