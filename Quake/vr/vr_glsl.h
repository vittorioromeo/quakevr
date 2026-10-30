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

// vr_glsl.h -- Quake VR's GLSL in Ironwail's shaders, kept out of gl_shaders.h (which includes this file before its
// own snippets): each macro here is spliced into a shader there by name, at a line marked // QVR. Macros only, so
// that the upstream snippets they use (LIGHT_BUFFER, NOISE_FUNCTIONS, ...) are expanded where the shader is defined.
// Everything here is the same text the shaders had inline (docs/vr-port/IRONWAIL_DIFF.md).

#pragma once

#include "vr_blood.h"
#include "vr_tonemap.h" // QVR_TONE_GLSL, the eyes' tone curve and grade (the post-process and the mirror)
#include "vr_retro.h" // QVR_RETRO_GLSL, retro textures (the world and model shaders)
#include "vr_retrolight.h" // QVR_RETROLIGHT_GLSL, retro lighting (in SHADOW_FUNCTIONS)

// the eyes' bloom (vr_bloom.cpp), under-water view (vr/vr_water.cpp), tone curve, grade and dither (vr/vr_tonemap.cpp)
#define QVR_POSTPROCESS_UNIFORMS \
"layout(binding=2) uniform sampler2D BloomTexture; // QVR: an eye's glow (vr_bloom.cpp)\n" \
"layout(location=1) uniform float BloomStrength; // QVR\n" \
"layout(binding=6) uniform sampler2D WaterScene; // QVR: GammaTexture read smoothly, under water (vr/vr_water.cpp)\n" \
"layout(location=2) uniform vec4 WaterParams; // QVR: time, wobble and blur (of the height; both 0: not under water)\n" \
"layout(location=3) uniform vec4 WaterProj; // QVR: the projection: ndc x = x + y * left / forward, ndc y = z + w * up / forward\n" \
"layout(location=4) uniform vec3 WaterFwd; // QVR: the eye's axes in the world\n" \
"layout(location=5) uniform vec3 WaterLeft; // QVR\n" \
"layout(location=6) uniform vec3 WaterUp; // QVR\n" \
"layout(binding=3) uniform sampler3D GradeLUT; // QVR: the eye's colour grade (vr_grade; vr/vr_tonemap.cpp)\n" \
"layout(location=7) uniform vec4 Tone; // QVR: exposure (0: no tone curve), the curve's knee and white point, the grade's strength (0: none)\n" \
"layout(location=8) uniform vec4 Dither; // QVR: the eye's last dither: amplitude (0: none), the right eye's own noise (1), frame offset (0: fixed), unused\n" \
"layout(location=9) uniform vec4 SlowLook; // QVR: bullet time's look (vr/vr_bullettime.cpp): strength (0: none), desaturation, vignette; w: the comfort vignette (vr_input.cpp)\n" \
"layout(location=10) uniform vec3 SlowTint; // QVR: ... its tint\n" \
"\n" \
QVR_TONE_GLSL

