#pragma once

#include <cstddef>
#include <cstdint>

#include "math/vector.h"

struct CViewSetup {
    int x;
    int unscaledX;
    int y;
    int unscaledY;
    int width;
    int unscaledWidth;
    int height;
    int stereoEye;
    int unscaledHeight;
    bool ortho;
    std::uint8_t orthoPadding[3];
    float orthoLeft;
    float orthoTop;
    float orthoRight;
    float orthoBottom;
    float fov;
    float fovViewmodel;
    Vector3 origin;
    Vector3 angles;
};

static_assert(offsetof(CViewSetup, origin) == 0x40);
static_assert(offsetof(CViewSetup, angles) == 0x4C);
