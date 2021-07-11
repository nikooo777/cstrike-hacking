//
// Created by Niko on 7/10/2021.
//

#ifndef CSTRIKE_HAX_PLAYERBASE_H
#define CSTRIKE_HAX_PLAYERBASE_H
#define STR_MERGE_IMPL(a, b) a##b
#define STR_MERGE(a, b) STR_MERGE_IMPL(a, b)
#define MAKE_PAD(size) STR_MERGE(_pad, __COUNTER__)[size]
#define DEFINE_MEMBER_N(type, name, offset) struct {unsigned char MAKE_PAD(offset); type name;}


#define	FL_ONGROUND				(1<<0)	// At rest / on the ground
#define FL_DUCKING				(1<<1)	// Player is fully crouched
#define	FL_WATERJUMP			(1<<2)	// player jumping out of water
#define FL_ONTRAIN				(1<<3) // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_INRAIN				(1<<4)	// Indicates the entity is standing in rain
#define FL_FROZEN				(1<<5) // Player is frozen for 3rd person camera
#define FL_ATCONTROLS			(1<<6) // Player can't move, but keeps key inputs for controlling another entity
#define	FL_CLIENT				(1<<7)	// Is a player
#define FL_FAKECLIENT			(1<<8)	// Fake client, simulated server side; don't send network messages to them
// NON-PLAYER SPECIFIC (i.e., not used by GameMovement or the client .dll ) -- Can still be applied to players, though
#define	FL_INWATER				(1<<9)	// In water

#define ALIVE 0

// Created with ReClass.NET 1.2 by KN4CK3R
struct Vector3 {float x,y,z;};
class PlayerBase
{
public:
    union
    {
        //              Type     Name    Offset
        DEFINE_MEMBER_N(char, m_iLifeState, 0x0093);
        DEFINE_MEMBER_N(int, m_iHealth, 0x0094);
        DEFINE_MEMBER_N(int, m_iTeamNum, 0x009C);
        DEFINE_MEMBER_N(Vector3, m_vecVelocity, 0x00F4);
        DEFINE_MEMBER_N(Vector3, m_vecSpeed, 0x0130);
        DEFINE_MEMBER_N(bool , m_bIsDormant, 0x017E);
        DEFINE_MEMBER_N(Vector3, m_vecPos, 0x0260);
        DEFINE_MEMBER_N(Vector3, m_vecViewAngle, 0x026C);
        DEFINE_MEMBER_N(Vector3, m_vecPos2, 0x0278);
        DEFINE_MEMBER_N(int, m_iFlags, 0x0350);
        DEFINE_MEMBER_N(int, m_iArmor, 0x1498);
        DEFINE_MEMBER_N(int, m_iCrosshair, 0x14f0);
    };
}; //Size: 0x1870


#endif //CSTRIKE_HAX_PLAYERBASE_H
