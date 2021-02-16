/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2019 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

// quakedef.h -- primary header for client

#define QUAKE_GAME // as opposed to utilities

#define VERSION 1.09
#define GLQUAKE_VERSION 1.00
#define D3DQUAKE_VERSION 0.01
#define WINQUAKE_VERSION 0.996
#define LINUX_VERSION 1.30
#define X11_VERSION 1.10

#define FITZQUAKE_VERSION 0.85 // johnfitz
#define QUAKESPASM_VERSION 0.93
#define QUAKEVR_VERSION "0.0.6"
#define QUAKESPASM_VER_PATCH 2 // helper to print a string like 0.93.2
#ifndef QUAKESPASM_VER_SUFFIX
#define QUAKESPASM_VER_SUFFIX // optional version suffix string literal like
                              // "-beta1"
#endif

#define QS_STRINGIFY_(x) #x
#define QS_STRINGIFY(x) QS_STRINGIFY_(x)

// combined version string like "0.92.1-beta1"
#define QUAKESPASM_VER_STRING                                    \
    QS_STRINGIFY(QUAKESPASM_VERSION)                             \
    "." QS_STRINGIFY(QUAKESPASM_VER_PATCH) QUAKESPASM_VER_SUFFIX \
        " | Quake VR " QUAKEVR_VERSION

#define ENGINE_NAME_AND_VER \
    "QSS"                   \
    " " QUAKESPASM_VER_STRING

// #define PARANOID // speed sapping error checking

// #define PSET_SCRIPT // enable the scriptable particle system (poorly ported
// from FTE)

// #define PSET_SCRIPT_EFFECTINFO // scripted particle system can load dp's
// effects

#define GAMENAME "id1" // directory to look in by default

#define MINIMUM_MEMORY 1'048'576 // 16 MB
#define MINIMUM_MEMORY_LEVELPAK (MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS 50

// up / down
#define PITCH 0

// left / right
#define YAW 1

// fall over
#define ROLL 2

#define MAX_QPATH 128 // max length of a quake game pathname

#define ON_EPSILON 0.1 // point on plane side epsilon

#define DIST_EPSILON \
    (0.03125f) // 1/32 epsilon to keep floating point happy (moved from world.c)

#define MAX_MSGLEN \
    96000 // max length of a reliable message //ericw -- was 32000
#define MAX_DATAGRAM \
    64000 // max length of unreliable message //johnfitz -- was 1024

#define DATAGRAM_MTU \
    1400 // johnfitz -- actual limit for unreliable messages to nonlocal clients

//
// per-level limits
//
#define MIN_EDICTS 256 // johnfitz -- lowest allowed value for max_edicts cvar
#define MAX_EDICTS \
    32000 // johnfitz -- highest allowed value for max_edicts cvar
          // ents past 8192 can't play sounds in the standard protocol
#define MAX_LIGHTSTYLES 1024
#define MAX_MODELS 4096 // johnfitz -- was 256
#define MAX_SOUNDS 2048 // johnfitz -- was 256

#define SAVEGAME_COMMENT_LENGTH 39

#define MAX_STYLESTRING 64

//
// stats are integers communicated to the client by the server
//
#define MAX_CL_STATS 256

