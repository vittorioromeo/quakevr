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

// vr_api_render.h -- the renderer's hooks into the Quake VR module (see vr_api.h). These follow
// Ironwail's OpenGL renderer (its framebuffers, post-process pass, projection matrix, alias
// instancing and 2D canvas): another engine places equivalent calls in its own renderer, and
// the module's renderer-bound code (vr_stereo, vr_gfx_gl, vr_render, vr_panel's canvas) is
// what changes with it (docs/vr-port/PORTING.md). They are only called from the main thread.

#ifndef QVR_VR_API_RENDER_H
#define QVR_VR_API_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

struct entity_s;
struct qmodel_s;

// Stereo rendering (gl_screen.c, gl_rmain.c).
int VR_RenderView (void);								// SCR_UpdateScreen: nonzero if it rendered the eyes
int VR_RenderingEye (void);							// forces the post-process path while rendering an eye
unsigned VR_PostProcessTarget (void);					// GL_PostProcess output framebuffer (0 = window)
void VR_OverrideProjection (float matrix[16]);			// R_SetFrustum: the eye's asymmetric projection
void VR_DrawHiddenArea (void);							// R_RenderScene, after R_Clear: the lenses' hidden area at the near plane (vr_visibility_mask)
void VR_DrawSceneOpaque (void);							// R_RenderScene, after the opaque entities
void VR_WaterView (int contents, int *waterwarp);			// R_SetupView, after r_waterwarp: the liquids' look this view (the eye in `contents`); may turn the warp off
void VR_DetailView (void);								// R_SetupView: the detail textures' settings this view (frame data), their array on unit 12 (vr_detail.cpp)
struct texture_s;
void VR_DetailCall (const struct texture_s *t, float out[4]);	// R_AddBModelCall: a texture's detail (s and t scales, strength, layer; zero: none)
void VR_WaterFog (float fog[4], float skyfog[4]);			// Fog_SetupFrame: an eye's fog in a liquid (vr_water.cpp)
void VR_PostProcessWater (void);						// GL_PostProcess, program in use: an eye's underwater wobble and blur
int VR_MapLiquidAlpha (void);							// R_UpdateLiquidAlpha: nonzero if a map's own liquid alphas (worldspawn) win over the settings (vr_map_liquid_alpha)
unsigned VR_WaterSceneDepth (int translucent);		// liquids drawn: how far the opaque scene is, to refract by and for the foam (0: none)
void VR_WaterMarkVis (const unsigned char *vis);		// R_MarkSurfaces: the geometric waves' mesh faces seen this view (vis: the PVS, null all)
int VR_WaterMeshActive (void);							// R_DrawBrushModels_Water: nonzero to draw the world's liquids from that mesh
int VR_WaterMeshRanges (int texnum, const unsigned **ranges);	// ... its (first index, count) pairs for a world texture
void VR_WaterMeshBind (void);							// ... binds its buffers, attributes 0-4 (4: the swells' pin, the foam's distance to the shore)
void VR_DrawHeatHaze (void);								// R_RenderScene, after the translucent pass: heat haze (vr_haze.cpp)
void VR_DrawSceneTranslucent (void);						// R_RenderScene, after the translucent pass (particles, blended 3D text)
int VR_SoftSprites (void);								// R_DrawSpriteModels: nonzero to leave the sprites to VR_DrawSceneTranslucent, soft (R_DrawSpriteModelsSoft)
float VR_SoftSpriteFade (float radius);				// ... how close in front of the scene a sprite of that radius fades out

// The 2D layer (gl_screen.c, gl_vidsdl.c): drawn to a canvas shown in the headset.
void VR_Begin2D (void);									// SCR_UpdateScreen, before GL_Set2D
void VR_End2D (void (*windowHud) (void));				// SCR_UpdateScreen, after Draw_Flush: windowHud draws the HUD on the window, under the canvas
int VR_SbarInCanvas (void);								// SCR_DrawSbar: 1 to draw the HUD as usual, 0 to leave it to VR_End2D's windowHud, 2 both (the canvas's a classic status bar, for a hand)
int VR_CanvasBlend (void);								// GL_SetStateEx, alpha blending: nonzero if it set the blend
int VR_MenuCanvas (float *scalex, float *scaley);		// Draw_GetCanvasTransform, CANVAS_MENU: nonzero to use these scales (the VR menu style: y scaled more to space the rows out, characters and pictures keeping their size)

