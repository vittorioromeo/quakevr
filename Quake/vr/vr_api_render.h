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
void VR_End2D (void);									// SCR_UpdateScreen, after Draw_Flush
int VR_CanvasBlend (void);								// GL_SetStateEx, alpha blending: nonzero if it set the blend
int VR_MenuCanvas (float *scalex, float *scaley);		// Draw_GetCanvasTransform, CANVAS_MENU: nonzero to use these scales (the VR menu style: y scaled more to space the rows out, characters and pictures keeping their size)

// Entities (gl_rmain.c, r_alias.c, r_world.c).
int VR_HideViewModel (void);							// R_IsViewModelVisible: VR draws its own weapons
int VR_IsViewEntity (const struct entity_s *e);		// gets the view model's minimum light
int VR_AliasMirrored (const struct entity_s *e);		// mirrored instances batch and cull separately
void VR_AliasPreTransform (const struct entity_s *e, float matrix[16]);	// after R_EntityMatrix
void VR_AliasPostTransform (const struct entity_s *e, float matrix[16]);	// after the model scale
void VR_BrushTransform (const struct entity_s *e, float matrix[16]);		// brush entities: the networked scale and offset
int VR_AliasZeroBlend (const struct entity_s *e, const void *aliashdr, int totalverts); // instance padding
void VR_AliasLightModifier (const struct entity_s *e, float lightcolor[3]); // end of R_SetupAliasLighting
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

// The DarkPlaces look (vr_lighting.cpp; docs/vr-port/LIGHTING.md, round 10).
float VR_PostProcessBloom (void);								// GL_PostProcess: an eye's glow bound to texture unit 2, and how much of it to add (0: none)
void VR_PostProcessGamma (float *gamma, float *contrast);	// GL_PostProcess: while rendering an eye, the headset's (vr_gamma, vr_contrast)
int VR_TextureSmoothing (void);							// TexMgr_ApplySettings: 1 replacement textures smooth, 2 all (vr_texture_smooth)
int VR_NormalMaps (void);								// Mod_LoadTextures, skins: nonzero to make normal maps (vr_normalmaps)
int VR_AlphaMipCoverage (void);							// TexMgr_LoadImage32: nonzero to keep alpha-tested textures' coverage in their mips (vr_alpha_coverage)
int VR_AlphaToCoverage (void);							// alpha-tested draws (r_world.c, r_alias.c): nonzero for alpha to coverage (vr_alpha_coverage, with MSAA)
float VR_ParallaxDepth (const struct entity_s *e, const float matrix[16], const float modelscale[3]); // instance: its parallax depth in units (0 off); matrix the drawn one, modelscale an alias model's (NULL: a brush model)
int VR_ModelLightParity (void);							// R_SetupAliasLighting: models as bright as the floor under them
float VR_ModelBumps (const struct entity_s *e);				// instance: how much the skin's bumps shade the model's own light (0 none)
float VR_ViewModelMinLight (void);						// R_SetupAliasLighting: least light on the hands and weapons (Quake's 24)

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_RENDER_H
