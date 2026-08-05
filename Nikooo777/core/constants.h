#pragma once

#define BUTTON_DOWN 0x8000
#define MAXPLAYERS 64

#define IN_ATTACK (1 << 0)
#define IN_JUMP   (1 << 1)

#define FL_ONGROUND   (1 << 0)
#define FL_DUCKING    (1 << 1)
#define FL_WATERJUMP  (1 << 2)
#define FL_ONTRAIN    (1 << 3)
#define FL_INRAIN     (1 << 4)
#define FL_FROZEN     (1 << 5)
#define FL_ATCONTROLS (1 << 6)
#define FL_CLIENT     (1 << 7)
#define FL_FAKECLIENT (1 << 8)
#define FL_INWATER    (1 << 9)

#define ALIVE 0

#define TEAM_UNASSIGNED 0
#define TEAM_SPEC 1
#define TEAM_T 2
#define TEAM_CT 3
