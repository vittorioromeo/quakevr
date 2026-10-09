/*
Copyright (C) 2020-2026 Vittorio Romeo and Quake VR contributors

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

// vr_api.h -- the interface between the engine and the Quake VR module (Quake/vr/, C++): the hooks
// every QuakeSpasm-lineage engine shares (host, filesystem, protocol, server, QuakeC, physics,
// menu). Engine code calls into VR only through these and vr_api_render.h, at call sites marked
// "QVR"; porting the module to another engine means placing the same calls (docs/vr-port/PORTING.md).
// The renderer's hooks, which each engine's renderer needs its own way, are in vr_api_render.h.

#ifndef QVR_VR_API_H
#define QVR_VR_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct client_s;
struct edict_s;
struct qmodel_s;
struct sizebuf_s;

// Teleporter brushes use water contents in BSP, but their interactions have no liquid effects.
int VR_LiquidContents(struct qmodel_s* model, const float* point, int contents);
int VR_NoLiquidEffects(struct qmodel_s* model, const float* point);

// PROTOCOL_RMQ flags (see vr/vr_protocol.hpp): the VR protocol (a server for VR clients, whatever
// its progs), and progs implementing Quake VR's gameplay (without it, VR in compatibility mode).
#define PRFL_QUAKEVR				(1 << 16)
#define PRFL_QUAKEVR_PROGS			(1 << 17)
#define VR_ENTITY_UPDATE_MAXSIZE	40		// bytes VR_WriteEntityUpdate may add
#define SOLID_NOT_BUT_TOUCHABLE		5		// Quake VR: not solid, but can be (hand) touched

// Host lifetime and pacing (host.c, main_sdl.c, gl_screen.c).
void VR_Init (void);		// after SV_Init (also on dedicated servers): registers cvars and commands
void VR_NewMap (void);		// R_NewMap: a map loaded (the per-map data rebuilt, even for the same model)
void VR_Shutdown (void);	// client shutdown, before video shutdown
void VR_StopDownloads (void);	// Host_Shutdown, before NET_Shutdown (curl_global_cleanup): the map index's and the
								// map installer's threads cancelled and joined (vr_mapindex.cpp, vr_mapinstall.cpp)
void VR_BeginFrame (void);	// once per host frame, after input events and before console commands
int VR_IsActive (void);		// nonzero while vr_enabled is set and a backend session is running: the
							// runtime paces frames (no frame cap, no sleeping when unfocused)
int VR_Unpaced (void);		// nonzero while the mock headset runs frames unpaced (vr_mock_fast, the game's clock
							// fixed): no frame cap
int VR_SkipScreen (void);	// SCR_UpdateScreen: nonzero to skip this frame's drawing (vr_mock_fast 2: unpaced
							// frames aren't drawn)
void VR_HeadlessView (void);	// ... and instead: the eyes' views and view entities set up (what the game reads), no GL
int VR_SkipSwap (void);		// GL_EndRendering: nonzero to leave this frame unpresented (unpaced frames present
							// ten a second: a present waits for the display's refresh, the bulk of such a frame)
void VR_FrameDrawn (void);	// GL_EndRendering, before the present: vr_screenshot_frames's screenshot of it
int VR_ModalMessageFrame (void); // SCR_ModalMessage's loop: with a headset, a frame showing the
							// dialog (the runtime paces it); zero without one (the loop sleeps)
int VR_TestModalAnswer (void);	// SCR_ModalMessage, M_Confirm: vr_test_modal_answer's answer once the dialog has shown half a
							// second (1 yes, 0 no; tests), else -1
double VR_HostFrameTime (double time);	// start of _Host_Frame: the frame's time (a motion take's own while
							// it plays back, vr_motion_play: the same frames at any speed)
int VR_Box3DSteps (void);		// host_tickstats: Box3D's world steps so far (0 without a world)
int VR_ServerFrameOverride (double *frametime); // _Host_Frame: whether the server runs this frame: -1 as
							// usual; 0 no; 1 yes, for *frametime seconds (a take's recorded server frames)
// Slow motion (vr_timescale.cpp; ROUND21.md, "Slow motion").
void VR_AdvanceTime (double dt);	// Host_AdvanceTime, after realtime: the time scale's ease, vr_gametime
double VR_TimeScale (void);		// the game's time per real second: 1 unless slow motion (vr_timescale, single player)
float VR_SndRate (void);		// the sounds' playback rate: VR_TimeScale with vr_timescale_sound, else 1
extern double vr_gametime;		// realtime slowed by the time scale (equal to it until slow motion is first used):
							// what VR's own client simulations step and time on (the player's body: real in Sandevistan)
double VR_PlayerMoveSpeedup (void);	// how many times faster than the world the player moves (1; Sandevistan,
							// vr_timescale_move_realtime: 1 over the scale)
double VR_PlayerRunBegin (void);	// Host_ServerFrame, round SV_RunClients: the player's moves in its own time;
void VR_PlayerRunEnd (double world);	// ... the world's frame put back (VR_PlayerRunBegin's result)
struct edict_s;
double VR_PhysicsEntityBegin (struct edict_s *ent, int num); // SV_Physics, round an entity's: the player and its
							// missiles in the player's own time (0: in the world's)
void VR_PhysicsEntityEnd (struct edict_s *ent, int num, double world); // ... its timers into the world's time
double VR_ThinkFrame (double frametime); // SV_RunThink: how far ahead a think is due (the world's frame, also
							// for an entity in the player's time)
void VR_HostFrameEnd (void);	// end of _Host_Frame, after the screen and the sound (the motion recorder's row)

// The frame cap (vr_sleep.cpp).
int VR_HiResSleepUntil (double endtime, double *now); // Sys_WaitUntil: sleeps on a high-resolution timer until its lateness before endtime; 0 without one

// The console (vr_cmdtoken.cpp); and the command system's high-water marks (cmd.c's cmd_limits, for vr_limits).
typedef struct
{
	int cbuf_peak;		// the command buffer's largest content, bytes
	int longest_line;	// the longest command line run
	int longest_token;	// the longest argument
	int max_argc;		// the most arguments in a command
} cmdlimits_t;
const char *VR_ParseToken (const char *data, const char **token); // Cmd_TokenizeString: COM_Parse for an argument of any length

// Automated test runs (QVR_NO_ERROR_DIALOG; vr_crash.cpp, Windows only).
const char *VR_BuildVersion (void);	// vr_crash.cpp: this build: its version, the last commit's date and short hash ("1.0.0-dev (2026-10-07 9460b8e1)"; "-dirty": changed files)
const char *VR_Version (void);		// vr_crash.cpp: the version, the repository's VERSION file ("1.0.0"; docs/vr-port/RELEASING.md, "Versions")
int VR_VersionIsDev (void);		// vr_crash.cpp: 1 unless the release script built this (Misc/release/make_release.ps1)
void VR_InstallCrashHandler (void);	// main, first: a crash writes qvr_crash.txt (the stack, the map) and qvr_crash.dmp (test runs and players' alike); Zancle's asserts reported (vr_zancle.cpp)
int VR_ErrorDialogSuppressed (const char *errorMsg);	// PL_ErrorDialog: nonzero if written to qvr_error.txt instead

// Start-up and map-load timing (vr_startup.cpp: vr_startup_times, vr_walltime).
void VR_TimeStart (void);	// main, after Sys_Init: the process's start
void VR_TimeInit (void);	// VR_Init: the commands
void VR_TimeMark (const char *stage);	// a stage of the start-up or of a map's load just ended
void VR_TimeLoadBegin (const char *what);	// SV_SpawnServer, CL_ParseServerInfo: a map's load starts
void VR_TimeLoadCommand (const char *what);	// map, changelevel, restart, load: the load's timing starts at the command (its spawn continues it)
// Screenshots saved on the game's thread pool (vr/vr_voicenotes.cpp): SCR_ScreenShot_f hands a PNG's RGB rows (bottom
// up, malloc'd: the job frees them) to be written as <game dir>/<name> (1: taken); a name being written is pending
// (not free for the next screenshot); shutdown waits for them.
int VR_ScreenshotWrite (const char *name, unsigned char *rgb, int width, int height);
int VR_ScreenshotPending (const char *name);
int VR_OnMainThread (void);	// whether this is the main thread (the console is only for it: a job on the game's thread pool prints nothing)
void VR_TimeAdd (const char *what, double seconds);	// time spent in a kind of work (model loads, normal maps...), summed per stage group
#define VR_TIMED(what, statement) do { double vr_timed_t0 = Sys_DoubleTime (); statement; VR_TimeAdd (what, Sys_DoubleTime () - vr_timed_t0); } while (0) // the statement's time, as VR_TimeAdd
void VR_TimeFrameEnd (int signedon, int idle);	// end of _Host_Frame: the first frame ends the start-up, the first signed on a load; idle (no server, not connected) ends a load that failed

// The loose files' presence while the game starts and a map loads (vr_fscache.cpp; COM_FindFile, Sys_fopen).
int VR_FileCacheHas (const char *path);	// 1 a file, 0 none, -1 not known (ask the file system)
void VR_FileCacheEnable (int on);	// VR_TimeStart, VR_TimeLoadBegin on; the first frame drawn off
void VR_FileCacheForget (void);	// a file written, a directory made

// The in-game relighting's line outside the menu on a flat screen (vr_relight.cpp; gl_screen.c SCR_DrawRelight): its text
// ("RELIGHT 3/12 45% 2:10") and how far it is (0..1), or null when none runs, the wrist gadget shows it, or a menu does.
const char *VR_RelightIndicator (float *fraction);

// Images decoded ahead on worker threads (vr_imgprefetch.cpp; image.c Image_LoadImage).
unsigned char *VR_ImagePrefetchTake (const char *name, FILE *f, int length, int *width, int *height);
void VR_ImagePrefetchNote (const char *name, double seconds);
void VR_ImagePrefetchEnd (void);	// the first map load's end: the workers joined, the rest freed, the list written
unsigned char *VR_ImageCacheFind (const char *name, FILE *f, int length, int *width, int *height, unsigned char *(*alloc) (int bytes, const char *what), const char *what); // decoded before (vr_imgcache.cpp): a copy from alloc, or NULL
int VR_ImageCachePut (const char *name, FILE *f, int length, unsigned char *pixels, int width, int height); // ... and kept now (1: the cache frees pixels)

// The normal maps made from skins, kept on disk (vr_texcache.cpp; gl_texmgr.c TexMgr_LoadImage32).
int VR_NormalCacheMode (void);	// vr_normalmap_cache: 0 off, 1 on, 2 check
int VR_NormalCacheLoad (const char *build, unsigned long long key, unsigned char *rgba, int width, int height);
void VR_NormalCacheStore (const char *build, unsigned long long key, const unsigned char *rgba, int width, int height);
void VR_NormalCacheChecked (int same, const char *name);	// vr_normalmap_cache 2: a map made again against its file	// end of _Host_Frame: the first frame drawn ends the start-up, and a load once signed on

// Filesystem (common.c).
void VR_BeforeAddGameDirectory (const char *dir);	// start of COM_AddGameDirectory
void VR_AfterAddGameDirectory (const char *dir);	// end of COM_AddGameDirectory
void VR_OnGameDirChanged (void);	// COM_SwitchGame, after Mod_ResetAll and the renderer's reload: caches of models and game files emptied
int VR_SkipSearchPath (const char *filename, const char *path);	// COM_FindFile: nonzero to skip a search path
void COM_AddAddonPath (const char *path);
void VR_CheckSpawnCampaignMap (const char *map);	// SV_SpawnServer: the map against the running campaign (Host_Error, never a switch)
void VR_ReloadVRGameKeepCampaign (void);	// vr_gamedir.cpp: the game folders rebuilt, the selected campaign kept
int VR_QuakeVRMounted (void);	// vr_gamedir.cpp: quakevr is on the search path	// common.c: a map package's folder on top of the search path (vr_mapinstall.cpp)
int VR_AddonForMapCommand (const char *map);	// Host_Map_f: the map package the map is played from made the active one (0: refused)
void VR_AddonForSave (const char *savepath, const char *map);	// Host_Loadgame_f: the save's map package made the active one
void VR_AddonOnSave (const char *savepath);	// Host_Savegame_f: the active map package noted beside the save
void VR_NoteMapSpawn (const char *map);	// SV_SpawnServer: the map and the map package mounted, the crash report's context line
void VR_SetCrashContext (const char *what);	// vr_crash.cpp: that line (qvr_crash.txt's second)
unsigned VR_DescribeCallers (char *out, int outSize, int skip, int depth);	// vr_crash.cpp: the caller's stack as one line ("fn (file.c:12) < caller ..."); a hash of it (0: none; not Windows)
const char *VR_ModelFile (const char *name);	// Mod_LoadModel, Mod_LoadLighting: the file to load a model from (relit maps)
int VR_ModelReplacementOk (const char *name, const char *md5mesh);	// loadMd5Replacement: 0 refuses a jointed hand the rig can't use (vr_handrig.cpp)
void VR_AliasPosesLoaded (const char *name, void *aliashdr, const stvert_t *stverts, const dtriangle_t *tris, trivertx_t **poses); // Mod_LoadAliasModel, after the frames

// Server QuakeC (pr_edict.c, pr_cmds.c, sv_main.c, host_cmd.c).
void VR_OnProgsLoaded (void);			// end of PR_LoadProgs, with the loaded qcvm current
void VR_OnSpawnServerBeforeLoad (void);	// SV_SpawnServer, before ED_LoadFromFile
void VR_OnEntitySpawned (edict_t *ent);	// ED_LoadFromFile, after an entity's spawn function ran
void VR_OnSpawnServerSpawned (void);		// SV_SpawnServer, after ED_LoadFromFile (before the settling frames)
void VR_OnClearMemory (void);			// Host_ClearMemory, before the hunk (edicts, cl_entities, models) is freed: every pointer into it forgotten
void VR_MonsterFell (edict_t *ent, float speed);	// SV_Physics_Step, a walking monster landed at `speed` (QC VR_Monster_Fall)
void VR_OnEdictFree (edict_t *ed);	// ED_Free (any VM's)
void VR_OnEdictAlloc (edict_t *ed);	// ED_Alloc (any VM's), the edict cleared: when the server's was made (.vr_born; vr_cheats.cpp)
// The edict index (vr_edictindex.cpp): find() on .classname and findflags() on a few fields without walking every edict.
// Every change to an edict's free flag, classname or watched fields reaches it through these (server VM; others ignored).
void VR_EdictIndex_Touch (edict_t *ed);		// the engine changed the edict (freed, taken, cleared, parsed, its classname set)
void VR_EdictIndex_Reset (void);			// everything read again at the next query (a load)
void VR_EdictIndex_StringSlot (int slot, int stable); // a known string's slot (re)assigned: stable when its text is never changed (PR_AllocString)
void VR_EdictIndex_Address (int ofs, int watched);	// OP_ADDRESS of a watched field (fieldwatch's value): its store to come
void VR_EdictIndex_Stored (int ofs);		// OP_STOREP into ofs while some are pending (watchpending)
void VR_EdictIndex_TopLevelDone (void);		// PR_ExecuteProgram, the server's outermost call returned
int VR_EdictIndex_Find (int start, int field, const char *s);	// PF_Find: the next match after start (0: none), -1 to walk
int VR_EdictIndex_FindFlags (int start, int field, int flags);	// PF_findflags: the same
int VR_MonsterFrozen (struct edict_s *ent);	// SV_Physics, past the clients: a living monster frozen (vr_freeze_monsters): skipped
void VR_OnSpawnServerAfterLoad (void);	// SV_SpawnServer, after serverinfo is sent
void VR_OnBeginLoadGame (void);			// Host_Loadgame_f, before SV_SpawnServer
void VR_CheckLoadedReferences (int num_edicts);	// Host_Loadgame_f, the edicts parsed: an entity reference past them is the world (a dev warning)
void VR_OnLoadGame (void);				// Host_Loadgame_f, after globals and edicts are restored
// Saved games: after the light styles, `// qvr_save <format> progs <crc> build <build>` and `// qvr_model <i> <name>` for
// the model precache list (SaveData_WriteHeader). VR_SAVE_FORMAT goes up when a save this build writes would load wrong
// in an older one; a save of a newer format is refused (VR_ReadSaveInfo).
#define VR_SAVE_FORMAT 1
int VR_ReadSaveInfo (const char *text, const char *relname);	// Host_Loadgame_f, before the old game ends: the save's build and models read (0: refused)
void VR_SaveFlashlightState (void); // before a save snapshot or changelevel parms are captured
void VR_OnFreshStart (void);			// Host_Map_f, Host_Loadgame_f: a game started afresh or loaded, not a changelevel (the flashlight off)
void VR_StoreSpawnParms (int client);	// after parm1..16 are copied from globals into a client_t
void VR_RestoreSpawnParms (int client);	// after parm1..16 are copied from a client_t into globals
int VR_ServerRandom (void);			// the server's own rand() (vr_srvrandom.cpp): 0..0x7fff, apart from the client's effects' C library rand()
void VR_ServerRandomMapLoad (void);	// SV_SpawnServer: the server's stream seeded (sv_random_seed; 0: the clock)
extern cvar_t sv_random_seed;			// pr_cmds.c: the server's random numbers' seed at each map load (tests; 0: the clock's)
int VR_ProbeRandom (void);			// QC's random() while the kinds that can appear are made as a map loads (vr_progs.cpp): its own numbers, 0..0x7fff; -1 otherwise
int VR_AllowLatePrecache (void);		// nonzero if precaches are allowed after map load
int VR_LatePrecacheModel (const char *name); // precache index for setmodel, or -1 if not allowed
int VR_DropToFloor (void);				// start of PF_droptofloor: nonzero if it handled the call
void VR_OnMakeStatic (edict_t *ent);	// PF_makestatic, before the entity is freed (a static torch or flame: vr_debris.cpp)
int VR_TossKeepsGround (struct edict_s *ent);	// SV_Physics_Toss, when on the ground: nonzero to stay
int VR_RigidToss (struct edict_s *ent);		// SV_Physics_Toss, after thinking: nonzero if it moved the entity (.vr_rigid)
void VR_PortalToss (struct edict_s *ent);	// SV_Physics_Toss, before the move: through a teleporter (vr_portals.cpp)
void VR_PortalMonsterCross (struct edict_s *ent);	// SV_Physics_Step, after its think: a monster through a paired teleporter (vr_portals.cpp)
void VR_PortalTraceBegin (void);			// PF_traceline: the last portal trace's crossings forgotten
void VR_PortalTrace (const float start[3], const float end[3], int type, struct edict_s *passedict, trace_t *trace); // ... MOVE_PORTALS: on through the teleporters it crosses
int VR_PortalHitsOwner (struct edict_s *missile); // SV_MoveRun: nonzero if a missile was carried through a teleporter (VR_PortalToss): it meets its owner (Quake's owner rule off)
void VR_PhysicsFrameEnd (void);				// end of SV_Physics's entity loop: Box3D's world steps (vr_box3d.cpp)
int VR_PushSkips (struct edict_s *ent);		// SV_PushMove: nonzero for an entity it must not move (a Box3D body: lifts carry it by contact)
void VR_PlayerBumps (struct edict_s *ent, struct edict_s *other, const float *normal); // SV_FlyMove, a move stopped by a plane of `other`: a player walking into a solid prop's side shoves it (vr_box3d_player_shove)
int VR_PropLetsOut (struct edict_s *mover, struct edict_s *touch, const float *start, const float *mins, const float *maxs,
	const float *end, int shaped); // SV_ClipToLinks, a move (his box mins, maxs; shaped: met by VR_PropClip) starting inside `touch`: nonzero if it doesn't block (a player in a solid prop: vr_box3d_player_unstick; vr_box3d_player_hold: not with his feet a little into its top, nor going deeper)
int VR_OwnPropMeets (struct edict_s *mover, struct edict_s *touch); // SV_ClipToLinks, `touch` is `mover`'s (Quake's owner rule): nonzero if it still meets it (a player and a solid prop he threw: vr_box3d_player_hold)
int VR_PropClip (struct edict_s *mover, struct edict_s *touch, const float *start, const float *mins, const float *maxs,
	const float *boxmins, const float *boxmaxs, const float *end, trace_t *trace); // SV_ClipToLinks: a player's own box (mins, maxs; boxmins, boxmaxs as it meets entities) against a solid prop's drawn box, turned (vr_box3d_player_shape): nonzero if traced
int VR_PropShotClip (struct edict_s *mover, struct edict_s *touch, const float *start, const float *mins, const float *maxs,
	const float *end, int type, trace_t *trace); // SV_ClipToLinks: a shot or a missile (type: SV_Move's, with its flags; mover: the passedict) against a solid prop's drawn box, turned (vr_box3d_shot_shape; the flying grappling hook always): nonzero if traced
void VR_MissileHitDebug (struct edict_s *ent, struct edict_s *other, const trace_t *trace); // SV_PushEntity: a flying thing's move met other (vr_debug_missiles prints it)
int VR_StandsOn (struct edict_s *ent, struct edict_s *ground, const float *normal);	// SV_FlyMove, a floor that isn't SOLID_BSP (met at normal): nonzero if it is ground to ent (a player on a solid Box3D prop's face no steeper than vr_box3d_player_slope)
int VR_CorpseBox (struct edict_s *mover, struct edict_s *touch, const float *start, const float *mins, const float *maxs,
	float *boxmins, float *boxmaxs); // SV_ClipToLinks, a body's move (mover's box mins, maxs from start) and a touchable `touch`: nonzero if it is a corpse in its way (vr_corpse_collide_player, _monsters; vr_box3d.cpp), the box it meets (from its origin) in boxmins, boxmaxs

// Protocol (cl_input.c, cl_parse.c, cl_tent.c, cl_main.c, cl_demo.c, sv_user.c, sv_main.c, host.c,
// host_cmd.c).
void VR_WriteMoveExtras (struct sizebuf_s *buf);			// end of CL_SendMove
void VR_WriteDemoState (struct sizebuf_s *msg);			// CL_Record_f, recording mid-game
void VR_AdjustMove (float *forwardmove, float *sidemove, float *upmove); // CL_SendCmd: thumbstick locomotion
void VR_ReadMoveExtras (struct client_s *client);		// end of SV_ReadClientMove
void VR_CalcStats (struct client_s *client, int *statsi, float *statsf); // end of SV_CalcStats
int VR_ActiveWeaponStat (struct edict_s *ent);			// SV_WriteClientdataToMessage: the active weapon item bit
int VR_EntityUpdateBits (struct edict_s *ent);			// SV_WriteEntitiesToClient, before U_EXTEND*
void VR_WriteEntityUpdate (struct sizebuf_s *msg, struct edict_s *ent, int bits); // after the update
int VR_StepLerpInterval (struct edict_s *ent);			// SV_WriteEntitiesToClient: U_LERPFINISH's byte for a stepping monster in the air (moved every server frame), -1 else (vr_server.cpp)
void VR_ParseEntityUpdate (int num, int bits);			// CL_ParseUpdate, after the fitz fields
int VR_ParseServerMessage (int cmd);					// unknown svc: nonzero if handled
int VR_ParseBeamEntity (int ent);						// CL_ParseBeam: beam key for an entity
enum { QVR_DLIGHT_MUZZLE, QVR_DLIGHT_ROCKET, QVR_DLIGHT_EXPLOSION };
void VR_DecalTempEntity (int scorch, const float *pos);	// cl_tent.c: a wall hit (0) or an explosion (1) leaves a mark
int VR_GibTrail (int ent, int zombie);					// CL_RelinkEntities: a gib's blood (a trail, drops on the floor, splats where it hits); nonzero if it drew the trail (not Quake's)
void VR_ExplosionDebrisTrail (int ent);					// CL_RelinkEntities, an entity without a trail of Quake's: an explosion's chunk's fire trail (vr_explosiondebris.cpp)
int VR_GrenadeTrail (int ent);						// CL_RelinkEntities: whether a grenade model smokes (not a hand grenade with its pin in: vr_grenade.qc)
int VR_BulletHoleSprite (int ent);						// CL_RelinkEntities: Hipnotic's bullet hole sprite, a chip decal instead; nonzero if it is not drawn
void VR_TuneDlight (int kind, int ent, void *dlight);	// after Quake sets a muzzle flash, rocket or explosion light up: size, colour, fade (the local player's flash at the gun)
void VR_ProjectileLight (int ent);						// CL_RelinkEntities, after the trails: glowing projectiles (hell knight flames, scrag spit, vore balls, lasers) light up the room
void VR_DistortionTrail (int ent);						// CL_RelinkEntities, after VR_ProjectileLight: bullet time's distortion trail follows a projectile (vr_bttrails.cpp)
void VR_ProjectileImpactLight (int kind, const float *pos); // cl_tent.c: a scrag's (0) or a hell knight's (1) spike hitting a wall flashes
void VR_HazeExplosion (const float *pos, float size);	// cl_tent.c: an explosion's heat haze (vr_haze.cpp; size 1 a rocket's)
int VR_ModelSpins (int ent);							// CL_RelinkEntities: nonzero to spin a model as EF_ROTATE (a weapon pickup drawn as its prop)
int VR_SuppressModelRotate (int ent);					// CL_RelinkEntities: nonzero to keep an EF_ROTATE model's angles (rigid bodies)
void VR_RelinkHeld (void);								// end of CL_RelinkEntities: the local player's held objects drawn in the hands (vr_held.cpp)
float VR_BeamScale (struct qmodel_s *model);				// CL_UpdateTEnts: scale of a beam's segments
int VR_UpdateBeam (int ent, float *start, float *end);	// CL_UpdateTEnts: moves the player's own beams with the gun; nonzero: a rope (no random roll)
int VR_BeamGone (int ent);								// CL_UpdateTEnts: nonzero if a hand's lightning outlived its gun there (thrown, dropped, holstered): ended at once
int VR_DrawRope (int ent, struct qmodel_s *model, const float *start, const float *end); // CL_UpdateTEnts: a grappling hook's rope, drawn in one piece along its curve (vr_rope.cpp); zero if the beam is not one (drawn as any beam)
void VR_ForgetEndedRopes (void); // CL_UpdateTEnts, before the beams: the ropes whose beams ended forgotten, the frame's ropes put anew
void VR_BeamLights (int index, struct qmodel_s *model, const float *start, const float *end); // CL_UpdateTEnts: a lightning beam lights the room along its length (vr_beam_lights)
void VR_BeamDrawn (int index, struct qmodel_s *model, const float *start, const float *end); // CL_UpdateTEnts: a lightning beam's ends as drawn this frame: Quad Damage's arcs along it (vr_beam_arcs)
void VR_WallTorchFlames (void);							// CL_ReadFromServer, after the temp entities: the taken wall torches' flames (vr_walltorch.cpp)
void VR_TestEffects (void);								// ... vr_particle_test quake's explosion sprite (vr_client.cpp)
#include "vr_modelmetadata.h" // shared model identities/traits and loader invalidation
// A campaign switch keeps the alias models whose files are the same in its game folders (vr_modelkeep.cpp)
void VR_ModelSourcesBegin (struct qmodel_s *mod, const char *file); // Mod_LoadModel, an alias model's: its lookups recorded (its own file, just found, first)
void VR_ModelSourcesEnd (struct qmodel_s *mod);			// ... to its loader's end
void VR_ModelSourcesForget (const struct qmodel_s *mod);	// its slot reloaded or emptied
void VR_FileLookupNoted (const char *name, int found);		// COM_FindFile, while com_lookups_noted: a lookup (com_filesource, file_from_pak, com_fileoffset its file)
void VR_ModelsKeepBefore (int campaign);				// COM_SwitchGame, before the game folders change: the palette's and colormap's files noted
void VR_ModelsKeepDecide (void);					// ... after: the alias models every lookup of which finds the same file again, kept
int VR_ModelKept (const struct qmodel_s *mod);			// nonzero: kept (Mod_ResetAll, Cache_FlushExcept, TexMgr_NewGame, GLMesh_DeleteVertexBuffers leave it)
qboolean VR_ModelCacheKept (cache_user_t *c);			// Cache_FlushExcept's test: a kept model's cache entry
void VR_ModelsKeepEnd (void);						// ... the switch done: none kept any more (their records stay)
int VR_SyntheticModel (struct qmodel_s *mod);				// Mod_LoadModel: a model made in memory from another ("<model>#rag": a ragdoll's skinned body, vr_ragdoll.cpp); nonzero if made
void VR_RagdollSwap (void);								// end of CL_RelinkEntities: the server's ragdolls drawn with their skinned models (vr_ragdoll.cpp)
void VR_RagdollRestore (void);							// CL_ReadFromServer, first: their own models back before the server's messages
unsigned char *VR_DerivedModelFile (const char *name, unsigned int *path_id); // Mod_LoadModel: a model made from another's file (a taken torch's flame), or NULL
const char *VR_ModelSkinName (const char *name);			// Mod_LoadAllSkins: the model name its external skins are found by
void VR_TorchLights (void);								// CL_ReadFromServer, after the temp entities: torches and flames flicker a small light onto the room (vr_torch_lights)
void VR_OnClientClearState (void);						// CL_ParseServerInfo, after CL_ClearState
void VR_OnSetAngle (float yaw);							// svc_setangle: the server turned the view
void VR_WriteClientSpawnState (struct sizebuf_s *msg);	// Host_Spawn_f, before the client data
void VR_ServerFrameEnd (void);							// Host_ServerFrame, before sending
// The unreliable broadcast's room (vr_server.cpp): a full sv.datagram drops whole messages, never part of one.
void VR_BroadcastClear (void);							// SV_ClearDatagram: clears sv.datagram
void VR_BroadcastQCRun (void);							// PR_ExecuteProgram, a server QuakeC run from the engine (not nested)
struct sizebuf_s *VR_BroadcastDest (int len);			// WriteDest's MSG_BROADCAST: where a QuakeC write of at most len bytes goes
void VR_BroadcastWritten (struct sizebuf_s *dest);		// ... after it
void VR_BroadcastMessageEnd (void);						// after the engine wrote a whole message to sv.datagram: a boundary
int VR_BroadcastSendable (int before, int room);		// SV_SendClientDatagram: how much of sv.datagram fits in a datagram with room bytes left (before: its bytes so far)
void VR_ReliableSent (void);								// SV_SendClientMessages, before sv.reliable_datagram goes to the clients

// Server physics (sv_phys.c, sv_user.c, world.c).
int VR_RunThink2 (struct edict_s *ent);				// start of SV_RunThink: 0 if the entity was freed
void VR_ClientPreMove (struct edict_s *ent);			// SV_Physics_Client: hand and weapon touches
void VR_FoeGrabPreThink (struct edict_s *ent);			// SV_Physics_Client, after VR_ClimbPreThink: holds on enemies taken and let go (vr_foegrab.cpp)
void VR_ClimbPreThink (struct edict_s *ent);			// SV_Physics_Client, before PlayerPreThink: ledge holds taken and let go (vr_climb.cpp)
int VR_PortalLerpFrom (const float older[3], const float newer[3], float from[3], float *yaw); // CL_RelinkEntities: 1 when
							// the older place carried through a teleporter (from; the gate's yaw) lands by the newer
int VR_ClientSpecialMove (struct edict_s *ent);		// SV_Physics_Client, before the move: 1 teleported, hung or mantled instead (to the post-think), -1 freed
int VR_ClimbHangsFrom (struct edict_s *check, struct edict_s *pusher);	// SV_PushMove: nonzero for a player hanging from (or mantling onto) the pusher: it rides it
int VR_ClimbCarryBlocked (struct edict_s *check, struct edict_s *pusher, const float *from, const float *move); // SV_PushMove: its ride stopped short: nonzero blocks the pusher (vr_climb_mover_crush), else it lets go
void VR_ClimbCarried (struct edict_s *pusher, const float *move);	// SV_PushMove, moved: the holds on it moved with it
void VR_ClientRoomscaleMove (struct edict_s *ent);		// SV_Physics_Client, after the move
void VR_UnstickMonster (struct edict_s *ent);			// SV_Physics_Step: a monster inside the map, not moving, for a while: to the nearest free spot (vr_unstick_monsters; vr_unstick.cpp)
int VR_Unstick (struct edict_s *ent);				// SV_CheckStuck, found in solid: nonzero if moved to the nearest free spot (vr_unstick; vr_unstick.cpp)
void VR_WalkMoveDebug (struct edict_s *ent, const char *what, const trace_t *trace); // SV_WalkMove, SV_FlyMove: the first player's move printed (vr_debug_walkmove; vr_unstick.cpp)
void VR_BeforePlayerPostThink (struct edict_s *ent);	// SV_Physics_Client, before PlayerPostThink
void VR_AfterPlayerPostThink (struct edict_s *ent);	// and after it
float *VR_MoveAngles (struct edict_s *ent, float *fallback); // angles steering walk/swim moves
int VR_NoclipAngles (struct edict_s *ent, float *out); // SV_NoclipMove: a headset's: the head's yaw, level (vr_cheats.cpp); 0: Quake's .v_angle
float VR_WaterStickScale (struct edict_s *ent, int swimming); // SV_ClientThink, before SV_WaterMove / SV_AirMove: the stick's speed in water
float VR_StaminaSpeedScale (struct edict_s *ent);	// SV_AirMove: tired, times the most walking speed (sv_maxspeed; vr_stamina_speed)
void VR_AfterWaterMove (struct edict_s *ent, float forwardmove, float sidemove, float upmove); // after SV_WaterMove: swimming strokes (the stick steering them)
void VR_GroundPlaneMet (struct edict_s *ent, const float *normal); // SV_FlyMove, SV_WalkMove: a player met walkable floor (vr_slope_walk)
void VR_GroundGravity (struct edict_s *ent, const float *before); // SV_Physics_Client, gravity added: on a walkable slope, only its part into the slope (vr_slope_walk)
float VR_StepSize (float fallback);					// SV_WalkMove step height
void VR_OnWaterLevelChange (struct edict_s *ent, float oldwaterlevel); // end of SV_CheckWater
int VR_AllowWaterSplash (struct edict_s *ent);			// SV_CheckWaterTransition splash sounds
int VR_TouchLinks (struct edict_s *ent);				// start of SV_TouchLinks: nonzero if handled
int VR_ExpandAbsBox (struct edict_s *ent);				// SV_LinkEdict: nonzero if it set the abs box
float VR_MissileExtent (float fallback);				// SV_Move MOVE_MISSILE box extent
// Portal body traces clip the actual box to one halfspace; they do not skip entire brushes.
int VR_PortalBodyMove(struct edict_s* ent, const float* start, const float* mins, const float* maxs,
    const float* end, int type, trace_t* trace);
int VR_HullClipPortal(struct edict_s* ent, const float* start, const float* mins, const float* maxs,
    const float* end, const float* plane, trace_t* trace);
// A player narrower than hull 1 against BSP models (vr_hull_width; vr_hull.cpp, docs/vr-port/HULLS.md).
int VR_HullMoveBox (struct edict_s *passedict, const float *mins, const float *maxs, float *boxmins, float *boxmaxs); // SV_Move: nonzero if its BSP clips use this box
int VR_HullOverDropoff (struct edict_s *ent, const float *origin, const float *vel, float speed); // SV_UserFriction's ledge test: nonzero if the floor drops away ahead (vr_hull_edge_probe: from the narrow box's leading edge; a point in solid has floor)
int VR_HullClipBSP (struct edict_s *ent, const float *start, const float *boxmins, const float *boxmaxs, const float *end,
	trace_t *trace);								// SV_ClipMoveToEntity for SOLID_BSP: nonzero if it traced (else the hull)
int VR_HullEntBox (struct edict_s *passedict, const float *mins, const float *maxs, float *boxmins, float *boxmaxs); // SV_Move: nonzero if the player's box meets other entities' boxes narrowed (vr_hull_ent_width)
int VR_HullNarrowsAgainst (struct edict_s *mover, struct edict_s *other);	// ... and nonzero if it does so against this one (its category's setting; a monster's, vr_mhull: all)
int VR_HullFootprint (struct edict_s *ent, float *absmins, float *absmaxs); // SV_CheckBottom: a monster's corners narrowed to its width (vr_mhull_ledges): nonzero if narrowed
int VR_HullTouchBox (struct edict_s *touch, struct edict_s *mover, float *boxmins, float *boxmaxs); // a player's box, narrowed, as a body moving into it meets it: nonzero if narrowed
int VR_HullHitBox (struct edict_s *touch, float *boxmins, float *boxmaxs, int projectile); // ... and as a shot or missile meets it (vr_hull_hit_width; a bullet or missile, `projectile`: to his head, vr_hull_hit_head): nonzero if changed
// Precise hit detection (vr_hitmodel.cpp): monsters' models, not their boxes, for moves with MOVE_HITMODEL (world.h).
float VR_HitModelTolerance (int type);				// SV_Move: the tolerance of the move's class; -1: not precise (the option off)
int VR_HitModelTarget (struct edict_s *ent);			// SV_ClipToLinks: nonzero if its model is what is hit
int VR_HitModelClip (struct edict_s *ent, const float *start, const float *mins, const float *maxs, const float *end, int type,
	float tolerance, float maxfraction, trace_t *trace);	// nonzero: hit (trace filled); zero: the move goes through its box
int VR_HitModelMoveFlags (struct edict_s *ent);		// SV_PushEntity: the flags a projectile's move adds
#define VR_SHOT_TARGET_REACH 16	// units: the most a shot target's cube reaches from its middle (.vr_shot_radius; SV_Move grows a shot's box by it)
int VR_ShotTargetClip (struct edict_s *mover, struct edict_s *touch, const float *start, const float *mins, const float *maxs,
	const float *end, int type, trace_t *trace);	// SV_ClipToLinks: a shot or missile (type: SV_Move's, with its flags) against a grenade shots set off (.vr_shot_radius): -1 not one (Quake's rules), 0 passes it, 1 met (trace filled)

// Client effects (r_part.c): Quake VR's particles in place of Quake's (nonzero if they took it).
int VR_RunParticleEffect (const float *org, const float *dir, int color, int count);	// impacts, blood (svc_particle)
int VR_ParticleExplosion (const float *org);										// TE_EXPLOSION
int VR_ParticleExplosion2 (const float *org, int colorStart, int colorLength);	// TE_EXPLOSION2
int VR_BlobExplosion (const float *org);											// TE_TAREXPLOSION
int VR_LavaSplash (const float *org);												// TE_LAVASPLASH
int VR_TeleportSplash (const float *org);											// TE_TELEPORT
int VR_EntityTrail (int ent, int type);											// CL_RocketTrail: rockets, lava balls, grenades, blood, scrag/knight/vore trails (R_RocketTrail's types)

// Client view (view.c): runs on the main thread, before the renderer.
void VR_SetupViewEntities (void);						// V_RenderView, before R_RenderView
void VR_OnDamage (int armor, int blood, const float *from);	// V_ParseDamage: a hit knocks the drawn hands, the controllers buzz (vr_painknock.cpp)

// Console (console.c).
int VR_NotifyOnWrist (void);							// Con_DrawNotify: nonzero to leave the notify lines to the wrist gadget's log
int VR_GameLineOnWrist (const char *text, int length);	// Con_DrawNotify: a server's line: nonzero to leave it to the hologram (vr_messages_hologram_only)
int VR_ConsoleLogLine (const char *text, int length, int server);	// Con_DrawNotify: nonzero to leave a line to the console (vr_hud_console_log 0: not a game message)
void VR_NotifyLogInfo (void);						// vr_notify_info: prints the wrist gadget's log lines

// Screen (gl_screen.c).
void VR_GameCenterPrint (const char *str);				// SCR_CenterPrint: a centre print, for the wrist gadget's hologram (vr_gadget.cpp)
int VR_CenterPrintOnWrist (void);						// SCR_CheckDrawCenterString: nonzero while that hologram shows it (not in view)

// Sound (cl_parse.c).
int VR_GameSound (int entnum, struct sfx_s *sfx);		// CL_ParseStartSoundPacket: nonzero when the wrist gadget plays it instead (its notification)

// Menu (menu.c).
void VR_Menu_Open (void);								// Options > VR Settings
void VR_Menu_OpenFromMain (int advanced);				// the main menu's VR Settings (advanced: Advanced VR) row: Back returns there
void VR_NavJump (int state);							// a jump to Ironwail's menu `state` (Levels: Play Custom Map, the corner's): Back from it returns here
void VR_NavEntered (int state, int previous);			// M_Menu_Maps_f: opened by a jump or by Back (kept), else from its own way in
int VR_NavBack (int state);								// its Back: nonzero if it went back where the jump came from
int VR_MenuMainShowsMods (void);
int VR_MenuMouseOnButtons (float x, float y);			// M_Mousemove: the spot (menu x, y) on one of the corner's buttons (the menu's rows left alone)						// M_Main_Draw: the main menu's Mods row asked for (vr_menu_main_mods)
void VR_StartTutorial (void);							// the main menu's VR Tutorial, confirmed: as the Play page's Tutorial (its map's command queued)
void VR_StartHub (void);								// the main menu's VR Hub, confirmed: as the Play page's VR Hub (vr_campaign_hub: vr_hub_map)
void VR_OpenMapLibrary (void);							// Single Player > Map Library: the map browser page (vr_menu_maps.inc)
void VR_Menu_Draw (void);								// M_Draw, m_vr
void VR_Menu_Key (int key, int repeat);				// M_Keydown, m_vr (repeat: the key's auto-repeat)
void VR_Menu_Mousemove (float cx, float cy);			// M_Mousemove, m_vr
void VR_Menu_Char (int key);							// M_Charinput, m_vr: a typed character (the Search page's box)
int VR_Menu_TextEntry (void);							// M_TextEntry, m_vr: a textmode_t (the Search page takes typing)
// The VR menu style's widgets (vr_menuui.cpp): nonzero if they drew it in place of Quake's.
int VR_MenuDrawSlider (int x, int y, float range, float marker, const char *desc); // M_DrawSliderWithMarkers (marker < 0: none)
int VR_MenuDrawCheckbox (int x, int y, int on);			// M_DrawCheckbox: a switch
int VR_MenuDrawTextBox (int x, int y, int width, int lines); // M_DrawTextBox: a panel
int VR_MenuDrawHighlight (int cx, int cy);				// M_DrawArrowCursor: the selected row's highlight; nonzero: no cursor (the corner's buttons have the selection)
int VR_MenuDrawButton (int x0, int x1, int y, int selected);	// M_Confirm's buttons (x0..x1 across, the label's row y): the VR menu style's; nonzero if drawn
// The corner's buttons (vr_menuui.cpp): "Back to game" closing the menu from any page, which reopens
// there; "Advanced VR" and "Levels" jumping to those from any page.
void VR_MenuDrawStatus (void);							// M_Draw, last: the status box (vr_menu_status) in the top right corner
void VR_MenuDrawOverlay (void);							// M_Draw, after the menu: the buttons
int VR_MenuHidesPlaque (void);							// M_DrawTransPic: the options pages' vertical Quake plaque left out (the VR menu style)
// The menus' branding (vr_menubrand.cpp): the Quake VR banner in place of Quake's plaque, and their browns turned red.
int VR_MenuDrawBanner (int x, int y);					// M_DrawPlaque: the banner where the plaque's top left would be; nonzero if drawn (0: no image, draw the plaque)
void VR_MenuDrawBannerColumn (void);					// M_Draw, before the page: the VR menu style's banner, in the left column under the corner's buttons
void VR_MenuDrawVersion (void);						// M_Draw, before the page: "Quake VR: Unleashed - v1.0" / "by Vittorio Romeo" / a "Support on Ko-fi" link, in a box in the bottom right corner (vr_menu_version)
void VR_MenuRecolor (float *params);					// Draw_SetMenuRecolor: the gui shader's MenuRecolor (vr_menu_recolor; x 0: off)
int VR_MenuKey (int key, int repeat);					// M_Keydown: nonzero if the buttons took the key (a click on one, the sticks' selection on them)
void VR_MenuBounds (int *left, int *top, int *width, int *height);	// M_UpdateBounds: the menus laid out from the canvas's bounds beside the corner's buttons
void VR_MenuSavePositions (void);						// Host_WriteConfigurationToFile: each VR page's selection and scroll into vr_menu_positions
void VR_ConfigMergeOthers (const char *path);			// Host_WriteConfigurationToFile, the game folder's config: another copy's changes in it kept (vr_cvars.cpp)
void VR_ConfigWritten (const char *path);				// and after writing it
int VR_RetiredCvar (const char *name);					// Cmd_ExecuteString, an unknown name: nonzero if it is a removed Quake VR setting (a config's stale line, ignored quietly)
const char *VR_CvarAlias (const char *name);			// Cvar_FindVar, a name not found: a renamed setting's new name (vr_slipgates: vr_teleporters), or NULL
int VR_MenuReopen (void);								// M_ToggleMenu_f, opening: nonzero if it reopened the page left
int VR_MenuRunsGame (void);								// Host_ServerFrame: nonzero if a single player game runs on under the menu (live preview)
int VR_RuntimeMenuPause (void);							// Host_ServerFrame, SV_RunClients: nonzero while the runtime's menu pauses a single player game (vr_xr_unfocused_pause)
// The main menu's lettering as a font (vr_bigfont.cpp): its letters cut from id's menu pictures in the pak, rows of text
// 24 pixels high whose small capitals end on row 15.
int VR_BigFont_CanDraw (const char *text);				// M_Main_Draw: nonzero if every letter of `text` is there
int VR_BigFont_Draw (int x, int y, const char *text);	// M_Main_Draw: draws it (its cell's top at y); returns its width
float VR_BigFont_DrawScaled (int x, int y, float scale, const char *text); // as VR_BigFont_Draw, `scale` times the size (the main menu's rows closer than 15); its width

// Hardcoded limits (vr_limits.cpp, the vr_limits command): a limit whose overflow used to be silent is counted, and warned
// about once a session.
enum
{
	QVR_LIMIT_TEMPENTS,	// CL_NewTempEntity: MAX_TEMP_ENTITIES full, the entity not drawn
	QVR_LIMIT_DLIGHTS,	// CL_AllocDlight: MAX_DLIGHTS full, the first light taken over
	QVR_LIMIT_PACKET,	// SV_WriteEntitiesToClient: the datagram full, the farther entities not sent this frame
	QVR_LIMIT_COUNT
};
void VR_LimitHit (int limit);
// SV_WriteEntitiesToClient: a client's entity kept from the other clients (a dead player whose ragdoll body lies there,
// vr_deathview.cpp).
int VR_SV_HiddenFromOthers (edict_t *ent);
// SV_WriteEntitiesToClient: the entities sent to `clent` this frame of those in its sight, their bytes, the datagram's
// room (vr_net_stats, vr_server.cpp).
void VR_NetStatsEntities (edict_t *clent, int sent, int insight, int bytes, int maxsize);

// Spatial audio (vr_audio.cpp: Steam Audio's HRTF, occlusion and reverb, Doppler, the near field, the hands' sounds),
// over Quake's mixer (snd_dma.c, snd_mix.c). Each does nothing with vr_snd_spatial 0, outside VR, or without phonon.dll,
// except the listener (the head, in VR) and the hands' and moving sounds (VR_SndSpatialize).
void VR_SndListener (float *origin, float *forward, float *right, float *up);	// S_Update: the listener (in VR, the head)
int VR_SndSpatialize (channel_t *ch);				// start of SND_Spatialize: nonzero if it set the volumes (a hand's sound); moves a sound following its entity
int VR_SndHandOf (const channel_t *ch, float *hand);	// snd_show 2: the hand (0 main, 1 off; -1 not a hand's) a channel plays from, `hand` its place
void VR_SndStarted (channel_t *ch);					// end of S_StartSound: a new sound on the channel
int VR_SndKeepStatics (void);						// S_Update: nonzero: the static sounds of one sample not combined (each has its place)
int VR_SndMixEnd (int paintedtime, int endtime);	// S_Update_: the mix-ahead's end, rounded down to whole frames of the voices
void VR_SndPaint (portable_samplepair_t *buffer, int start, int end);	// S_PaintChannels, each chunk: the voices' mix added
int VR_SndOwns (const channel_t *ch);				// S_PaintChannels: nonzero for a channel a voice renders (Quake skips it)
void VR_SndBus (portable_samplepair_t *buffer, int count);		// S_PaintChannels, the effects' sum: clipped (wider with vr_snd_limiter), then halved
void VR_SndLimit (portable_samplepair_t *buffer, int count);		// S_PaintChannels, the whole mix before the transfer: vr_snd_limiter
void VR_SndCapture (const portable_samplepair_t *buffer, int count);	// S_PaintChannels, before the transfer: vr_snd_capture's recording
void VR_SndShadow (int count);						// S_PaintChannels, each chunk before the music: the game-time render's share of it (vr_timescale_wav, vr_snd_capture_game)
void VR_SndGameMix (const portable_samplepair_t *buffer, int count);	// the game-time render's samples: its limiter, vr_snd_capture_game, the WAV
const char *VR_SndDerived (const char *name, float *rate);	// S_LoadSound, no file of `name`: the Quake sound it is made from, played at `rate` (slower: lower), or NULL
int VR_SndFullBand (void);							// vr_snd_fullband: 0 Quake's 11 kHz lowpass on every sound, 1 not on the voices, 2 on none (Quake's channels painted from the band-limited copy)
void VR_SndBypass (portable_samplepair_t *buffer, int count);	// S_PaintChannels, after Quake's lowpass: the voices held out of it (vr_snd_fullband) added, halved as VR_SndBus
float VR_SndFullBandAt (const short *data, int length, int loopstart, double pos);	// SND_PaintChannelRate, vr_snd_fullband 2: a band-limited copy between its samples (windowed sinc)
void VR_SndBandLimit (const unsigned char *data, int width, int samples, int loopstart, int fracstep, short *out, int outcount);	// S_LoadSound: the sound resampled band-limited (ResampleSfx's timing: `fracstep`/256 of its samples an output sample)
double VR_SndBenchNow (void);						// vr_snd_bench (vr_audiobench.cpp): the time, 0 when no bench is recording
void VR_SndBenchAdd (int stage, double since);		// vr_snd_bench: now - since into this frame's stage (VR_SNDBENCH_*: vr_audiobench.hpp's Stage)
enum { VR_SNDBENCH_PAINT = 3, VR_SNDBENCH_QUAKE = 12, VR_SNDBENCH_FILTERS = 13 };

int VR_PortalReachMove(struct edict_s* player, const float* start, const float* mins, const float* maxs, const float* end, int type, trace_t* trace);

void VR_RegisterPackStatus(void);
int VR_CanLoadCampaignMap(const char *map);
int VR_IsVrMap(const char *map);			// vrstart, vrtutorial, vrfiringrange: Quake VR's own maps, run in Quake's campaign
const char *VR_HubMap(void);				// the hub the game starts in and returns to: vrstart
int VR_CanChangeCampaignMap(const char *map);
int VR_CanLoadCampaignSave(const char *text);

// Official campaign context and read-only source roots.
const char *VR_GameDirectoryRoot(const char *dir, int index);
void VR_PrepareCampaignDirectories(const char *paths);
void VR_InitCampaignDirectories(void);
char *VR_LoadOwnedLocalization(const char *file);
// "owned/<folder>/<path>": a file of a discovered Dopa/MG1/MG3 folder read in place, unmounted (COM_FindFile).
int VR_OwnedFile(const char *name, char *out, int size, int *offset, int *length, int *packed);
int VR_IsNativeCampaignLaunch(void);
int VR_IsNewCampaignDirectory(const char *dir);
int VR_ShouldMountCampaignDirectory(const char *dir);
int VR_HasNativeCampaignDirectory(const char *paths);
int VR_CampaignDataAvailable(const char *dir);
void VR_OpenCampaignSelector(void);
const char *VR_CampaignLabel(int index);
const char *VR_CampaignHelp(int index);
void VR_SelectCampaign(int index);
int VR_CampaignUnavailable(int index);

// Music read in place (vr_music.cpp): a CD track from the game folders on the search path, or from the owned
// rerelease/store installs where they are (never mounted, copied or written), per campaign.
typedef struct vr_musicfile_s
{
    char path[MAX_OSPATH];   // the track's file, or the pak that holds it
    long offset, length;     // the track inside `path` (a loose file: 0 and its size)
    int pak;
    char ext[16];            // its extension: the codec bgmusic opens it with
    char name[64];           // "music/track02.ogg": the stream's name
    char source[MAX_OSPATH]; // where it was found, for the log: a game folder or an owned install's root
} vr_musicfile_t;
int VR_FindMusicTrack(int track, const char *const *exts, int numExts, vr_musicfile_t *out);
const char *VR_ActiveCampaignFolder(void);
const char *VR_OwnedReadRoot(int index);

#ifdef __cplusplus
}
#endif



#endif // QVR_VR_API_H
