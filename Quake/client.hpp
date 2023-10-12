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

#include "render.hpp"
#include "world.hpp"
#include "keys.hpp"
#include "quakeglm_qvec3.hpp"
#include "quakedef.hpp"
#include "worldtext.hpp"
#include "common.hpp"
#include "gl_model.hpp"
#include "quakedef_macros.hpp"
#include "sizebuf.hpp"
#include "qcvm.hpp"

#include <vector>

typedef struct
{
    int length;
    char map[MAX_STYLESTRING];
    char average; // johnfitz
    char peak;    // johnfitz
} lightstyle_t;

typedef struct
{
    char name[MAX_SCOREBOARDNAME];
    float entertime;
    int frags;
    int colors; // two 4 bit fields
    int ping;   // QSS
    byte translations[VID_GRADES * 256];
} scoreboard_t;

typedef struct
{
    int destcolor[3];
    float percent; // 0-256 // QSS
} cshift_t;


//
// client_state_t should hold all pieces of the client state
//

struct dlight_t
{
    qvec3 origin;
    float radius;
    float die;      // stop lighting after this time
    float decay;    // drop this each second
    float minlight; // don't add when contributing less
    int key;
    qvec3 color; // johnfitz -- lit support via lordhavoc
};

struct beam_t
{
    int entity;
    int disambiguator;
    qmodel_t* model;
    float endtime;
    qvec3 start, end;
    bool spin;
    float scaleRatioX;
    // SomeEnum special; // TODO VR: (P1) consider adding this and experiment
    // with particles/dlights

    // QSS
    const char* trailname;
    struct trailstate_s* trailstate;
};

typedef enum
{
    ca_dedicated,    // a dedicated server with no ability to start a client
    ca_disconnected, // full screen console with no connection
    ca_connected     // valid netcon, talking to a server
} cactive_t;

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
    cactive_t state;

    // personalization data sent to server
    char spawnparms[MAX_MAPSTRING]; // to restart a level

    // demo loop control
    int demonum;                         // -1 = don't play demos
    char demos[MAX_DEMOS][MAX_DEMONAME]; // when not playing

    // demo recording info must be here, because record is started before
    // entering a map (and clearing client_state_t)
    bool demorecording;
    bool demoplayback;

    // did the user pause demo playback? (separate from cl.paused because we
    // don't want a svc_setpause inside the demo to actually pause demo
    // playback).
    bool demopaused;

    bool timedemo;
    int forcetrack; // -1 = use normal cd track
    FILE* demofile;
    int td_lastframe;   // to meter out one message a frame
    int td_startframe;  // host_framecount at start
    float td_starttime; // realtime at second frame of timedemo

    // connection information
    int signon; // 0 to SIGNONS
    struct qsocket_s* netcon;
    sizebuf_t message; // writing buffer to send to server

    // QSS
    // downloads don't restart/fail when the server sends random serverinfo
    // packets
    struct
    {
        bool active;
        unsigned int size;
        FILE* file;
        char current[MAX_QPATH]; // also prevents us from repeatedly trying to
                                 // download the same file
        char temp[MAX_OSPATH];   // the temp filename for the download, will be
                                 // renamed to current
        float starttime;
    } download;

} client_static_t;

extern client_static_t cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
struct client_state_t
{
    int movemessages;     // since connecting to this server
                          // throw out the first couple, so the player
                          // doesn't accidentally do something the
                          // first frame
    usercmd_t cmd;        // last command sent to the server
    usercmd_t pendingcmd; // accumulated state from mice+joysticks. // QSS

    // information for local display
    int stats[MAX_CL_STATS];    // health, etc
    float statsf[MAX_CL_STATS]; // QSS
    int items;                  // inventory bit flags
    float item_gettime[32];     // cl.time of aquiring item, for blinking
    float faceanimtime;         // use anim frame if cl.time < this

    cshift_t cshifts[NUM_CSHIFTS];      // color shifts for damage, powerups
    cshift_t prev_cshifts[NUM_CSHIFTS]; // and content types

