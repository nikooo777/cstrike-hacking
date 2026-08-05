#pragma once

#include <Windows.h>

#include "math/vector.h"
#include "core/padding.h"
#include "netvars/netvars.h"
#include "sdk/entity/c_local.h"

// C_BasePlayer-ish view of a client entity (CS:S). Networked fields come from
// RecvTables; the remaining DEFINE_MEMBER fields are client-only layout data.
class CBasePlayer {
public:
    DEFINE_NETVAR(char, m_lifeState, "DT_BasePlayer", "m_lifeState");
    DEFINE_NETVAR(int, m_iHealth, "DT_BasePlayer", "m_iHealth");
    DEFINE_NETVAR(int, m_iTeamNum, "DT_BaseEntity", "m_iTeamNum");
    DEFINE_NETVAR(Vector3, m_vecViewOffset, "DT_LocalPlayerExclusive", "m_vecViewOffset");
    DEFINE_NETVAR(Vector3, m_vecVelocity, "DT_LocalPlayerExclusive", "m_vecVelocity");
    DEFINE_NETVAR(Vector3, m_vecBaseVelocity, "DT_LocalPlayerExclusive", "m_vecBaseVelocity");
    DEFINE_MEMBER(bool, m_bDormant, 0x17E);
    DEFINE_NETVAR(Vector3, m_vecOrigin, "DT_BaseEntity", "m_vecOrigin");
    DEFINE_NETVAR(Vector3, m_angRotation, "DT_BaseEntity", "m_angRotation");
    DEFINE_MEMBER(Vector3, m_vecAbsOrigin, 0x278);
    DEFINE_NETVAR(int, m_fFlags, "DT_BasePlayer", "m_fFlags");
    DEFINE_MEMBER(int, m_nForceBone, 0x560);
    DEFINE_MEMBER(DWORD, m_dwBoneMatrix, 0x578);
    DEFINE_NETVAR(CLocal, m_Local, "DT_LocalPlayerExclusive", "m_Local");
};
