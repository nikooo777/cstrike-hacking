#pragma once

#define BUTTON_DOWN 0x8000
#define ENTGAP 0x10
#define MAXPLAYERS 64

#define    FL_ONGROUND                (1<<0)    // At rest / on the ground
#define FL_DUCKING                (1<<1)    // Player is fully crouched
#define    FL_WATERJUMP            (1<<2)    // player jumping out of water
#define FL_ONTRAIN                (1<<3) // Player is _controlling_ a train, so movement commands should be ignored on client during prediction.
#define FL_INRAIN                (1<<4)    // Indicates the entity is standing in rain
#define FL_FROZEN                (1<<5) // Player is frozen for 3rd person camera
#define FL_ATCONTROLS            (1<<6) // Player can't move, but keeps key inputs for controlling another entity
#define    FL_CLIENT                (1<<7)    // Is a player
#define FL_FAKECLIENT            (1<<8)    // Fake client, simulated server side; don't send network messages to them
// NON-PLAYER SPECIFIC (i.e., not used by GameMovement or the client .dll ) -- Can still be applied to players, though
#define    FL_INWATER                (1<<9)    // In water

#define ALIVE 0

#define TEAM_UNASSIGNED 0
#define TEAM_SPEC 1
#define TEAM_T 2
#define TEAM_CT 3
