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

// PROTOCOL_RMQ flags (see vr/vr_protocol.hpp): the VR protocol (a server for VR clients, whatever
// its progs), and progs implementing Quake VR's gameplay (without it, VR in compatibility mode).
#define PRFL_QUAKEVR				(1 << 16)
#define PRFL_QUAKEVR_PROGS			(1 << 17)
#define VR_ENTITY_UPDATE_MAXSIZE	36		// bytes VR_WriteEntityUpdate may add
#define SOLID_NOT_BUT_TOUCHABLE		5		// Quake VR: not solid, but can be (hand) touched

// Host lifetime and pacing (host.c, main_sdl.c, gl_screen.c).
void VR_Init (void);		// after SV_Init (also on dedicated servers): registers cvars and commands
void VR_NewMap (void);		// R_NewMap: a map loaded (the per-map data rebuilt, even for the same model)
void VR_Shutdown (void);	// client shutdown, before video shutdown
void VR_BeginFrame (void);	// once per host frame, after input events and before console commands
int VR_IsActive (void);		// nonzero while vr_enabled is set and a backend session is running: the
							// runtime paces frames (no frame cap, no sleeping when unfocused)
int VR_Unpaced (void);		// nonzero while the mock headset runs frames unpaced (vr_mock_fast, the game's clock
							// fixed): no frame cap
int VR_ModalMessageFrame (void); // SCR_ModalMessage's loop: with a headset, a frame showing the
							// dialog (the runtime paces it); zero without one (the loop sleeps)
double VR_HostFrameTime (double time);	// start of _Host_Frame: the frame's time (a motion take's own while
							// it plays back, vr_motion_play: the same frames at any speed)
int VR_ServerFrameOverride (double *frametime); // _Host_Frame: whether the server runs this frame: -1 as
							// usual; 0 no; 1 yes, for *frametime seconds (a take's recorded server frames)
void VR_HostFrameEnd (void);	// end of _Host_Frame, after the screen and the sound (the motion recorder's row)

// Start-up and map-load timing (vr_startup.cpp: vr_startup_times, vr_walltime).
void VR_TimeStart (void);	// main, after Sys_Init: the process's start
void VR_TimeInit (void);	// VR_Init: the commands
void VR_TimeMark (const char *stage);	// a stage of the start-up or of a map's load just ended
void VR_TimeLoadBegin (const char *what);	// SV_SpawnServer, CL_ParseServerInfo: a map's load starts
void VR_TimeAdd (const char *what, double seconds);	// time spent in a kind of work (model loads, normal maps...), summed per stage group
void VR_TimeFrameEnd (int signedon, int idle);	// end of _Host_Frame: the first frame ends the start-up, the first signed on a load; idle (no server, not connected) ends a load that failed

// The loose files' presence while the game starts and a map loads (vr_fscache.cpp; COM_FindFile, Sys_fopen).
int VR_FileCacheHas (const char *path);	// 1 a file, 0 none, -1 not known (ask the file system)
void VR_FileCacheEnable (int on);	// VR_TimeStart, VR_TimeLoadBegin on; the first frame drawn off
void VR_FileCacheForget (void);	// a file written, a directory made

// Images decoded ahead on worker threads (vr_imgprefetch.cpp; image.c Image_LoadImage).
unsigned char *VR_ImagePrefetchTake (const char *name, FILE *f, int length, int *width, int *height);
void VR_ImagePrefetchNote (const char *name, double seconds);
void VR_ImagePrefetchEnd (void);	// the first map load's end: the workers joined, the rest freed, the list written

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
const char *VR_ModelFile (const char *name);	// Mod_LoadModel, Mod_LoadLighting: the file to load a model from (relit maps)
int VR_ModelReplacementOk (const char *name, const char *md5mesh);	// loadMd5Replacement: 0 refuses a jointed hand the rig can't use (vr_handrig.cpp)
void VR_AliasPosesLoaded (const char *name, void *aliashdr, const stvert_t *stverts, const dtriangle_t *tris, trivertx_t **poses); // Mod_LoadAliasModel, after the frames