// Entities (gl_rmain.c, r_alias.c, r_world.c).
int VR_HideViewModel (void);							// R_IsViewModelVisible: VR draws its own weapons
int VR_IsViewEntity (const struct entity_s *e);		// gets the view model's minimum light
int VR_AliasMirrored (const struct entity_s *e);		// mirrored instances batch and cull separately
void VR_AliasPreTransform (const struct entity_s *e, float matrix[16]);	// after R_EntityMatrix
void VR_AliasPostTransform (const struct entity_s *e, float matrix[16]);	// after the model scale
void VR_BrushTransform (const struct entity_s *e, float matrix[16]);		// brush entities: the networked scale and offset
float VR_EntityScale (const struct entity_s *e);	// ENTSCALE_DECODE(e->scale) times a prop's Size (Held Object Offsets): its bounds as drawn
int VR_AliasZeroBlend (const struct entity_s *e, const void *aliashdr, int totalverts); // instance padding
// The alias instance's Quake VR data, after Ironwail's (r_alias.c's aliasinstance_t; the shaders' InstanceData).
typedef struct vraliasinstance_s
{
	float		lightdir[4]; // the direction it is shaded from (vr_modellight.cpp)
	float		glow[4]; // the force grab glow (vr_fgfx.cpp), the shading on a par with the world and bumps, the sights' glow, the parallax depth
	float		ambient[6][4]; // the light around it, +X -X +Y -Y +Z -Z (vr_ambient.cpp); a morph in .w (vr_render.cpp)
	float		surface[4]; // rim light, reflections' strength and blur (vr_envmap.cpp)
	float		ao[4]; // dynamic ambient occlusion: its own group, its per-vertex occlusion's strength (vr_ao.cpp); z the normal map's strength
	float		wound[4]; // its wound mask (vr_wounds.cpp): layer + 1 (0 none), its size in texels, the time
} vraliasinstance_t;
void VR_AliasInstance (const struct entity_s *e, const float matrix[16], const void *aliashdr, int kind, vraliasinstance_t *out); // R_DrawAliasModel_Real: kind 1 standard, 0 showtris/showskel, 2 depth only (all zero)
void VR_AliasFlameRefs (const void *aliashdr, unsigned short *refs); // GLMesh_LoadVertexBuffer: per VBO vertex, 0 or 1 + the gun vertex a muzzle flash's vertex rides on
void VR_AliasMorph (const struct entity_s *e, const void *aliashdr, float ambient[24]); // instance: a weapon's morph into its other model (Ambient[2..5].w)
void VR_AliasLightModifier (const struct entity_s *e, float lightcolor[3]); // end of R_SetupAliasLighting
void VR_AliasLightOrigin (const struct entity_s *e, float origin[3]);		// R_SetupAliasLighting: where the light is sampled
void VR_AliasLightDir (const struct entity_s *e, float dir[4]);	// instance: the direction the model is shaded from (w 0: the fixed one)
void VR_AliasAmbient (const struct entity_s *e, const float matrix[16], const void *aliashdr, int enabled, float cube[24]); // instance: the light around it, 6 faces (vr_ambient.cpp)
void VR_AliasSurface (const struct entity_s *e, float out[4]); // instance: rim light, reflections' strength and blur (vr_envmap.cpp)
unsigned VR_EnvCubeTexture (void);							// the reflections' cube map (0: none yet; vr_envmap.cpp)
float VR_EntityGlow (const struct entity_s *e);				// the force grab glow round an entity (0..1)
void VR_EntityGlowColor (float rgb[3]);						// and its colour (the player's hue; SceneTone.yzw in the frame data)
float VR_EntityFullbrightBoost (const struct entity_s *e);	// how much brighter its dim fullbright texels shine (0 none): the held weapons' sights (vr_weapon_glow)
void VR_AliasLightCurve (float lightcolor[3]);				// R_SetupAliasLighting, before the minimum light: the lightmap contrast
int VR_ModelDlightsPerPixel (void);						// R_SetupAliasLighting: nonzero to skip adding dynamic lights (the shader does)
void VR_RenderShadowMaps (void);						// R_SetupView, before R_PushDlights (once per frame)
struct gpulight_s;
void VR_DlightShadow (int index, struct gpulight_s *out);	// R_PushDlights, per light sent: its shadow (and a spot light's cone)
float VR_SpotCone (const struct gpulight_s *l, const float point[3]); // how much of a light its cone lets reach a point (1: a point light)
void VR_PushMapLights (void);							// R_PushDlights, after the dynamic lights
int VR_AliasBonePoses (const struct entity_s *e, const float **matrices); // bone count of an IK-posed skeletal entity (0: none), its 3x4 skinning matrices
void VR_AliasWound (const struct entity_s *e, float out[4]);	// instance: its wound mask (vr_wounds.cpp): layer + 1 (0 none), size in texels, time
unsigned VR_WoundTexture (void);							// the wound masks' texture array (0: none; vr_wounds.cpp)

