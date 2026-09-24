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
struct sizebuf_s;

// PROTOCOL_RMQ flag set by servers running Quake VR progs (see vr/vr_protocol.hpp).
#define PRFL_QUAKEVR				(1 << 16)
#define VR_ENTITY_UPDATE_MAXSIZE	36		// bytes VR_WriteEntityUpdate may add

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

// Protocol (cl_input.c, cl_parse.c, cl_tent.c, sv_user.c, sv_main.c, host.c, host_cmd.c).
void VR_WriteMoveExtras (struct sizebuf_s *buf);			// end of CL_SendMove
void VR_ReadMoveExtras (struct client_s *client);		// end of SV_ReadClientMove
void VR_CalcStats (struct client_s *client, int *statsi, float *statsf); // end of SV_CalcStats
int VR_EntityUpdateBits (struct edict_s *ent);			// SV_WriteEntitiesToClient, before U_EXTEND*
void VR_WriteEntityUpdate (struct sizebuf_s *msg, struct edict_s *ent, int bits); // after the update
void VR_ParseEntityUpdate (int num, int bits);			// CL_ParseUpdate, after the fitz fields
int VR_ParseServerMessage (int cmd);					// unknown svc: nonzero if handled
int VR_ParseBeamEntity (int ent);						// CL_ParseBeam: beam key for an entity
void VR_OnClientClearState (void);						// CL_ParseServerInfo, after CL_ClearState
void VR_WriteClientSpawnState (struct sizebuf_s *msg);	// Host_Spawn_f, before the client data
void VR_ServerFrameEnd (void);							// Host_ServerFrame, before sending

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_H
