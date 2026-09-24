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

// vr_api.h -- the only interface between the Ironwail engine and the Quake VR module.
//
// Engine code calls into VR exclusively through the functions declared here, at call sites
// marked with a "QVR" comment. The implementation lives in Quake/vr/ (C++), which in turn may
// use any engine header through vr_engine.hpp.

#ifndef QVR_VR_API_H
#define QVR_VR_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct client_s;
struct edict_s;
struct entity_s;
struct sizebuf_s;

// PROTOCOL_RMQ flag set by servers running Quake VR progs (see vr/vr_protocol.hpp).
#define PRFL_QUAKEVR				(1 << 16)
#define VR_ENTITY_UPDATE_MAXSIZE	36		// bytes VR_WriteEntityUpdate may add
#define SOLID_NOT_BUT_TOUCHABLE		5		// Quake VR: not solid, but can be (hand) touched

// Host lifetime (host.c).
void VR_Init (void);		// after SV_Init (also on dedicated servers): registers cvars and commands
void VR_Shutdown (void);	// client shutdown, before video shutdown
void VR_BeginFrame (void);	// once per host frame, after input events and before console commands

// Queries.
int VR_IsActive (void);		// nonzero while vr_enabled is set and a backend session is running

// Filesystem (common.c).
void VR_BeforeAddGameDirectory (const char *dir);	// start of COM_AddGameDirectory
void VR_AfterAddGameDirectory (const char *dir);	// end of COM_AddGameDirectory

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

// Protocol (cl_input.c, cl_parse.c, cl_tent.c, sv_user.c, sv_main.c, host.c, host_cmd.c).
void VR_WriteMoveExtras (struct sizebuf_s *buf);			// end of CL_SendMove
void VR_WriteDemoState (struct sizebuf_s *msg);			// CL_Record_f, recording mid-game
void VR_AdjustMove (float *forwardmove, float *sidemove, float *upmove); // CL_SendCmd: thumbstick locomotion
void VR_SendHaptic (struct edict_s *player, int hand, float delay, float duration, float frequency, float amplitude);
void VR_ReadMoveExtras (struct client_s *client);		// end of SV_ReadClientMove
void VR_CalcStats (struct client_s *client, int *statsi, float *statsf); // end of SV_CalcStats
int VR_ActiveWeaponStat (struct edict_s *ent);			// SV_WriteClientdataToMessage: the active weapon item bit
int VR_EntityUpdateBits (struct edict_s *ent);			// SV_WriteEntitiesToClient, before U_EXTEND*
void VR_WriteEntityUpdate (struct sizebuf_s *msg, struct edict_s *ent, int bits); // after the update
void VR_ParseEntityUpdate (int num, int bits);			// CL_ParseUpdate, after the fitz fields
int VR_ParseServerMessage (int cmd);					// unknown svc: nonzero if handled
int VR_ParseBeamEntity (int ent);						// CL_ParseBeam: beam key for an entity
void VR_OnClientClearState (void);						// CL_ParseServerInfo, after CL_ClearState
void VR_OnSetAngle (float yaw);							// svc_setangle: the server turned the view
void VR_WriteClientSpawnState (struct sizebuf_s *msg);	// Host_Spawn_f, before the client data
void VR_ServerFrameEnd (void);							// Host_ServerFrame, before sending

// Server physics (sv_phys.c, sv_user.c, world.c).
int VR_RunThink2 (struct edict_s *ent);				// start of SV_RunThink: 0 if the entity was freed
void VR_ClientPreMove (struct edict_s *ent);			// SV_Physics_Client: hand and weapon touches
int VR_ClientTeleport (struct edict_s *ent);			// SV_Physics_Client: 1 teleported, -1 freed
void VR_ClientRoomscaleMove (struct edict_s *ent);		// SV_Physics_Client, after the move
float *VR_MoveAngles (struct edict_s *ent, float *fallback); // angles steering walk/swim moves
float VR_StepSize (float fallback);					// SV_WalkMove step height
void VR_OnWaterLevelChange (struct edict_s *ent, float oldwaterlevel); // end of SV_CheckWater
int VR_AllowWaterSplash (struct edict_s *ent);			// SV_CheckWaterTransition splash sounds
int VR_TouchLinks (struct edict_s *ent);				// start of SV_TouchLinks: nonzero if handled
int VR_ExpandAbsBox (struct edict_s *ent);				// SV_LinkEdict: nonzero if it set the abs box
float VR_MissileExtent (float fallback);				// SV_Move MOVE_MISSILE box extent

// View and rendering (view.c, gl_rmain.c, r_alias.c).
void VR_SetupViewEntities (void);						// V_RenderView, before R_RenderView
int VR_HideViewModel (void);							// R_IsViewModelVisible: VR draws its own weapons
int VR_IsViewEntity (const struct entity_s *e);		// gets the view model's minimum light
int VR_AliasMirrored (const struct entity_s *e);		// mirrored instances batch and cull separately
void VR_AliasPreTransform (const struct entity_s *e, float matrix[16]);	// after R_EntityMatrix
void VR_AliasPostTransform (const struct entity_s *e, float matrix[16]);	// after the model scale
void VR_BrushTransform (const struct entity_s *e, float matrix[16]);		// brush entities: the networked scale and offset
int VR_AliasZeroBlend (const struct entity_s *e, const void *aliashdr, int totalverts); // instance padding
void VR_AliasLightModifier (const struct entity_s *e, float lightcolor[3]); // end of R_SetupAliasLighting
int VR_AliasBonePoses (const struct entity_s *e, const float **matrices); // bone count of an IK-posed skeletal entity (0: none), its 3x4 skinning matrices

// Stereo rendering (gl_screen.c, gl_rmain.c).
int VR_RenderView (void);								// SCR_UpdateScreen: nonzero if it rendered the eyes
int VR_RenderingEye (void);							// forces the post-process path while rendering an eye
unsigned VR_PostProcessTarget (void);					// GL_PostProcess output framebuffer (0 = window)
void VR_OverrideProjection (float matrix[16]);			// R_SetFrustum: the eye's asymmetric projection
int VR_SkipSearchPath (const char *filename, const char *path);	// COM_FindFile: nonzero to skip a search path
float VR_BeamScale (struct qmodel_s *model);				// CL_UpdateTEnts: scale of a beam's segments
void VR_DrawSceneOpaque (void);							// R_RenderScene, after the opaque entities
void VR_Menu_Open (void);								// menu.c: Options > VR Settings
void VR_Menu_Command (void);							// menu_vr [page]: the VR Settings, or one of its pages (1: Advanced VR Options)
void VR_Menu_Draw (void);								// M_Draw, m_vr
void VR_Menu_Key (int key);								// M_Keydown, m_vr
int VR_CanvasBlend (void);								// GL_SetStateEx, alpha blending: nonzero if it set the blend
void VR_Begin2D (void);									// SCR_UpdateScreen, before GL_Set2D
void VR_End2D (void);									// SCR_UpdateScreen, after Draw_Flush

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_H