    // the client maintains its own idea of view angles, which are
    // sent to the server each frame.  The server sets punchangle when
    // the view is temporarliy offset, and an angle reset commands at the start
    // of each level and after teleporting.
    qvec3 mviewangles[2]; // during demo playback viewangles is lerped
                          // between these
    qvec3 viewangles;

    qvec3 aimangles;
    qvec3 vmeshoffset;
    qvec3 handpos[2];
    qvec3 handrot[2];
    qvec3 prevhandrot[2];
    qvec3 handvel[2];
    qvec3 handthrowvel[2];
    float handvelmag[2];
    qvec3 handavel[2];
    qvec3 headvel;
    qvec3 visual_handrot[2];

    qvec3 mvelocity[2]; // update by server, used for lean+bob
                        // (0 is newest)
    qvec3 velocity;     // lerped between mvelocity[0] and [1]

    qvec3 punchangle; // temporary offset

    // pitch drifting vars
    float pitchvel;
    bool nodrift;
    float driftmove;
    double laststop;

    float crouch; // local amount for smoothing stepups

    bool paused; // send over by server
    bool onground;
    bool inwater;

    int intermission;   // don't change view angle, full screen, etc
    int completed_time; // latched at intermission start

    double mtime[2]; // the timestamp of last two messages
    double time;     // clients view of time, should be between
                     // servertime and oldservertime to generate
                     // a lerp point for other data
    double oldtime;  // previous cl.time, time-oldtime is used
                     // to decay light values and smooth step ups


    float last_received_message; // (realtime) for net trouble icon

    //
    // information that is static for the entire time connected to a server
    //
    qmodel_t* model_precache[MAX_MODELS];
    struct sfx_t* sound_precache[MAX_SOUNDS];

    char mapname[128];
    char levelname[128]; // for display on solo scoreboard //johnfitz -- was 40.
    int viewentity;      // cl.entities[cl.viewentity] = player
    int maxclients;
    int gametype;

    // refresh related state
    qmodel_t* worldmodel; // cl.entities[0].model
    struct efrag_t* free_efrags;
    int num_efrags;

    // TODO VR: (P2) all of these should be in some sort of list to avoid
    // repetition
    entity_t viewent;         // the gun model
    entity_t offhand_viewent; // the offhand gun model

    entity_t left_hip_holster;
    entity_t right_hip_holster;
    entity_t left_upper_holster;
    entity_t right_upper_holster;
    entity_t vrtorso;
    entity_t left_hip_holster_slot;
    entity_t right_hip_holster_slot;
    entity_t left_upper_holster_slot;
    entity_t right_upper_holster_slot;

    entity_t mainhand_wpn_button;
    entity_t offhand_wpn_button;

    textentity_t mainhand_wpn_text;
    textentity_t offhand_wpn_text;

    struct hand_entities
    {
        entity_t base;
        entity_t f_thumb;
        entity_t f_index;
        entity_t f_middle;
        entity_t f_ring;
        entity_t f_pinky;
    };

    hand_entities left_hand_entities;
    hand_entities right_hand_entities;

    hand_entities left_hand_ghost_entities;
    hand_entities right_hand_ghost_entities;

    entity_t* entities; // spike -- moved into here
    int max_edicts;
    int num_entities;

    entity_t** static_entities; // spike -- was static
    int max_static_entities;
    int num_statics;

    int cdtrack, looptrack; // cd audio

    // frag scoreboard
    scoreboard_t* scores; // [cl.maxclients]

    unsigned protocol; // johnfitz
    unsigned protocolflags;
    unsigned protocol_pext2; // spike -- flag of fte protocol extensions

    // QSS
    bool protocol_dpdownload;

#ifdef PSET_SCRIPT
    bool protocol_particles;
    struct
    {
        const char* name;
        int index;
    } particle_precache[MAX_PARTICLETYPES];
    struct
    {
        const char* name;
        int index;
    } local_particle_precache[MAX_PARTICLETYPES];
#endif

    int ackframes[8]; // big enough to cover burst
    unsigned int ackframes_count;
    bool requestresend;

