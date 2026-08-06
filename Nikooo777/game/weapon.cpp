#include "game/weapon.h"

#include <Windows.h>

#include <cmath>
#include <limits>

#include "game/interfaces.h"
#include "memory/mem.h"
#include "netvars/netvars.h"
#include "sdk/entity/c_cs_player.h"

namespace game {

namespace {

// C_WeaponCSBase vtable slots verified on x86 and x64 (aidocs/005).
constexpr int kGetInaccuracySlot = 382;
constexpr int kGetSpreadSlot = 383;

// Soft bounds for CSS-style accuracy floats (reject garbage, not exact maxes).
constexpr float kMaxPlausibleCone = 5.0f;
constexpr float kMaxPlausiblePenalty = 10.0f;
constexpr int kMaxPlausibleClip = 255;

#if defined(_M_IX86) || defined(__i386__)
using WeaponFloatFn = float(__thiscall *)(void *thisPtr);
#else
using WeaponFloatFn = float (*)(void *thisPtr);
#endif

bool CallWeaponFloat(WeaponEntity weapon, int slot, float &out) {
    out = 0.0f;
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

    void *method = nullptr;
    const auto methodAddress = vtableAddress + index * sizeof(void *);
    if (!mem::ReadValue(reinterpret_cast<const void *>(methodAddress),
                        method) ||
        method == nullptr || !mem::IsExecutable(method)) {
        return false;
    }

    // The entity list can hand us an object during a weapon swap/destruction
    // window. The executable-slot check avoids ordinary bad pointers, but it
    // cannot make a concurrent client object lifetime safe. Keep this probe
    // fail-closed instead of taking down the host process.
    __try {
        out = reinterpret_cast<WeaponFloatFn>(method)(weapon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0.0f;
        return false;
    }
    return std::isfinite(out) && out >= 0.0f && out <= kMaxPlausibleCone;
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
    // Compensation requires live radii; clip must be known and non-empty.
    state.usableForCompensation =
        state.methodsOk && state.clipOk && state.clip1 > 0;
    return state.methodsOk;
}

} // namespace game