// under water, the view wobbled and blurred; the bloom added
#define QVR_POSTPROCESS_WATER_BLOOM \
"	vec2 buv = gl_FragCoord.xy / vec2(textureSize(GammaTexture, 0)); // QVR\n" \
"	if (WaterParams.y + WaterParams.z > 0.0) // QVR: under water: a slow wobble of the direction looked in, in the world\n" \
"	{ // (the same in both eyes, fixed to the world as the head turns), and a little blur\n" \
"		vec2 size = vec2(textureSize(GammaTexture, 0));\n" \
"		vec2 ndc = buv * 2.0 - 1.0;\n" \
"		vec3 dir = normalize(WaterFwd + WaterLeft * ((ndc.x - WaterProj.x) / WaterProj.y) + WaterUp * ((ndc.y - WaterProj.z) / WaterProj.w));\n" \
"		float t = WaterParams.x;\n" \
"		vec2 w = vec2(sin(dot(dir, vec3(6.1, 2.3, 4.7)) + t * 1.1) + 0.5 * sin(dot(dir, vec3(-3.7, 8.3, 2.9)) - t * 1.6),\n" \
"			sin(dot(dir, vec3(2.9, -5.9, 6.3)) + t * 0.9) + 0.5 * sin(dot(dir, vec3(7.7, 1.9, -4.9)) + t * 1.4));\n" \
"		vec2 aspect = vec2(size.y / size.x, 1.0);\n" \
"		buv = clamp(buv + w * (WaterParams.y * aspect), vec2(0.0), vec2(1.0));\n" \
"		vec2 b = max(WaterParams.z, 0.5 / size.y) * aspect;\n" \
"		out_fragcolor.rgb = (texture(WaterScene, buv + b).rgb + texture(WaterScene, buv - b).rgb +\n" \
"			texture(WaterScene, buv + vec2(b.x, -b.y)).rgb + texture(WaterScene, buv + vec2(-b.x, b.y)).rgb) * 0.25;\n" \
"	}\n" \
"	if (BloomStrength > 0.0) // QVR: smoothed up from a quarter of the size by four bilinear taps\n" \
"	{\n" \
"		vec2 bt = 0.5 / vec2(textureSize(BloomTexture, 0));\n" \
"		out_fragcolor.rgb += (texture(BloomTexture, buv + vec2(-bt.x, -bt.y)).rgb + texture(BloomTexture, buv + vec2(bt.x, -bt.y)).rgb +\n" \
"			texture(BloomTexture, buv + vec2(-bt.x, bt.y)).rgb + texture(BloomTexture, buv + vec2(bt.x, bt.y)).rgb) * (0.25 * BloomStrength);\n" \
"	}\n" \
"	else if (BloomStrength < 0.0) // QVR: vr_bloom_fast: by one tap, the smoothing done at the quarter (vr_bloom.cpp)\n" \
"		out_fragcolor.rgb -= texture(BloomTexture, buv).rgb * BloomStrength;\n"

// graded (vr_grade) and dithered into the 8-bit image
#define QVR_POSTPROCESS_GRADE_DITHER \
"	if (SlowLook.x > 0.0) // QVR: bullet time's look: the colour drained towards grey, tinted, the edges darkened\n" \
"	{\n" \
"		vec2 sv = gl_FragCoord.xy / vec2(textureSize(GammaTexture, 0)) * 2.0 - 1.0;\n" \
"		float luma = dot(out_fragcolor.rgb, vec3(0.299, 0.587, 0.114));\n" \
"		vec3 c = mix(out_fragcolor.rgb, vec3(luma), SlowLook.x * SlowLook.y);\n" \
"		c *= mix(vec3(1.0), SlowTint, SlowLook.x);\n" \
"		c *= 1.0 - SlowLook.x * SlowLook.z * smoothstep(0.35, 1.7, dot(sv, sv));\n" \
"		out_fragcolor.rgb = c;\n" \
"	}\n" \
"	if (SlowLook.w > 0.0) // QVR: the comfort vignette (vr_comfort_vignette): the edges darkened while the sticks move or\n" \
"	{ // turn you, the clear middle narrower the stronger it is\n" \
"		vec2 cv = gl_FragCoord.xy / vec2(textureSize(GammaTexture, 0)) * 2.0 - 1.0;\n" \
"		float r0 = mix(2.0, 0.1, SlowLook.w);\n" \
"		out_fragcolor.rgb *= 1.0 - smoothstep(r0, r0 + 0.6, dot(cv, cv));\n" \
"	}\n" \
"	if (Tone.w > 0.0) // QVR: graded (vr_grade)\n" \
"		out_fragcolor.rgb = QvrGrade(GradeLUT, out_fragcolor.rgb, Tone.w);\n" \
"	if (Dither.x > 0.0) // QVR: dithered last, into the 8-bit image: triangular noise, 1/255 either way, from two hashes\n" \
"	{ // (no pattern; the eyes' own, unrelated, so they don't fuse into a layer seen \"in the lens\"); less on black\n" \
"		vec2 p = floor(gl_FragCoord.xy) + Dither.y * vec2(1733.0, 911.0) + Dither.z * vec2(193.0, 97.0);\n" \
"		float n = whitenoise01(p) + whitenoise01(p + vec2(57.0, 113.0)) - 1.0;\n" \
"		float m = max(out_fragcolor.r, max(out_fragcolor.g, out_fragcolor.b));\n" \
"		out_fragcolor.rgb += n * Dither.x * clamp(m * 255.0, 0.0, 1.0);\n" \
"	}\n"

