/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

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

// client.h

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
	char	average; //johnfitz
	char	peak; //johnfitz
} lightstyle_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors;			// two 4 bit fields
	byte	translations[VID_GRADES*256];
} scoreboard_t;

typedef struct
{
	int		destcolor[3];
	float	percent;		// 0-256
} cshift_t;


#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#define	NAME_LENGTH	64


//
// client_state_t should hold all pieces of the client state
//

#define	SIGNONS		4			// signon messages to receive before connected

#define	MAX_DLIGHTS		64 //johnfitz -- was 32
typedef struct
{
	vec3_t	origin;
	float	radius;
	float	spawn;				// stop lighting before this time
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
	int		key;
	vec3_t	color;				//johnfitz -- lit support via lordhavoc
} dlight_t;


#define	MAX_BEAMS	32 //johnfitz -- was 24
typedef struct
{
	int		entity;
	struct qmodel_s	*model;
	float	starttime;
	float	endtime;
	vec3_t	start, end;
} beam_t;

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
ca_dedicated, 		// a dedicated server with no ability to start a client
ca_disconnected, 	// full screen console with no connection
ca_connected		// valid netcon, talking to a server
} cactive_t;

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
	cactive_t	state;

// personalization data sent to server
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// demo loop control
	qboolean	demoloop;
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];	// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;

// did the user pause demo playback? (separate from cl.paused because we don't
// want a svc_setpause inside the demo to actually pause demo playback).
	qboolean	demopaused;

// demo playback speed (<0 = rewinding; 0 = paused; >0 = forward)
	float		demospeed;

// base demo speed; different from demospeed when fast-forwarding/rewinding or when playback is paused
// (we want to be able to set playback speed to 1/2x, pause, and then resume playback at 1/2x not 1x)
	float		basedemospeed;

	qboolean	timedemo;
	int		forcetrack;		// -1 = use normal cd track
	char		demofilename[MAX_OSPATH];
	FILE		*demofile;
	qfileofs_t	demofilestart;	// for demos in pak files
	qfileofs_t	demofilesize;
	int		td_lastframe;		// to meter out one message a frame
	int		td_startframe;		// host_framecount at start
	float		td_starttime;		// realtime at second frame of timedemo

// connection information
	int		signon;			// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server

} client_static_t;

// cls: the current seat's (#define below, with cl's)

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	int			movemessages;	// since connecting to this server
								// throw out the first couple, so the player
								// doesn't accidentally do something the
								// first frame
	usercmd_t	cmd;			// last command sent to the server
	usercmd_t	pendingcmd;		// accumulated state from mice+joysticks.

// information for local display
	int			stats[MAX_CL_STATS];	// health, etc
	float		statsf[MAX_CL_STATS];
	char		*statss[MAX_CL_STATS];
	int			items;			// inventory bit flags
	float	item_gettime[32];	// cl.time of aquiring item, for blinking
	float		faceanimtime;	// use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// and content types

// the client maintains its own idea of view angles, which are
// sent to the server each frame.  The server sets punchangle when
// the view is temporarliy offset, and an angle reset commands at the start
// of each level and after teleporting.
	vec3_t		mviewangles[2];	// during demo playback viewangles is lerped
								// between these
	vec3_t		viewangles;

	vec3_t		mvelocity[2];	// update by server, used for lean+bob
								// (0 is newest)
	vec3_t		velocity;		// lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// temporary offset
	double		punchtime;

// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;
	double		lastcenterstart;	// last time centerview was called

	float		wheel_pitch;	// for looking up/down using the mouse wheel

	float		viewheight;
	float		crouch;			// local amount for smoothing stepups

	qboolean	paused;			// send over by server
	qboolean	onground;
	qboolean	inwater;
	qboolean	fixangle;		// freeze view angle until next server frame

	int			intermission;	// don't change view angle, full screen, etc
	int			completed_time;	// latched at intermission start

	double		mtime[2];		// the timestamp of last two messages
	double		time;			// clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double		oldtime;		// previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups


	float		last_received_message;	// (realtime) for net trouble icon
	float		spawntime;		// time when signon 4 was received

//
// information that is static for the entire time connected to a server
//
	struct qmodel_s		*model_precache[MAX_MODELS];
	struct sfx_s		*sound_precache[MAX_SOUNDS];

	char		mapname[128];
	char		levelname[128];	// for display on solo scoreboard //johnfitz -- was 40.
	int			viewentity;		// cl_entitites[cl.viewentity] = player
	int			maxclients;
	int			gametype;