// Server QuakeC (pr_edict.c, pr_cmds.c, sv_main.c, host_cmd.c).
void VR_OnProgsLoaded (void);			// end of PR_LoadProgs, with the loaded qcvm current
void VR_OnSpawnServerBeforeLoad (void);	// SV_SpawnServer, before ED_LoadFromFile
void VR_OnClearMemory (void);			// Host_ClearMemory, before the hunk (edicts, cl_entities, models) is freed: every pointer into it forgotten
void VR_OnEdictFree (edict_t *ed);	// ED_Free (any VM's)
void VR_OnSpawnServerAfterLoad (void);	// SV_SpawnServer, after serverinfo is sent
void VR_OnBeginLoadGame (void);			// Host_Loadgame_f, before SV_SpawnServer
void VR_OnLoadGame (void);				// Host_Loadgame_f, after globals and edicts are restored
void VR_OnFreshStart (void);			// Host_Map_f, Host_Loadgame_f: a game started afresh or loaded, not a changelevel (the flashlight off)
void VR_StoreSpawnParms (int client);	// after parm1..16 are copied from globals into a client_t
void VR_RestoreSpawnParms (int client);	// after parm1..16 are copied from a client_t into globals
int VR_AllowLatePrecache (void);		// nonzero if precaches are allowed after map load
int VR_LatePrecacheModel (const char *name); // precache index for setmodel, or -1 if not allowed
int VR_DropToFloor (void);				// start of PF_droptofloor: nonzero if it handled the call
void VR_OnMakeStatic (edict_t *ent);	// PF_makestatic, before the entity is freed (a static torch or flame: vr_debris.cpp)
int VR_TossKeepsGround (struct edict_s *ent);	// SV_Physics_Toss, when on the ground: nonzero to stay
int VR_RigidToss (struct edict_s *ent);		// SV_Physics_Toss, after thinking: nonzero if it moved the entity (.vr_rigid)
void VR_PhysicsFrameEnd (void);				// end of SV_Physics's entity loop: Box3D's world steps (vr_box3d.cpp)
int VR_PushSkips (struct edict_s *ent);		// SV_PushMove: nonzero for an entity it must not move (a Box3D body: lifts carry it by contact)

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
void VR_ParseEntityUpdate (int num, int bits);			// CL_ParseUpdate, after the fitz fields
int VR_ParseServerMessage (int cmd);					// unknown svc: nonzero if handled
int VR_ParseBeamEntity (int ent);						// CL_ParseBeam: beam key for an entity
enum { QVR_DLIGHT_MUZZLE, QVR_DLIGHT_ROCKET, QVR_DLIGHT_EXPLOSION };
void VR_DecalTempEntity (int scorch, const float *pos);	// cl_tent.c: a wall hit (0) or an explosion (1) leaves a mark
int VR_GibTrail (int ent, int zombie);					// CL_RelinkEntities: a gib's blood (a trail, drops on the floor, splats where it hits); nonzero if it drew the trail (not Quake's)
int VR_GrenadeTrail (int ent);						// CL_RelinkEntities: whether a grenade model smokes (not a hand grenade with its pin in: vr_grenade.qc)
int VR_BulletHoleSprite (int ent);						// CL_RelinkEntities: Hipnotic's bullet hole sprite, a chip decal instead; nonzero if it is not drawn
void VR_TuneDlight (int kind, int ent, void *dlight);	// after Quake sets a muzzle flash, rocket or explosion light up: size, colour, fade (the local player's flash at the gun)
void VR_ProjectileLight (int ent);						// CL_RelinkEntities, after the trails: glowing projectiles (hell knight flames, scrag spit, vore balls, lasers) light up the room
void VR_ProjectileImpactLight (int kind, const float *pos); // cl_tent.c: a scrag's (0) or a hell knight's (1) spike hitting a wall flashes
void VR_HazeExplosion (const float *pos, float size);	// cl_tent.c: an explosion's heat haze (vr_haze.cpp; size 1 a rocket's)
int VR_SuppressModelRotate (int ent);					// CL_RelinkEntities: nonzero to keep an EF_ROTATE model's angles (rigid bodies)
void VR_RelinkHeld (void);								// end of CL_RelinkEntities: the local player's held objects drawn in the hands (vr_held.cpp)
float VR_BeamScale (struct qmodel_s *model);				// CL_UpdateTEnts: scale of a beam's segments
int VR_UpdateBeam (int ent, float *start, float *end);	// CL_UpdateTEnts: moves the player's own beams with the gun; nonzero: a rope (no random roll)
int VR_DrawRope (int ent, struct qmodel_s *model, const float *start, const float *end); // CL_UpdateTEnts: a grappling hook's rope, drawn in one piece along its curve (vr_rope.cpp); zero if the beam is not one (drawn as any beam)
void VR_ForgetEndedRopes (void); // CL_UpdateTEnts, before the beams: the ropes whose beams ended forgotten, the frame's ropes put anew
void VR_BeamLights (int index, struct qmodel_s *model, const float *start, const float *end); // CL_UpdateTEnts: a lightning beam lights the room along its length (vr_beam_lights)
void VR_WallTorchFlames (void);							// CL_ReadFromServer, after the temp entities: the taken wall torches' flames (vr_walltorch.cpp)
unsigned char *VR_DerivedModelFile (const char *name, unsigned int *path_id); // Mod_LoadModel: a model made from another's file (a taken torch's flame), or NULL
const char *VR_ModelSkinName (const char *name);			// Mod_LoadAllSkins: the model name its external skins are found by
void VR_TorchLights (void);								// CL_ReadFromServer, after the temp entities: torches and flames flicker a small light onto the room (vr_torch_lights)
void VR_OnClientClearState (void);						// CL_ParseServerInfo, after CL_ClearState
void VR_OnSetAngle (float yaw);							// svc_setangle: the server turned the view
void VR_WriteClientSpawnState (struct sizebuf_s *msg);	// Host_Spawn_f, before the client data
void VR_ServerFrameEnd (void);							// Host_ServerFrame, before sending

