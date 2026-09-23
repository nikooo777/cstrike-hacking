#include "game/model.h"

#include <cstring>

#include "game/interfaces.h"
#include "memory/mem.h"
#include "sdk/render_view.h"
#include "sdk/studio_model.h"

namespace game {

namespace {

const void *CallGetModel(sdk::render::GetModelFn getModel, void *renderable) {
    __try {
        return getModel(renderable);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

const void *CallGetStudiomodel(sdk::render::GetStudiomodelFn getStudiomodel,
                               void *modelInfo, const void *model) {
    __try {
        return getStudiomodel(modelInfo, model);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool ReadStudioInt(std::uintptr_t address, std::size_t offset,
                   std::int32_t &value) {
    return mem::AddDoesNotOverflow(address, offset) &&
           mem::ReadValue(reinterpret_cast<const void *>(address + offset),
                          value);
}

bool ResolveStudioHeader(const CBasePlayer *player, BoneHierarchyInfo &info) {
    info.entityAddress = reinterpret_cast<std::uintptr_t>(player);
    if (!mem::AddDoesNotOverflow(info.entityAddress,
                                 sdk::render::kRenderableSubobjectOffset)) {
        return false;
    }
    info.renderableAddress =
        info.entityAddress + sdk::render::kRenderableSubobjectOffset;
    auto *renderable = reinterpret_cast<void *>(info.renderableAddress);

    const auto getModel = mem::GetVirtual<sdk::render::GetModelFn>(
        renderable, sdk::render::kGetModelVtableIndex);
    if (getModel == nullptr) {
        return false;
    }

    const void *model = CallGetModel(getModel, renderable);
    if (model == nullptr ||
        !mem::IsReadable(model, sizeof(std::uintptr_t))) {
        return false;
    }
    info.modelAddress = reinterpret_cast<std::uintptr_t>(model);
    info.modelResolved = true;

    auto *modelInfo = GetModelInfo();
    const auto getStudiomodel = mem::GetVirtual<sdk::render::GetStudiomodelFn>(
        modelInfo, sdk::render::kGetStudiomodelVtableIndex);
    if (getStudiomodel == nullptr) {
        return false;
    }

    const void *studioHeader =
        CallGetStudiomodel(getStudiomodel, modelInfo, model);
    if (studioHeader == nullptr ||
        !mem::IsReadable(studioHeader, sdk::studio::kHeaderBoneIndexOffset +
                                           sizeof(std::int32_t))) {
        return false;
    }
    info.studioHeaderAddress = reinterpret_cast<std::uintptr_t>(studioHeader);
    info.studioHeaderResolved = true;
    return true;
}

bool ReadParents(BoneHierarchyInfo &info, std::vector<int> &parents) {
    std::int32_t studioId = 0;
    std::int32_t studioLength = 0;
    std::int32_t boneCount = 0;
    std::int32_t boneIndex = 0;
    if (!ReadStudioInt(info.studioHeaderAddress, 0, studioId) ||
        !ReadStudioInt(info.studioHeaderAddress,
                       sdk::studio::kHeaderLengthOffset, studioLength) ||
        !ReadStudioInt(info.studioHeaderAddress,
                       sdk::studio::kHeaderBoneCountOffset, boneCount) ||
        !ReadStudioInt(info.studioHeaderAddress,
                       sdk::studio::kHeaderBoneIndexOffset, boneIndex) ||
        studioId != sdk::studio::kStudioHeaderId || studioLength <= 0 ||
        studioLength > sdk::studio::kMaxStudioLength || boneCount <= 0 ||
        boneCount > sdk::studio::kMaxBones || boneIndex <= 0) {
        return false;
    }

    const auto boneRecordsSize =
        static_cast<std::size_t>(boneCount - 1) * sdk::studio::kBoneStride +
        sdk::studio::kBoneParentOffset + sizeof(std::int32_t);
    const auto boneIndexOffset = static_cast<std::uintptr_t>(boneIndex);
    if (!mem::AddDoesNotOverflow(info.studioHeaderAddress, boneIndexOffset) ||
        !mem::AddDoesNotOverflow(info.studioHeaderAddress + boneIndexOffset,
                                 boneRecordsSize)) {
        return false;
    }

    std::vector<std::uint8_t> boneRecords(boneRecordsSize);
    if (!mem::ReadBytes(reinterpret_cast<const void *>(
                            info.studioHeaderAddress + boneIndexOffset),
                        boneRecords.data(), boneRecords.size())) {
        return false;
    }

    parents.resize(static_cast<std::size_t>(boneCount));
    for (int bone = 0; bone < boneCount; ++bone) {
        std::int32_t parent = -1;
        std::memcpy(&parent,
                    boneRecords.data() +
                        static_cast<std::size_t>(bone) *
                            sdk::studio::kBoneStride +
                        sdk::studio::kBoneParentOffset,
                    sizeof(parent));
        if (parent < -1 || parent >= boneCount) {
            return false;
        }
        parents[static_cast<std::size_t>(bone)] = parent;
    }

    info.boneCount = boneCount;
    info.parentDataReadable = true;
    return true;
}

} // namespace

bool GetBoneParents(const CBasePlayer *player, std::vector<int> &parents,
                    BoneHierarchyInfo *info) {
    parents.clear();
    BoneHierarchyInfo localInfo{};
    const bool resolved = player != nullptr &&
                          ResolveStudioHeader(player, localInfo) &&
                          ReadParents(localInfo, parents);
    if (!resolved) {
        parents.clear();
    }
    if (info != nullptr) {
        *info = localInfo;
    }
    return resolved;
}

} // namespace game
