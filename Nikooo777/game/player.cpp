#include "game/player.h"

#include "core/arch.h"
#include "core/constants.h"
#include "game/interfaces.h"
#include "memory/mem.h"
#include "sdk/client_offsets.h"

namespace game {

namespace {

constexpr int kMaxSupportedBones = 256;

// matrix3x4_t: three rows of four floats; the fourth column is the origin.
struct BoneMatrix {
    float m[3][4];
};
static_assert(sizeof(BoneMatrix) == 0x30);

Vector3 BoneOrigin(const BoneMatrix &matrix) {
    return Vector3{matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
}

#if ARCH_X64()
// IClientUnknown::GetClientNetworkable in the client entity's primary
// vtable. The x64 entity-list wrapper at client.dll+0xDF750 calls the same
// interface family through the +0x20 (slot 4) entry.
constexpr std::size_t kGetClientNetworkableVtableIndex = 4;
// IClientNetworkable::IsDormant, counted from the interface declaration.
constexpr std::size_t kIsDormantVtableIndex = 8;
constexpr std::size_t kFireAnglesVtableIndex = 143;
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

#if ARCH_X64()
    using GetClientNetworkableFn = void *(*)(void *);
    using IsDormantFn = bool (*)(void *);
    auto *entity = const_cast<CBasePlayer *>(player);
    const auto getClientNetworkable = mem::GetVirtual<GetClientNetworkableFn>(
        entity, kGetClientNetworkableVtableIndex);
    if (getClientNetworkable == nullptr) {
        return false;
    }

    void *networkable = nullptr;
    __try {
        networkable = getClientNetworkable(entity);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    const auto isDormant =
        mem::GetVirtual<IsDormantFn>(networkable, kIsDormantVtableIndex);
    if (isDormant == nullptr) {
        return false;
    }

    bool dormant = true;
    __try {
        dormant = isDormant(networkable);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    info.dormant = dormant;
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

#if ARCH_X64()
    using GetFireAnglesFn = const Vector3 *(*)(const CBasePlayer *);
    const auto getFireAngles =
        mem::GetVirtual<GetFireAnglesFn>(player, kFireAnglesVtableIndex);
    if (getFireAngles == nullptr) {
        return false;
    }

    const Vector3 *fireAngles = nullptr;
    __try {
        fireAngles = getFireAngles(player);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (fireAngles == nullptr || !mem::ReadValue(fireAngles, angles)) {
        return false;
    }

    return IsFinite(angles);
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
    if (!mem::AddDoesNotOverflow(entityAddress, sdk::offsets::kBoneMatrix) ||
        !mem::AddDoesNotOverflow(entityAddress, sdk::offsets::kBoneCount)) {
        return false;
    }

    const auto matrixAddress = entityAddress + sdk::offsets::kBoneMatrix;
    const auto countAddress = entityAddress + sdk::offsets::kBoneCount;
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
                            sizeof(BoneMatrix);
    BoneMatrix matrix{};
    if (!mem::AddDoesNotOverflow(cache.matrix, boneOffset) ||
        !mem::ReadValue(reinterpret_cast<const void *>(cache.matrix + boneOffset),
                        matrix)) {
        return false;
    }

    position = BoneOrigin(matrix);
    return IsFinite(position);
}

bool GetBonePositions(const CBasePlayer *player,
                      std::vector<Vector3> &positions) {
    positions.clear();
    BoneCacheInfo cache;
    if (!GetBoneCacheInfo(player, cache)) {
        return false;
    }

    std::vector<BoneMatrix> matrices(static_cast<std::size_t>(cache.count));
    const auto size = matrices.size() * sizeof(BoneMatrix);
    if (!mem::AddDoesNotOverflow(cache.matrix, size) ||
        !mem::ReadBytes(reinterpret_cast<const void *>(cache.matrix),
                        matrices.data(), size)) {
        return false;
    }

    positions.reserve(matrices.size());
    for (const auto &matrix : matrices) {
        positions.push_back(BoneOrigin(matrix));
    }
    return true;
}

} // namespace game