// refresh related state
	struct qmodel_s	*worldmodel;	// cl_entitites[0].model
	int			num_efrags;
	int			num_entities;	// held in cl_entities array
	int			num_statics;	// held in cl_staticentities array
	entity_t	viewent;			// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]

	unsigned	protocol; //johnfitz
	unsigned	protocolflags;

	qboolean	sendprespawn;

	char		stuffcmdbuf[1024];	//comment-extensions are a thing with certain servers, make sure we can handle them properly without further hacks/breakages. there's also some server->client only console commands that we might as well try to handle a bit better, like reconnect

	qcvm_t		qcvm;	//for csqc.

	float		zoom;
	float		zoomdir;

	qboolean	forceunderwater;	// force underwater warping/sound distortion even when camera is not submerged (e.g. alk1.2 liquidbrush)
} client_state_t;


//
// cvars
//
extern	cvar_t	cl_name;
extern	cvar_t	cl_color;

extern	cvar_t	cl_upspeed;
extern	cvar_t	cl_forwardspeed;
extern	cvar_t	cl_backspeed;
extern	cvar_t	cl_sidespeed;

extern	cvar_t	cl_movespeedkey;

extern	cvar_t	cl_yawspeed;
extern	cvar_t	cl_pitchspeed;

extern	cvar_t	cl_anglespeedkey;

extern	cvar_t	cl_alwaysrun; // QuakeSpasm

extern	cvar_t	cl_autofire;

extern	cvar_t	cl_shownet;
extern	cvar_t	cl_nolerp;

extern	cvar_t	cfg_unbindall;

extern	cvar_t	cl_pitchdriftspeed;
extern	cvar_t	freelook;
extern	cvar_t	lookspring;
extern	cvar_t	lookstrafe;
extern	cvar_t	sensitivity;

extern	cvar_t	m_pitch;
extern	cvar_t	m_yaw;
extern	cvar_t	m_forward;
extern	cvar_t	m_side;

extern	cvar_t	cl_startdemos;
extern	cvar_t	cl_confirmquit;


#define	MAX_TEMP_ENTITIES	256		//johnfitz -- was 64
#define	MAX_STATIC_ENTITIES	4096	//ericw -- was 512	//johnfitz -- was 128
#define	MAX_VISEDICTS		16384	// larger, now we support BSP2

// QVR (splitscreen phase 5, docs/vr-port/SPLITSCREEN.md): a seat's client, its connection and its client world: Ironwail's
// cls, cl and the entity tables (once cl_main.c's, cl_tent.c's and cl_parse.c's globals), one a seat (vr_seat.hpp's
// Seat::net). The names below are the current seat's (cl_seat: seat::Scope and CL_SeatSet move it), so the code's
// thousands of `cl.` and `cls.` read as before. With one seat cl_seat is seat 0's, always. Because `cl` and `cls` are
// macros, nothing else may be named so (a local or a member named so fails to compile: rename it).
typedef struct clientseat_s
{
	client_static_t	cstatic;	// cls
	client_state_t	cstate;		// cl
	entity_t		static_entities[MAX_STATIC_ENTITIES];
	lightstyle_t	lightstyle[MAX_LIGHTSTYLES];
	dlight_t		dlights[MAX_DLIGHTS];
	entity_t		temp_entities[MAX_TEMP_ENTITIES];
	int				num_temp_entities;
	beam_t			beams[MAX_BEAMS];
	entity_t		*visedicts[MAX_VISEDICTS];
	int				numvisedicts;
	unsigned int	entityupdates;	// CL_ParseUpdate calls so far (worldtrace::world's brush entity list)
	entity_t		*entities;		//johnfitz -- was a static array, now on hunk
	int				max_edicts;		//johnfitz -- only changes when new map loads
	qboolean		warn_nehahra;	// cl_parse.c's warning, once a connection
	qboolean		ready;			// seats 1-3: made ready on first use (CL_SeatNetReady)
	byte			messagebuf[4096];	// seats 1-3's cls.message (seat 0's is CL_Init's, on the hunk as before)
	// the seat's loopback pair (net_loop.c; phase 6): its client side (cls.netcon once connected), the server's side,
	// and a connect waiting for the server to take it
	struct qsocket_s	*loopclient;
	struct qsocket_s	*loopserver;
	qboolean		loopconnect;
} clientseat_t;

