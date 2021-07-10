//
// Created by Niko on 7/10/2021.
//

#ifndef CSTRIKE_HAX_PLAYERBASE_H
#define CSTRIKE_HAX_PLAYERBASE_H
#define STR_MERGE_IMPL(a, b) a##b
#define STR_MERGE(a, b) STR_MERGE_IMPL(a, b)
#define MAKE_PAD(size) STR_MERGE(_pad, __COUNTER__)[size]
#define DEFINE_MEMBER_N(type, name, offset) struct {unsigned char MAKE_PAD(offset); type name;}

// Created with ReClass.NET 1.2 by KN4CK3R
struct Vector3 {float x,y,z;};
class PlayerBase
{
public:
    union
    {
        //              Type     Name    Offset
        DEFINE_MEMBER_N(int, m_iLifeState, 0x0093);
        DEFINE_MEMBER_N(int, m_iHealth, 0x0094);
        DEFINE_MEMBER_N(Vector3, m_vecVelocity, 0x00F4);
        DEFINE_MEMBER_N(Vector3, m_vecSpeed, 0x0130);
        DEFINE_MEMBER_N(bool , m_bIsDormant, 0x017E);
        DEFINE_MEMBER_N(Vector3, m_vecPos, 0x0260);
        DEFINE_MEMBER_N(Vector3, m_vecViewAngle, 0x026C);
        DEFINE_MEMBER_N(Vector3, m_vecPos2, 0x0278);
        DEFINE_MEMBER_N(int, m_iFlags, 0x0350);
        DEFINE_MEMBER_N(int, m_iArmor, 0x1498);
    };
}; //Size: 0x1870


#endif //CSTRIKE_HAX_PLAYERBASE_H