    char
        stuffcmdbuf[1024]; // comment-extensions are a thing with certain
                           // servers, make sure we can handle them properly
                           // without further hacks/breakages. there's also some
                           // server->client only console commands that we might
                           // as well try to handle a bit better, like reconnect
    enum
    {
        PRINT_NONE,
        PRINT_PINGS,
        //		PRINT_STATUSINFO,
        //		PRINT_STATUSPLAYER,
        //		PRINT_STATUSIP,
    } printtype;
    int printplayer;
    float expectingpingtimes;
    float printversionresponse;

    // spike -- moved this stuff here to deal with downloading content named by
    // the server
    bool sendprespawn; // download+load content, send the prespawn command
                       // once done
    int model_count;
    int model_download;
    char model_name[MAX_MODELS][MAX_QPATH];
    int sound_count;
    int sound_download;
    char sound_name[MAX_SOUNDS][MAX_QPATH];
    // spike -- end downloads

    qcvm_t qcvm; // for csqc.

    bool csqc_cursorforced; // we want a mouse cursor.
    float csqc_sensitivity; // scaler for sensitivity
    // ---

    std::vector<WorldText> worldTexts;

    int hotspot[2];

    [[nodiscard]] bool isValidWorldTextHandle(
        const WorldTextHandle wth) const noexcept;

    void OnMsg_WorldTextHMake() noexcept;
    void OnMsg_WorldTextHSetText() noexcept;
    void OnMsg_WorldTextHSetPos() noexcept;
    void OnMsg_WorldTextHSetAngles() noexcept;
    void OnMsg_WorldTextHSetHAlign() noexcept;
    void OnMsg_WorldTextHSetScale() noexcept;
};

template <typename F>
bool anyViewmodel(client_state_t& clientState, F&& f)
{
    return                                                   //
        f(clientState.viewent)                               //
        || f(clientState.offhand_viewent)                    //
        || f(clientState.left_hip_holster)                   //
        || f(clientState.right_hip_holster)                  //
        || f(clientState.left_upper_holster)                 //
        || f(clientState.right_upper_holster)                //
        || f(clientState.vrtorso)                            //
        || f(clientState.left_hip_holster_slot)              //
        || f(clientState.right_hip_holster_slot)             //
        || f(clientState.left_upper_holster_slot)            //
        || f(clientState.right_upper_holster_slot)           //
        || f(clientState.mainhand_wpn_button)                //
        || f(clientState.offhand_wpn_button)                 //
        || f(clientState.left_hand_entities.base)            //
        || f(clientState.left_hand_entities.f_thumb)         //
        || f(clientState.left_hand_entities.f_index)         //
        || f(clientState.left_hand_entities.f_middle)        //
        || f(clientState.left_hand_entities.f_ring)          //
        || f(clientState.left_hand_entities.f_pinky)         //
        || f(clientState.right_hand_entities.base)           //
        || f(clientState.right_hand_entities.f_thumb)        //
        || f(clientState.right_hand_entities.f_index)        //
        || f(clientState.right_hand_entities.f_middle)       //
        || f(clientState.right_hand_entities.f_ring)         //
        || f(clientState.right_hand_entities.f_pinky)        //
        || f(clientState.left_hand_ghost_entities.base)      //
        || f(clientState.left_hand_ghost_entities.f_thumb)   //
        || f(clientState.left_hand_ghost_entities.f_index)   //
        || f(clientState.left_hand_ghost_entities.f_middle)  //
        || f(clientState.left_hand_ghost_entities.f_ring)    //
        || f(clientState.left_hand_ghost_entities.f_pinky)   //
        || f(clientState.right_hand_ghost_entities.base)     //
        || f(clientState.right_hand_ghost_entities.f_thumb)  //
        || f(clientState.right_hand_ghost_entities.f_index)  //
        || f(clientState.right_hand_ghost_entities.f_middle) //
        || f(clientState.right_hand_ghost_entities.f_ring)   //
        || f(clientState.right_hand_ghost_entities.f_pinky);
}

