#pragma once

#include "sdk/entity/c_base_player.h"
#include "math/vector.h"
#include "core/padding.h"

// CS:S-specific player fields. Same client entity pointer as CBasePlayer.
class CCSPlayer : public CBasePlayer {
public:
    DEFINE_MEMBER(int, m_bResumeZoom, 0x1400);
    DEFINE_MEMBER(int, m_iLastZoom, 0x1404);
    DEFINE_MEMBER(int, m_iPlayerState, 0x1408);
    DEFINE_MEMBER(int, m_bIsDefusing, 0x140C);
    DEFINE_MEMBER(int, m_bInBombZone, 0x140D);
    DEFINE_MEMBER(int, m_bInBuyZone, 0x140E);
    DEFINE_MEMBER(int, m_iThrowGrenadeCounter, 0x1410);
    DEFINE_MEMBER(int, m_iAddonBits, 0x1414);
    DEFINE_MEMBER(int, m_iPrimaryAddon, 0x1418);
    DEFINE_MEMBER(int, m_iSecondaryAddon, 0x141C);
    DEFINE_MEMBER(int, m_iProgressBarDuration, 0x1420);
    DEFINE_MEMBER(float, m_flProgressBarStartTime, 0x1424);
    DEFINE_MEMBER(float, m_flStamina, 0x1428);
    DEFINE_MEMBER(int, m_iDirection, 0x142C);
    DEFINE_NETVAR(int, m_iShotsFired, "DT_CSLocalPlayerExclusive", "m_iShotsFired");
    DEFINE_MEMBER(int, m_bNightVisionOn, 0x1434);
    DEFINE_MEMBER(int, m_bHasNightVision, 0x1435);
    DEFINE_MEMBER(float, m_flVelocityModifier, 0x1438);
    DEFINE_MEMBER(int, m_hRagdoll, 0x1440);
    DEFINE_MEMBER(float, m_flFlashMaxAlpha, 0x1450);
    DEFINE_MEMBER(float, m_flFlashDuration, 0x1454);
    DEFINE_MEMBER(int, m_iAccount, 0x148C);
    DEFINE_MEMBER(int, m_bHasHelmet, 0x1490);
    DEFINE_MEMBER(int, m_iClass, 0x1494);
    DEFINE_MEMBER(int, m_ArmorValue, 0x1498);
    DEFINE_MEMBER(float, m_angEyeAngles_0, 0x149C);
    DEFINE_MEMBER(float, m_angEyeAngles_1, 0x14A0);
    DEFINE_MEMBER(int, m_bHasDefuser, 0x14A8);
    DEFINE_MEMBER(int, m_bInHostageRescueZone, 0x14A9);
    DEFINE_MEMBER(Vector3, m_vecRagdollVelocity, 0x14B4);
#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    // C_CSPlayer::GetIDTarget reads the client-only current target here in
    // the current x64 client. It is not a RecvProp, so netvar lookup cannot
    // discover it.
    DEFINE_MEMBER(int, m_iIDEntIndex, 0x1B20);
    int m_iCrosshairID() const { return m_iIDEntIndex(); }
#else
    // No RecvProp in the x86 client sample; verified client-only access.
    DEFINE_MEMBER(int, m_iCrosshairID, 0x14F0);
#endif
    DEFINE_MEMBER(int, m_cycleLatch, 0x1570);

    bool *m_bPlayerDominated() {
        return reinterpret_cast<bool *>(reinterpret_cast<std::uintptr_t>(this) + 0x1578);
    }

    bool *m_bPlayerDominatingMe() {
        return reinterpret_cast<bool *>(reinterpret_cast<std::uintptr_t>(this) + 0x15BA);
    }
};
