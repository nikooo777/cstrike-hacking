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

    float zNear;
    float zFar;
    float zNearViewmodel;
    float zFarViewmodel;
    bool renderToSubrectOfLargerScreen;
    std::uint8_t renderToSubrectPadding[3];
    float aspectRatio;
    bool offCenter;
    std::uint8_t offCenterPadding[3];
    float offCenterTop;
    float offCenterBottom;
    float offCenterLeft;
    float offCenterRight;
    bool doBloomAndToneMapping;
    bool cacheFullSceneState;
    bool viewToProjectionOverride;
    float viewToProjection[4][4];
};

static_assert(offsetof(CViewSetup, origin) == 0x40);
static_assert(offsetof(CViewSetup, angles) == 0x4C);
static_assert(offsetof(CViewSetup, zNear) == 0x58);
static_assert(offsetof(CViewSetup, renderToSubrectOfLargerScreen) == 0x68);
static_assert(offsetof(CViewSetup, aspectRatio) == 0x6C);
static_assert(offsetof(CViewSetup, offCenter) == 0x70);
static_assert(offsetof(CViewSetup, doBloomAndToneMapping) == 0x84);
static_assert(offsetof(CViewSetup, viewToProjection) == 0x88);
static_assert(sizeof(CViewSetup) == 0xC8);