// The DarkPlaces look (vr_lighting.cpp; docs/vr-port/LIGHTING.md, round 10).
float VR_PostProcessBloom (void);								// GL_PostProcess: an eye's glow bound to texture unit 2, and how much of it to add (0: none)
void VR_PostProcessGamma (float *gamma, float *contrast);	// GL_PostProcess: while rendering an eye, the headset's (vr_gamma, vr_contrast)
int VR_TextureSmoothing (void);							// TexMgr_ApplySettings: 1 replacement textures smooth, 2 all (vr_texture_smooth)
int VR_NormalMaps (void);								// Mod_LoadTextures, skins: nonzero to make normal maps (vr_normalmaps)
int VR_AlphaMipCoverage (void);							// TexMgr_LoadImage32: nonzero to keep alpha-tested textures' coverage in their mips (vr_alpha_coverage)
int VR_AlphaToCoverage (void);							// alpha-tested draws (r_world.c, r_alias.c): nonzero for alpha to coverage (vr_alpha_coverage, with MSAA)
float VR_ParallaxDepth (const struct entity_s *e, const float matrix[16], const float modelscale[3], int heights); // instance: its parallax depth in units (0 off); matrix the drawn one, modelscale an alias model's (NULL: a brush model); heights: its skin's normal map's (TexMgr_NormalMapParallax: 0 none, 1 made, 2 authored)
int VR_ModelLightParity (void);							// R_SetupAliasLighting: models as bright as the floor under them
float VR_ModelBumps (const struct entity_s *e, int authored);	// instance: how much the skin's bumps shade the model's own light (0 none; authored: its normal map is a file's)
float VR_ModelNormalMapScale (int authored);				// instance: how much its normal map bends the normal (vr_normalmap_strength, or vr_normalmap_authored)
float VR_ViewModelMinLight (void);						// R_SetupAliasLighting: least light on the hands and weapons (Quake's 24)

// Normal maps and alpha-tested mipmaps, made as textures load (vr_normalmaps.cpp; gl_texmgr.c's TexMgr_LoadImage32).
// Normal maps for world textures and model skins (gl_texmgr.c; vr_normalmaps): made from the texture's shading
// (NORMALMAP_SHADING: its luminance as height, or a *_bump height map's), or an authored *_norm map (NORMALMAP_AUTHORED).
// The world's (NORMALMAP_HEIGHTS or'ed in) carry the height parallax mapping walks in alpha (vr_parallax).
// NORMALMAP_FILE: from an authored file (*_norm, or a *_bump's heights): a real shape, drawn at its own strength on
// models (vr_normalmap_authored). NORMALMAP_SKIN: made from a model skin's colours (TexMgr_SkinToNormals: edges,
// materials and larger forms, not brightness as height). NORMALMAP_TYPE: the first three. NORMALMAP_FLAT (set on
// loading): an authored map with NORMALMAP_HEIGHTS whose alpha is all 255, no heights (no parallax on it).
enum { NORMALMAP_NONE, NORMALMAP_SHADING, NORMALMAP_AUTHORED, NORMALMAP_HEIGHTS = 4, NORMALMAP_FILE = 8, NORMALMAP_SKIN = 16,
	NORMALMAP_FLAT = 32 };
#define NORMALMAP_TYPE(kind) ((kind) & 3)
struct gltexture_s;
void VR_NormalMapMipSize (int worldwidth, int *mipwidth, int *mipheight);	// a made one's size (NORMALMAP_SHADING): mipmapped down to at most this
int VR_MakeNormalMap (struct gltexture_s *glt, unsigned char *data, int kind, int worldwidth); // its RGBA texels made from the shading, or an authored one's heights; returns the kind (NORMALMAP_FLAT)
float VR_AlphaCoverage (const unsigned char *data, int count);	// an alpha-tested texture's share of texels that pass the test
void VR_AlphaCoverageMip (const unsigned char *in, unsigned char *out, int count, float coverage); // a mip level with that coverage kept (vr_alpha_coverage)
void VR_SetHeightMask (const unsigned char *mask, int width, int height); // a skin's islands for the heights made next (NULL: none)
void VR_SetHeightMaskLazy (const float *corners, int numtris, int width, int height,
	unsigned char *(*make) (const float *corners, int numtris, int w, int h)); // the same, made from the triangles only if needed