// the frame data's Quake VR fields (lights, liquids, detail, tone)
#define QVR_FRAMEDATA_FIELDS \
"	uint	ShadowFlags; // QVR\n" \
"	vec4	LightTweak; // QVR: lightmap contrast, the normal maps' share of the baked light, specular intensity, normal map strength\n" \
"	vec4	Parallax; // QVR: parallax mapping: depth in units (0 off), the distance it ends at, the most steps; w specular anti-aliasing (vr_specular_aa, 0 off)\n" \
"	vec4	Parallax2; // QVR: ... x the steps refining the hit (vr_parallax_refine), y z the cosines its grazing fade starts and ends at (vr_parallax_grazing; 0 0: none), w 1: the hits' depth written (vr_parallax_depth_write)\n" \
"	vec4	Water; // QVR: liquids (vr/vr_water.cpp): waves, fresnel, refraction (0: no scene to read), glints\n" \
"	vec4	Water2; // QVR: lava glow, caustics (0 off), the eye in a liquid (1), unused\n" \
"	vec4	CausticsOrigin; // QVR: xyz where the liquid volume (LiquidVolume) starts, in the world; w its cell size\n" \
"	vec4	CausticsScale; // QVR: xyz one over its size in units; w 1: the scene's distances to refract by and for the foam (LiquidDepth)\n" \
"	vec4	Detail; // QVR: detail textures (vr/vr_detail.cpp): strength (0 off), the distance they start fading, where gone, the fine octave's scale (0 none)\n" \
"	vec4	SceneTone; // QVR: x the brightest the world and models write (1: Quake's clamp; more in the eyes' float scene with vr_tonemap: vr/vr_tonemap.cpp), yzw the force grab glow's colour (vr/vr_fgfx.cpp)\n" \
"	vec4	Water3; // QVR: x shoreline foam (vr/vr_water.cpp: vr_water_foam, 0 off), y the ripples' slopes in the shading, times their shape's (vr_water_ripple_normal), zw unused\n" \
"	vec4	RetroLight[6]; // QVR: retro lighting (vr/vr_retrolight.h; all 0: off)\n" \
"	vec4	Ripple; // QVR: splash ripples (vr/vr_water.cpp: vr_water_ripples): x how many, y their rings' speed (units/s), z the wave number, w the share of them in the geometry\n" \
"	vec4	RippleAt[32]; // QVR: ... each's centre (xy), the surface's height (z), its age in seconds (w)\n" \
"	vec4	RippleAmp[8]; // QVR: ... each's height now, in units (four a vec4)\n" \
"	vec4	DecalClock; // QVR: the decals on the world (vr/vr_decals.cpp, QVR_DECAL_FUNCTIONS): x now on their clock, y vr_decal_life, z 1 on (0: none, or drawn as meshes), w the decals' retro textures set (vr_retro.h; 0 none)\n" \
"	vec4	WaterCube; // QVR: water reflections (vr/vr_envmap.cpp: vr_water_reflections): the water cube's centre (xyz), strength (w, 0 off)\n" \
"	vec4	WaterCube2; // QVR: ... the height of the surface it is for, how far from its centre it fades out, its sharpest mip level read, its last\n" \
"	vec4	PortalPlane[8]; // QVR: teleporters (vr/vr_portals.cpp: vr_portals): the side shown in this view, its plane (normal, distance)\n" \
"	vec4	PortalMin[8]; // QVR: ... its box (xyz), how much of the view through it is shown (w, 0 none)\n" \
"	vec4	PortalMax[8]; // QVR: ... its box (xyz)\n" \
"	vec4	PortalUV; // QVR: scene viewport to portal composite pixels\n" \
"	vec4	TeleportLook; // QVR: starry surface size and opacity\n" \
"	vec4	AmbientLight; // QVR: the baked light's own fill light (vr/vr_lighting.cpp: vr_ambient_light): x is the share of Quake's full light added to the lightmap before its contrast (0 none); yzw unused\n" \
"	vec4	PermDecals; // QVR: the permanent decals' layer (vr/vr_permdecals.cpp, QVR_PERMDECAL_FUNCTIONS): x 1 on (0 none; 2 Quake's own textures drawn nearest, 3 every texture: TexMgr_NearestKind), y its atlas's slots a row, z one over its side in texels\n" \
"	vec4	Parallax3; // QVR: parallax mapping, more: x where the surface's polygon sits in the relief (0 its top, 1 its bottom: the world's); y the depth write's split\n" \
"	vec4	DecalCarve; // QVR: the carved decals (vr/vr_decals.cpp, QVR_DECAL_FUNCTIONS' CarveAt): x the depth's multiplier (0 off), y the rim's, z the most steps a ray takes, w the distance past which they are flat\n" \
"	vec4	Foam[3]; // QVR: the shoreline foam's look, each liquid's (vr/vr_water.cpp foamLook, LiquidFoam; 0 water, 1 lava, 2 slime): x its opacity (0 off), y how finely it is broken up, z how far it reaches (times), w its wavy edge (units, 0 off)\n" \
"	vec4	FoamColor[3]; // QVR: ... its colour (rgb), w unused\n" \
"	vec4	FoamBreath[3]; // QVR: ... its breathing: x how much the band grows and shrinks (a share of its width, 0 none), y how fast, z how unevenly along the shore, w unused\n" \
"	vec4	BloodInfo; // QVR: blood in the liquids (vr/vr_water.cpp: vr_water_blood; LiquidBlood): x how many clouds (0 none), yzw unused\n" \
"	vec4	BloodAt[16]; // QVR: ... each's centre (xy), the surface's height (z), its radius now (w)\n" \
"	vec4	BloodAmp[4]; // QVR: ... each's strength now (four a vec4)\n"