#define STAT_HEALTH 0
#define STAT_FRAGS 1
#define STAT_WEAPON 2
#define STAT_AMMO 3
#define STAT_ARMOR 4
#define STAT_WEAPONFRAME 5
#define STAT_SHELLS 6
#define STAT_NAILS 7
#define STAT_ROCKETS 8
#define STAT_CELLS 9
#define STAT_ACTIVEWEAPON 10
#define STAT_TOTALSECRETS 11
#define STAT_TOTALMONSTERS 12
#define STAT_SECRETS 13  // bumped on client side by svc_foundsecret
#define STAT_MONSTERS 14 // bumped by svc_killedmonster
#define STAT_WEAPON2 15
#define STAT_WEAPONMODEL2 16
#define STAT_WEAPONFRAME2 17
#define STAT_HOLSTERWEAPON0 18
#define STAT_HOLSTERWEAPON1 19
#define STAT_HOLSTERWEAPON2 20
#define STAT_HOLSTERWEAPON3 21
#define STAT_HOLSTERWEAPONMODEL0 22
#define STAT_HOLSTERWEAPONMODEL1 23
#define STAT_HOLSTERWEAPONMODEL2 24
#define STAT_HOLSTERWEAPONMODEL3 25
#define STAT_AMMO2 26
#define STAT_AMMOCOUNTER 27
#define STAT_AMMOCOUNTER2 28
#define STAT_HOLSTERWEAPON4 29
#define STAT_HOLSTERWEAPON5 30
#define STAT_HOLSTERWEAPONMODEL4 31
#define STAT_HOLSTERWEAPONMODEL5 32
#define STAT_MAINHAND_WID 33
#define STAT_OFFHAND_WID 34
#define STAT_WEAPONFLAGS 35
#define STAT_WEAPONFLAGS2 36
#define STAT_HOLSTERWEAPONFLAGS0 37
#define STAT_HOLSTERWEAPONFLAGS1 38
#define STAT_HOLSTERWEAPONFLAGS2 39
#define STAT_HOLSTERWEAPONFLAGS3 40
#define STAT_HOLSTERWEAPONFLAGS4 41
#define STAT_HOLSTERWEAPONFLAGS5 42
#define STAT_ITEMS 43      // replaces clc_clientdata info
#define STAT_VIEWHEIGHT 44 // replaces clc_clientdata info
//#define STAT_TIME			45	//zquake, redundant for nq.
//#define STAT_MATCHSTARTTIME 46
//#define STAT_VIEW2		47
#define STAT_VIEWZOOM 48 // DP
//#define STAT_UNUSED3		49
//#define STAT_UNUSED2		50
//#define STAT_UNUSED1		51
#define STAT_IDEALPITCH 52   // nq-emu
#define STAT_PUNCHANGLE_X 53 // nq-emu
#define STAT_PUNCHANGLE_Y 54 // nq-emu
#define STAT_PUNCHANGLE_Z 55 // nq-emu
#define STAT_PUNCHVECTOR_X 56
#define STAT_PUNCHVECTOR_Y 57
#define STAT_PUNCHVECTOR_Z 58
#define STAT_WEAPONCLIP 59
#define STAT_WEAPONCLIP2 60
#define STAT_HOLSTERWEAPONCLIP0 61
#define STAT_HOLSTERWEAPONCLIP1 62
#define STAT_HOLSTERWEAPONCLIP2 63
#define STAT_HOLSTERWEAPONCLIP3 64
#define STAT_HOLSTERWEAPONCLIP4 65
#define STAT_HOLSTERWEAPONCLIP5 66



// stock defines
//
// clang-format off
#define IT_SHOTGUN          VRUTIL_POWER_OF_TWO(0)
#define IT_SUPER_SHOTGUN    VRUTIL_POWER_OF_TWO(1)
#define IT_NAILGUN          VRUTIL_POWER_OF_TWO(2)
#define IT_SUPER_NAILGUN    VRUTIL_POWER_OF_TWO(3)
#define IT_GRENADE_LAUNCHER VRUTIL_POWER_OF_TWO(4)
#define IT_ROCKET_LAUNCHER  VRUTIL_POWER_OF_TWO(5)
#define IT_LIGHTNING        VRUTIL_POWER_OF_TWO(6)
#define IT_SUPER_LIGHTNING  VRUTIL_POWER_OF_TWO(7)
#define IT_SHELLS           VRUTIL_POWER_OF_TWO(8)
#define IT_NAILS            VRUTIL_POWER_OF_TWO(9)
#define IT_ROCKETS          VRUTIL_POWER_OF_TWO(10)
#define IT_CELLS            VRUTIL_POWER_OF_TWO(11)
#define IT_AXE              VRUTIL_POWER_OF_TWO(12)
#define IT_ARMOR1           VRUTIL_POWER_OF_TWO(13)
#define IT_ARMOR2           VRUTIL_POWER_OF_TWO(14)
#define IT_ARMOR3           VRUTIL_POWER_OF_TWO(15)
#define IT_SUPERHEALTH      VRUTIL_POWER_OF_TWO(16)
#define IT_KEY1             VRUTIL_POWER_OF_TWO(17)
#define IT_KEY2             VRUTIL_POWER_OF_TWO(18)
#define IT_INVISIBILITY     VRUTIL_POWER_OF_TWO(19)
#define IT_INVULNERABILITY  VRUTIL_POWER_OF_TWO(20)
#define IT_SUIT             VRUTIL_POWER_OF_TWO(21)
#define IT_QUAD             VRUTIL_POWER_OF_TWO(22)
// clang-format on
#define IT_SIGIL1 (1 << 28)
#define IT_SIGIL2 (1 << 29)
#define IT_SIGIL3 (1 << 30)
#define IT_SIGIL4 (1 << 31)

//===========================================
// rogue changed and added defines

