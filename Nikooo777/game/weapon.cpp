#include "game/weapon.h"

#include <Windows.h>

#include <cmath>
#include <limits>

#include "config/config.h"
#include "core/constants.h"
#include "game/interfaces.h"
#include "game/timing.h"
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/entity/c_cs_player.h"

namespace game {

namespace {

// C_WeaponCSBase vtable slots verified on x86 and x64 (aidocs/005).
constexpr int kGetInaccuracySlot = 382;
constexpr int kGetSpreadSlot = 383;
constexpr int kGetWeaponIdSlot = 371;

// Soft bounds for CSS-style accuracy floats (reject garbage, not exact maxes).
constexpr float kMaxPlausibleCone = 5.0f;
constexpr float kMaxPlausiblePenalty = 10.0f;
constexpr int kMaxPlausibleClip = 255;
constexpr int kMaxPlausibleShots = 255;

#if defined(_M_IX86) || defined(__i386__)
constexpr std::size_t kAccuracyQuadraticOffset = 0x89c;
constexpr std::size_t kAccuracyDivisorOffset = 0x8a0;
constexpr std::size_t kAccuracyOffset = 0x8a4;
constexpr std::size_t kAccuracyMaxOffset = 0x8a8;
#else
constexpr std::size_t kAccuracyQuadraticOffset = 0x8cc;
constexpr std::size_t kAccuracyDivisorOffset = 0x8d0;
constexpr std::size_t kAccuracyOffset = 0x8d4;
constexpr std::size_t kAccuracyMaxOffset = 0x8d8;
#endif

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
constexpr std::size_t kAccuracyStateOffset = 0xca8;
constexpr std::size_t kWeaponInfoIndexOffset = 0xc62;
constexpr std::size_t kAccuracyBranchObjectOffset = 0x58;
constexpr std::size_t kMoveTypeOffset = 0x1f4;
constexpr std::size_t kAccuracyModifierOffset = 0xbf4;
constexpr std::size_t kCrouchInaccuracyOffset = 0x8e4;
constexpr std::size_t kStandingInaccuracyOffset = 0x8ec;
constexpr std::size_t kLadderInaccuracyOffset = 0x904;
constexpr std::size_t kStandingRecoveryOffset = 0x91c;
constexpr std::size_t kCrouchRecoveryOffset = 0x920;
constexpr std::size_t kAccuracyModifierInfoOffset = 0x924;
constexpr std::uint8_t kMoveTypeLadder = 9;
constexpr float kAirborneDecayConstant = -0.7675284f;
constexpr float kGroundDecayConstant = -2.3025851f;
#endif

#if defined(_M_IX86) || defined(__i386__)
using WeaponFloatFn = float(__thiscall *)(void *thisPtr);
using WeaponIdFn = int(__thiscall *)(void *thisPtr);
using WeaponInfoLookupFn = void *(__cdecl *)(std::uint16_t weaponId);
#else
using WeaponFloatFn = float (*)(void *thisPtr);
using WeaponIdFn = int (*)(void *thisPtr);
using WeaponInfoLookupFn = void *(*)(std::uint16_t weaponId);
#endif

bool ReadWeaponMethod(WeaponEntity weapon, int slot, void *&method) {
    method = nullptr;
    if (weapon == nullptr) {
        return false;
    }

    auto *vtable = mem::ReadPointer<void>(weapon);
    if (vtable == nullptr) {
        return false;
    }

    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    const auto index = static_cast<std::size_t>(slot);
    if (index > ((std::numeric_limits<std::uintptr_t>::max)() -
                 vtableAddress) / sizeof(void *)) {
        return false;
    }

    const auto methodAddress = vtableAddress + index * sizeof(void *);
    if (!mem::ReadValue(reinterpret_cast<const void *>(methodAddress),
                        method) ||
        method == nullptr || !mem::IsExecutable(method)) {
        return false;
    }

    return true;
}

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
bool ReadAccuracyBranchState(WeaponEntity weapon, WeaponSpreadState &state) {
    void *method = nullptr;
    if (!ReadWeaponMethod(weapon, kGetInaccuracySlot, method)) {
        return false;
    }

    std::uint8_t bytes[0x14]{};
    if (!mem::ReadBytes(method, bytes, sizeof(bytes)) ||
        bytes[0x6] != 0x48 || bytes[0x7] != 0x8b || bytes[0x8] != 0x05 ||
        bytes[0x10] != 0x83 || bytes[0x11] != 0x78 ||
        bytes[0x12] != 0x58 || bytes[0x13] != 0x01) {
        return false;
    }

    const auto methodAddress = reinterpret_cast<std::uintptr_t>(method);
    if (methodAddress > (std::numeric_limits<std::uintptr_t>::max)() - 0x6) {
        return false;
    }

    std::uintptr_t branchSlot = 0;
    if (!mem::DecodeRipRelative32(
            reinterpret_cast<const void *>(methodAddress + 0x6), 0x3, 0, 0x7,
            branchSlot)) {
        return false;
    }

    std::uintptr_t branchObject = 0;
    if (!mem::ReadValue(reinterpret_cast<const void *>(branchSlot),
                        branchObject) ||
        branchObject == 0 ||
        branchObject > (std::numeric_limits<std::uintptr_t>::max)() -
                           kAccuracyBranchObjectOffset) {
        return false;
    }

    int branchValue = -1;
    if (!mem::ReadValue(reinterpret_cast<const void *>(
                            branchObject + kAccuracyBranchObjectOffset),
                        branchValue)) {
        return false;
    }

    state.accuracyBranchSlot = branchSlot;
    state.accuracyBranchObject = branchObject;
    state.accuracyBranchValue = branchValue;
    state.accuracyBranchOk = true;
    return true;
}
#else
bool ReadAccuracyBranchState(WeaponEntity, WeaponSpreadState &) {
    return false;
}
#endif

bool CallWeaponFloat(WeaponEntity weapon, int slot, float &out) {
    out = 0.0f;
    void *method = nullptr;
    if (!ReadWeaponMethod(weapon, slot, method)) {
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
                             kAccuracyMaxOffset,
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

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
bool PredictPreFireAccuracy(const CCSPlayer *player, void *weaponInfo,
                            WeaponSpreadState &state) {
    if (player == nullptr || weaponInfo == nullptr ||
        !state.modeOk || !state.penaltyOk || !state.inaccuracyOk ||
        !state.accuracyBranchOk || state.accuracyBranchValue == 1) {
        return false;
    }

    std::uint8_t moveType = 0;
    std::uint8_t accuracyModifier = 0;
    int flags = 0;
    const auto *playerBytes = reinterpret_cast<const char *>(player);
    const auto *weaponBytes = reinterpret_cast<const char *>(state.weapon);
    if (!mem::ReadValue(playerBytes + kMoveTypeOffset, moveType) ||
        !mem::ReadValue(weaponBytes + kAccuracyModifierOffset,
                        accuracyModifier) ||
        !ReadNetvarInt(player, "DT_BasePlayer", "m_fFlags", flags)) {
        return false;
    }

    const auto modeOffset = static_cast<std::size_t>(state.mode) *
                            sizeof(float);
    float baseline = 0.0f;
    float recoveryTime = 0.0f;
    float decayConstant = kGroundDecayConstant;
    if (moveType == kMoveTypeLadder) {
        float ladder = 0.0f;
        if (!ReadWeaponInfoFloat(
                weaponInfo, kStandingInaccuracyOffset + modeOffset,
                baseline) ||
            !ReadWeaponInfoFloat(
                weaponInfo, kLadderInaccuracyOffset + modeOffset, ladder) ||
            !ReadWeaponInfoFloat(weaponInfo, kStandingRecoveryOffset,
                                 recoveryTime)) {
            return false;
        }
        baseline += ladder;
    } else if ((flags & FL_DUCKING) != 0) {
        if (!ReadWeaponInfoFloat(
                weaponInfo, kCrouchInaccuracyOffset + modeOffset,
                baseline)) {
            return false;
        }
    } else if (!ReadWeaponInfoFloat(
                   weaponInfo, kStandingInaccuracyOffset + modeOffset,
                   baseline)) {
        return false;
    }

    if (moveType != kMoveTypeLadder && (flags & FL_ONGROUND) == 0) {
        decayConstant = kAirborneDecayConstant;
        if (!ReadWeaponInfoFloat(weaponInfo, kCrouchRecoveryOffset,
                                 recoveryTime)) {
            return false;
        }
    } else if (moveType != kMoveTypeLadder &&
               (flags & FL_DUCKING) != 0) {
        if (!ReadWeaponInfoFloat(weaponInfo, kCrouchRecoveryOffset,
                                 recoveryTime)) {
            return false;
        }
    } else if (moveType != kMoveTypeLadder &&
               !ReadWeaponInfoFloat(weaponInfo, kStandingRecoveryOffset,
                                    recoveryTime)) {
        return false;
    }

    if (accuracyModifier != 0) {
        float extraBaseline = 0.0f;
        if (!ReadWeaponInfoFloat(weaponInfo,
                                 kAccuracyModifierInfoOffset,
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
            decayConstant, nextPenalty)) {
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

bool GetWeaponMethodAddress(WeaponEntity weapon, int slot,
                            std::uintptr_t &address) {
    address = 0;
    void *method = nullptr;
    if (!ReadWeaponMethod(weapon, slot, method)) {
        return false;
    }

    address = reinterpret_cast<std::uintptr_t>(method);
    return address != 0;
}

bool GetWeaponId(WeaponEntity weapon, int &out) {
    out = -1;
    void *method = nullptr;
    if (!ReadWeaponMethod(weapon, kGetWeaponIdSlot, method)) {
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
    ReadAccuracyBranchState(state.weapon, state);
#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    if (state.accuracyBranchOk) {
        const auto weaponAddress = reinterpret_cast<std::uintptr_t>(state.weapon);
        if (weaponAddress <= (std::numeric_limits<std::uintptr_t>::max)() -
                                 kAccuracyStateOffset) {
            state.accuracyStateOk = mem::ReadValue(
                reinterpret_cast<const void *>(weaponAddress +
                                               kAccuracyStateOffset),
                state.accuracyState);
            state.accuracyStateOk =
                state.accuracyStateOk && PlausiblePenalty(state.accuracyState);
        }
    }
#endif

    state.weaponIdOk = GetWeaponId(state.weapon, state.weaponId);

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    state.weaponInfoIndexOk = mem::ReadValue(
        reinterpret_cast<const char *>(state.weapon) +
            kWeaponInfoIndexOffset,
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
        if (ReadWeaponInfoByte(weaponInfo, kAccuracyQuadraticOffset,
                               quadraticByte) &&
            ReadWeaponInfoFloat(weaponInfo, kAccuracyDivisorOffset, divisor) &&
            ReadWeaponInfoFloat(weaponInfo, kAccuracyOffset, offset) &&
            ReadWeaponInfoFloat(weaponInfo, kAccuracyMaxOffset, maximum) &&
            PredictNextAccuracyPenalty(
                state.shotsFired, divisor, quadraticByte != 0, offset,
                maximum, state.accuracyPenalty,
                state.fireAccuracyPenalty) &&
            PlausiblePenalty(state.fireAccuracyPenalty)) {
            state.nextPenaltyOk = true;
        }
    }

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
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