// Single-pass stereo (vr/vr_stereo_single.cpp, docs/vr-port/STEREO.md): a multiview twin (QVR_MULTIVIEW) reads each
// view's view-projection and eye position from this block; the blocks that declare ViewProj, EyePos, FrameViewProj and
// FrameEyePos name their own members otherwise there (QVR_MONO), and these macros stand for them. Without
// QVR_MULTIVIEW, nothing.
#define QVR_STEREO_VIEWS \
"#if defined(QVR_MULTIVIEW) && !defined(QVR_STEREO_VIEWS)\n"\
"#define QVR_STEREO_VIEWS\n"\
"layout(std140, binding=5) uniform QvrStereoUBO\n"\
"{\n"\
"	mat4	QvrViewProj[2];\n"\
"	vec4	QvrEyePos[2];\n"\
"};\n"\
"#define ViewProj QvrViewProj[QVR_VIEW]\n"\
"#define EyePos QvrEyePos[QVR_VIEW].xyz\n"\
"#define FrameViewProj QvrViewProj[QVR_VIEW]\n"\
"#define FrameEyePos QvrEyePos[QVR_VIEW].xyz\n"\
"#endif\n"

// the frame data the alias shaders read (vr/vr_lighting.cpp); its own names, since the alias
// instance buffer has a ViewProj, Fog, EyePos and ScreenDither of its own.
#define ALIAS_FRAMEDATA_BUFFER \
QVR_STEREO_VIEWS \
"layout(std140, binding=0) uniform FrameDataUBO\n"\
"{\n"\
"#ifdef QVR_MULTIVIEW\n"\
"	mat4	FrameViewProjMono;\n"\
"#else\n"\
"	mat4	FrameViewProj;\n"\
"#endif\n"\
"	vec4	FrameFog;\n"\
"	vec4	FrameSkyFog;\n"\
"	vec3	FrameWindDir;\n"\
"	float	FrameWindPhase;\n"\
"	float	FrameScreenDither;\n"\
"	float	FrameTextureDither;\n"\
"	float	ShadowBias;\n"\
"	float	DlightAngle;\n"\
"#ifdef QVR_MULTIVIEW\n"\
"	vec3	FrameEyePosMono;\n"\
"#else\n"\
"	vec3	FrameEyePos;\n"\
"#endif\n"\
"	float	FrameTime;\n"\
"	float	ZLogScale;\n"\
"	float	ZLogBias;\n"\
"	uint	NumLights;\n"\
"	uint	ShadowFlags;\n"\
"	vec4	LightTweak;\n"\
"	vec4	Parallax;\n"\
"	vec4	Parallax2;\n"\
"	vec4	FrameWater; // QVR: the rest as FRAMEDATA_BUFFER's, up to SceneTone\n"\
"	vec4	FrameWater2;\n"\
"	vec4	FrameCausticsOrigin;\n"\
"	vec4	FrameCausticsScale;\n"\
"	vec4	FrameDetail;\n"\
"	vec4	SceneTone; // QVR: x the brightest models write (vr/vr_tonemap.cpp), yzw the force grab glow's colour\n"\
"	vec4	FrameWater3; // QVR: zw your own wounds' relief, burns' and blood's (vr/vr_wounds.cpp: vr_wounds_bump_burns, vr_wounds_bump_blood)\n"\
"	vec4	RetroLight[6]; // QVR: retro lighting (vr/vr_retrolight.h), as FRAMEDATA_BUFFER's\n"\
"	vec4	FrameRipple; // QVR: as FRAMEDATA_BUFFER's, up to the teleporters shown (QVR_BEHIND_SHOWN_GATE)\n"\
"	vec4	FrameRippleAt[32];\n"\
"	vec4	FrameRippleAmp[8];\n"\
"	vec4	FrameDecalClock;\n"\
"	vec4	FrameWaterCube;\n"\
"	vec4	FrameWaterCube2;\n"\
"	vec4	FramePortalPlane[8];\n"\
"	vec4	FramePortalMin[8];\n"\
"	vec4	FramePortalMax[8];\n"\
"};\n"\
"\n"\