extern	clientseat_t	*cl_seat;	// the current seat's (vr_seat.cpp)
clientseat_t *CL_SeatNetOf (int seat);	// seat 0..3's, made ready (vr_seat.cpp)
void CL_SeatNetReady (clientseat_t *net);	// a fresh seat's: disconnected, its message buffer (cl_main.c)

#define	cls					(cl_seat->cstatic)
#define	cl					(cl_seat->cstate)
#define	cl_static_entities	(cl_seat->static_entities)
#define	cl_lightstyle		(cl_seat->lightstyle)
#define	cl_dlights			(cl_seat->dlights)
#define	cl_temp_entities	(cl_seat->temp_entities)
#define	num_temp_entities	(cl_seat->num_temp_entities)
#define	cl_beams			(cl_seat->beams)
#define	cl_visedicts		(cl_seat->visedicts)
#define	cl_numvisedicts		(cl_seat->numvisedicts)
#define	cl_entityupdates	(cl_seat->entityupdates)
#define	cl_entities			(cl_seat->entities)
#define	cl_max_edicts		(cl_seat->max_edicts)

//=============================================================================

//
// cl_main
//
dlight_t *CL_AllocDlight (int key);
void	CL_DecayLights (void);
void	CL_SetLightstyle (int i, const char *str);

void CL_Init (void);

void CL_EstablishConnection (const char *host);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);

//
// cl_input
//
typedef struct
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
} kbutton_t;

// QVR: splitscreen (docs/vr-port/SPLITSCREEN.md): the command buttons and the impulse are a seat's own (Ironwail's
// in_forward, in_attack... globals). Their +commands and the move act on the current seat's (CL_SeatInput); C++
// reaches them as seat::current().input (vr_seat.hpp).
typedef struct seatinput_s
{
	kbutton_t	mlook, klook;
	kbutton_t	left, right, forward, back;
	kbutton_t	lookup, lookdown, moveleft, moveright;
	kbutton_t	strafe, speed, use, jump, attack;
	kbutton_t	up, down;
	int			impulse;
} seatinput_t;

seatinput_t *CL_SeatInput (void);			// the current seat's (vr_seat.cpp)
seatinput_t *CL_SeatInputOf (int seat);	// seat 0..3's (seat 0's: the config's +mlook line)
// QVR: splitscreen phase 3: a seat's view (view.c's globals and statics) and the window area it is drawn in. The view's
// and HUD's draws take the seat's (V_RenderView, SCR_UpdateScreen's 2D pass: the current seat's, passed down); a
// message or command that changes them (a hit's kick, v_cshift, a centre print, +showscores) the current seat's.
typedef struct seatview_s
{
	vec3_t		punchangles[2];		// cl.punchangle as the view saw it: 0 now, 1 before (johnfitz's v_punchangles)
	float		dmg_time, dmg_roll, dmg_pitch;	// a hit's kick (V_ParseDamage)
	float		blend[4];			// the screen's tint, rgba 0..1 (v_blend)
	cshift_t	contents;			// v_cshift's empty contents' tint (cshift_empty; the demo rewind keeps it)
	float		gunyaw, gunpitch;	// CalcGunAngle's smoothing (its old statics)
	float		stepz;				// V_CalcRefdef's stair smoothing (its old static oldz)
	float		liftz;				// the view entity's height last frame (a lift's rise followed: VR_ViewLiftRise)
	vrect_t		rect;				// the window area this seat's view and HUD are drawn in: the whole window until
									// the layouts (SPLITSCREEN.md phases 7, 11: SCR_SeatRect)
	vrect_t		viewrect;			// the 3D view's part of it (scr_vrect: viewsize, the status bar's room)
	// phase 7: the seat's last drawn view (gl_screen.c SCR_DrawSeats; vr_seat_render_test reads them)
	int			drawnframes;		// its tile drawn this many times
	vrect_t		drawnrect;			// the tile it was drawn in
	vec3_t		drawnorg;			// the view's origin then (r_refdef.vieworg)
	vec3_t		drawnright;			// ... and its right (vright): the seat's sound listener (snd_dma.c)
	byte		probe[4];			// scr_seatmarkers 2: its marker's pixel read back after the frame (rgb)
	unsigned	probesum;			// ... and a checksum of the tile's middle (each tile a picture of its own)
} seatview_t;

