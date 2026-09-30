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
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end); // world.c
int S_KnownSfxCount (void);							// snd_dma.c (vr_limits)
int S_KnownSfxMax (void);							// snd_dma.c
int Mod_KnownCount (void);							// gl_model.c
int Mod_KnownMax (void);							// gl_model.c
qboolean Mod_CheckFullbrights (byte *pixels, int count);	// gl_model.c (vr_modelload.cpp)
int TexMgr_Count (void);							// gl_texmgr.c
int TexMgr_Max (void);								// gl_texmgr.c
void TexMgr_ReloadImagesNamed (const char *prefix);	// gl_texmgr.c: the textures whose names start with `prefix` uploaded again (vr_sights.cpp)
int Draw_CachedPicsMax (void);						// gl_draw.c
extern int menu_numcachepics;						// gl_draw.c
int Cmd_AliasCount (void);							// cmd.c
int Cmd_CommandCount (void);						// cmd.c
extern cmdlimits_t cmd_limits;						// cmd.c: the command system's high-water marks (vr_api.h)
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
int Con_NotifyLine (int age, const char **text, int *length, double *seconds, int *server); // console.c: a notify line (the wrist gadget's log)
int Cvar_Count (void);								// cvar.c
size_t Draw_PicBytes (void);						// gl_draw.c: the bytes a pic made by Draw_ReplacePic takes
void Draw_ReplacePic (qpic_t *pic, const char *name, int width, int height, byte *data); // gl_draw.c: a lasting pic of 8-bit data
int Mod_ReloadAliasModels (qboolean (*match) (const char *name, void *ctx), void *ctx); // gl_model.c (vr_model_reload)
void TexMgr_ReloadAlphaTested (void);				// gl_texmgr.c: as vr_alpha_coverage changes
gltexture_t *TexMgr_LoadNormalMap (gltexture_t *base, const char *name, int width, int height, enum srcformat format,
	byte *data, const char *source_file, src_offset_t source_offset, int kind, int worldwidth); // gl_texmgr.c: a texture's normal map (VR_LoadNormalMap)
qboolean TexMgr_NormalMapAuthored (gltexture_t *glt); // gl_texmgr.c: whether its normal map is an authored file's (NORMALMAP_FILE)
int TexMgr_NormalMapParallax (gltexture_t *glt);	// gl_texmgr.c: its normal map's heights: 0 none, 1 made ones, 2 an authored file's alpha
void R_RestoreTranslucentTarget (void);			// gl_rmain.c: the translucent pass's framebuffer and viewport again
void R_SetupGL (void);								// gl_rmain.c: the scene's framebuffer and viewport again (after a pass of vr_water.cpp's or vr_haze.cpp's)
void R_DrawAliasModelsDepth (entity_t **ents, int count); // r_alias.c: depth only (the shadow maps' casters)
qboolean R_PaintAliasWounds (entity_t *e, int numsplats, const float *splats); // r_alias.c: into its wound mask (vr_wounds.cpp)
qboolean R_SoftSpritesPending (void);				// r_sprite.c: sprites left for R_DrawSpriteModelsSoft (VR_SoftSprites)
void R_DrawSpriteModelsSoft (GLuint distances);		// r_sprite.c: them, soft, after the translucent pass (vr_particles.cpp)
qboolean GL_GetShaderStorageRange (GLuint index, GLuint *buffer, GLintptr *offset, GLsizeiptr *size); // gl_rmisc.c
int NUM_FOR_EDICT_CHECKED (const edict_t *e);		// pr_edict.c: -1 instead of a Host_Error
void V_SetupView (void);							// view.c: V_RenderView without the drawing
void SV_AreaEdicts (const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace); // world.c: the linked edicts whose boxes touch mins..maxs
int SV_HullPointContents (hull_t *hull, int num, vec3_t p); // world.c: the ledge map (vr_ledges.cpp)
void Z_Usage (int *used, int *peak, int *size);		// zone.c: the zone's bytes in use, most ever, and total (vr_limits)
void Hunk_Usage (int *used, int *peak, int *size, int *segments, int *maxsegments); // zone.c (vr_limits)
}

#include "vr_api.h"
#include "vr_api_render.h"
