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

#ifndef _CLIENT_H_
#define _CLIENT_H_

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
#define MAX_VISEDICTS 4096       // larger, now we support BSP2

#include "quakeglm.hpp"
#include "quakedef.hpp"

// client.h

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
    byte translations[VID_GRADES * 256];
} scoreboard_t;

typedef struct
{
    int destcolor[3];
    int percent; // 0-256
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

} client_static_t;

extern client_static_t cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
struct client_state_t
{
    int movemessages; // since connecting to this server
                      // throw out the first couple, so the player
                      // doesn't accidentally do something the
                      // first frame
    usercmd_t cmd;    // last command sent to the server

    // information for local display
    int stats[MAX_CL_STATS]; // health, etc
    int items;               // inventory bit flags
    float item_gettime[32];  // cl.time of aquiring item, for blinking
    float faceanimtime;      // use anim frame if cl.time < this

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

    qvec3 mvelocity[2]; // update by server, used for lean+bob
                            // (0 is newest)
    qvec3 velocity;     // lerped between mvelocity[0] and [1]

    qvec3 punchangle; // temporary offset

    // pitch drifting vars
    float idealpitch;
    float pitchvel;
    bool nodrift;
    float driftmove;
    double laststop;

    float viewheight;
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
    int viewentity;      // cl_entities[cl.viewentity] = player
    int maxclients;
    int gametype;

    // refresh related state
    qmodel_t* worldmodel; // cl_entities[0].model
    struct efrag_s* free_efrags;
    int num_efrags;
    int num_entities; // held in cl_entities array
    int num_statics;  // held in cl_staticentities array

    // TODO VR: (P2) all of these should be in some sort of list to avoid
    // repetition
    entity_t viewent;         // the gun model
    entity_t offhand_viewent; // the offhand gun model

    entity_t left_hip_holster;
    entity_t right_hip_holster;
    entity_t left_upper_holster;
    entity_t right_upper_holster;
    entity_t vrtorso;
    entity_t left_hand;
    entity_t right_hand;
    entity_t left_hip_holster_slot;
    entity_t right_hip_holster_slot;
    entity_t left_upper_holster_slot;
    entity_t right_upper_holster_slot;

    entity_t mainhand_wpn_button;
    entity_t offhand_wpn_button;

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

    int cdtrack, looptrack; // cd audio

    // frag scoreboard
    scoreboard_t* scores; // [cl.maxclients]

    unsigned protocol; // johnfitz
    unsigned protocolflags;
};

template <typename F>
bool anyViewmodel(client_state_t& clientState, F&& f)
{
    return                                            //
        f(clientState.viewent)                        //
        || f(clientState.offhand_viewent)             //
        || f(clientState.left_hip_holster)            //
        || f(clientState.right_hip_holster)           //
        || f(clientState.left_upper_holster)          //
        || f(clientState.right_upper_holster)         //
        || f(clientState.left_hand)                   //
        || f(clientState.right_hand)                  //
        || f(clientState.vrtorso)                     //
        || f(clientState.left_hip_holster_slot)       //
        || f(clientState.right_hip_holster_slot)      //
        || f(clientState.left_upper_holster_slot)     //
        || f(clientState.right_upper_holster_slot)    //
        || f(clientState.mainhand_wpn_button)         //
        || f(clientState.offhand_wpn_button)          //
        || f(clientState.left_hand_entities.base)     //
        || f(clientState.left_hand_entities.f_thumb)  //
        || f(clientState.left_hand_entities.f_index)  //
        || f(clientState.left_hand_entities.f_middle) //
        || f(clientState.left_hand_entities.f_ring)   //
        || f(clientState.left_hand_entities.f_pinky)  //
        || f(clientState.right_hand_entities.base)     //
        || f(clientState.right_hand_entities.f_thumb)  //
        || f(clientState.right_hand_entities.f_index)  //
        || f(clientState.right_hand_entities.f_middle) //
        || f(clientState.right_hand_entities.f_ring)   //
        || f(clientState.right_hand_entities.f_pinky);
}

template <typename F>
void forAllViewmodels(client_state_t& clientState, F&& f)
{
    anyViewmodel(clientState, [&](entity_t& e) {
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
extern entity_t cl_static_entities[MAX_STATIC_ENTITIES];
extern lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
extern dlight_t cl_dlights[MAX_DLIGHTS];
extern entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
extern beam_t cl_beams[MAX_BEAMS];
extern entity_t* cl_visedicts[MAX_VISEDICTS];
extern int cl_numvisedicts;

extern entity_t* cl_entities; // johnfitz -- was a static array, now on hunk
extern int cl_max_edicts;     // johnfitz -- only changes when new map loads

//=============================================================================

//
// cl_main
//
dlight_t* CL_AllocDlight(int key);
void CL_DecayLights(void);

void CL_Init(void);

void CL_EstablishConnection(const char* host);
void CL_Signon1(void);
void CL_Signon2(void);
void CL_Signon3(void);
void CL_Signon4(void);

void CL_Disconnect(void);
void CL_Disconnect_f(void);
void CL_NextDemo(void);

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

void CL_InitInput(void);
void CL_SendCmd(void);
void CL_SendMove(const usercmd_t* cmd);
int CL_ReadFromServer(void);
void CL_BaseMove(usercmd_t* cmd);

void CL_ParseTEnt(void);
void CL_UpdateTEnts(void);

void CL_ClearState(void);

//
// cl_demo.c
//
void CL_StopPlayback(void);
int CL_GetMessage(void);

void CL_Stop_f(void);
void CL_Record_f(void);
void CL_PlayDemo_f(void);
void CL_TimeDemo_f(void);

//
// cl_parse.c
//
void CL_ParseServerMessage(void);
void CL_NewTranslation(int slot);

//
// view
//
void V_StartPitchDrift(void);
void V_StopPitchDrift(void);

void V_RenderView(void);
// void V_UpdatePalette (void); //johnfitz
void V_Register(void);
void V_ParseDamage(void);
void V_SetContentsColor(int contents);

//
// cl_tent
//
void CL_InitTEnts(void);
void CL_SignonReply(void);

//
// chase
//
extern cvar_t chase_active;

void Chase_Init(void);

struct trace_t;
[[nodiscard]] trace_t TraceLine(const qvec3& start, const qvec3& end);
[[nodiscard]] trace_t TraceLineToEntity(
    const qvec3& start, const qvec3& end, edict_t* ent);
void Chase_UpdateForClient(void);                                 // johnfitz
void Chase_UpdateForDrawing(refdef_t& refdef, entity_t* viewent); // johnfitz

#endif /* _CLIENT_H_ */
