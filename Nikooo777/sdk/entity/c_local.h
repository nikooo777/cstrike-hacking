#pragma once

#include "math/vector.h"
#include "core/padding.h"
#include "netvars/netvars.h"

#pragma pack(push, 1)

// CPlayerLocalData (m_Local) — embedded in CBasePlayer.
class CLocal {
public:
    union {
        DEFINE_MEMBER_N(unsigned char, m_chAreaBits[32], 0x4);
        DEFINE_MEMBER_N(unsigned char, m_chAreaPortalBits[24], 0x24);
        DEFINE_MEMBER_N(int, m_iHideHUD, 0x3C);
        DEFINE_MEMBER_N(float, m_flFOVRate, 0x40);
        DEFINE_MEMBER_N(bool, m_bDucked, 0x44);
        DEFINE_MEMBER_N(bool, m_bDucking, 0x45);
        DEFINE_MEMBER_N(bool, m_bInDuckJump, 0x46);
        DEFINE_MEMBER_N(float, m_flDucktime, 0x48);
        DEFINE_MEMBER_N(float, m_flDuckJumpTime, 0x4C);
        DEFINE_MEMBER_N(float, m_flJumpTime, 0x50);
        DEFINE_MEMBER_N(float, m_flFallVelocity, 0x58);
        // The punch-angle fields are resolved from DT_Local below. They cannot
        // live in this padding union because runtime accessors are functions.
        DEFINE_MEMBER_N(bool, m_bDrawViewmodel, 0xE4);
        DEFINE_MEMBER_N(bool, m_bWearingSuit, 0xE5);
        DEFINE_MEMBER_N(bool, m_bPoisoned, 0xE6);
        DEFINE_MEMBER_N(float, m_flStepSize, 0xE8);
        DEFINE_MEMBER_N(bool, m_bAllowAutoMovement, 0xEC);
        DEFINE_MEMBER_N(int, m_skybox3d_scale, 0xF4);
        DEFINE_MEMBER_N(Vector3, m_skybox3d_origin, 0xF8);
        DEFINE_MEMBER_N(int, m_skybox3d_area, 0x104);
        DEFINE_MEMBER_N(Vector3, m_skybox3d_fog_dirPrimary, 0x10C);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_colorPrimary, 0x118);
        DEFINE_MEMBER_N(int, m_skybox3d_fog_colorSecondary, 0x11C);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_start, 0x128);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_end, 0x12C);
        DEFINE_MEMBER_N(float, m_skybox3d_fog_maxdensity, 0x134);
        DEFINE_MEMBER_N(bool, m_skybox3d_fog_enable, 0x148);
        DEFINE_MEMBER_N(bool, m_skybox3d_fog_blend, 0x149);
        DEFINE_MEMBER_N(int, m_PlayerFog_m_hCtrl, 0x150);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_0, 0x174);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_1, 0x180);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_2, 0x18C);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_3, 0x198);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_4, 0x1A4);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_5, 0x1B0);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_6, 0x1BC);
        DEFINE_MEMBER_N(Vector3, m_audio_localSound_7, 0x1C8);
        DEFINE_MEMBER_N(int, m_audio_soundscapeIndex, 0x1D4);
        DEFINE_MEMBER_N(int, m_audio_localBits, 0x1D8);
        DEFINE_MEMBER_N(int, m_audio_ent, 0x1DC);
    };

    DEFINE_NETVAR(Vector3, m_vecPunchAngle, "DT_Local", "m_vecPunchAngle");
    DEFINE_NETVAR(Vector3, m_vecPunchAngleVel, "DT_Local", "m_vecPunchAngleVel");
};

#pragma pack(pop)