// The shaders' pieces by family, in their old order (the head above and the tail below stay here).
#include "vr_glsl_lighting.h"
#include "vr_glsl_surface.h"
#include "vr_glsl_world.h"
#include "vr_glsl_alias.h"

// What lies behind a teleporter shown in this view (vr/vr_portals.cpp VR_PortalFrameData: PortalPlane, PortalMin,
// PortalMax), seen through its aperture from the eye: the view through the gate covers it, but a see-through gate
// surface (vr_teleporter_surface_opacity under 1) writes no depth, and what is drawn after it (Quake's particles, sprites,
// see-through models) showed over it. `pos`: in the world.
// (The alias shaders' frame data has its own names: QVR_BEHIND_SHOWN_GATE_ALIAS.)
#define QVR_BEHIND_SHOWN_GATE \
"bool BehindShownGate(vec3 pos)\n" \
"{\n" \
"	vec3 eye = EyePos;\n" \
"	for (int i = 0; i < 8; ++i)\n" \
"	{\n" \
"		if (PortalMin[i].w <= 0.)\n" \
"			continue;\n" \
"		float a = dot(eye, PortalPlane[i].xyz) - PortalPlane[i].w;\n" \
"		float b = dot(pos, PortalPlane[i].xyz) - PortalPlane[i].w;\n" \
"		if (a <= 0. || b >= 0.)\n" \
"			continue;\n" \
"		vec3 c = eye + (pos - eye) * (a / (a - b));\n" \
"		if (all(greaterThanEqual(c, PortalMin[i].xyz - 1.)) && all(lessThanEqual(c, PortalMax[i].xyz + 1.)))\n" \
"			return true;\n" \
"	}\n" \
"	return false;\n" \
"}\n"

#define QVR_BEHIND_SHOWN_GATE_ALIAS \
"#define PortalPlane FramePortalPlane\n" \
"#define PortalMin FramePortalMin\n" \
"#define PortalMax FramePortalMax\n" \
"#ifndef QVR_MULTIVIEW\n" /* (a multiview twin's EyePos is the view's already: QVR_STEREO_VIEWS) */ \
"#define EyePos FrameEyePos\n" \
"#endif\n" \
QVR_BEHIND_SHOWN_GATE \
"#ifndef QVR_MULTIVIEW\n" \
"#undef EyePos\n" \
"#endif\n"

