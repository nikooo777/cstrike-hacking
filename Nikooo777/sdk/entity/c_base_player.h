#pragma once

#include <Windows.h>

#include "math/vector.h"
#include "core/padding.h"
#include "sdk/entity/c_local.h"

// C_BasePlayer-ish view of a client entity (CS:S). Offsets from ReClass / dumps.
// Absolute DEFINE_MEMBER accessors so CCSPlayer can inherit safely.
class CBasePlayer {
public:
    DEFINE_MEMBER(char, m_lifeState, 0x93);
    DEFINE_MEMBER(int, m_iHealth, 0x94);
    DEFINE_MEMBER(int, m_iTeamNum, 0x9C);
    DEFINE_MEMBER(Vector3, m_vecViewOffset, 0xE8);
    DEFINE_MEMBER(Vector3, m_vecVelocity, 0xF4);
    DEFINE_MEMBER(Vector3, m_vecBaseVelocity, 0x130);
    DEFINE_MEMBER(bool, m_bDormant, 0x17E);
    DEFINE_MEMBER(Vector3, m_vecOrigin, 0x260);
    DEFINE_MEMBER(Vector3, m_angRotation, 0x26C);
    DEFINE_MEMBER(Vector3, m_vecAbsOrigin, 0x278);
    DEFINE_MEMBER(int, m_fFlags, 0x350);
    DEFINE_MEMBER(int, m_nForceBone, 0x560);
    DEFINE_MEMBER(DWORD, m_dwBoneMatrix, 0x578);
    DEFINE_MEMBER(CLocal, m_Local, 0xDDC);
};
