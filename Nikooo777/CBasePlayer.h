#pragma once

#include "vector.h"
#include "padding.h"
#include "CLocal.h"

class CBasePlayer {
public:
    union {
        //              Type     Name    Offset
        DEFINE_MEMBER_N(char, m_iLifeState, 0x93);
        DEFINE_MEMBER_N(int, m_iHealth, 0x94);
        DEFINE_MEMBER_N(int, m_iTeamNum, 0x9C);
        DEFINE_MEMBER_N(Vector3, m_vecVelocity, 0xF4);
        DEFINE_MEMBER_N(Vector3, m_vecSpeed, 0x130);
        DEFINE_MEMBER_N(bool, m_bIsDormant, 0x17E);
        DEFINE_MEMBER_N(Vector3, m_vecPos, 0x260);
        DEFINE_MEMBER_N(Vector3, m_vecViewAngle, 0x26C);
        DEFINE_MEMBER_N(Vector3, m_vecPos2, 0x278);
        DEFINE_MEMBER_N(int, m_iFlags, 0x350);
        DEFINE_MEMBER_N(CLocal, m_Local, 0xddc);
    };
};