// soft sprites, fading out close in front of the scene
#define QVR_SPRITES_FS_SOFT \
"	if (Soft.y > 0.0) // QVR: soft: fading out as the opaque scene comes close behind (its distances: half size, the nearest of four)\n" \
"	{\n" \
"		float f = 1.0;\n" \
"		if (Soft.x > 0.0)\n" \
"		{\n" \
"			ivec2 p = min(ivec2(gl_FragCoord.xy) >> 1, textureSize(SceneDistances, 0) - 1);\n" \
"			f = clamp((texelFetch(SceneDistances, p, 0).r - in_depth) / Soft.x, 0.0, 1.0);\n" \
"			f = f * f * (3.0 - 2.0 * f);\n" \
"			if (f < 0.004)\n" \
"				discard; // nor its depth\n" \
"		}\n" \
"		f *= Soft.z; // its entity's alpha (r_sprite.c: AD's see-through flames)\n" \
"		out_fragcolor = vec4(out_fragcolor.rgb * f, f);\n" \
"	}\n"

// a spot light's bounding sphere
#define QVR_CLUSTER_SPOT_BOUNDS \
"			vec3 center = l.origin;\n" \
"			float radius = l.radius;\n" \
"			float s = length(l.spot.xyz);\n" \
"			if (s > 0.) // QVR: a spot light: the smallest sphere round its cone (out to its radius)\n" \
"			{\n" \
"				vec3 dir = l.spot.xyz / s;\n" \
"				float c = clamp((l.spot.w - 1.0) / s, 0.0, 1.0); // the cosine of the outer cone\n" \
"				float along = c >= 0.7071 ? l.radius / (2.0 * c) : l.radius * c;\n" \
"				radius = c >= 0.7071 ? along : l.radius * sqrt(1.0 - c * c);\n" \
"				center += dir * along;\n" \
"				radius = min(radius * 1.01 + 1.0, l.radius);\n" \
"			}\n"

// Temporal anti-aliasing's motion vectors (vr_temporal.cpp, docs/vr-port/TEMPORAL.md 5): the eye's view-projection a
// frame ago (unjittered) and this frame's jitter; all zero outside the eyes' scenes (TemporalJitter.w 0: none written).
#define QVR_TEMPORAL_BLOCK \
"layout(std140, binding=7) uniform TemporalBlock\n" \
"{\n" \
"	mat4	PrevViewProj;\n" \
"	vec4	TemporalJitter; // xy the jitter in uv, z the eye's stamp, w 1 while the eye collects motion vectors\n" \
"	vec4	TemporalSize; // xy 1 / the eye's size\n" \
"};\n"

// The vertex's clip position a frame ago (0: it does not move by itself; the camera's motion comes from the depth).
#define QVR_TEMPORAL_VS_OUTPUT "layout(location=29) out vec4 out_prevclip;\n"

// The motion target (colour attachment 1, written while VR_TemporalMotionWrites has it on): xy uv a frame ago minus uv
// now (unjittered), z the depth, w the eye's stamp (-1: the camera's motion; + 0.5: a liquid, its warp animated).
// Written first in main (before any early return: an output left unwritten is undefined).
#define QVR_TEMPORAL_FS_OUTPUT \
"#if !OIT && !defined(QVR_MULTIVIEW)\n" \
"layout(location=1) out vec4 out_motion;\n" \
"layout(location=29) in vec4 in_prevclip;\n" \
"#define QVR_TEMPORAL_WRITE(liquid) out_motion = in_prevclip.w == 0.0 ? vec4(0.0, 0.0, 0.0, -1.0) : vec4(in_prevclip.xy / in_prevclip.w * 0.5 + 0.5 - (gl_FragCoord.xy * TemporalSize.xy - TemporalJitter.xy), gl_FragCoord.z, TemporalJitter.z + (liquid))\n" \
"#else\n" \
"#define QVR_TEMPORAL_WRITE(liquid)\n" \
"#endif\n"
