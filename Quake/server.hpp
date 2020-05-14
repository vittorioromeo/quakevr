/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

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

#ifndef _QUAKE_SERVER_H
#define _QUAKE_SERVER_H

#include "vr_macros.hpp"

#include <vector>

struct qmodel_t;

// server.h

typedef struct
{
    int maxclients;
    int maxclientslimit;
    struct client_s* clients; // [maxclients]
    int serverflags;          // episode completion information
    bool changelevel_issued;  // cleared when at SV_SpawnServer
} server_static_t;

//=============================================================================

typedef enum
{
    ss_loading,
    ss_active
} server_state_t;

typedef struct
{
    bool active; // false if only a net client

    bool paused;
    bool loadgame; // handle connections specially

    double time;

    int lastcheck; // used by PF_checkclient
    double lastchecktime;

    char name[64];      // map name
    char modelname[64]; // maps/<name>.bsp, for model_precache[0]
    qmodel_t* worldmodel;
    const char* model_precache[MAX_MODELS]; // nullptr terminated
    qmodel_t* models[MAX_MODELS];
    const char* sound_precache[MAX_SOUNDS]; // nullptr terminated
    const char* lightstyles[MAX_LIGHTSTYLES];
    int num_edicts;
    int max_edicts;
    edict_t* edicts;      // can NOT be array indexed, because
                          // edict_t is variable sized, but can
                          // be used to reference the world ent
    server_state_t state; // some actions are only valid during load

    sizebuf_t datagram;
    byte datagram_buf[MAX_DATAGRAM];

    sizebuf_t reliable_datagram; // copied to all clients at end of frame
    byte reliable_datagram_buf[MAX_DATAGRAM];

    sizebuf_t signon;
    byte signon_buf[MAX_MSGLEN - 2]; // johnfitz -- was 8192, now uses
                                     // MAX_MSGLEN

    unsigned protocol; // johnfitz
    unsigned protocolflags;

    std::vector<WorldText> worldTexts;
    std::vector<WorldTextHandle> freeWorldTextHandles;
} server_t;


#define NUM_PING_TIMES 16
#define NUM_SPAWN_PARMS 32

typedef struct client_s
{
    bool active;     // false = client is free
    bool spawned;    // false = don't send datagrams
    bool dropasap;   // has been told to go to another level
    bool sendsignon; // only valid before spawned

    double last_message; // reliable messages must be sent
                         // periodically

    struct qsocket_s* netconnection; // communications handle

    usercmd_t cmd;     // movement
    qvec3 wishdir; // intended motion calced from cmd

    sizebuf_t message; // can be added to at any time,
                       // copied and clear once per frame
    byte msgbuf[MAX_MSGLEN];
    edict_t* edict; // EDICT_NUM(clientnum+1)
    char name[32];  // for printing to other people
    int colors;

    float ping_times[NUM_PING_TIMES];
    int num_pings; // ping_times[num_pings%NUM_PING_TIMES]

    // spawn parms are carried from level to level
    float spawn_parms[NUM_SPAWN_PARMS];

    // client known data for deltas
    int old_frags;
} client_t;


//=============================================================================

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

// edict->solid values
#define SOLID_NOT 0               // no interaction with other objects
#define SOLID_TRIGGER 1           // touch on edge, but not blocking
#define SOLID_BBOX 2              // touch on edge, block
#define SOLID_SLIDEBOX 3          // touch on edge, but not an onground
#define SOLID_BSP 4               // bsp clip, touch on edge, block
#define SOLID_NOT_BUT_TOUCHABLE 5 // not solid, but can be [hand]touched

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
// clang-format on

// entity effects
// clang-format off
#define EF_BRIGHTFIELD  VRUTIL_POWER_OF_TWO(0)
#define EF_MUZZLEFLASH  VRUTIL_POWER_OF_TWO(1)
#define EF_BRIGHTLIGHT  VRUTIL_POWER_OF_TWO(2)
#define EF_DIMLIGHT     VRUTIL_POWER_OF_TWO(3)
#define EF_VERYDIMLIGHT VRUTIL_POWER_OF_TWO(4)
#define EF_MINIROCKET   VRUTIL_POWER_OF_TWO(5)
#define EF_LAVATRAIL    VRUTIL_POWER_OF_TWO(6)
// clang-format on

#define SPAWNFLAG_NOT_EASY 256
#define SPAWNFLAG_NOT_MEDIUM 512
#define SPAWNFLAG_NOT_HARD 1024
#define SPAWNFLAG_NOT_DEATHMATCH 2048

//============================================================================

extern cvar_t teamplay;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t fraglimit;
extern cvar_t timelimit;

extern server_static_t svs; // persistant server info
extern server_t sv;         // local server

extern client_t* host_client;

extern edict_t* sv_player;

//===========================================================

void SV_Init();

void SV_StartParticle(const qfvec3& org, const qfvec3& dir,
    const int color, const int count);
void SV_StartParticle2(const qfvec3& org, const qfvec3& dir,
    const int preset, const int count);
void SV_StartSound(edict_t* entity, int channel, const char* sample, int volume,
    float attenuation);

void SV_DropClient(bool crash);

void SV_SendClientMessages();
void SV_ClearDatagram();

int SV_ModelIndex(const char* name);

void SV_SetIdealPitch();

void SV_AddUpdates();

void SV_ClientThink();
void SV_AddClientToServer(struct qsocket_s* ret);

void SV_ClientPrintf(const char* fmt, ...) FUNC_PRINTF(1, 2);
void SV_BroadcastPrintf(const char* fmt, ...) FUNC_PRINTF(1, 2);

void SV_Physics();

bool SV_CheckBottom(edict_t* ent);
bool SV_movestep(edict_t* ent, qfvec3 move, bool relink);

void SV_WriteClientdataToMessage(edict_t* ent, sizebuf_t* msg);

void SV_MoveToGoal();

void SV_CheckForNewClients();
void SV_RunClients();
void SV_SaveSpawnparms();
void SV_SpawnServer(const char* server, const bool fromSaveFile);

#endif /* _QUAKE_SERVER_H */
