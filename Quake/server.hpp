/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

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

#include "vr_macros.hpp"
#include "worldtext.hpp"
#include "cvar.hpp"
#include "common.hpp"
#include "quakedef_macros.hpp"
#include "progs.hpp"
#include "sizebuf.hpp"
#include "snd_voip.hpp"
#include "qcvm.hpp"
#include "modeleffects.hpp"
#include "serverdefines.hpp"

#include <vector>

struct qmodel_t;

// server.h

typedef struct
{
    int maxclients;
    int maxclientslimit;
    struct client_t* clients; // [maxclients]
    int serverflags;          // episode completion information
    bool changelevel_issued;  // cleared when at SV_SpawnServer
} server_static_t;

//=============================================================================

typedef enum
{
    ss_loading,
    ss_active
} server_state_t;

struct ambientsound_s
{
    qvec3 origin;
    unsigned int soundindex;
    float volume;
    float attenuation;
};

struct server_t
{
    bool active; // false if only a net client

    bool paused;
    bool loadgame; // handle connections specially

    int lastcheck; // used by PF_checkclient
    double lastchecktime;

    qcvm_t qcvm; // Spike: entire qcvm state

    char name[64];      // map name
    char modelname[64]; // maps/<name>.bsp, for model_precache[0]
    const char* model_precache[MAX_MODELS]; // nullptr terminated
    qmodel_t* models[MAX_MODELS];
    const char* sound_precache[MAX_SOUNDS]; // nullptr terminated
    const char* lightstyles[MAX_LIGHTSTYLES];
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

    sizebuf_t
        multicast; // selectively copied to clients by the multicast builtin
    byte multicast_buf[MAX_DATAGRAM];

    const char* particle_precache[MAX_PARTICLETYPES]; // NULL terminated

    entity_state_t* static_entities;
    int num_statics;
    int max_statics;

    ambientsound_s* ambientsounds;
    int num_ambients;
    int max_ambients;

    struct svcustomstat_s
    {
        int idx;
        int type;
        int fld;
        eval_t* ptr;
    } customstats[MAX_CL_STATS * 2]; // strings or numeric...
    size_t numcustomstats;
    std::vector<WorldText> worldTexts;
    std::vector<WorldTextHandle> freeWorldTextHandles;

    [[nodiscard]] bool isValidWorldTextHandle(
        const WorldTextHandle wth) const noexcept;

    [[nodiscard]] bool hasAnyFreeWorldTextHandle() const noexcept;

    [[nodiscard]] WorldTextHandle makeWorldTextHandle() noexcept;

    [[nodiscard]] WorldText& getWorldText(const WorldTextHandle wth) noexcept;

    void initializeWorldTexts();

    // World text "server -> client" messages
    void SendMsg_WorldTextHMake(
        client_t& client, const WorldTextHandle wth) noexcept;

    void SendMsg_WorldTextHSetText(client_t& client, const WorldTextHandle wth,
        const char* const text) noexcept;

    void SendMsg_WorldTextHSetPos(
        client_t& client, const WorldTextHandle wth, const qvec3& pos) noexcept;

    void SendMsg_WorldTextHSetAngles(client_t& client,
        const WorldTextHandle wth, const qvec3& angles) noexcept;

    void SendMsg_WorldTextHSetHAlign(client_t& client,
        const WorldTextHandle wth, const WorldText::HAlign hAlign) noexcept;

    void SendMsg_WorldTextHSetScale(client_t& client,
        const WorldTextHandle wth, float scale) noexcept;
};

#define NUM_PING_TIMES 16
#define NUM_BASIC_SPAWN_PARMS 16
#define NUM_TOTAL_SPAWN_PARMS 64

struct client_t
{
    bool active; // false = client is free

    bool spawned; // false = don't send datagrams (set when client acked the
                  // first entities)

    bool dropasap; // has been told to go to another level

    int sendsignon; // only valid before spawned
    int signonidx;

    double last_message; // reliable messages must be sent
                         // periodically

    struct qsocket_s* netconnection; // communications handle