template <typename F>
void forAllViewmodels(client_state_t& clientState, F&& f)
{
    anyViewmodel(clientState,
        [&](entity_t& e)
        {
            f(e);
            return false;
        });
}



//
// cvars
//
extern cvar_t cl_name;
extern cvar_t cl_color;

extern cvar_t cl_upspeed;
extern cvar_t cl_forwardspeed;
extern cvar_t cl_backspeed;
extern cvar_t cl_sidespeed;

extern cvar_t cl_movespeedkey;

extern cvar_t cl_yawspeed;
extern cvar_t cl_pitchspeed;

extern cvar_t cl_anglespeedkey;

extern cvar_t cl_alwaysrun; // QuakeSpasm

extern cvar_t cl_autofire;

extern cvar_t cl_recordingdemo; // QSS
extern cvar_t cl_shownet;
extern cvar_t cl_nolerp;

extern cvar_t cfg_unbindall;

extern cvar_t cl_pitchdriftspeed;
extern cvar_t lookspring;
extern cvar_t lookstrafe;
extern cvar_t sensitivity;

extern cvar_t m_pitch;
extern cvar_t m_yaw;
extern cvar_t m_forward;
extern cvar_t m_side;



extern client_state_t cl;

// FIXME, allocate dynamically
extern lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
extern dlight_t cl_dlights[MAX_DLIGHTS];
extern entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
extern beam_t cl_beams[MAX_BEAMS];
extern entity_t** cl_visedicts;
extern int cl_numvisedicts;
extern int cl_maxvisedicts; // extended if we exceeded it the previous frame

//=============================================================================

//
// cl_main
//
dlight_t* CL_AllocDlight(int key);
void CL_DecayLights();

void CL_Init();

void CL_EstablishConnection(const char* host);
void CL_Signon1();
void CL_Signon2();
void CL_Signon3();
void CL_Signon4();

void CL_Disconnect();
void CL_Disconnect_f();
void CL_NextDemo();

//
// cl_input
//
typedef struct
{
    int down[2]; // key nums holding it down
    int state;   // low bit is down state
} kbutton_t;

extern kbutton_t in_mlook, in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;
extern kbutton_t in_grableft, in_grabright;
extern kbutton_t in_reloadleft, in_reloadright, in_flickreloadleft,
    in_flickreloadright;

void CL_InitInput();
void CL_AccumulateCmd(); // QSS
void CL_SendCmd();
void CL_SendMove(const usercmd_t* cmd);
int CL_ReadFromServer();
void CL_AdjustAngles(); // QSS
void CL_BaseMove(usercmd_t* cmd);

void CL_Download_Data();  // QSS
bool CL_CheckDownloads(); // QSS

void CL_ParseEffect(bool big); // QSS
void CL_ParseTEnt();
void CL_UpdateTEnts();

void CL_ClearState();
void CL_ClearTrailStates(); // QSS

//
// cl_demo.c
//
void CL_StopPlayback();
int CL_GetMessage();

void CL_Stop_f();
void CL_Record_f();
void CL_PlayDemo_f();
void CL_TimeDemo_f();

//
// cl_parse.c
//
void CL_ParseServerMessage();
void CL_RegisterParticles(); // QSS

//
// view
//
void V_StartPitchDrift();
void V_StopPitchDrift();

void V_RenderView();
// void V_UpdatePalette (); //johnfitz
void V_Register();
void V_ParseDamage();
void V_SetContentsColor(int contents);

//
// cl_tent
//
void CL_InitTEnts();
void CL_SignonReply();
float CL_TraceLine(const qvec3& start, const qvec3& end, const qvec3& impact,
    const qvec3& normal, int* ent); // QSS

//
// chase
//
extern cvar_t chase_active;

void Chase_Init();

struct trace_t;
[[nodiscard]] trace_t TraceLine(const qvec3& start, const qvec3& end);
[[nodiscard]] trace_t TraceLineToEntity(
    const qvec3& start, const qvec3& end, edict_t* ent);
void Chase_UpdateForClient();                                     // johnfitz
void Chase_UpdateForDrawing(refdef_t& refdef, entity_t* viewent); // johnfitz