// QVR: splitscreen phase 3: a seat's HUD state (gl_screen.c's centre print, sbar.c's scores shown).
typedef struct seathud_s
{
	char		centerstring[1024];	// the centre print (scr_centerstring...)
	float		centertime_start;	// cl.time it began: the finale's slow printing
	float		centertime_off;		// seconds it has left
	int			center_lines, center_maxcols, erase_lines, erase_center;
	qboolean	showscores;			// +showscores held (sb_showscores)
} seathud_t;

seatview_t *V_SeatView (void);				// the current seat's (vr_seat.cpp)
seatview_t *V_SeatViewOf (int seat);
seathud_t *SCR_SeatHud (void);				// the current seat's
seathud_t *SCR_SeatHudOf (int seat);

int CL_SeatIndex (void);					// the current seat (0 outside a seat loop)
int CL_SeatSet (int seat);					// makes a seat current, returns the one that was (C's seat::Scope: set it back)
#define MAX_SPLITSEATS 4					// QVR: the seats there are (seat::maxSeats)
int CL_SeatCount (void);					// the seats there are (seat::maxSeats)
int CL_SeatsInPlay (void);					// the seats playing (seat::inPlay; seat 0 always)
// Phase 6 (docs/vr-port/SPLITSCREEN.md): seats 1-3 join the local game and leave it (vr/vr_seatplay.cpp), each over its
// own loopback connection (net_loop.c) into a server slot of its own; the host frame runs each playing seat's client
// frame with that seat current (host.c). Until phase 7's per-seat render they run headless: no view, sound, music,
// particles or links into the shared world (CL_SeatHeadless).
int CL_SeatPlays (int seat);				// seat 0..3 in play
int CL_SeatPad (int seat);					// the pad that joined seat 1..3 (-1: none)
qboolean CL_SeatHeadless (void);			// the current seat is one of seats 1-3: no sound, music, particles or links into
											// the shared world (seat 0's world has them; phases 7-8 for the rest)
qboolean CL_SeatDrawn (void);				// the current seat's view is drawn (seat 0, or any seat in a tile: phase 7):
											// its entities relinked each frame
// Phase 7 part 2: the shared effects (one particle pool, drawn in every tile; the world's sounds) fed once per event. Each
// seat's client reads the same event (an explosion, a puff, a sound) when it is in that seat's sight: the first seat
// this frame to feed it does, the others skip it (kind, key and the place rounded to a unit; key: an entity's number for
// its trail, or 0). A seat not drawn feeds nothing; one seat drawn: always true, as before.
enum { SEATFX_EXPLOSION, SEATFX_EXPLOSION2, SEATFX_BLOB, SEATFX_EFFECT, SEATFX_LAVA, SEATFX_TELEPORT, SEATFX_TRAIL,
	SEATFX_BRIGHTFIELD, SEATFX_DEBRIS, SEATFX_SOUND, SEATFX_VR };
