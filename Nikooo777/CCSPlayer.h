#pragma once

#include "vector.h"
#include "padding.h"

#pragma pack(push, 1)

class CCSPlayer {
public:
    union {
        DEFINE_MEMBER_N(Vector3, m_vecOrigin, 0x338);
        DEFINE_MEMBER_N(int, m_bResumeZoom, 0x1400);
        DEFINE_MEMBER_N(int, m_iLastZoom, 0x1404);
        DEFINE_MEMBER_N(int, m_iPlayerState, 0x1408);
        DEFINE_MEMBER_N(int, m_bIsDefusing, 0x140c);
        DEFINE_MEMBER_N(int, m_bInBombZone, 0x140d);
        DEFINE_MEMBER_N(int, m_bInBuyZone, 0x140e);
        DEFINE_MEMBER_N(int, m_iThrowGrenadeCounter, 0x1410);
        DEFINE_MEMBER_N(int, m_iAddonBits, 0x1414);
        DEFINE_MEMBER_N(int, m_iPrimaryAddon, 0x1418);
        DEFINE_MEMBER_N(int, m_iSecondaryAddon, 0x141c);
        DEFINE_MEMBER_N(int, m_iProgressBarDuration, 0x1420);
        DEFINE_MEMBER_N(float, m_flProgressBarStartTime, 0x1424);
        DEFINE_MEMBER_N(float, m_flStamina, 0x1428);
        DEFINE_MEMBER_N(int, m_iDirection, 0x142c);
        DEFINE_MEMBER_N(int, m_iShotsFired, 0x1430);
        DEFINE_MEMBER_N(int, m_bNightVisionOn, 0x1434);
        DEFINE_MEMBER_N(int, m_bHasNightVision, 0x1435);
        DEFINE_MEMBER_N(float, m_flVelocityModifier, 0x1438);
        DEFINE_MEMBER_N(int, m_hRagdoll, 0x1440);
        DEFINE_MEMBER_N(float, m_flFlashMaxAlpha, 0x1450);
        DEFINE_MEMBER_N(float, m_flFlashDuration, 0x1454);
        DEFINE_MEMBER_N(int, m_iAccount, 0x148c);
        DEFINE_MEMBER_N(int, m_bHasHelmet, 0x1490);
        DEFINE_MEMBER_N(int, m_iClass, 0x1494);
        DEFINE_MEMBER_N(int, m_ArmorValue, 0x1498);
        DEFINE_MEMBER_N(float, m_angEyeAngles_0, 0x149c);
        DEFINE_MEMBER_N(float, m_angEyeAngles_1, 0x14a0);
        DEFINE_MEMBER_N(int, m_bHasDefuser, 0x14a8);
        DEFINE_MEMBER_N(int, m_bInHostageRescueZone, 0x14a9);
        DEFINE_MEMBER_N(Vector3, m_vecRagdollVelocity, 0x14b4);
        DEFINE_MEMBER_N(int, m_iCrosshair, 0x14f0);
        DEFINE_MEMBER_N(int, m_cycleLatch, 0x1570);
        DEFINE_MEMBER_N(bool, m_bPlayerDominated[66], 0x1578);
        DEFINE_MEMBER_N(bool, m_bPlayerDominatingMe[66], 0x15ba);
    };
};

#pragma pack(pop)
