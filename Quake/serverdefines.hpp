#pragma once

#include "vr_macros.hpp"

// edict->movetype values
#define MOVETYPE_NONE 0 // never moves
#define MOVETYPE_ANGLENOCLIP 1
#define MOVETYPE_ANGLECLIP 2
#define MOVETYPE_WALK 3 // gravity
#define MOVETYPE_STEP 4 // gravity, special edge handling
#define MOVETYPE_FLY 5
#define MOVETYPE_TOSS 6 // gravity
#define MOVETYPE_PUSH 7 // no clip to world, push and crush
#define MOVETYPE_NOCLIP 8
#define MOVETYPE_FLYMISSILE 9 // extra size to monsters
#define MOVETYPE_BOUNCE 10
//#define MOVETYPE_EXT_BOUNCEMISSILE 11
#define MOVETYPE_EXT_FOLLOW 12

// edict->solid values
#define SOLID_NOT 0               // no interaction with other objects
#define SOLID_TRIGGER 1           // touch on edge, but not blocking
#define SOLID_BBOX 2              // touch on edge, block
#define SOLID_SLIDEBOX 3          // touch on edge, but not an onground
#define SOLID_BSP 4               // bsp clip, touch on edge, block
#define SOLID_NOT_BUT_TOUCHABLE 5 // not solid, but can be [hand]touched
#define SOLID_EXT_CORPSE \
    6 // passes through slidebox+other corpses, but not bsp/bbox/triggers

// edict->deadflag values
#define DEAD_NO 0
#define DEAD_DYING 1
#define DEAD_DEAD 2

#define DAMAGE_NO 0
#define DAMAGE_YES 1
#define DAMAGE_AIM 2

// edict->flags
// clang-format off
#define FL_FLY            VRUTIL_POWER_OF_TWO(0)
#define FL_SWIM           VRUTIL_POWER_OF_TWO(1)
#define FL_CONVEYOR       VRUTIL_POWER_OF_TWO(2)
#define FL_CLIENT         VRUTIL_POWER_OF_TWO(3)
#define FL_INWATER        VRUTIL_POWER_OF_TWO(4)
#define FL_MONSTER        VRUTIL_POWER_OF_TWO(5)
#define FL_GODMODE        VRUTIL_POWER_OF_TWO(6)
#define FL_NOTARGET       VRUTIL_POWER_OF_TWO(7)
#define FL_ITEM           VRUTIL_POWER_OF_TWO(8)
#define FL_ONGROUND       VRUTIL_POWER_OF_TWO(9)
#define FL_PARTIALGROUND  VRUTIL_POWER_OF_TWO(10) // not all corners are valid
#define FL_WATERJUMP      VRUTIL_POWER_OF_TWO(11) // player jumping out of water
#define FL_JUMPRELEASED   VRUTIL_POWER_OF_TWO(12) // for jump debouncing
#define FL_EASYHANDTOUCH  VRUTIL_POWER_OF_TWO(13) // adds bonus to boundaries for handtouch
#define FL_SPECIFICDAMAGE VRUTIL_POWER_OF_TWO(14) // HONEY.
#define FL_FORCEGRABBABLE VRUTIL_POWER_OF_TWO(15) // VR.
// clang-format on

#define SPAWNFLAG_NOT_EASY 256
#define SPAWNFLAG_NOT_MEDIUM 512
#define SPAWNFLAG_NOT_HARD 1024
#define SPAWNFLAG_NOT_DEATHMATCH 2048