    usercmd_t cmd; // movement
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
    float spawn_parms[NUM_TOTAL_SPAWN_PARMS];

    // client known data for deltas
    int old_frags;

    // QSS
    sizebuf_t datagram;
    byte datagram_buf[MAX_DATAGRAM];

    // QSS
    unsigned int limit_entities;   // vanilla is 600
    unsigned int limit_unreliable; // max allowed size for unreliables
    unsigned int limit_reliable;   // max (total) size of a reliable message.
    unsigned int limit_models;     //
    unsigned int limit_sounds;     //

    // QSS
    bool pextknown;
    unsigned int protocol_pext2;
    unsigned int
        resendstats[MAX_CL_STATS / 32]; // the stats which need to be resent.
    int oldstats_i[MAX_CL_STATS];   // previous values of stats. if these differ
                                    // from the current values, reflag
                                    // resendstats.
    float oldstats_f[MAX_CL_STATS]; // previous values of stats. if these differ
                                    // from the current values, reflag
                                    // resendstats.
    struct entity_num_state_s
    {
        unsigned int num; // ascending order, there can be gaps.
        entity_state_t state;
    }* previousentities;
    size_t numpreviousentities;
    size_t maxpreviousentities;
    unsigned int snapshotresume;
    unsigned int* pendingentities_bits; // UF_ flags for each entity
    size_t numpendingentities;          // realloc if too small
    struct deltaframe_s
    { // quick overview of how this stuff actually works:
        // when the server notices a gap in the ack sequence, we walk through
        // the dropped frames and reflag everything that was dropped. if the
        // server isn't tracking enough frames, then we just treat those as
        // dropped; small note: when an entity is new, it re-flags itself as new
        // for the next packet too, this reduces the immediate impact of
        // packetloss on new entities. reflagged state includes stats updates,
        // entity updates, and entity removes.
        int sequence; // to see if its stale
        float timestamp;
        unsigned int resendstats[MAX_CL_STATS / 32];
        struct frameent_t
        {
            unsigned int num;
            unsigned int bits;
        }* ents;
        int numents; // doesn't contain an entry for every entity, just ones
                     // that were sent this frame. no 0 bits
        int maxents;
    }* frames;
    size_t numframes; // preallocated power-of-two
    int lastacksequence;
    int lastmovemessage;

    // QSS
    client_voip_t voip; // spike -- for voip
    struct
    {
        char name[MAX_QPATH];
        FILE* file;
        bool started; // actually sending
        unsigned int
            startpos; // within the pak, so we don't break stuff when seeking
        unsigned int size;
        unsigned int sendpos; // file offset we last tried sending
        unsigned int ackpos;  // if they don't ack this properly, we restart
                              // sending from here instead.
        // for more speed, the server should build a collection of blocks to
        // track which parts were actually acked, thereby avoiding redundant
        // resends, but in the intererest of simplicity...
    } download;
    bool knowntoqc; // putclientinserver was called
};

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

void SV_StartParticle(
    const qvec3& org, const qvec3& dir, const int color, const int count);
void SV_StartParticle2(
    const qvec3& org, const qvec3& dir, const int preset, const int count);
void SV_StartSound(edict_t* entity, const qvec3* origin, int channel,
    const char* sample, int volume, float attenuation);

void SV_DropClient(bool crash);

void SVFTE_Ack(client_t* client, int sequence);
void SVFTE_DestroyFrames(client_t* client);
void SV_BuildEntityState(edict_t* ent, entity_state_t* state);
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
bool SV_movestep(edict_t* ent, qvec3 move, bool relink);

void SV_WriteClientdataToMessage(client_t* client, sizebuf_t* msg);

void SV_MoveToGoal();

void SV_ConnectClient(
    int clientnum); // called from the netcode to add new clients. also called
                    // from pr_ext to spawn new botclients.

void SV_CheckForNewClients();
void SV_RunClients();
void SV_SaveSpawnparms();

enum class SpawnServerSrc
{
    FromSaveFile,
    FromMapCmd,
    FromChangelevelCmd,
    FromRestart
};

void SV_SpawnServer(const char* server, const SpawnServerSrc src);
