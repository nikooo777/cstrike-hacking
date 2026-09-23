#pragma once

#include <cstdint>

namespace features {

struct BoneEspDiagnostics {
    bool enabled = false;
    bool viewportReady = false;
    bool viewReady = false;
    bool matrixReady = false;
    int candidates = 0;
    int hierarchies = 0;
    int projectedLines = 0;
    std::uintptr_t modelAddress = 0;
    std::uintptr_t studioHeaderAddress = 0;
    int boneCount = 0;
};

void DrawBoneEsp(float viewportX, float viewportY, float viewportWidth,
                 float viewportHeight);
BoneEspDiagnostics GetBoneEspDiagnostics();

} // namespace features
