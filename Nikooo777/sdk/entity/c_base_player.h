#pragma once

#include <Windows.h>
#include <cstdint>

#include "math/vector.h"
#include "core/padding.h"
#include "netvars/netvars.h"
#include "sdk/entity/c_local.h"

// C_BasePlayer-ish view of a client entity (CS:S). Networked fields come from
// RecvTables; the remaining DEFINE_MEMBER fields are client-only layout data.
class CBasePlayer {
public:
#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
    // SetupBones receives the IClientRenderable subobject (entity + 0x8).
    // Its [this + 0xB40]/[this + 0xB50] accesses therefore become these
    // offsets when starting from the IClientEntity pointer returned by the
    // entity list.
    static constexpr std::uintptr_t kBoneMatrixOffset = 0xB48;
    static constexpr std::uintptr_t kBoneCountOffset = 0xB58;
#else
    static constexpr std::uintptr_t kBoneMatrixOffset = 0x578;
    static constexpr std::uintptr_t kBoneCountOffset = 0x57C;
#endif

    DEFINE_NETVAR(char, m_lifeState, "DT_BasePlayer", "m_lifeState");
    DEFINE_NETVAR(int, m_iHealth, "DT_BasePlayer", "m_iHealth");
    DEFINE_NETVAR(int, m_iTeamNum, "DT_BaseEntity", "m_iTeamNum");
    DEFINE_NETVAR(Vector3, m_vecViewOffset, "DT_LocalPlayerExclusive", "m_vecViewOffset");
    DEFINE_NETVAR(Vector3, m_vecVelocity, "DT_LocalPlayerExclusive", "m_vecVelocity");
    DEFINE_NETVAR(Vector3, m_vecBaseVelocity, "DT_LocalPlayerExclusive", "m_vecBaseVelocity");
#if !defined(_WIN64) && !defined(_M_X64) && !defined(__x86_64__)
    DEFINE_MEMBER(bool, m_bDormant, 0x17E);
#endif
    DEFINE_NETVAR(Vector3, m_vecOrigin, "DT_BaseEntity", "m_vecOrigin");
    DEFINE_NETVAR(Vector3, m_angRotation, "DT_BaseEntity", "m_angRotation");
    DEFINE_MEMBER(Vector3, m_vecAbsOrigin, 0x278);
    DEFINE_NETVAR(int, m_fFlags, "DT_BasePlayer", "m_fFlags");
#if !defined(_WIN64) && !defined(_M_X64) && !defined(__x86_64__)
    DEFINE_MEMBER(int, m_nForceBone, 0x560);
#endif
    DEFINE_MEMBER(std::uintptr_t, m_dwBoneMatrix, kBoneMatrixOffset);
    DEFINE_MEMBER(int, m_nBoneCount, kBoneCountOffset);
    DEFINE_NETVAR(CLocal, m_Local, "DT_LocalPlayerExclusive", "m_Local");
};
