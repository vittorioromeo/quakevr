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
void VR_Shutdown (void);	// client shutdown, before video shutdown
void VR_BeginFrame (void);	// once per host frame, after input events and before console commands
int VR_IsActive (void);		// nonzero while vr_enabled is set and a backend session is running: the
							// runtime paces frames (no frame cap, no sleeping when unfocused)
int VR_ModalMessageFrame (void); // SCR_ModalMessage's loop: with a headset, a frame showing the
							// dialog (the runtime paces it); zero without one (the loop sleeps)

// Filesystem (common.c).
void VR_BeforeAddGameDirectory (const char *dir);	// start of COM_AddGameDirectory
void VR_AfterAddGameDirectory (const char *dir);	// end of COM_AddGameDirectory
int VR_SkipSearchPath (const char *filename, const char *path);	// COM_FindFile: nonzero to skip a search path
const char *VR_ModelFile (const char *name);	// Mod_LoadModel, Mod_LoadLighting: the file to load a model from (relit maps)
void VR_AliasPosesLoaded (const char *name, void *aliashdr, const stvert_t *stverts, const dtriangle_t *tris, trivertx_t **poses); // Mod_LoadAliasModel, after the frames

// Server QuakeC (pr_edict.c, pr_cmds.c, sv_main.c, host_cmd.c).
void VR_OnProgsLoaded (void);			// end of PR_LoadProgs, with the loaded qcvm current
void VR_OnSpawnServerBeforeLoad (void);	// SV_SpawnServer, before ED_LoadFromFile
void VR_OnSpawnServerAfterLoad (void);	// SV_SpawnServer, after serverinfo is sent
void VR_OnBeginLoadGame (void);			// Host_Loadgame_f, before SV_SpawnServer
void VR_OnLoadGame (void);				// Host_Loadgame_f, after globals and edicts are restored
void VR_StoreSpawnParms (int client);	// after parm1..16 are copied from globals into a client_t
void VR_RestoreSpawnParms (int client);	// after parm1..16 are copied from a client_t into globals
int VR_AllowLatePrecache (void);		// nonzero if precaches are allowed after map load
int VR_LatePrecacheModel (const char *name); // precache index for setmodel, or -1 if not allowed
int VR_DropToFloor (void);				// start of PF_droptofloor: nonzero if it handled the call
int VR_TossKeepsGround (struct edict_s *ent);	// SV_Physics_Toss, when on the ground: nonzero to stay
int VR_RigidToss (struct edict_s *ent);		// SV_Physics_Toss, after thinking: nonzero if it moved the entity (.vr_rigid)

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
void VR_TuneDlight (int kind, int ent, void *dlight);	// after Quake sets a muzzle flash, rocket or explosion light up: size, colour, fade (the local player's flash at the gun)
int VR_SuppressModelRotate (int ent);					// CL_RelinkEntities: nonzero to keep an EF_ROTATE model's angles (rigid bodies)
float VR_BeamScale (struct qmodel_s *model);				// CL_UpdateTEnts: scale of a beam's segments
int VR_UpdateBeam (int ent, float *start, float *end);	// CL_UpdateTEnts: moves the player's own beams with the gun; nonzero: a rope (no random roll)
void VR_OnClientClearState (void);						// CL_ParseServerInfo, after CL_ClearState
void VR_OnSetAngle (float yaw);							// svc_setangle: the server turned the view
void VR_WriteClientSpawnState (struct sizebuf_s *msg);	// Host_Spawn_f, before the client data
void VR_ServerFrameEnd (void);							// Host_ServerFrame, before sending

// Server physics (sv_phys.c, sv_user.c, world.c).
int VR_RunThink2 (struct edict_s *ent);				// start of SV_RunThink: 0 if the entity was freed
void VR_ClientPreMove (struct edict_s *ent);			// SV_Physics_Client: hand and weapon touches
int VR_ClientTeleport (struct edict_s *ent);			// SV_Physics_Client: 1 teleported, -1 freed
void VR_ClientRoomscaleMove (struct edict_s *ent);		// SV_Physics_Client, after the move
void VR_BeforePlayerPostThink (struct edict_s *ent);	// SV_Physics_Client, before PlayerPostThink
void VR_AfterPlayerPostThink (struct edict_s *ent);	// and after it
float *VR_MoveAngles (struct edict_s *ent, float *fallback); // angles steering walk/swim moves
float VR_WaterStickScale (struct edict_s *ent, int swimming); // SV_ClientThink, before SV_WaterMove / SV_AirMove: the stick's speed in water
void VR_AfterWaterMove (struct edict_s *ent, float forwardmove, float sidemove, float upmove); // after SV_WaterMove: swimming strokes (the stick steering them)
float VR_StepSize (float fallback);					// SV_WalkMove step height
void VR_OnWaterLevelChange (struct edict_s *ent, float oldwaterlevel); // end of SV_CheckWater
int VR_AllowWaterSplash (struct edict_s *ent);			// SV_CheckWaterTransition splash sounds
int VR_TouchLinks (struct edict_s *ent);				// start of SV_TouchLinks: nonzero if handled
int VR_ExpandAbsBox (struct edict_s *ent);				// SV_LinkEdict: nonzero if it set the abs box
float VR_MissileExtent (float fallback);				// SV_Move MOVE_MISSILE box extent

// Client effects (r_part.c): Quake VR's particles in place of Quake's (nonzero if they took it).
int VR_RunParticleEffect (const float *org, const float *dir, int color, int count);	// impacts, blood (svc_particle)
int VR_ParticleExplosion (const float *org);										// TE_EXPLOSION
int VR_ParticleExplosion2 (const float *org, int colorStart, int colorLength);	// TE_EXPLOSION2

// Client view (view.c): runs on the main thread, before the renderer.
void VR_SetupViewEntities (void);						// V_RenderView, before R_RenderView

// Menu (menu.c).
void VR_Menu_Open (void);								// Options > VR Settings
void VR_Menu_Draw (void);								// M_Draw, m_vr
void VR_Menu_Key (int key);								// M_Keydown, m_vr

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_H
