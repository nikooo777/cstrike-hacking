#pragma once

#include "vector.h"
#include "padding.h"

#pragma pack(push, 1)

class CLocal {
public:
    union {
        DEFINE_MEMBER_N(bool, m_chAreaBits[32], 0x4);
        DEFINE_MEMBER_N(bool, m_chAreaPortalBits[24], 0x24);
        DEFINE_MEMBER_N(int, m_iHideHUD, 0x3c);
        DEFINE_MEMBER_N(float, m_flFOVRate, 0x40);
        DEFINE_MEMBER_N(int, m_bDucked, 0x44);
        DEFINE_MEMBER_N(int, m_bDucking, 0x45);
        DEFINE_MEMBER_N(int, m_bInDuckJump, 0x46);
        DEFINE_MEMBER_N(float, m_flDucktime, 0x48);
        DEFINE_MEMBER_N(float, m_flDuckJumpTime, 0x4c);
        DEFINE_MEMBER_N(float, m_flJumpTime, 0x50);
        DEFINE_MEMBER_N(float, m_flFallVelocity, 0x58);
        DEFINE_MEMBER_N(Vector3, m_vecPunchAngle, 0x6c);
        DEFINE_MEMBER_N(Vector3, m_vecPunchAngleVel, 0xa8);
        DEFINE_MEMBER_N(int, m_bDrawViewmodel, 0xe4);
        DEFINE_MEMBER_N(int, m_bWearingSuit, 0xe5);
        DEFINE_MEMBER_N(int, m_bPoisoned, 0xe6);
        DEFINE_MEMBER_N(float, m_flStepSize, 0xe8);
        DEFINE_MEMBER_N(int, m_bAllowAutoMovement, 0xec);
        DEFINE_MEMBER_N(int, m_skybox3d_scale, 0xf4);
        DEFINE_MEMBER_N(Vector3, m_skybox3d_origin, 0xf8);
        DEFINE_MEMBER_N(int, m_skybox3d_area, 0x104);
        DEFINE_MEMBER_N(Vector3, m_skybox3d_fog_dirPrimary, 0x10c);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_colorPrimary, 0x118);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_colorSecondary, 0x11c);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_start, 0x128);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_end, 0x12c);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_maxdensity, 0x134);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_enable, 0x148);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_blend, 0x149);
        DEFINE_MEMBER_N(int, m_PlayerFog_m_hCtrl, 0x150);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_0, 0x174);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_1, 0x180);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_2, 0x18c);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_3, 0x198);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_4, 0x1a4);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_5, 0x1b0);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_6, 0x1bc);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_7, 0x1c8);
        DEFINE_MEMBER_N(int, m_audio_soundscapeIndex, 0x1d4);
        DEFINE_MEMBER_N(int, m_audio_localBits, 0x1d8);
        DEFINE_MEMBER_N(int, m_audio_ent, 0x1dc);
    };
};

#pragma pack(pop)