// Server physics (sv_phys.c, sv_user.c, world.c).
int VR_RunThink2 (struct edict_s *ent);				// start of SV_RunThink: 0 if the entity was freed
void VR_ClientPreMove (struct edict_s *ent);			// SV_Physics_Client: hand and weapon touches
int VR_ClientTeleport (struct edict_s *ent);			// SV_Physics_Client: 1 teleported, -1 freed
void VR_ClimbPreThink (struct edict_s *ent);			// SV_Physics_Client, before PlayerPreThink: ledge holds taken and let go (vr_climb.cpp)
int VR_ClientClimb (struct edict_s *ent);				// SV_Physics_Client, before the move: 1 hung or mantled instead, -1 freed
int VR_ClimbHangsFrom (struct edict_s *check, struct edict_s *pusher);	// SV_PushMove: nonzero for a player hanging from (or mantling onto) the pusher: it rides it
int VR_ClimbCarryBlocked (struct edict_s *check, struct edict_s *pusher, const float *from, const float *move); // SV_PushMove: its ride stopped short: nonzero blocks the pusher (vr_climb_mover_crush), else it lets go
void VR_ClimbCarried (struct edict_s *pusher, const float *move);	// SV_PushMove, moved: the holds on it moved with it
void VR_ClientRoomscaleMove (struct edict_s *ent);		// SV_Physics_Client, after the move
void VR_BeforePlayerPostThink (struct edict_s *ent);	// SV_Physics_Client, before PlayerPostThink
void VR_AfterPlayerPostThink (struct edict_s *ent);	// and after it
float *VR_MoveAngles (struct edict_s *ent, float *fallback); // angles steering walk/swim moves
float VR_WaterStickScale (struct edict_s *ent, int swimming); // SV_ClientThink, before SV_WaterMove / SV_AirMove: the stick's speed in water
float VR_StaminaSpeedScale (struct edict_s *ent);	// SV_AirMove: tired, times the most walking speed (sv_maxspeed; vr_stamina_speed)
void VR_AfterWaterMove (struct edict_s *ent, float forwardmove, float sidemove, float upmove); // after SV_WaterMove: swimming strokes (the stick steering them)
float VR_StepSize (float fallback);					// SV_WalkMove step height
void VR_OnWaterLevelChange (struct edict_s *ent, float oldwaterlevel); // end of SV_CheckWater
int VR_AllowWaterSplash (struct edict_s *ent);			// SV_CheckWaterTransition splash sounds
int VR_TouchLinks (struct edict_s *ent);				// start of SV_TouchLinks: nonzero if handled
int VR_ExpandAbsBox (struct edict_s *ent);				// SV_LinkEdict: nonzero if it set the abs box
float VR_MissileExtent (float fallback);				// SV_Move MOVE_MISSILE box extent
// Precise hit detection (vr_hitmodel.cpp): monsters' models, not their boxes, for moves with MOVE_HITMODEL (world.h).
float VR_HitModelTolerance (int type);				// SV_Move: the tolerance of the move's class; -1: not precise (the option off)
int VR_HitModelTarget (struct edict_s *ent);			// SV_ClipToLinks: nonzero if its model is what is hit
int VR_HitModelClip (struct edict_s *ent, const float *start, const float *mins, const float *maxs, const float *end, int type,
	float tolerance, float maxfraction, trace_t *trace);	// nonzero: hit (trace filled); zero: the move goes through its box