qboolean CL_SeatEffectFirst (int kind, int key, const float *org);
// Phase 7 part 3: the drawn seats' listeners for the sounds (snd_dma.c; seatview_t's drawnorg and drawnright, the seat's
// player's entity): their count, 0 unless seats are drawn in tiles (the one listener, as before).
int CL_SeatListeners (vec3_t org[MAX_SPLITSEATS], vec3_t right[MAX_SPLITSEATS], int ent[MAX_SPLITSEATS]);
// Phase 7 part 3: a server event's effects fed once (Quake VR's spawners: gore, impacts, the gun effects, ...). Around a
// temporary entity's or a Quake VR message's parse (cl_parse.c, vr_client.cpp): the bytes read are the event (the same in
// every seat's client that reads it); a later seat's identical one this frame has the particles it spawned (both pools)
// taken back. Seats in tiles only (else nothing: one seat as before).
// Phase 8: known as the parse begins (repeat: the length of an identical event another drawn seat fed this frame, found
// by looking ahead in the message; 0: a first), so the parsers whose memories are all shared (gore, wounds, hit reactions,
// gore sounds) skip a repeat's bytes, and others (tracers, shocks, casings) keep only their seat's own part of it.
typedef struct { int readcount, vrparticles, particles, on, repeat, prevrepeat; } seatfxmark_t;
void CL_SeatFxBegin (seatfxmark_t *mark, int readcount);
void CL_SeatFxEnd (const seatfxmark_t *mark);
int CL_SeatFxRepeat (void);					// inside a parse (CL_SeatFxBegin/End): the repeat's length, 0 if none
int R_ParticleCount (void);
void R_ParticleTruncate (int n);
clientseat_t *CL_SeatNetIfMade (int seat);	// seat 0..3's client if ever made (NULL: never used)
void CL_SeatName (int seat, char *out, size_t size);	// the player's name for seat n: cl_name, then "<cl_name> <n+1>"
void CL_SeatFrame (void);					// once a host frame: seats joining (connecting) or gone (vr_seatplay.cpp)
// Splitscreen phase 9 (vr_seatmenu.cpp): seats 2-4's own menus, drawn in their tiles; seat 1 has the full menu and the
// console. scr_splitmenu 0 (default): a seat 2-4's Start opens its seat menu; 1: the full menu, as before phase 9.
int VR_SeatMenusOn (void);					// scr_splitmenu 0 with more than one seat in play: seats 2-4 have their own menus
int VR_SeatMenuIsOpen (int seat);			// seat 1..3's menu open
void VR_SeatMenuToggle (void);				// the current seat's (1-3) menu opened or shut (its Start)
void VR_SeatMenuKey (int key, int down);	// a key of the current seat (1-3) while its menu is open
void VR_SeatMenuDraw (void);				// the current seat's menu (or the pause's note) in its tile (gl_screen.c's seat pass)
int VR_SeatMenusPause (void);				// scr_splitpause: a menu open (seat 1's full one or a seat menu) holds the local game
float VR_SeatLookScale (void);				// the current seat's stick look sensitivity (seat 0: exactly 1)
int VR_SeatInvertLook (void);				// the current seat's own look invert (seat 0: 0)
void CL_SeatConnectLocal (void);			// the current seat (1-3) connects to the local server (cl_main.c)
void CL_SeatDisconnect (void);				// the current seat (1-3) leaves its server, the game running on (cl_main.c)
qboolean Host_SeatsMaySave (const char *name);	// more than one seat: the level-start coop autosave only (host_cmd.c)
float CL_KeyState (kbutton_t *key);			// a button's share of the frame held (clears its edges)

void CL_InitInput (void);
void CL_AccumulateCmd (void);
void CL_SendCmd (void);
void CL_SendMove (const usercmd_t *cmd);
int  CL_ReadFromServer (void);
void CL_ReadLockstep (void); // QVR: a lockstep client's waiting frame's reading (vr_mp_lockstep_ahead, B14e)
void CL_AdjustAngles (void);
void CL_BaseMove (usercmd_t *cmd);
void CL_ReleaseMoveButtons (void); // QVR: every held movement button let go (a menu opened over the running game)
int  CL_MoveButtonsHeld (void);    // QVR: how many are held (vr_menu_stop_test)
qboolean CL_InCutscene (void);

qboolean CL_IsPlayerEnt (const entity_t *ent);

void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);

void CL_FreeState(void);
void CL_ClearState (void);
void CL_ForgetModels (void); // QVR: COM_SwitchGameInternal, the models reset: the client's state as at start-up

//
// cl_demo.c
//
void CL_StopPlayback (void);
int CL_GetMessage (void);
void CL_ClearSignons (void);
void CL_AdvanceTime (void);
void CL_FinishDemoFrame (void);
void CL_AddDemoRewindSound (int entnum, int channel, sfx_t *sfx, vec3_t pos, int vol, float atten);

void CL_Stop_f (void);
void CL_Record_f (void);
void CL_PlayDemo_f (void);
void CL_TimeDemo_f (void);

//
// cl_parse.c
//
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot);

//
// view
//
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_RenderView (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);

//
// cl_tent
//
void CL_InitTEnts (void);
void CL_SignonReply (void);

//
// chase
//
extern	cvar_t	chase_active;

void Chase_Init (void);
void TraceLine (vec3_t start, vec3_t end, vec3_t impact);
void Chase_UpdateForClient (void);	//johnfitz
void Chase_UpdateForDrawing (void);	//johnfitz

#endif	/* _CLIENT_H_ */