void VR_LoadNormalMap (struct gltexture_s *glt, const char *image, const char *shared, unsigned char *data, enum srcformat format,
	int worldwidth, int flags);								// Mod_LoadTextures, skins: a texture's normal map (authored beside `image` or `shared`, or made from `data`)
unsigned char *VR_NormalMapSource (unsigned char *data, enum srcformat fmt, int width, int height); // Mod_LoadTextures: an RGBA image kept whole for it

// Model loading (vr_modelload.cpp; gl_model.c). `mod` is the model loading, `hdr` its aliashdr_t.
void VR_LoadLux (struct qmodel_s *mod, lump_t *l);		// Mod_LoadLighting: the light's directions (a .lux beside the .lit; deluxemaps)
void VR_ItemTextureClamp (struct qmodel_s *mod);			// Mod_LoadBrushModel: an item box's faces' texture ranges (maps/b_*: parallax stops at their edges)
void VR_SkinsBegin (void);								// Mod_LoadAllSkins, first
void VR_ExternalSkin (struct qmodel_s *mod, void *hdr, struct gltexture_s **skin, struct gltexture_s **fb, unsigned texflags,
	unsigned char *texels, int size, src_offset_t offset, int i, int j); // Mod_LoadAllSkins: skin i (frame j of a group, -1 none) replaced by a full-colour one, its normal map queued
void VR_LoadSkinNormalMaps (struct qmodel_s *mod, void *hdr, const stvert_t *verts, const dtriangle_t *tris); // Mod_LoadAliasModel: the queued skins' normal maps, their islands from the triangles
void VR_MD5SkinNormalMap (void *surf, struct gltexture_s *glt, const char *shader, int skin, int frame, unsigned char *data,
	enum srcformat fmt, int w, int h);						// Mod_LoadMD5Skins: an MD5 skin's normal map
void VR_MD5SkinsReset (void);							// Mod_LoadMD5Skins, before and after a surface's skins


// Dynamic ambient occlusion (vr_ao.cpp; see vr_ao.hpp).
// GLMesh_LoadVertexBuffer: a Quake model's per-pose, per-vertex visibility (0 fully occluded .. 255 open), numposes x
// numverts in the model's own vertex order; NULL when there is none (not a single-surface .mdl). Cached by model.
const unsigned char *VR_AliasVertexAO (struct qmodel_s *model, const void *aliashdr);

// R_DrawAliasModel_Real: the instance's occlusion settings: [0] its own occluder group (0 none), [1] how much of its
// baked per-vertex occlusion applies (vr_ao_models), [2] [3] unused.
void VR_AliasAO (const struct entity_s *e, float out[4]);

// R_InitBModelInstance: a brush model's own occluder group (0 none), which its box never darkens.
float VR_BrushAOSelf (const struct entity_s *e);

// The weapons' sights in the chosen colour (vr_sights.cpp; see vr_sights.hpp).
// TexMgr_LoadImage8: the palette the 8-bit texture `texname` is uploaded with, `palette` (the one
// Quake would use) or, for a sighted weapon's skin with a hue other than the sights' own, a copy of
// it with the sight colours recoloured (valid until the next call).
unsigned int* VR_SightPalette(const char* texname, unsigned int* palette);

// The eyes' tone curve, grade and dither (vr_tonemap.cpp; see vr_tonemap.h).
unsigned VR_SceneColorFormat (unsigned format);	// GL_CreateFrameBuffers: the scene's colour format (the eyes' float one with vr_tonemap)
int VR_SceneSamples (int samples);	// GL_CreateFrameBuffers: the scene's MSAA samples (the spectator camera's: vr_spectator_aa)
float VR_SceneTone (void);						// R_SetupView: the brightest the world and models write (1: Quake's clamp)
float VR_SceneDither (float dither);			// R_SetupView: the scene's screen dither (0 in the eyes with vr_dither: the post-process dithers last)
void VR_PostProcessTone (void);					// GL_PostProcess, the non-palettized program in use: the tone curve, grade (unit 3) and dither

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_RENDER_H
