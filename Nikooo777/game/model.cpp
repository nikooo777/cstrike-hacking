#include "game/model.h"

#include <limits>

#include "game/interfaces.h"
#include "memory/mem.h"
#include "sdk/render_view.h"
#include "sdk/studio_model.h"

namespace game {

namespace {

bool AddDoesNotOverflow(std::uintptr_t base, std::uintptr_t offset) {
    return offset <= (std::numeric_limits<std::uintptr_t>::max)() - base;
}

bool ReadVtableFunction(std::uintptr_t objectAddress, std::size_t index,
                        std::uintptr_t &functionAddress) {
    functionAddress = 0;
    std::uintptr_t vtableAddress = 0;
    if (!mem::ReadValue(reinterpret_cast<const void *>(objectAddress),
                        vtableAddress) ||
        vtableAddress == 0 ||
        index > (std::numeric_limits<std::size_t>::max)() / sizeof(void *) ||
        !AddDoesNotOverflow(vtableAddress, index * sizeof(void *)) ||
        !mem::ReadValue(reinterpret_cast<const void *>(
                            vtableAddress + index * sizeof(void *)),
                        functionAddress)) {
        return false;
    }

    return functionAddress != 0 &&
           mem::IsExecutable(reinterpret_cast<const void *>(functionAddress));
}

bool ReadStudioInt(std::uintptr_t address, std::size_t offset,
                   std::int32_t &value) {
    if (!AddDoesNotOverflow(address, offset)) {
        return false;
    }

    return mem::ReadValue(reinterpret_cast<const void *>(address + offset),
                          value);
}

} // namespace

bool GetBoneParents(const CBasePlayer *player, std::vector<int> &parents,
                    BoneHierarchyInfo *info) {
    parents.clear();
    BoneHierarchyInfo localInfo{};
    if (player == nullptr) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    localInfo.entityAddress = reinterpret_cast<std::uintptr_t>(player);
    if (!AddDoesNotOverflow(localInfo.entityAddress,
                            sdk::render::kRenderableSubobjectOffset)) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }
    localInfo.renderableAddress =
        localInfo.entityAddress + sdk::render::kRenderableSubobjectOffset;

    std::uintptr_t getModelAddress = 0;
    if (!ReadVtableFunction(localInfo.renderableAddress,
                            sdk::render::kGetModelVtableIndex,
                            getModelAddress)) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    const void *model = nullptr;
    __try {
        model = reinterpret_cast<sdk::render::GetModelFn>(getModelAddress)(
            reinterpret_cast<void *>(localInfo.renderableAddress));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        model = nullptr;
    }
    if (model == nullptr ||
        !mem::IsReadable(model, sizeof(std::uintptr_t))) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }
    localInfo.modelAddress = reinterpret_cast<std::uintptr_t>(model);
    localInfo.modelResolved = true;

    auto *modelInfo = GetModelInfo();
    if (modelInfo == nullptr) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    std::uintptr_t getStudioModelAddress = 0;
    if (!ReadVtableFunction(reinterpret_cast<std::uintptr_t>(modelInfo),
                            sdk::render::kGetStudiomodelVtableIndex,
                            getStudioModelAddress)) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    const void *studioHeader = nullptr;
    __try {
        studioHeader = reinterpret_cast<sdk::render::GetStudiomodelFn>(
            getStudioModelAddress)(modelInfo, model);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        studioHeader = nullptr;
    }
    if (studioHeader == nullptr ||
        !mem::IsReadable(studioHeader, sdk::studio::kHeaderBoneIndexOffset +
                                      sizeof(std::int32_t))) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }
    localInfo.studioHeaderAddress =
        reinterpret_cast<std::uintptr_t>(studioHeader);
    localInfo.studioHeaderResolved = true;

    std::int32_t studioId = 0;
    std::int32_t studioLength = 0;
    std::int32_t boneCount = 0;
    std::int32_t boneIndex = 0;
    if (!ReadStudioInt(localInfo.studioHeaderAddress, 0, studioId) ||
        !ReadStudioInt(localInfo.studioHeaderAddress,
                       sdk::studio::kHeaderLengthOffset, studioLength) ||
        !ReadStudioInt(localInfo.studioHeaderAddress,
                       sdk::studio::kHeaderBoneCountOffset, boneCount) ||
        !ReadStudioInt(localInfo.studioHeaderAddress,
                       sdk::studio::kHeaderBoneIndexOffset, boneIndex) ||
        studioId != sdk::studio::kStudioHeaderId || studioLength <= 0 ||
        studioLength > sdk::studio::kMaxStudioLength || boneCount <= 0 ||
        boneCount > sdk::studio::kMaxBones || boneIndex <= 0) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    const auto boneBaseOffset = static_cast<std::uintptr_t>(boneIndex);
    const auto lastBoneOffset =
        static_cast<std::uintptr_t>(boneCount - 1) * sdk::studio::kBoneStride;
    std::uintptr_t parentDataOffset = boneBaseOffset;
    if (!AddDoesNotOverflow(parentDataOffset, lastBoneOffset) ||
        !AddDoesNotOverflow(parentDataOffset,
                            sdk::studio::kBoneParentOffset +
                                sizeof(std::int32_t)) ||
        !AddDoesNotOverflow(localInfo.studioHeaderAddress,
                            parentDataOffset)) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    const auto boneBase = localInfo.studioHeaderAddress + boneBaseOffset;
    const auto parentDataSize = lastBoneOffset +
                                sdk::studio::kBoneParentOffset +
                                sizeof(std::int32_t);
    if (!mem::IsReadable(reinterpret_cast<const void *>(boneBase),
                         parentDataSize)) {
        if (info != nullptr) {
            *info = localInfo;
        }
        return false;
    }

    parents.resize(static_cast<std::size_t>(boneCount));
    for (int bone = 0; bone < boneCount; ++bone) {
        const auto parentAddress =
            boneBase + static_cast<std::uintptr_t>(bone) *
                           sdk::studio::kBoneStride +
            sdk::studio::kBoneParentOffset;
        std::int32_t parent = -1;
        if (!mem::ReadValue(reinterpret_cast<const void *>(parentAddress),
                            parent) ||
            parent < -1 || parent >= boneCount) {
            parents.clear();
            if (info != nullptr) {
                *info = localInfo;
            }
            return false;
        }
        parents[static_cast<std::size_t>(bone)] = parent;
    }

    localInfo.boneCount = boneCount;
    localInfo.parentDataReadable = true;
    if (info != nullptr) {
        *info = localInfo;
    }
    return true;
}

} // namespace game
