// vr_engine.hpp -- lets the C++ VR module use the (C) engine headers.
//
// Always include this instead of quakedef.h from VR sources. glm is included first because
// the engine defines function-like macros (DotProduct, VectorCopy, ...) that must not leak
// into glm's templates. Every engine symbol the module uses that no engine header declares is
// declared here, so that this file lists what a port to another engine must provide besides the
// headers (docs/vr-port/PORTING.md).

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

extern "C" {
#include "quakedef.h"

// Engine symbols that no engine header declares.
extern cvar_t sv_gravity;							// sv_phys.c
extern cvar_t sv_maxvelocity;						// sv_phys.c
int ED_FindFieldOffset (const char *name);			// pr_edict.c
int S_KnownSfxCount (void);							// snd_dma.c (vr_limits)
int S_KnownSfxMax (void);							// snd_dma.c
int Mod_KnownCount (void);							// gl_model.c
int Mod_KnownMax (void);							// gl_model.c
int TexMgr_Count (void);							// gl_texmgr.c
int TexMgr_Max (void);								// gl_texmgr.c
int Draw_CachedPicsMax (void);						// gl_draw.c
extern int menu_numcachepics;						// gl_draw.c
int Cmd_AliasCount (void);							// cmd.c
int Cmd_CommandCount (void);						// cmd.c
extern int num_temp_entities;						// cl_tent.c
extern qboolean scr_drawloading;					// gl_screen.c
extern qboolean scr_drawdialog;						// gl_screen.c
extern cvar_t crosshair;							// gl_screen.c
extern cvar_t r_lerpmodels;							// r_alias.c
extern cvar_t r_lerpmove;							// gl_rmain.c
extern cvar_t gl_farclip;							// gl_rmain.c
extern cvar_t vid_fsaa;								// gl_vidsdl.c
extern GLuint gl_bmodel_vbo;						// r_brush.c
extern size_t gl_bmodel_vbo_size;					// r_brush.c: its size in bytes (glvert_t each)
extern gltexture_t* lightmap_texture;				// r_brush.c: the map's lightmaps
extern vec3_t lightcolor;							// gl_rlight.c: R_LightPoint's light (vr_coil.cpp)
extern gltexture_t* lux_texture;					// r_brush.c: the map's light directions (deluxemaps), or NULL
extern gltexture_t* char_texture;					// gl_draw.c
extern char com_gamenames[];						// common.c
byte *Image_LoadImage (const char *name, int *width, int *height, enum srcformat *fmt); // image.c (image.h hides it from C++)
void M_DrawSlider (int x, int y, float range, const char *desc);	// menu.c
void M_DrawArrowCursor (int cx, int cy);			// menu.c
qboolean SV_RunThink (edict_t *ent);				// sv_phys.c
int SV_FlyMove (edict_t *ent, float time, trace_t *steptrace);
void SV_CheckStuck (edict_t *ent);
qboolean SV_CheckWater (edict_t *ent);
void SV_WalkMove (edict_t *ent);
void SV_CheckVelocity (edict_t *ent);
void SV_CheckWaterTransition (edict_t *ent);
void SV_Impact (edict_t *e1, edict_t *e2);
entity_t *CL_NewTempEntity (void);				// cl_tent.c: a visedict for this frame (after CL_UpdateTEnts)
}

#include "vr_api.h"
#include "vr_api_render.h"
