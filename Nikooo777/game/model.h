#pragma once

#include <cstdint>
#include <vector>

#include "sdk/entity/c_base_player.h"

namespace game {

struct BoneHierarchyInfo {
    std::uintptr_t entityAddress = 0;
    std::uintptr_t renderableAddress = 0;
    std::uintptr_t modelAddress = 0;
    std::uintptr_t studioHeaderAddress = 0;
    int boneCount = 0;
    bool modelResolved = false;
    bool studioHeaderResolved = false;
    bool parentDataReadable = false;
};

bool GetBoneParents(const CBasePlayer *player, std::vector<int> &parents,
                    BoneHierarchyInfo *info = nullptr);

} // namespace game
