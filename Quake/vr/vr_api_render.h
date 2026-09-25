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
void VR_DrawSceneOpaque (void);							// R_RenderScene, after the opaque entities
void VR_DrawSceneTranslucent (void);						// R_RenderScene, after the translucent pass (particles)

// The 2D layer (gl_screen.c, gl_vidsdl.c): drawn to a canvas shown in the headset.
void VR_Begin2D (void);									// SCR_UpdateScreen, before GL_Set2D
void VR_End2D (void);									// SCR_UpdateScreen, after Draw_Flush
int VR_CanvasBlend (void);								// GL_SetStateEx, alpha blending: nonzero if it set the blend

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
float VR_EntityGlow (const struct entity_s *e);				// the force grab glow round an entity (0..1)
void VR_AliasLightCurve (float lightcolor[3]);				// R_SetupAliasLighting, before the minimum light: the lightmap contrast
int VR_ModelDlightsPerPixel (void);						// R_SetupAliasLighting: nonzero to skip adding dynamic lights (the shader does)
void VR_RenderShadowMaps (void);						// R_SetupView, before R_PushDlights (once per frame)
struct gpulight_s;
void VR_DlightShadow (int index, struct gpulight_s *out);	// R_PushDlights, per light sent: its shadow
void VR_PushMapLights (void);							// R_PushDlights, after the dynamic lights
int VR_AliasBonePoses (const struct entity_s *e, const float **matrices); // bone count of an IK-posed skeletal entity (0: none), its 3x4 skinning matrices

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_RENDER_H
