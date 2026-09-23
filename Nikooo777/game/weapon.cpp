#include "game/weapon.h"

#include <Windows.h>

#include <cmath>
#include <cstring>

#include "core/arch.h"
#include "config/config.h"
#include "core/constants.h"
#include "game/interfaces.h"
#include "game/timing.h"
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/client_offsets.h"
#include "sdk/entity/c_cs_player.h"

namespace game {

namespace {

namespace offsets = sdk::offsets;
namespace weapon_info = sdk::offsets::weapon_info;
#if ARCH_X64()
namespace accuracy_model = sdk::offsets::accuracy_model;
#endif

// C_WeaponCSBase vtable slots verified on x86 and x64 (aidocs/005).
constexpr std::size_t kGetInaccuracySlot = 382;
constexpr std::size_t kGetSpreadSlot = 383;
constexpr std::size_t kGetWeaponIdSlot = 371;

// Soft bounds for CSS-style accuracy floats (reject garbage, not exact maxes).
constexpr float kMaxPlausibleCone = 5.0f;
constexpr float kMaxPlausiblePenalty = 10.0f;
constexpr int kMaxPlausibleClip = 255;
constexpr int kMaxPlausibleShots = 255;

#if ARCH_X64()
constexpr int kUnmodeledAccuracyModel = 1;
constexpr std::uint8_t kMoveTypeLadder = 9;
#endif

using WeaponFloatFn = float(ARCH_THISCALL *)(void *thisPtr);
using WeaponIdFn = int(ARCH_THISCALL *)(void *thisPtr);
using WeaponInfoLookupFn = void *(__cdecl *)(std::uint16_t weaponId);

#if ARCH_X64()
bool ReadAccuracyModel(WeaponEntity weapon, WeaponSpreadState &state) {
    void *method = nullptr;
    if (!mem::ReadVirtual(weapon, kGetInaccuracySlot, method)) {
        return false;
    }

    std::uint8_t prologue[accuracy_model::kCompareOffset +
                          sizeof(accuracy_model::kCompare)]{};
    if (!mem::ReadBytes(method, prologue, sizeof(prologue)) ||
        std::memcmp(prologue + accuracy_model::kLoadOffset, accuracy_model::kLoad,
                    sizeof(accuracy_model::kLoad)) != 0 ||
        std::memcmp(prologue + accuracy_model::kCompareOffset,
                    accuracy_model::kCompare,
                    sizeof(accuracy_model::kCompare)) != 0) {
        return false;
    }

    const auto methodAddress = reinterpret_cast<std::uintptr_t>(method);
    if (!mem::AddDoesNotOverflow(methodAddress, accuracy_model::kLoadOffset)) {
        return false;
    }

    std::uintptr_t parentSlot = 0;
    if (!mem::DecodeRipRelative32(
            reinterpret_cast<const void *>(methodAddress +
                                           accuracy_model::kLoadOffset),
            accuracy_model::kLoadDisplacement, 0, accuracy_model::kLoadLength,
            parentSlot)) {
        return false;
    }

    std::uintptr_t conVar = 0;
    if (!mem::ReadValue(reinterpret_cast<const void *>(parentSlot), conVar) ||
        conVar == 0 ||
        !mem::AddDoesNotOverflow(conVar, accuracy_model::kConVarIntValue)) {
        return false;
    }

    int model = -1;
    if (!mem::ReadValue(
            reinterpret_cast<const void *>(conVar + accuracy_model::kConVarIntValue),
            model)) {
        return false;
    }

    state.accuracyModelParentSlot = parentSlot;
    state.accuracyModelConVar = conVar;
    state.accuracyModel = model;
    state.accuracyModelOk = true;
    return true;
}
#else
bool ReadAccuracyModel(WeaponEntity, WeaponSpreadState &) {
    return false;
}
#endif

bool CallWeaponFloat(WeaponEntity weapon, std::size_t slot, float &out) {
    out = 0.0f;
    void *method = nullptr;
    if (!mem::ReadVirtual(weapon, slot, method)) {
        return false;
    }

    __try {
        out = reinterpret_cast<WeaponFloatFn>(method)(weapon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0.0f;
        return false;
    }
    return std::isfinite(out) && out >= 0.0f && out <= kMaxPlausibleCone;
}

WeaponInfoLookupFn ResolveWeaponInfoLookup() {
    static bool attempted = false;
    static WeaponInfoLookupFn lookup = nullptr;
    if (!attempted) {
        attempted = true;
        if (config::IsLoaded()) {
            std::size_t matchCount = 0;
            auto *match = game::FindConfiguredSignature(
                config::Get().weaponInfoLookup, matchCount);
            if (match != nullptr && mem::IsExecutable(match)) {
                lookup = reinterpret_cast<WeaponInfoLookupFn>(match);
            }
        }
    }
    return lookup;
}

void *LookupWeaponInfo(int weaponId) {
    const auto lookup = ResolveWeaponInfoLookup();
    if (lookup == nullptr || weaponId < 0 || weaponId > 0xffff) {
        return nullptr;
    }

    void *info = nullptr;
    __try {
        info = lookup(static_cast<std::uint16_t>(weaponId));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    if (info == nullptr ||
        !mem::IsReadable(reinterpret_cast<const char *>(info) +
                             weapon_info::kMaxInaccuracy,
                         sizeof(float))) {
        return nullptr;
    }
    return info;
}

bool ReadWeaponInfoFloat(const void *info, std::size_t offset, float &value) {
    return info != nullptr &&
           mem::ReadValue(reinterpret_cast<const char *>(info) + offset,
                          value);
}

bool ReadWeaponInfoByte(const void *info, std::size_t offset,
                        std::uint8_t &value) {
    return info != nullptr &&
           mem::ReadValue(reinterpret_cast<const char *>(info) + offset,
                          value);
}

int NetvarOffset(const char *table, const char *property) {
    return netvars::GetOffset(table, property);
}

bool ReadNetvarInt(const void *entity, const char *table, const char *property,
                   int &value) {
    const int offset = NetvarOffset(table, property);
    if (offset < 0 || entity == nullptr) {
        return false;
    }
    return mem::ReadValue(
        reinterpret_cast<const char *>(entity) + offset, value);
}

bool ReadNetvarFloat(const void *entity, const char *table,
                     const char *property, float &value) {
    const int offset = NetvarOffset(table, property);
    if (offset < 0 || entity == nullptr) {
        return false;
    }
    return mem::ReadValue(
        reinterpret_cast<const char *>(entity) + offset, value);
}

bool PlausibleMode(int mode) {
    return mode == 0 || mode == 1;
}

bool PlausibleClip(int clip) {
    return clip >= 0 && clip <= kMaxPlausibleClip;
}

bool PlausiblePenalty(float penalty) {
    return std::isfinite(penalty) && penalty >= 0.0f &&
           penalty <= kMaxPlausiblePenalty;
}

bool PlausibleShots(int shots) {
    return shots >= 0 && shots <= kMaxPlausibleShots;
}

#if ARCH_X64()
bool PredictPreFireAccuracy(const CCSPlayer *player, void *weaponInfo,
                            WeaponSpreadState &state) {
    if (player == nullptr || weaponInfo == nullptr ||
        !state.modeOk || !state.penaltyOk || !state.inaccuracyOk ||
        !state.accuracyModelOk ||
        state.accuracyModel == kUnmodeledAccuracyModel) {
        return false;
    }

    std::uint8_t moveType = 0;
    std::uint8_t accuracyModifier = 0;
    int flags = 0;
    const auto *playerBytes = reinterpret_cast<const char *>(player);
    const auto *weaponBytes = reinterpret_cast<const char *>(state.weapon);
    if (!mem::ReadValue(playerBytes + offsets::kMoveType, moveType) ||
        !mem::ReadValue(weaponBytes + offsets::kWeaponAccuracyModifier,
                        accuracyModifier) ||
        !ReadNetvarInt(player, "DT_BasePlayer", "m_fFlags", flags)) {
        return false;
    }

    const auto rule = SelectPenaltyDecayRule(moveType == kMoveTypeLadder,
                                             (flags & FL_ONGROUND) != 0,
                                             (flags & FL_DUCKING) != 0);
    const auto modeOffset = static_cast<std::size_t>(state.mode) *
                            sizeof(float);
    const auto baselineOffset = rule.baseline == PenaltyBaseline::Crouch
                                    ? weapon_info::kCrouchInaccuracy
                                    : weapon_info::kStandInaccuracy;
    const auto recoveryOffset = rule.recovery == PenaltyRecovery::Crouch
                                    ? weapon_info::kCrouchRecovery
                                    : weapon_info::kStandRecovery;
    float baseline = 0.0f;
    float recoveryTime = 0.0f;
    if (!ReadWeaponInfoFloat(weaponInfo, baselineOffset + modeOffset,
                             baseline) ||
        !ReadWeaponInfoFloat(weaponInfo, recoveryOffset, recoveryTime)) {
        return false;
    }
    if (rule.baseline == PenaltyBaseline::StandPlusLadder) {
        float ladder = 0.0f;
        if (!ReadWeaponInfoFloat(weaponInfo,
                                 weapon_info::kLadderInaccuracy + modeOffset,
                                 ladder)) {
            return false;
        }
        baseline += ladder;
    }

    if (accuracyModifier != 0) {
        float extraBaseline = 0.0f;
        if (!ReadWeaponInfoFloat(weaponInfo,
                                 weapon_info::kAccuracyModifierBaseline,
                                 extraBaseline)) {
            return false;
        }
        baseline += extraBaseline;
    }

    float interval = 0.0f;
    float nextPenalty = 0.0f;
    if (!GetIntervalPerTick(interval) ||
        !PredictAccuracyPenaltyDecay(
            state.accuracyPenalty, baseline, recoveryTime, interval,
            rule.decayConstant, nextPenalty)) {
        return false;
    }

    const float fireInaccuracy =
        state.inaccuracy + nextPenalty - state.accuracyPenalty;
    if (!std::isfinite(fireInaccuracy) || fireInaccuracy < 0.0f ||
        fireInaccuracy > kMaxPlausibleCone) {
        return false;
    }

    state.decayedAccuracyPenalty = nextPenalty;
    state.accuracyBaseline = baseline;
    state.accuracyRecoveryTime = recoveryTime;
    state.intervalPerTick = interval;
    state.fireInaccuracy = fireInaccuracy;
    state.preFireDecayOk = true;
    state.fireInaccuracyOk = true;
    return true;
}
#endif

} // namespace

bool GetActiveWeapon(const CCSPlayer *player, WeaponEntity &weapon,
                     std::uint32_t *handleOut, bool *handleReadOut) {
    weapon = nullptr;
    if (handleOut != nullptr) {
        *handleOut = 0;
    }
    if (handleReadOut != nullptr) {
        *handleReadOut = false;
    }
    if (player == nullptr) {
        return false;
    }

    const int offset =
        NetvarOffset("DT_BaseCombatCharacter", "m_hActiveWeapon");
    if (offset < 0) {
        return false;
    }

    std::uint32_t handle = 0;
    const bool handleRead = mem::ReadValue(
        reinterpret_cast<const char *>(player) + offset, handle);
    if (handleOut != nullptr) {
        *handleOut = handle;
    }
    if (handleReadOut != nullptr) {
        *handleReadOut = handleRead;
    }
    if (!handleRead || handle == 0 || handle == 0xFFFFFFFFu) {
        return false;
    }

    auto *entityList = GetClientEntityList();
    if (entityList == nullptr ||
        !mem::IsReadable(entityList, sizeof(void *))) {
        return false;
    }

    void *entity = nullptr;
    const CBaseHandle entityHandle(handle);
    __try {
        entity = entityList->GetClientEntityFromHandle(entityHandle);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (entity == nullptr ||
        !mem::IsReadable(entity, sizeof(void *))) {
        return false;
    }

    weapon = entity;
    return true;
}

bool GetWeaponInaccuracy(WeaponEntity weapon, float &out) {
    return CallWeaponFloat(weapon, kGetInaccuracySlot, out);
}

bool GetWeaponSpread(WeaponEntity weapon, float &out) {
    return CallWeaponFloat(weapon, kGetSpreadSlot, out);
}

bool GetWeaponMethodAddress(WeaponEntity weapon, std::size_t slot,
                            std::uintptr_t &address) {
    address = 0;
    void *method = nullptr;
    if (!mem::ReadVirtual(weapon, slot, method)) {
        return false;
    }

    address = reinterpret_cast<std::uintptr_t>(method);
    return address != 0;
}

bool GetWeaponId(WeaponEntity weapon, int &out) {
    out = -1;
    void *method = nullptr;
    if (!mem::ReadVirtual(weapon, kGetWeaponIdSlot, method)) {
        return false;
    }

    int weaponId = -1;
    __try {
        weaponId = reinterpret_cast<WeaponIdFn>(method)(weapon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (weaponId < 0 || weaponId > 0xffff) {
        return false;
    }

    out = weaponId;
    return true;
}

bool ReadWeaponSpreadState(const CCSPlayer *player, WeaponSpreadState &state) {
    state = {};
    if (player == nullptr) {
        return false;
    }

    if (!GetActiveWeapon(player, state.weapon, &state.handle,
                         &state.handleReadOk)) {
        return false;
    }
    state.weaponResolved = true;
    GetWeaponMethodAddress(state.weapon, kGetInaccuracySlot,
                           state.inaccuracyMethod);
    GetWeaponMethodAddress(state.weapon, kGetSpreadSlot, state.spreadMethod);
    ReadAccuracyModel(state.weapon, state);
#if ARCH_X64()
    if (state.accuracyModelOk) {
        const auto weaponAddress = reinterpret_cast<std::uintptr_t>(state.weapon);
        if (mem::AddDoesNotOverflow(weaponAddress, offsets::kWeaponAccuracyState)) {
            state.accuracyStateOk = mem::ReadValue(
                reinterpret_cast<const void *>(weaponAddress +
                                               offsets::kWeaponAccuracyState),
                state.accuracyState);
            state.accuracyStateOk =
                state.accuracyStateOk && PlausiblePenalty(state.accuracyState);
        }
    }
#endif

    state.weaponIdOk = GetWeaponId(state.weapon, state.weaponId);

#if ARCH_X64()
    state.weaponInfoIndexOk = mem::ReadValue(
        reinterpret_cast<const char *>(state.weapon) +
            offsets::kWeaponInfoIndex,
        state.weaponInfoIndex);
#else
    if (state.weaponIdOk) {
        state.weaponInfoIndex =
            static_cast<std::uint16_t>(state.weaponId);
        state.weaponInfoIndexOk = true;
    }
#endif

    int mode = 0;
    if (ReadNetvarInt(state.weapon, "DT_WeaponCSBase", "m_weaponMode", mode) &&
        PlausibleMode(mode)) {
        state.mode = mode;
        state.modeOk = true;
    }

    int clip = -1;
    if (ReadNetvarInt(state.weapon, "DT_LocalWeaponData", "m_iClip1", clip) ||
        ReadNetvarInt(state.weapon, "DT_BaseCombatWeapon", "m_iClip1", clip)) {
        if (PlausibleClip(clip)) {
            state.clip1 = clip;
            state.clipOk = true;
        }
    }

    int shotsFired = 0;
    if (ReadNetvarInt(player, "DT_CSLocalPlayerExclusive", "m_iShotsFired",
                      shotsFired) &&
        PlausibleShots(shotsFired)) {
        state.shotsFired = shotsFired;
        state.shotsFiredOk = true;
    }

    float penalty = 0.0f;
    if (ReadNetvarFloat(state.weapon, "DT_WeaponCSBase", "m_fAccuracyPenalty",
                        penalty) &&
        PlausiblePenalty(penalty)) {
        state.accuracyPenalty = penalty;
        state.penaltyOk = true;
    }

    state.inaccuracyOk = GetWeaponInaccuracy(state.weapon, state.inaccuracy);
    state.spreadOk = GetWeaponSpread(state.weapon, state.spread);
    state.methodsOk = state.inaccuracyOk && state.spreadOk;

    void *weaponInfo = nullptr;
    if (state.weaponInfoIndexOk) {
        weaponInfo = LookupWeaponInfo(state.weaponInfoIndex);
    }

    if (state.methodsOk && state.penaltyOk && weaponInfo != nullptr &&
        state.shotsFiredOk) {
        float divisor = 0.0f;
        float offset = 0.0f;
        float maximum = 0.0f;
        std::uint8_t quadraticByte = 0;
        if (ReadWeaponInfoByte(weaponInfo, weapon_info::kAccuracyQuadratic,
                               quadraticByte) &&
            ReadWeaponInfoFloat(weaponInfo, weapon_info::kAccuracyDivisor, divisor) &&
            ReadWeaponInfoFloat(weaponInfo, weapon_info::kAccuracyOffset, offset) &&
            ReadWeaponInfoFloat(weaponInfo, weapon_info::kMaxInaccuracy, maximum) &&
            PredictNextAccuracyPenalty(
                state.shotsFired, divisor, quadraticByte != 0, offset,
                maximum, state.accuracyPenalty,
                state.fireAccuracyPenalty) &&
            PlausiblePenalty(state.fireAccuracyPenalty)) {
            state.nextPenaltyOk = true;
        }
    }

#if ARCH_X64()
    PredictPreFireAccuracy(player, weaponInfo, state);
#else
    if (state.inaccuracyOk) {
        state.fireInaccuracy = state.inaccuracy;
        state.fireInaccuracyOk = true;
    }
#endif

    state.usableForCompensation = state.fireInaccuracyOk && state.spreadOk &&
                                  state.clipOk &&
                                  state.clip1 > 0;
    return state.methodsOk;
}

} // namespace game
