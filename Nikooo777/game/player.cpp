#include "game/player.h"

#include <cmath>
#include <limits>

#include "core/constants.h"
#include "game/interfaces.h"
#include "memory/mem.h"

namespace game {

namespace {

constexpr int kMaxSupportedBones = 256;
constexpr std::uintptr_t kBoneMatrixStride = 0x30;

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
// IClientUnknown::GetClientNetworkable in the client entity's primary
// vtable. The x64 entity-list wrapper at client.dll+0xDF750 calls the same
// interface family through the +0x20 (slot 4) entry.
constexpr std::size_t kGetClientNetworkableVtableIndex = 4;
// IClientNetworkable::IsDormant, counted from the interface declaration.
constexpr std::size_t kIsDormantVtableIndex = 8;
constexpr std::size_t kFireAnglesVtableIndex = 143;
#endif

bool AddDoesNotOverflow(std::uintptr_t base, std::uintptr_t offset) {
    return offset <= (std::numeric_limits<std::uintptr_t>::max)() - base;
}

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
bool ReadVtableFunction(std::uintptr_t objectAddress,
                        std::size_t index,
                        std::uintptr_t &functionAddress) {
    functionAddress = 0;
    std::uintptr_t vtableAddress = 0;
    if (!mem::ReadValue(reinterpret_cast<const void *>(objectAddress),
                        vtableAddress) ||
        vtableAddress == 0 ||
        index > (std::numeric_limits<std::size_t>::max)() /
                    sizeof(std::uintptr_t) ||
        !AddDoesNotOverflow(vtableAddress,
                            index * sizeof(std::uintptr_t)) ||
        !mem::ReadValue(reinterpret_cast<const void *>(
                            vtableAddress + index * sizeof(std::uintptr_t)),
                        functionAddress)) {
        return false;
    }

    return functionAddress != 0 &&
           mem::IsExecutable(reinterpret_cast<const void *>(functionAddress));
}
#endif

} // namespace

bool IsAlive(const CBasePlayer *player) {
    return player && player->m_lifeState() == ALIVE;
}

bool IsEnemy(const CCSPlayer *local, const CCSPlayer *other) {
    if (!local || !other) {
        return false;
    }
    int team = other->m_iTeamNum();
    if (team == TEAM_UNASSIGNED || team == TEAM_SPEC) {
        return false;
    }
    return team != local->m_iTeamNum();
}

bool GetDormancyInfo(const CBasePlayer *player, DormancyInfo &info) {
    info = {};
    info.dormant = true;
    if (player == nullptr) {
        return false;
    }

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    const auto entityAddress = reinterpret_cast<std::uintptr_t>(player);
    std::uintptr_t getNetworkableAddress = 0;
    if (!ReadVtableFunction(entityAddress, kGetClientNetworkableVtableIndex,
                            getNetworkableAddress)) {
        return false;
    }

    using GetClientNetworkableFn = void *(*)(void *);
    auto getClientNetworkable = reinterpret_cast<GetClientNetworkableFn>(
        getNetworkableAddress);
    void *networkable = getClientNetworkable(const_cast<void *>(
        reinterpret_cast<const void *>(entityAddress)));
    if (networkable == nullptr) {
        return false;
    }

    std::uintptr_t isDormantAddress = 0;
    if (!ReadVtableFunction(reinterpret_cast<std::uintptr_t>(networkable),
                            kIsDormantVtableIndex, isDormantAddress)) {
        return false;
    }

    using IsDormantFn = bool (*)(void *);
    auto isDormant = reinterpret_cast<IsDormantFn>(isDormantAddress);
    info.dormant = isDormant(networkable);
    info.resolved = true;
    return true;
#else
    info.dormant = player->m_bDormant();
    info.resolved = true;
    return true;
#endif
}

bool GetLocalEyeAngles(const CBasePlayer *player, Vector3 &angles) {
    angles = {};
    if (player == nullptr) {
        return false;
    }

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    std::uintptr_t functionAddress = 0;
    if (!ReadVtableFunction(reinterpret_cast<std::uintptr_t>(player),
                            kFireAnglesVtableIndex, functionAddress)) {
        return false;
    }

    using GetFireAnglesFn = const Vector3 *(*)(const CBasePlayer *);
    auto getFireAngles = reinterpret_cast<GetFireAnglesFn>(functionAddress);
    const Vector3 *fireAngles = getFireAngles(player);
    if (fireAngles == nullptr || !mem::ReadValue(fireAngles, angles)) {
        return false;
    }

    return std::isfinite(angles.x) && std::isfinite(angles.y) &&
           std::isfinite(angles.z);
#else
    return false;
#endif
}

bool IsValidTarget(const CCSPlayer *local, const CCSPlayer *other) {
    DormancyInfo dormancy;
    return other && IsAlive(other) && GetDormancyInfo(other, dormancy) &&
           !dormancy.dormant && IsEnemy(local, other);
}

bool IsVisible(const CCSPlayer *local, const CCSPlayer *other,
               const Vector3 &targetPosition) {
    if (local == nullptr || other == nullptr) {
        return false;
    }

    float fraction = 0.0f;
    if (!TraceLine(EyePosition(local), targetPosition, local, other,
                   fraction)) {
        return false;
    }

    return fraction >= 0.999f;
}

Vector3 EyePosition(const CBasePlayer *player) {
    return player->m_vecOrigin() + player->m_vecViewOffset();
}

bool GetBoneCacheInfo(const CBasePlayer *player, BoneCacheInfo &cache) {
    cache = {};
    if (player == nullptr) {
        return false;
    }

    const auto entityAddress = reinterpret_cast<std::uintptr_t>(player);
    if (!AddDoesNotOverflow(entityAddress, CBasePlayer::kBoneMatrixOffset) ||
        !AddDoesNotOverflow(entityAddress, CBasePlayer::kBoneCountOffset)) {
        return false;
    }

    const auto matrixAddress = entityAddress + CBasePlayer::kBoneMatrixOffset;
    const auto countAddress = entityAddress + CBasePlayer::kBoneCountOffset;
    cache.entityAddress = entityAddress;
    cache.matrixReadable = mem::ReadValue(
        reinterpret_cast<const void *>(matrixAddress), cache.matrix);
    cache.countReadable = mem::ReadValue(
        reinterpret_cast<const void *>(countAddress), cache.count);

    return cache.matrixReadable && cache.countReadable &&
           cache.matrix != 0 && cache.count > 0 &&
           cache.count <= kMaxSupportedBones;
}

bool GetBonePosition(const CBasePlayer *player, int bone, Vector3 &position) {
    position = {};
    if (bone < 0) {
        return false;
    }

    BoneCacheInfo cache;
    if (!GetBoneCacheInfo(player, cache) || bone >= cache.count) {
        return false;
    }

    const auto boneOffset = static_cast<std::uintptr_t>(bone) *
                            kBoneMatrixStride;
    if (!AddDoesNotOverflow(cache.matrix, boneOffset)) {
        return false;
    }

    const auto matrixAddress = cache.matrix + boneOffset;
    if (!mem::IsReadable(reinterpret_cast<const void *>(matrixAddress),
                         kBoneMatrixStride)) {
        return false;
    }

    if (!mem::ReadValue(reinterpret_cast<const void *>(matrixAddress + 0x0C),
                        position.x) ||
        !mem::ReadValue(reinterpret_cast<const void *>(matrixAddress + 0x1C),
                        position.y) ||
        !mem::ReadValue(reinterpret_cast<const void *>(matrixAddress + 0x2C),
                        position.z)) {
        position = {};
        return false;
    }

    return std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z);
}

} // namespace game