#define RIT_SHELLS 128
#define RIT_NAILS 256
#define RIT_ROCKETS 512
#define RIT_CELLS 1024
#define RIT_AXE 2048
#define RIT_LAVA_NAILGUN 4096
#define RIT_LAVA_SUPER_NAILGUN 8192
#define RIT_MULTI_GRENADE 16384
#define RIT_MULTI_ROCKET 32768
#define RIT_PLASMA_GUN 65536
#define RIT_ARMOR1 8388608
#define RIT_ARMOR2 16777216
#define RIT_ARMOR3 33554432
#define RIT_LAVA_NAILS 67108864
#define RIT_PLASMA_AMMO 134217728
#define RIT_MULTI_ROCKETS 268435456
#define RIT_SHIELD 536870912
#define RIT_ANTIGRAV 1073741824
#define RIT_SUPERHEALTH 2147483648

// MED 01/04/97 added hipnotic defines
//===========================================
// hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT 7
#define HIT_LASER_CANNON_BIT 23
#define HIT_PROXIMITY_GUN (1 << HIT_PROXIMITY_GUN_BIT) // 65536
#define HIT_MJOLNIR (1 << HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON (1 << HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT (1 << (23 + 2))
#define HIT_EMPATHY_SHIELDS (1 << (23 + 3))

//===========================================

// weapon ids
#define WID_FIST 0
#define WID_GRAPPLE 1
#define WID_AXE 2
#define WID_MJOLNIR 3
#define WID_SHOTGUN 4
#define WID_SUPER_SHOTGUN 5
#define WID_NAILGUN 6
#define WID_SUPER_NAILGUN 7
#define WID_GRENADE_LAUNCHER 8
#define WID_PROXIMITY_GUN 9
#define WID_ROCKET_LAUNCHER 10
#define WID_LIGHTNING 11
#define WID_LASER_CANNON 12

// ammo ids
#define AID_NONE 0
#define AID_SHELLS 1
#define AID_NAILS 2
#define AID_ROCKETS 3
#define AID_CELLS 4
#define AID_LAVA_NAILS 5
#define AID_MULTI_ROCKETS 6
#define AID_PLASMA 7

// Quake VR hotspots
#define QVR_HS_NONE 0
#define QVR_HS_OFFHAND_2H_GRAB 1  // 2H grab - helper offhand
#define QVR_HS_MAINHAND_2H_GRAB 2 // 2H grab - helper mainhand
#define QVR_HS_LEFT_SHOULDER_HOLSTER 3
#define QVR_HS_RIGHT_SHOULDER_HOLSTER 4
#define QVR_HS_LEFT_HIP_HOLSTER 5
#define QVR_HS_RIGHT_HIP_HOLSTER 6
#define QVR_HS_HAND_SWITCH 7
#define QVR_HS_LEFT_UPPER_HOLSTER 8
#define QVR_HS_RIGHT_UPPER_HOLSTER 9

// Quake VR - vrbits0 bits
// clang-format off
#define QVR_VRBITS0_TELEPORTING           VRUTIL_POWER_OF_TWO(0)
#define QVR_VRBITS0_OFFHAND_GRABBING      VRUTIL_POWER_OF_TWO(1)
#define QVR_VRBITS0_OFFHAND_PREVGRABBING  VRUTIL_POWER_OF_TWO(2)
#define QVR_VRBITS0_MAINHAND_GRABBING     VRUTIL_POWER_OF_TWO(3)
#define QVR_VRBITS0_MAINHAND_PREVGRABBING VRUTIL_POWER_OF_TWO(4)
#define QVR_VRBITS0_2H_AIMING             VRUTIL_POWER_OF_TWO(5)
// clang-format on

#define MAX_SCOREBOARD 255
#define MAX_SCOREBOARDNAME 32

#define SOUND_CHANNELS 8



// From client.hpp

#define CSHIFT_CONTENTS 0
#define CSHIFT_DAMAGE 1
#define CSHIFT_BONUS 2
#define CSHIFT_POWERUP 3
#define NUM_CSHIFTS 4
#define NAME_LENGTH 64
#define SIGNONS 4      // signon messages to receive before connected
#define MAX_DLIGHTS 64 // johnfitz -- was 32
#define MAX_BEAMS 128  // johnfitz -- was 24
#define MAX_MAPSTRING 2048
#define MAX_DEMOS 8
#define MAX_DEMONAME 16
#define MAX_TEMP_ENTITIES 512    // johnfitz -- was 64
#define MAX_STATIC_ENTITIES 4096 // ericw -- was 512	//johnfitz -- was 128

// QSS
#define MAX_PARTICLETYPES 2048
#define MAX_LIGHTSTYLES_VANILLA 64