int VR_HitModelMoveFlags (struct edict_s *ent);		// SV_PushEntity: the flags a projectile's move adds

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

// Console (console.c).
int VR_NotifyOnWrist (void);							// Con_DrawNotify: nonzero to leave the notify lines to the wrist gadget's log
int VR_GameLineOnWrist (const char *text, int length);	// Con_DrawNotify: a server's line: nonzero to leave it to the hologram (vr_messages_hologram_only)

// Screen (gl_screen.c).
void VR_GameCenterPrint (const char *str);				// SCR_CenterPrint: a centre print, for the wrist gadget's hologram (vr_gadget.cpp)
int VR_CenterPrintOnWrist (void);						// SCR_CheckDrawCenterString: nonzero while that hologram shows it (not in view)

// Sound (cl_parse.c).
int VR_GameSound (int entnum, struct sfx_s *sfx);		// CL_ParseStartSoundPacket: nonzero when the wrist gadget plays it instead (its notification)

// Menu (menu.c).
void VR_Menu_Open (void);								// Options > VR Settings
void VR_Menu_Draw (void);								// M_Draw, m_vr
void VR_Menu_Key (int key, int repeat);				// M_Keydown, m_vr (repeat: the key's auto-repeat)
void VR_Menu_Mousemove (float cx, float cy);			// M_Mousemove, m_vr
// The VR menu style's widgets (vr_menuui.cpp): nonzero if they drew it in place of Quake's.
int VR_MenuDrawSlider (int x, int y, float range, float marker, const char *desc); // M_DrawSliderWithMarkers (marker < 0: none)
int VR_MenuDrawCheckbox (int x, int y, int on);			// M_DrawCheckbox: a switch
int VR_MenuDrawTextBox (int x, int y, int width, int lines); // M_DrawTextBox: a panel
int VR_MenuDrawHighlight (int cx, int cy);				// M_DrawArrowCursor: the selected row's highlight; nonzero: no cursor (the corner's buttons have the selection)
// The corner's buttons (vr_menuui.cpp): "Back to game" closing the menu from any page, which reopens
// there; "Advanced VR" and "Levels" jumping to those from any page.
void VR_MenuDrawOverlay (void);							// M_Draw, after the menu: the buttons
int VR_MenuHidesPlaque (void);							// M_DrawTransPic: the options pages' vertical Quake plaque left out (the VR menu style)
int VR_MenuKey (int key, int repeat);					// M_Keydown: nonzero if the buttons took the key (a click on one, the sticks' selection on them)
void VR_MenuBounds (int *top, int *height);				// M_UpdateBounds: the menus laid out from the canvas's bounds start below the buttons
void VR_MenuSavePositions (void);						// Host_WriteConfigurationToFile: each VR page's selection and scroll into vr_menu_positions
void VR_ConfigMergeOthers (const char *path);			// Host_WriteConfigurationToFile, the game folder's config: another copy's changes in it kept (vr_cvars.cpp)
void VR_ConfigWritten (const char *path);				// and after writing it
int VR_MenuReopen (void);								// M_ToggleMenu_f, opening: nonzero if it reopened the page left
int VR_MenuRunsGame (void);								// Host_ServerFrame: nonzero if a single player game runs on under the menu (live preview)

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

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_H
