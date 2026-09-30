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

#include "vr_tonemap.h" // QVR_TONE_GLSL, the eyes' tone curve and grade (the post-process and the mirror)

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
"	}\n"

// graded (vr_grade) and dithered into the 8-bit image
#define QVR_POSTPROCESS_GRADE_DITHER \
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
"	vec4	Water; // QVR: liquids (vr/vr_water.cpp): waves, fresnel, refraction (0: no scene to read), glints\n" \
"	vec4	Water2; // QVR: lava glow, caustics (0 off), the eye in a liquid (1), unused\n" \
"	vec4	CausticsOrigin; // QVR: xyz where the liquid volume (LiquidVolume) starts, in the world; w its cell size\n" \
"	vec4	CausticsScale; // QVR: xyz one over its size in units; w 1: the scene's distances to refract by and for the foam (LiquidDepth)\n" \
"	vec4	Detail; // QVR: detail textures (vr/vr_detail.cpp): strength (0 off), the distance they start fading, where gone, the fine octave's scale (0 none)\n" \
"	vec4	SceneTone; // QVR: x the brightest the world and models write (1: Quake's clamp; more in the eyes' float scene with vr_tonemap: vr/vr_tonemap.cpp), yzw the force grab glow's colour (vr/vr_fgfx.cpp)\n" \
"	vec4	Water3; // QVR: x shoreline foam (vr/vr_water.cpp: vr_water_foam, 0 off), y the ripples' slopes in the shading, times their shape's (vr_water_ripple_normal), zw unused\n" \
"	vec4	Ripple; // QVR: splash ripples (vr/vr_water.cpp: vr_water_ripples): x how many, y their rings' speed (units/s), z the wave number, w the share of them in the geometry\n" \
"	vec4	RippleAt[32]; // QVR: ... each's centre (xy), the surface's height (z), its age in seconds (w)\n" \
"	vec4	RippleAmp[8]; // QVR: ... each's height now, in units (four a vec4)\n"

// the frame data the alias shaders read (vr/vr_lighting.cpp); its own names, since the alias
// instance buffer has a ViewProj, Fog, EyePos and ScreenDither of its own.
#define ALIAS_FRAMEDATA_BUFFER \
"layout(std140, binding=0) uniform FrameDataUBO\n"\
"{\n"\
"	mat4	FrameViewProj;\n"\
"	vec4	FrameFog;\n"\
"	vec4	FrameSkyFog;\n"\
"	vec3	FrameWindDir;\n"\
"	float	FrameWindPhase;\n"\
"	float	FrameScreenDither;\n"\
"	float	FrameTextureDither;\n"\
"	float	ShadowBias;\n"\
"	float	DlightAngle;\n"\
"	vec3	FrameEyePos;\n"\
"	float	FrameTime;\n"\
"	float	ZLogScale;\n"\
"	float	ZLogBias;\n"\
"	uint	NumLights;\n"\
"	uint	ShadowFlags;\n"\
"	vec4	LightTweak;\n"\
"	vec4	Parallax;\n"\
"	vec4	FrameWater; // QVR: the rest as FRAMEDATA_BUFFER's, up to SceneTone\n"\
"	vec4	FrameWater2;\n"\
"	vec4	FrameCausticsOrigin;\n"\
"	vec4	FrameCausticsScale;\n"\
"	vec4	FrameDetail;\n"\
"	vec4	SceneTone; // QVR: x the brightest models write (vr/vr_tonemap.cpp), yzw the force grab glow's colour\n"\
"};\n"\
"\n"\

// shadows and per-pixel light (vr/vr_lighting.cpp). Needs the frame data and LIGHT_BUFFER.
// ShadowFlags: bits 0-1 filter (1, 4, 9 or 16 taps), 4 dynamic lights uncapped, 8 per-pixel
// dynamic lights on models, 16 DarkPlaces' falloff (a light's minlight is then its ambient), 32
// models lit on a par with the world (vr_model_light_parity). A light's six faces (+x -x +y -y +z -z; right and up below, forward
// the axis) sit 3 x 2 from its origin in the atlas; depth is reversed, SHADOW_NEAR / distance
// along the face's axis; each face has a border of SHADOW_BORDER texels for filtering.
#define SHADOW_FUNCTIONS \
"layout(binding=4) uniform sampler2DShadow ShadowAtlas;\n"\
"layout(binding=5) uniform sampler2DShadow ShadowStatic;\n"\
"\n"\
"#define SHADOW_NEAR 1.0\n"\
"#define SHADOW_BORDER 4.0\n"\
"// The faces' right and up (vr_lighting.cpp's cube views): +x (-y, z), -x (y, z), +y (x, z), -y (-x, z), +z (y, x), -z (-y, x).\n"\
"\n"\
"float ShadowTap(sampler2DShadow atlas, vec2 texel, vec2 inv, float ref)\n"\
"{\n"\
"	return texture(atlas, vec3(texel * inv, ref));\n"\
"}\n"\
"\n"\
"// The filtered compare at texel (atlas texels) against the reversed depth ref: 1, 4, 9 or 16 bilinear taps a texel\n"\
"// apart (about 2x2 .. 5x5 texels). Those kernels are separable: along each axis a box of 1 .. 4 texels widened by a\n"\
"// bilinear ramp at each end (weights 1 - g, 1, .., 1, g). The 9 and 16 tap ones read pairs of their texels with one\n"\
"// bilinear tap each, placed by the pair's weights (Castano's PCF; the same weights): 2 x 2 and 3 x 3 taps.\n"\
"float ShadowFilter(sampler2DShadow atlas, vec2 texel, float ref)\n"\
"{\n"\
"	vec2 inv = 1.0 / vec2(textureSize(atlas, 0));\n"\
"	uint filt = ShadowFlags & 3u;\n"\
"	if (filt == 0u)\n"\
"		return ShadowTap(atlas, texel, inv, ref);\n"\
"	if (filt == 1u)\n"\
"		return 0.25 * (ShadowTap(atlas, texel + vec2(-0.5, -0.5), inv, ref) + ShadowTap(atlas, texel + vec2(0.5, -0.5), inv, ref)\n"\
"			+ ShadowTap(atlas, texel + vec2(-0.5, 0.5), inv, ref) + ShadowTap(atlas, texel + vec2(0.5, 0.5), inv, ref));\n"\
"	if (filt == 2u) // 3 x 3: texels i - 1 .. i + 2 weighted 1 - g, 1, 1, g (i the texel under texel - 0.5, g the rest)\n"\
"	{\n"\
"		vec2 s = texel - 0.5, i = floor(s), g = s - i;\n"\
"		vec2 wa = 2.0 - g, wb = 1.0 + g;\n"\
"		vec2 ua = (i + (1.0 / wa - 0.5)) * inv, ub = (i + (g / wb + 1.5)) * inv;\n"\
"		return (wa.y * (wa.x * texture(atlas, vec3(ua, ref)) + wb.x * texture(atlas, vec3(ub.x, ua.y, ref)))\n"\
"			+ wb.y * (wa.x * texture(atlas, vec3(ua.x, ub.y, ref)) + wb.x * texture(atlas, vec3(ub, ref)))) * (1.0 / 9.0);\n"\
"	}\n"\
"	// 4 x 4: texels j .. j + 4 weighted 1 - h, 1, 1, 1, h (j the texel under texel - 2, h the rest)\n"\
"	vec2 q = texel - 2.0, j = floor(q), h = q - j;\n"\
"	vec2 wa = 2.0 - h;\n"\
"	vec2 ua = (j + (1.0 / wa + 0.5)) * inv, ub = (j + 3.0) * inv, uc = (j + 4.5) * inv;\n"\
"	float ra = wa.x * texture(atlas, vec3(ua.x, ua.y, ref)) + 2.0 * texture(atlas, vec3(ub.x, ua.y, ref)) + h.x * texture(atlas, vec3(uc.x, ua.y, ref));\n"\
"	float rb = wa.x * texture(atlas, vec3(ua.x, ub.y, ref)) + 2.0 * texture(atlas, vec3(ub.x, ub.y, ref)) + h.x * texture(atlas, vec3(uc.x, ub.y, ref));\n"\
"	float rc = wa.x * texture(atlas, vec3(ua.x, uc.y, ref)) + 2.0 * texture(atlas, vec3(ub.x, uc.y, ref)) + h.x * texture(atlas, vec3(uc.x, uc.y, ref));\n"\
"	return (wa.y * ra + 2.0 * rb + h.y * rc) * (1.0 / 16.0);\n"\
"}\n"\
"\n"\
"// Where the point d away from a light falls in its six faces (face size `size`): xy in texels from the block's origin,\n"\
"// z the reversed depth to compare; cell the face, 0 .. 5 (+x -x +y -y +z -z). The face is d's longest axis; right and\n"\
"// up as above (branchless).\n"\
"vec3 ShadowFace(float size, vec3 d, out float cell)\n"\
"{\n"\
"	vec3 a = abs(d);\n"\
"	bool bx = a.x >= a.y && a.x >= a.z, by = !bx && a.y >= a.z;\n"\
"	float z = max(bx ? a.x : by ? a.y : a.z, SHADOW_NEAR);\n"\
"	bool neg = (bx ? d.x : by ? d.y : d.z) <= 0.; // the odd faces\n"\
"	vec2 st = bx ? vec2(d.y, d.z) : by ? vec2(d.x, d.z) : vec2(d.y, d.x);\n"\
"	st.x = (bx != neg) ? -st.x : st.x; // +x and -y, -z turn right the other way\n"\
"	cell = (bx ? 0. : by ? 2. : 4.) + (neg ? 1. : 0.); // laid out 3 x 2\n"\
"	vec2 org = vec2(cell < 3. ? cell : cell - 3., cell < 3. ? 0. : 1.);\n"\
"	float k = 1.0 - 2.0 * SHADOW_BORDER / size;\n"\
"	return vec3(org * size + (st / z * (0.5 * k) + 0.5) * size, SHADOW_NEAR / z * (1.0 + 0.002 * ShadowBias));\n"\
"}\n"\
"\n"\
"// How much of a light reaches the point d away from it, 0..1, from its faces at tile (xy origin, z face size).\n"\
"float ShadowLookup(sampler2DShadow atlas, vec3 tile, vec3 d)\n"\
"{\n"\
"	float cell;\n"\
"	vec3 f = ShadowFace(tile.z, d, cell);\n"\
"	return ShadowFilter(atlas, tile.xy + f.xy, f.z);\n"\
"}\n"\
"\n"\
"// The receiver moved along its normal by about a shadow texel (normal-offset bias), more where the light grazes it.\n"\
"vec3 ShadowOffset(vec3 d, vec3 n, float size)\n"\
"{\n"\
"	float z = max(abs(d.x), max(abs(d.y), abs(d.z)));\n"\
"	float texel = 2.0 * z / max(size - 2.0 * SHADOW_BORDER, 1.0);\n"\
"	float grazing = 1.0 - clamp(dot(n, -d) / max(length(d), 1e-3), 0.0, 1.0);\n"\
"	return d + n * texel * ShadowBias * (1.0 + grazing);\n"\
"}\n"\
"\n"\
"// A spot light's cone at pos: 1 inside its inner cone, smoothly down to 0 at its outer one; 1 for a point light\n"\
"// (spot: xyz the direction / (cos inner - cos outer), w cos inner / (cos inner - cos outer); zero for a point light).\n"\
"float SpotCone(Light l, vec3 pos)\n"\
"{\n"\
"	vec3 d = pos - l.origin;\n"\
"	return 1.0 - smoothstep(0.0, 1.0, l.spot.w - dot(l.spot.xyz, d) * inversesqrt(max(dot(d, d), 1e-6)));\n"\
"}\n"\
"\n"\
"// A spot light's shadow: one tile (l.shadow: xy origin, z size), a perspective projection about its direction,\n"\
"// l.shadow2.x the tangent of the tile's half angle (inside its border); right and up as vr_lighting.cpp's spotFrame.\n"\
"float SpotShadow(Light l, vec3 pos, vec3 n)\n"\
"{\n"\
"	vec3 fwd = normalize(l.spot.xyz);\n"\
"	vec3 right = normalize(cross(fwd, abs(fwd.z) < 0.9 ? vec3(0., 0., 1.) : vec3(1., 0., 0.)));\n"\
"	vec3 up = cross(right, fwd);\n"\
"	float size = l.shadow.z, spread = l.shadow2.x;\n"\
"	vec3 d = pos - l.origin;\n"\
"	float texel = 2.0 * max(dot(d, fwd), SHADOW_NEAR) * spread / max(size - 2.0 * SHADOW_BORDER, 1.0);\n"\
"	float grazing = 1.0 - clamp(dot(n, -d) / max(length(d), 1e-3), 0.0, 1.0);\n"\
"	d += n * texel * ShadowBias * (1.0 + grazing);\n"\
"	float z = max(dot(d, fwd), SHADOW_NEAR);\n"\
"	vec2 st = clamp(vec2(dot(d, right), dot(d, up)) / (z * spread), -1.0, 1.0);\n"\
"	float k = 1.0 - 2.0 * SHADOW_BORDER / size;\n"\
"	return ShadowFilter(ShadowAtlas, l.shadow.xy + (st * (0.5 * k) + 0.5) * size, SHADOW_NEAR / z * (1.0 + 0.002 * ShadowBias));\n"\
"}\n"\
"\n"\
"// A dynamic light's shadow at pos (normal n), times a spot light's cone.\n"\
"float LightShadow(Light l, vec3 pos, vec3 n)\n"\
"{\n"\
"	float cone = SpotCone(l, pos); // QVR: 1 for a point light\n"\
"	if (l.shadow.z <= 0. || cone <= 0.)\n"\
"		return cone;\n"\
"	if (l.shadow2.x > 0.)\n"\
"		return cone * SpotShadow(l, pos, n);\n"\
"	return cone * ShadowLookup(ShadowAtlas, l.shadow.xyz, ShadowOffset(pos - l.origin, n, l.shadow.z));\n"\
"}\n"\
"\n"\
"// Quake's dynamic lights ignore the angle they reach a surface at: DlightAngle blends in Lambert's.\n"\
"float LightAngle(Light l, vec3 pos, vec3 n, float ambient)\n"\
"{\n"\
"	vec3 dir = normalize(l.origin - pos);\n"\
"	return mix(1.0, ambient + (1.0 - ambient) * 2.0 * max(dot(n, dir), 0.0), DlightAngle);\n"\
"}\n"\
"\n"\
"// DarkPlaces' falloff (ShadowFlags 16): full light to about 40% of the radius, then smoothly down to none.\n"\
"float DarkPlacesAtten(float dist, float radius)\n"\
"{\n"\
"	float d = dist / max(radius, 1.0);\n"\
"	return clamp((1.0 - d) * 2.0 / (1.0 + d * d), 0.0, 1.0);\n"\
"}\n"\
"\n"\
"// DarkPlaces' angle term: Lambert's, over the light's ambient (its minlight: an explosion lights what faces away too).\n"\
"float LightAngleDP(Light l, vec3 pos, vec3 n)\n"\
"{\n"\
"	vec3 dir = normalize(l.origin - pos);\n"\
"	return mix(1.0, l.minlight + (1.0 - l.minlight) * max(dot(n, dir), 0.0), DlightAngle);\n"\
"}\n"\
"\n"\
"// QVR: specular anti-aliasing (vr_specular_aa: Parallax.w, 0 off). Bumps smaller than a pixel make the sheen sparkle\n"\
"// as the head moves (the filtered normal map still gets the full exponent): the lobe is widened by how much the\n"\
"// normal varies under the pixel. Blinn's exponent p is a lobe of variance about 1/p, and the variances add: Toksvig's\n"\
"// from the normal map's mip (BumpedNormal sets BumpSpread: its averaged normal is shorter where the normals under it\n"\
"// differ) and the screen derivatives of the normal (Kaplanyan and Tokuyoshi's geometric specular AA: bumps about a\n"\
"// pixel in size, curved models, clamped). The intensity keeps the lobe's energy ((p' + 1) / (p + 1)): a patch of\n"\
"// sparkles becomes a dimmer, steady sheen as bright on the whole. SpecularAA (SPECULAR_AA_FUNCTIONS, fragment\n"\
"// shaders) sets the pixel's lobe.\n"\
"float BumpSpread = 0.0;\n"\
"vec2 SpecLobe = vec2(32.0, 1.0); // the exponent, the intensity's scale\n"\
"\n"\
"// A dynamic light's sheen towards the eye (DarkPlaces' r_shadow_gloss 2): Blinn's, exponent 32, LightTweak.z strong.\n"\
"float LightSpecular(Light l, vec3 pos, vec3 n, vec3 eye)\n"\
"{\n"\
"	if (LightTweak.z <= 0.)\n"\
"		return 0.0;\n"\
"	vec3 dir = normalize(l.origin - pos);\n"\
"	if (dot(n, dir) <= 0.)\n"\
"		return 0.0;\n"\
"	vec3 h = normalize(dir + normalize(eye - pos));\n"\
"	return pow(max(dot(n, h), 0.0), SpecLobe.x) * SpecLobe.y * LightTweak.z; // QVR: SpecLobe (SpecularAA)\n"\
"}\n"\
"\n"\
"// The normal n bent by a normal map (tangent space, green up; `strength` deepens it: LightTweak.w, or a model's own), in the frame the texture\n"\
"// lies in on the surface: a cotangent frame from the derivatives of the position and texture coordinates, exact on\n"\
"// the world's flat faces, the same in both eyes. The derivatives come from the caller (taken before any discard).\n"\
"// QVR: `axes` (models) makes t and b each of unit length, as a baker's tangent frame has them (Blender's): model\n"\
"// skins are rarely square (512 x 178), and scaled together, a slope along the long side came out a third as steep.\n"\
"vec3 BumpedNormalFrame(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, float strength, bool axes)\n"\
"{\n"\
"	vec3 dp2perp = cross(dpdy, n);\n"\
"	vec3 dp1perp = cross(n, dpdx);\n"\
"	vec3 t = dp2perp * duvdx.x + dp1perp * duvdy.x;\n"\
"	vec3 b = dp2perp * duvdx.y + dp1perp * duvdy.y;\n"\
"	float k = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-24));\n"\
"	if (axes)\n"\
"	{\n"\
"		t *= inversesqrt(max(dot(t, t), 1e-24));\n"\
"		b *= inversesqrt(max(dot(b, b), 1e-24));\n"\
"		k = 1.0;\n"\
"	}\n"\
"	vec4 s = textureGrad(tex, uv, duvdx, duvdy);\n"\
"	vec2 m = s.xy * 2.0 - 1.0; // x and y (RG8): z makes it unit length\n"\
"	float z = sqrt(max(1.0 - dot(m, m), 0.0025));\n"\
"	if (Parallax.w > 0. && s.z > 0.25) // QVR: RGBA maps keep z: the mip's averaged normal, shorter where the normals under it differ (SpecularAA)\n"\
"	{\n"\
"		vec2 size = vec2(textureSize(tex, 0)), ex = duvdx * size, ey = duvdy * size;\n"\
"		float len = length(s.xyz * 2.0 - 1.0), minified = clamp(0.5 * log2(max(max(dot(ex, ex), dot(ey, ey)), 1e-8)), 0.0, 1.0);\n"\
"		BumpSpread = max(1.0 - len - 0.004, 0.0) / max(len, 0.1) * strength * strength * minified; // 0.004: 8 bits' rounding; not magnified (a blend of two texels is a slope)\n"\
"	}\n"\
"	m *= strength;\n"\
"	return normalize((t * m.x - b * m.y) * k + n * z);\n"\
"}\n"\
"vec3 BumpedNormalK(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, float strength)\n"\
"{\n"\
"	return BumpedNormalFrame(tex, uv, duvdx, duvdy, dpdx, dpdy, n, strength, true);\n"\
"}\n"\
"vec3 BumpedNormal(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n)\n"\
"{\n"\
"	return BumpedNormalFrame(tex, uv, duvdx, duvdy, dpdx, dpdy, n, LightTweak.w, false);\n"\
"}\n"\
"\n"\
"// The baked light on the bumps (vr_normalmap_baked: LightTweak.y): a light from a guessed direction, towards where\n"\
"// the lightmap gets brighter over the surface (dlum: the screen derivatives of its brightness lum) and a little from\n"\
"// above, at most 45 degrees off the normal n; as bright as before on the flat. Light straight on the surface (the\n"\
"// flat light of floors and ceilings) is DarkPlaces' r_glsl_deluxemapping 2.\n"\
"float BakedBump(vec3 n, vec3 bumped, vec3 dpdx, vec3 dpdy, float lum, vec2 dlum)\n"\
"{\n"\
"	float det = dot(n, cross(dpdx, dpdy));\n"\
"	vec3 grad = (cross(dpdy, n) * dlum.x + cross(n, dpdx) * dlum.y) / (abs(det) > 1e-8 ? det : 1e-8); // per unit\n"\
"	vec3 tilt = grad * (96.0 / max(lum, 0.03)) + (vec3(0.0, 0.0, 0.5) - n * (0.5 * n.z));\n"\
"	float len = length(tilt);\n"\
"	vec3 l = normalize(n + tilt * (min(len, 1.0) / max(len, 1e-4)));\n"\
"	return max(mix(1.0, max(dot(bumped, l), 0.0) / dot(n, l), LightTweak.y), 0.0);\n"\
"}\n"\
"\n"\
"// A map light's shadow of moving things: the share of the baked light `lit` at pos (normal n) that the\n"\
"// light gave and something moving now blocks, where the world itself does not (the baked light has that).\n"\
"float MapLightShadow(Light l, vec3 pos, vec3 n, vec3 lit)\n"\
"{\n"\
"	vec3 d = pos - l.origin;\n"\
"	float dist = length(d);\n"\
"	float given = (l.shadow2.z - dist * l.shadow2.w) * (0.5 + 0.5 * max(dot(n, -d / max(dist, 1e-3)), 0.0)) / 255.0;\n"\
"	if (given <= 0.)\n"\
"		return 1.0;\n"\
"	float cell;\n"\
"	vec3 f = ShadowFace(l.shadow.z, ShadowOffset(d, n, l.shadow.z), cell); // the same place in both maps' blocks\n"\
"	if (((uint(l.shadow.w) - 1u) & (1u << uint(cell))) == 0u) // nothing moving drawn in this face (l.shadow.w: 1 + the\n"\
"		return 1.0; // faces that have, vr_lighting.cpp)\n"\
"	float moving = ShadowFilter(ShadowAtlas, l.shadow.xy + f.xy, f.z); // first: nothing moving in the way, mostly\n"\
"	if (moving > 0.999) // (the filter's weights may not sum to exactly 1)\n"\
"		return 1.0;\n"\
"	float blocked = ShadowFilter(ShadowStatic, l.shadow2.xy + f.xy, f.z) * (1.0 - moving);\n"\
"	float lum = max(lit.r, max(lit.g, lit.b));\n"\
"	return 1.0 - clamp(given / max(lum, 1e-3), 0.0, 1.0) * blocked * l.color.x;\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// dynamic ambient occlusion (vr/vr_ao.cpp): ellipsoids round monsters, items, gibs and your body, boxes round
// brush models (doors, lifts, the item boxes), darkening the baked light near them; in world space, the same in both
// eyes. Uniform block 2 (vr_ao.cpp's GpuBlock): up to 64 occluders of 6 vec4s (the centre and the reach round it; 3
// rows into its own space: an ellipsoid's unit sphere, a box's frame; its radii squared or half sizes, w 1 a box;
// strength, reach (an ellipsoid's in radii, a box's in units), -, its model's group), and each eye's screen tiles (the
// light clusters' 32 x 16, two per uvec4) and depth slices (their 32): which occluders can reach the tile, and the
// slice. Needs LIGHT_BUFFER's tile counts and the frame data's ZLogScale and ZLogBias.
#define AO_FUNCTIONS \
"layout(std140, binding=2) uniform AOBlock\n"\
"{\n"\
"	vec4	AOParams; // x the occluders (0: none)\n"\
"	vec4	AOOcc[64 * 6];\n"\
"	uvec4	AOTiles[LIGHT_TILES_X * LIGHT_TILES_Y / 2];\n"\
"	uvec4	AOSlices[LIGHT_TILES_Z / 2]; // the light clusters' depth slices, two per uvec4: which occluders reach their depths\n"\
"};\n"\
"\n"\
"// An edge's term in Lambert's formula over 2 pi (a, b: unit vectors to its ends): the angle between them over its sine\n"\
"// by Heitz et al.'s rational fit (\"Real-Time Polygonal-Light Shading with Linearly Transformed Cosines\"), no atan.\n"\
"float AOEdge(vec3 a, vec3 b, vec3 n)\n"\
"{\n"\
"	float x = dot(a, b), y = abs(x);\n"\
"	float v = (0.8543985 + (0.4965155 + 0.0145206 * y) * y) / (3.4175940 + (4.1616724 + y) * y);\n"\
"	return dot(n, cross(a, b)) * (x > 0.0 ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v);\n"\
"}\n"\
"float AOFace(vec3 p, vec3 n, vec3 o, vec3 u, vec3 w) // a box face (centre o, half edges u and w, u x w outwards)\n"\
"{\n"\
"	vec3 a = normalize(o - u - w - p), b = normalize(o + u - w - p), c = normalize(o + u + w - p), d = normalize(o - u + w - p);\n"\
"	return AOEdge(a, b, n) + AOEdge(b, c, n) + AOEdge(c, d, n) + AOEdge(d, a, n);\n"\
"}\n"\
"// A box's occlusion at p (normal n; both in its frame; r its half sizes): the form factor of the faces facing p. Not\n"\
"// clipped at p's horizon: a box partly behind the surface counts less than it should. A face in p's own plane (a wall\n"\
"// flush with a door or a lift's side) is not seen: its polygon would wind round p and count as the whole hemisphere.\n"\
"float AOBox(vec3 p, vec3 n, vec3 r)\n"\
"{\n"\
"	vec3 s = sign(p);\n"\
"	float sum = 0.0;\n"\
"	if (abs(p.x) > r.x + 0.125)\n"\
"		sum += AOFace(p, n, vec3(s.x * r.x, 0.0, 0.0), s.x > 0. ? vec3(0.0, r.y, 0.0) : vec3(0.0, 0.0, r.z), s.x > 0. ? vec3(0.0, 0.0, r.z) : vec3(0.0, r.y, 0.0));\n"\
"	if (abs(p.y) > r.y + 0.125)\n"\
"		sum += AOFace(p, n, vec3(0.0, s.y * r.y, 0.0), s.y > 0. ? vec3(0.0, 0.0, r.z) : vec3(r.x, 0.0, 0.0), s.y > 0. ? vec3(r.x, 0.0, 0.0) : vec3(0.0, 0.0, r.z));\n"\
"	if (abs(p.z) > r.z + 0.125)\n"\
"		sum += AOFace(p, n, vec3(0.0, 0.0, s.z * r.z), s.z > 0. ? vec3(r.x, 0.0, 0.0) : vec3(0.0, r.y, 0.0), s.z > 0. ? vec3(0.0, r.y, 0.0) : vec3(r.x, 0.0, 0.0));\n"\
"	return clamp(-sum, 0.0, 1.0);\n"\
"}\n"\
"// An ellipsoid's occlusion at q (normal m; both in its unit sphere's space): a sphere's form factor, s^2 cos (s the\n"\
"// sine of its angular radius, cos towards its centre), smoothed where it sinks below the horizon ((cos + s)^2 / 4s),\n"\
"// fading out to none at `reach` radii.\n"\
"float AOEllipsoid(vec3 q, vec3 m, float reach)\n"\
"{\n"\
"	float d = max(length(q), 1e-4);\n"\
"	float h = -dot(m, q) / d;\n"\
"	float s = min(1.0 / d, 1.0);\n"\
"	float ff = h >= s ? h * s * s : h <= -s ? 0.0 : (h + s) * (h + s) * s * 0.25;\n"\
"	return ff * (1.0 - smoothstep(1.0, reach, d));\n"\
"}\n"\
"// The normal the occlusion is worked out for: leaning towards where the baked light comes from (l: the deluxemap's\n"\
"// direction or the model's light; zero: from above, as Quake's lamps mostly are). The light is mostly direct, from\n"\
"// lamps: a lift below a wall hides little of the lamp above it, though it fills much of the wall's hemisphere.\n"\
"vec3 AONormal(vec3 n, vec3 l)\n"\
"{\n"\
"	vec3 m = n + 0.6 * (dot(l, l) > 0. ? l : vec3(0.0, 0.0, 1.0));\n"\
"	return dot(m, m) > 0.04 ? normalize(m) : n;\n"\
"}\n"\
"// The light left at p (normal n) by the occluders of its tile (coord: the light clusters' screen coordinates), 1 none;\n"\
"// `self` the receiver's own group (0 none), which does not darken it. Each occluder takes away its share (at most\n"\
"// 90%), the shares multiplied.\n"\
"float DynamicAO(vec3 p, vec3 n, float self, vec2 coord, float depth)\n"\
"{\n"\
"	if (AOParams.x <= 0.)\n"\
"		return 1.0;\n"\
"	ivec2 t = clamp(ivec2(coord), ivec2(0), ivec2(LIGHT_TILES_X - 1, LIGHT_TILES_Y - 1));\n"\
"	int tile = t.x + t.y * LIGHT_TILES_X;\n"\
"	uvec4 pair = AOTiles[tile >> 1];\n"\
"	uvec2 mask = (tile & 1) != 0 ? pair.zw : pair.xy;\n"\
"	int slice = clamp(int(floor(log2(depth) * ZLogScale + ZLogBias)), 0, LIGHT_TILES_Z - 1);\n"\
"	uvec4 spair = AOSlices[slice >> 1];\n"\
"	mask &= (slice & 1) != 0 ? spair.zw : spair.xy;\n"\
"	float vis = 1.0;\n"\
"	vec4 hp = vec4(p, 1.0);\n"\
"	for (int i = 0; i < 2; i++)\n"\
"	{\n"\
"		uint bits = mask[i];\n"\
"		while (bits != 0u)\n"\
"		{\n"\
"			int j = findLSB(bits);\n"\
"			bits ^= 1u << j;\n"\
"			int o = (i * 32 + j) * 6;\n"\
"			vec4 c = AOOcc[o];\n"\
"			vec3 d = p - c.xyz;\n"\
"			vec4 k = AOOcc[o + 5];\n"\
"			if (dot(d, d) >= c.w * c.w || k.w == self)\n"\
"				continue;\n"\
"			vec4 r0 = AOOcc[o + 1], r1 = AOOcc[o + 2], r2 = AOOcc[o + 3], sz = AOOcc[o + 4];\n"\
"			vec3 q = vec3(dot(r0, hp), dot(r1, hp), dot(r2, hp));\n"\
"			vec3 e = sz.w > 0.5 ? max(abs(q) - sz.xyz, 0.0) : q; // from its faces, or its centre in radii\n"\
"			float d2 = dot(e, e);\n"\
"			if (d2 >= k.y * k.y) // out of its reach\n"\
"				continue;\n"\
"			vec3 m = vec3(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n));\n"\
"			float ao;\n"\
"			if (sz.w > 0.5)\n"\
"			{\n"\
"				if (dot(m, q) >= dot(abs(m), sz.xyz)) // all of it behind p's plane\n"\
"					continue;\n"\
"				ao = AOBox(q, m, sz.xyz) * (1.0 - smoothstep(0.25 * k.y, k.y, sqrt(d2)));\n"\
"			}\n"\
"			else\n"\
"				ao = AOEllipsoid(q, normalize(m * sz.xyz), k.y);\n"\
"			vis *= 1.0 - min(ao * k.x, 0.9);\n"\
"		}\n"\
"	}\n"\
"	return max(vis, 0.1);\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// parallax occlusion mapping (vr_parallax): the texture coordinates where the ray from the eye through this
// pixel (ray: from the eye to it; n: the surface's normal, facing the eye) meets the height field under the surface
// (tex's alpha: 1 the surface, 0 `depth` units deep; the instance's: the world's, an item box's or a model's).
// The ray is walked in steps, more at grazing angles (`steps` at most, half as many straight on; no more than 1.5 for
// each texel of the height field it crosses, 4 at least: small boxes and models cross few), then refined
// twice between the last two (secant). The texture's axes on the surface are the gradients of its coordinates, from
// the derivatives the bumps' frame is made from: exact on flat faces and a model's triangles; each eye walks its own
// ray. It fades out over the last quarter of Parallax.y units away, and at grazing angles, where it would swim (and
// smear: the shift along the surface is at most 3 times the depth); none where the whole shift is under a third of
// a pixel. `uvclamp` (an item box's texture_t): the rays stay in the part of the texture the face shows, the shift
// shrinking towards its edges, instead of reading past them (the texture's unused rest, or its other side).
#define PARALLAX_FUNCTIONS \
"vec2 ParallaxUV(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, vec3 ray, float depth,\n"\
"	float steps, vec4 uvclamp)\n"\
"{\n"\
"	float dist = length(ray);\n"\
"	if (dist >= Parallax.y || depth <= 0.)\n"\
"		return uv;\n"\
"	ray /= max(dist, 1e-3);\n"\
"	float cosa = -dot(ray, n); // n faces the eye\n"\
"	float fade = (1.0 - smoothstep(Parallax.y * 0.75, Parallax.y, dist)) * smoothstep(0.12, 0.35, cosa);\n"\
"	float det = dot(n, cross(dpdx, dpdy));\n"\
"	float reach = depth * fade * min(1.0 / max(cosa, 1e-3), 3.0); // how far along the surface it shifts, at most\n"\
"	if (reach * reach < 0.11 * max(dot(dpdx, dpdx), dot(dpdy, dpdy)) || abs(det) < 1e-12)\n"\
"		return uv;\n"\
"	vec3 gu = (cross(dpdy, n) * duvdx.x + cross(n, dpdx) * duvdy.x) / det; // the coordinates' change per unit\n"\
"	vec3 gv = (cross(dpdy, n) * duvdx.y + cross(n, dpdx) * duvdy.y) / det;\n"\
"	vec3 shift = (ray + n * cosa) * reach; // along the surface, to the bottom\n"\
"	vec2 duv = vec2(dot(shift, gu), dot(shift, gv));\n"\
"	if (uvclamp.z > uvclamp.x || uvclamp.w > uvclamp.y)\n"\
"	{\n"\
"		vec2 margin = 0.5 / vec2(textureSize(tex, 0));\n"\
"		vec2 base = floor(uv - (uvclamp.xy + uvclamp.zw) * 0.5 + 0.5); // the whole textures the face is moved by\n"\
"		vec2 lo = uvclamp.xy + base + margin - uv, hi = uvclamp.zw + base - margin - uv; // how far it may go\n"\
"		float k = 1.0;\n"\
"		if (uvclamp.z > uvclamp.x && abs(duv.x) > 1e-7)\n"\
"			k = min(k, max((duv.x > 0. ? hi.x : lo.x) / duv.x, 0.));\n"\
"		if (uvclamp.w > uvclamp.y && abs(duv.y) > 1e-7)\n"\
"			k = min(k, max((duv.y > 0. ? hi.y : lo.y) / duv.y, 0.));\n"\
"		duv *= k;\n"\
"	}\n"\
"	steps = ceil(mix(steps, steps * 0.5, cosa));\n"\
"	steps = min(steps, max(4.0, ceil(length(duv * vec2(textureSize(tex, 0))) * 1.5))); // 1.5 a texel the ray crosses\n"\
"	float stepsize = 1.0 / steps;\n"\
"	float h = 1.0 - textureGrad(tex, uv, duvdx, duvdy).a; // how deep the height field is, 0..1\n"\
"	if (h <= 0.)\n"\
"		return uv;\n"\
"	float ray_depth = 0., prev = h; // prev: how far over the height field the ray was, the step before\n"\
"	for (int i = 1; i <= 64; i++)\n"\
"	{\n"\
"		prev = h - ray_depth;\n"\
"		ray_depth = float(i) * stepsize;\n"\
"		h = 1.0 - textureGrad(tex, uv + duv * ray_depth, duvdx, duvdy).a;\n"\
"		if (h <= ray_depth || float(i) >= steps)\n"\
"			break;\n"\
"	}\n"\
"	float over = h - ray_depth; // <= 0: under it\n"\
"	if (over > 0.)\n"\
"		return uv + duv; // the bottom\n"\
"	// secant between the ray's depths a (over it) and b (under it)\n"\
"	float a = ray_depth - stepsize, b = ray_depth, fa = prev, fb = over;\n"\
"	float t = a + (b - a) * fa / max(fa - fb, 1e-5);\n"\
"	float ft = 1.0 - textureGrad(tex, uv + duv * t, duvdx, duvdy).a - t;\n"\
"	if (ft > 0.)\n"\
"	{\n"\
"		a = t;\n"\
"		fa = ft;\n"\
"	}\n"\
"	else\n"\
"	{\n"\
"		b = t;\n"\
"		fb = ft;\n"\
"	}\n"\
"	t = a + (b - a) * fa / max(fa - fb, 1e-5);\n"\
"	return uv + duv * t;\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// specular anti-aliasing (vr_specular_aa; see SpecLobe in LIGHT_BUFFER), fragment shaders only. NormalSpread: the
// variance of the normal n over the pixel from its screen derivatives (Kaplanyan and Tokuyoshi: a pixel's filter of
// variance 0.25 over its change per pixel, at most 0.09). SpecularAA: the pixel's lobe from it (0 where there are no
// derivatives: after a discard) and the normal map's (BumpSpread).
#define SPECULAR_AA_FUNCTIONS \
"float NormalSpread(vec3 n)\n"\
"{\n"\
"	vec3 dx = dFdx(n), dy = dFdy(n);\n"\
"	return min(0.25 * (dot(dx, dx) + dot(dy, dy)), 0.09);\n"\
"}\n"\
"void SpecularAA(float spread)\n"\
"{\n"\
"	float p = 1.0 / (1.0 / 32.0 + (spread + BumpSpread) * Parallax.w);\n"\
"	SpecLobe = vec2(p, (p + 1.0) / 33.0);\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// detail textures (vr/vr_detail.cpp, vr_detail): a fine grain (stone, metal, wood, grime...) from a texture array,
// multiplied over the world's textures close to the eye. Needs the frame data (Detail).
#define DETAIL_FUNCTIONS \
"layout(binding=12) uniform sampler2DArray DetailTex;\n"\
"\n"\
"// The factor to multiply a texture's colour by (1 on average): d the call's detail (s and t scales, s < 0 the axes\n"\
"// swapped; the strength; the layer), uv its coordinates (after parallax) and their screen derivatives dx and dy, dist\n"\
"// the distance from the eye. Full up to Detail.y, gone by Detail.z; a finer octave (Detail.w times, off the coarse\n"\
"// one's grid) within half those. The layers' mips average to mid-grey: the grain fades as it gets small on screen.\n"\
"float DetailFactor(vec4 d, vec2 uv, vec2 dx, vec2 dy, float dist)\n"\
"{\n"\
"	float fade = 1.0 - smoothstep(Detail.y, Detail.z, dist);\n"\
"	if (fade <= 0.)\n"\
"		return 1.0;\n"\
"	vec2 k = abs(d.xy);\n"\
"	uv *= k;\n"\
"	dx *= k;\n"\
"	dy *= k;\n"\
"	if (d.x < 0.) // the grain along t\n"\
"	{\n"\
"		uv = uv.yx;\n"\
"		dx = dx.yx;\n"\
"		dy = dy.yx;\n"\
"	}\n"\
"	float g = textureGrad(DetailTex, vec3(uv, d.w), dx, dy).r * 2.0 - 1.0;\n"\
"	float fine = Detail.w > 0. ? 1.0 - smoothstep(Detail.y * 0.5, Detail.z * 0.5, dist) : 0.0;\n"\
"	if (fine > 0.)\n"\
"	{\n"\
"		float g2 = textureGrad(DetailTex, vec3(uv * Detail.w + vec2(0.37, 0.61), d.w), dx * Detail.w, dy * Detail.w).r * 2.0 - 1.0;\n"\
"		g = g * (1.0 - 0.3 * fine) + g2 * (0.6 * fine);\n"\
"	}\n"\
"	return max(1.0 + g * d.z * Detail.x * fade, 0.0);\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// liquids (vr/vr_water.cpp): waves, fresnel, glints, refraction, lava's glow, and caustics on what is under water.
// Everything is a function of the world position and the time (the same in both eyes) and of the view vector. Needs
// the frame data. The kind is in a call's flags (bits 3-5): 1 lava, 2 slime, 3 teleport, 4 water.
// LIQUID_SWELL, in the vertex shaders too: the geometric waves (vr_water_geo_waves).
#define LIQUID_SWELL \
"uint LiquidKind(uint flags)\n"\
"{\n"\
"	return (flags >> 3) & 7u;\n"\
"}\n"\
"\n"\
"// The swells over a level liquid at p (the world's xy), Water2.w units high (0: off): xy the height's gradient, z the\n"\
"// height. Long and slow, three sines; lava's bigger and slower, slime's small and sluggish, teleports' none.\n"\
"vec3 LiquidSwell(vec2 p, uint kind)\n"\
"{\n"\
"	if (Water2.w <= 0. || kind == 3u)\n"\
"		return vec3(0.);\n"\
"	float scale = kind == 1u ? 1.8 : kind == 2u ? 1.2 : 1.0;\n"\
"	float speed = kind == 1u ? 0.25 : kind == 2u ? 0.4 : 1.0;\n"\
"	float amp = Water2.w * (kind == 1u ? 1.3 : kind == 2u ? 0.6 : 1.0);\n"\
"	const vec2 dirs[3] = vec2[3](vec2(0.96, 0.28), vec2(-0.45, 0.89), vec2(0.6, -0.8));\n"\
"	const float lens[3] = float[3](173., 109., 71.);\n"\
"	const float shares[3] = float[3](0.5, 0.3, 0.2);\n"\
"	vec3 r = vec3(0.);\n"\
"	for (int i = 0; i < 3; i++)\n"\
"	{\n"\
"		float k = 6.2831853 / (lens[i] * scale);\n"\
"		float x = dot(dirs[i], p) * k + Time * sqrt(200. * k) * speed + float(i) * 2.3;\n"\
"		float h = amp * shares[i];\n"\
"		r.xy += dirs[i] * (h * k * cos(x));\n"\
"		r.z += h * sin(x);\n"\
"	}\n"\
"	return r;\n"\
"}\n"\
"\n"\
"// The splashes' ripples (vr_water.cpp, vr_water_ripples; Ripple.x of them, 0: none) over a level liquid at pos: xy the\n"\
"// height's gradient, z the height. Round each on a surface at pos.z, a ring of crests: a packet 0.7 wavelengths wide\n"\
"// spreading at Ripple.y units/s, lower the farther it has spread, its crests running out through it twice as fast;\n"\
"// with fine > 0, that much of ripples a third as long riding in it (the shading's). Lava's slow and long, slime's\n"\
"// slower, teleports' none.\n"\
"vec3 LiquidRipples(vec3 pos, uint kind, float fine)\n"\
"{\n"\
"	vec3 r = vec3(0.);\n"\
"	int n = int(Ripple.x);\n"\
"	if (n <= 0 || kind == 3u)\n"\
"		return r;\n"\
"	float speed = Ripple.y * (kind == 1u ? 0.35 : kind == 2u ? 0.7 : 1.0); // (vr_water.cpp's rippleSpeed)\n"\
"	float k = Ripple.z / (kind == 1u ? 1.5 : 1.0);\n"\
"	float amp = kind == 1u ? 0.6 : kind == 2u ? 0.8 : 1.0;\n"\
"	float width = 4.4 / k;\n"\
"	for (int i = 0; i < n; i++)\n"\
"	{\n"\
"		vec4 e = RippleAt[i];\n"\
"		vec2 d = pos.xy - e.xy;\n"\
"		float dist = length(d);\n"\
"		float front = speed * e.w;\n"\
"		float u = (dist - front) / width;\n"\
"		if (abs(pos.z - e.z) > 4. || abs(u) > 3.)\n"\
"			continue;\n"\
"		float a = amp * RippleAmp[i >> 2][i & 3] / sqrt(1.0 + dist * k * 0.16);\n"\
"		float env = exp(-u * u);\n"\
"		float denv = -2.0 * u / width * env;\n"\
"		float ph = k * (dist - 2.0 * front);\n"\
"		float h = -a * env * cos(ph);\n"\
"		float dh = -a * (denv * cos(ph) - env * k * sin(ph));\n"\
"		if (fine > 0.)\n"\
"		{\n"\
"			float phf = 3.0 * k * (dist - 1.6 * front) + 1.3;\n"\
"			float af = 0.14 * a * fine;\n"\
"			h -= af * env * cos(phf);\n"\
"			dh -= af * (denv * cos(phf) - env * 3.0 * k * sin(phf));\n"\
"		}\n"\
"		r.z += h;\n"\
"		r.xy += d * (dh / max(dist, 1e-3));\n"\
"	}\n"\
"	return r;\n"\
"}\n"\
"\n"\
"// A vertex of the swells' mesh (vr_water.cpp) raised: rim 1 + how far the pool's rim is in units (0: a face not held,\n"\
"// or any other liquid face: attribute 4 unset); the pin 0 at the rim to 1 32 units in. Fading out from 512 to 1024\n"\
"// units from the eye (vr_water.cpp's kFadeEnd). The splashes' ripples too (Ripple.w of them: none where the grid is\n"\
"// too coarse for them).\n"\
"vec3 LiquidDisplace(vec3 pos, float rim, uint kind)\n"\
"{\n"\
"	if (rim <= 0. || (Water2.w <= 0. && Ripple.x <= 0.))\n"\
"		return pos;\n"\
"	float pin = smoothstep(1.0, 33.0, rim);\n"\
"	float fade = 1.0 - smoothstep(512.0, 1024.0, distance(pos, EyePos));\n"\
"	float swell = LiquidSwell(pos.xy, kind).z * pin * fade;\n"\
"	float rip = Ripple.w > 0. ? LiquidRipples(pos, kind, 0.).z * Ripple.w * pin * fade : 0.;\n"\
"	// a ripple's crest (or trough) kept off the eye: not within 6 units of its height where it is, more further out\n"\
"	// (a body's splash round you, with your head just over the water)\n"\
"	float above = EyePos.z - pos.z - swell;\n"\
"	float room = max(abs(above) - 6.0, 0.0) + max(distance(pos.xy, EyePos.xy) - 24.0, 0.0);\n"\
"	rip = above >= 0. ? min(rip, room) : max(rip, -room);\n"\
"	return vec3(pos.xy, pos.z + swell + rip);\n"\
"}\n"\
"\n"\

#define LIQUID_FUNCTIONS \
"layout(binding=6) uniform sampler2D LiquidScene; // the opaque scene, while translucent liquids draw into the OIT buffers\n"\
"layout(binding=7) uniform sampler3D LiquidVolume; // where the water and slime are (1), a cell round them into walls\n"\
"layout(binding=8) uniform sampler2D LiquidDepth; // how far the opaque scene is (vr_water.cpp: half the size), with LiquidScene\n"\
"\n"\
LIQUID_SWELL \
"// Quake's warp of the liquids' texture coordinates; lava's slower with the waves on.\n"\
"vec2 LiquidWarp(vec2 uv, uint kind)\n"\
"{\n"\
"	float t = kind == 1u && Water.x > 0. ? Time * 0.35 : Time;\n"\
"	return uv * 2.0 + 0.125 * sin(uv.yx * (3.14159265 * 2.0) + t);\n"\
"}\n"\
"\n"\
"// The waves: a sum of sines over the plane with normal n, in world units along the world axes the plane lies in (t, b).\n"\
"// xy the height's gradient along t and b, z the height. Lava's are long and slow, slime's slower than water's.\n"\
"vec2 LiquidSwellHere = vec2(0.); // the swell's height here, y 1 once LiquidWaves has it\n"\
"vec3 LiquidWaves(vec3 pos, vec3 n, uint kind)\n"\
"{\n"\
"	vec3 a = abs(n);\n"\
"	vec2 p = a.z >= a.x && a.z >= a.y ? pos.xy : a.x >= a.y ? pos.yz : pos.xz;\n"\
"	float scale = kind == 1u ? 2.5 : kind == 2u ? 1.5 : 1.0;\n"\
"	float speed = kind == 1u ? 0.12 : kind == 2u ? 0.4 : 0.8;\n"\
"	float steep = kind == 1u ? 0.05 : kind == 2u ? 0.06 : 0.07;\n"\
"	const vec2 dirs[5] = vec2[5](vec2(0.87, 0.49), vec2(-0.57, 0.82), vec2(0.17, -0.98), vec2(-0.94, -0.33), vec2(0.62, 0.78));\n"\
"	const float lens[5] = float[5](67., 41., 26., 16., 10.);\n"\
"	vec3 r = vec3(0.);\n"\
"	for (int i = 0; i < 5; i++)\n"\
"	{\n"\
"		float k = 6.2831853 / (lens[i] * scale);\n"\
"		float x = dot(dirs[i], p) * k + Time * sqrt(200. * k) * speed + float(i) * 1.7;\n"\
"		r.xy += dirs[i] * (steep * cos(x));\n"\
"		r.z += steep / k * sin(x);\n"\
"	}\n"\
"	r *= Water.x;\n"\
"	if (a.z >= a.x && a.z >= a.y)\n"\
"	{\n"\
"		vec3 swell = LiquidSwell(pos.xy, kind); // the geometric waves' slopes, whether this face's vertices rise or not\n"\
"		LiquidSwellHere = vec2(swell.z, 1.0); // (for the foam)\n"\
"		r += swell; // and the splashes' ripples' (Water3.y: how steep, times their shape's), finer ones close by\n"\
"		if (Water3.y > 0.)\n"\
"			r += LiquidRipples(pos, kind, 1.0 - smoothstep(300.0, 800.0, distance(pos, EyePos))) * Water3.y;\n"\
"	}\n"\
"	return r;\n"\
"}\n"\
"\n"\
"// The normal of the waves w over the flat surface facing (towards the eye).\n"\
"vec3 LiquidNormal(vec3 w, vec3 facing)\n"\
"{\n"\
"	vec3 a = abs(facing);\n"\
"	bool level = a.z >= a.x && a.z >= a.y; // the axes LiquidWaves takes\n"\
"	vec3 t = !level && a.x >= a.y ? vec3(0., 1., 0.) : vec3(1., 0., 0.);\n"\
"	vec3 b = level ? vec3(0., 1., 0.) : vec3(0., 0., 1.);\n"\
"	return normalize(facing - (t * w.x + b * w.y));\n"\
"}\n"\
"\n"\
"// A liquid's colour: tex its texture, lit that lit (light: by how much, 1 Quake's full light), n the waves' normal and\n"\
"// facing the flat one (towards the eye), h the waves' height. alpha: its opacity, in and out. Water and slime are more\n"\
"// see-through looking down and reflect a dim room colour at grazing angles (fresnel), glinting where the waves face a\n"\
"// light from above; lava glows, its hot parts brightest (to bloom), pulsing slowly; teleports shimmer.\n"\
"vec3 LiquidShade(vec3 tex, vec3 lit, vec3 light, vec3 pos, vec3 n, vec3 facing, float h, uint kind, inout float alpha)\n"\
"{\n"\
"	vec3 ndx = dFdx(n), ndy = dFdy(n); // QVR: the waves' spread under the pixel widens the glints (vr_specular_aa: Parallax.w)\n"\
"	float spread = min(0.25 * (dot(ndx, ndx) + dot(ndy, ndy)), 0.09) * Parallax.w;\n"\
"	if (kind == 1u)\n"\
"	{\n"\
"		float hot = smoothstep(0.2, 0.6, dot(tex, vec3(0.3, 0.59, 0.11)));\n"\
"		float pulse = 0.75 + 0.25 * sin(Time * 0.8 + h * 1.5 + dot(pos, vec3(0.011, 0.017, 0.0)));\n"\
"		return mix(lit, max(lit, tex), min(Water2.x, 1.0)) * (1.0 + Water2.x * (0.3 + 1.2 * hot * pulse));\n"\
"	}\n"\
"	if (kind == 3u)\n"\
"		return lit * (1.0 + 0.2 * sin(Time * 3.0 + h * 8.0) * min(Water.x, 1.0));\n"\
"	vec3 v = normalize(EyePos - pos);\n"\
"	float cosv = clamp(dot(n, v), 0.0, 1.0);\n"\
"	float l = min(dot(light, vec3(1.0 / 3.0)), 1.5);\n"\
"	float f, fresnel;\n"\
"	vec3 env;\n"\
"	if (Water2.z > 0.) // seen from inside: clear looking up, the murk mirrored past the critical angle\n"\
"	{\n"\
"		f = smoothstep(0.25, 0.4, 1.0 - cosv);\n"\
"		env = l * (kind == 2u ? vec3(0.05, 0.16, 0.03) : vec3(0.1, 0.16, 0.15));\n"\
"	}\n"\
"	else\n"\
"	{\n"\
"		float m = 1.0 - cosv;\n"\
"		f = 0.02 + 0.98 * m * m * m;\n"\
"		env = l * (kind == 2u ? vec3(0.16, 0.22, 0.11) : vec3(0.22, 0.26, 0.3));\n"\
"	}\n"\
"	fresnel = f * Water.y;\n"\
"	vec3 c = mix(lit, env, fresnel);\n"\
"	vec3 hv = normalize(normalize(facing + vec3(0.25, 0.15, 0.0)) + v);\n"\
"	float nh = max(dot(n, hv), 0.0);\n"\
"	vec2 p = 1.0 / (vec2(1.0 / 200.0, 1.0 / 24.0) + spread); // QVR: Blinn's exponents widened, their energy kept (SpecularAA)\n"\
"	float glint = (pow(nh, p.x) * 2.0 * (p.x + 1.0) / 201.0 + pow(nh, p.y) * 0.08 * (p.y + 1.0) / 25.0) * (0.15 + 0.85 * f) * (1.0 - Water2.z);\n"\
"	c += glint * Water.w * l * (kind == 2u ? vec3(0.7, 1.0, 0.5) : vec3(1.0));\n"\
"	alpha = mix(alpha * (1.0 - 0.3 * Water.y), 1.0, fresnel);\n"\
"	if (kind == 2u)\n"\
"		alpha = mix(alpha, 1.0, 0.3 * Water.y); // murky\n"\
"	return c;\n"\
"}\n"\
"\n"\
"// The distance along the view (clip w) of the opaque scene at pixel p (LiquidDepth: half the size, the nearest of each\n"\
"// four pixels).\n"\
"float LiquidSceneDistance(vec2 p)\n"\
"{\n"\
"	return texelFetch(LiquidDepth, clamp(ivec2(p * 0.5), ivec2(0), textureSize(LiquidDepth, 0) - 1), 0).r;\n"\
"}\n"\
"vec2 LiquidSceneHere = vec2(0.); // ... at this pixel, read once (the foam's and the refraction's); y 1 once read\n"\
"float LiquidSceneDistanceHere()\n"\
"{\n"\
"	if (LiquidSceneHere.y == 0.)\n"\
"		LiquidSceneHere = vec2(LiquidSceneDistance(gl_FragCoord.xy), 1.0);\n"\
"	return LiquidSceneHere.x;\n"\
"}\n"\
"\n"\
"// A translucent liquid over what is behind it, that bent by the waves (n, facing as above): read from the opaque scene\n"\
"// (Water.z: how far, 0: nothing to read), the liquid then opaque over it. The bend is a shift in the world, projected.\n"\
"// Only what is behind the surface is read (by the scene's distances, CausticsScale.w 1): where the shifted pixel is\n"\
"// of something in front of it (a hand or a gun over the water, a monster, a text), the bend is shortened, or none,\n"\
"// or that thing's colours would smear into the liquid round it, a halo; and it fades out where the liquid is shallow\n"\
"// along the view (its edges, round what stands in it).\n"\
"vec4 LiquidRefract(vec4 c, vec3 pos, vec3 n, vec3 facing, uint kind)\n"\
"{\n"\
"	if (Water.z <= 0. || c.a >= 1. || kind == 1u || kind == 3u)\n"\
"		return c;\n"\
"	float dist = distance(pos, EyePos);\n"\
"	vec3 d = (n - facing) * (Water.z * min(dist * 0.08, 12.0) * (kind == 2u ? 0.5 : 1.0));\n"\
"	vec4 c0 = ViewProj * vec4(pos, 1.0);\n"\
"	vec4 c1 = c0 + ViewProj * vec4(d, 0.0);\n"\
"	vec2 size = vec2(textureSize(LiquidScene, 0));\n"\
"	vec2 shift = (c1.xy / c1.w - c0.xy / c0.w) * 0.5;\n"\
"	if (CausticsScale.w != 0.)\n"\
"	{\n"\
"		shift *= smoothstep(0.0, 12.0, LiquidSceneDistanceHere() - c0.w);\n"\
"		if (LiquidSceneDistance(gl_FragCoord.xy + shift * size) < c0.w)\n"\
"			shift *= LiquidSceneDistance(gl_FragCoord.xy + shift * (0.3 * size)) < c0.w ? 0.0 : 0.3;\n"\
"	}\n"\
"	vec3 behind = texture(LiquidScene, gl_FragCoord.xy / size + shift).rgb;\n"\
"	return vec4(mix(behind, c.rgb, c.a), 1.0);\n"\
"}\n"\
"\n"\
"// Value noise, 0..1, on the lattice of whole p (an integer hash: the same everywhere in the world).\n"\
"float LiquidNoise(vec2 p)\n"\
"{\n"\
"	vec2 i = floor(p), f = p - i;\n"\
"	f = f * f * (3.0 - 2.0 * f);\n"\
"	uvec2 q = uvec2(ivec2(i) + 65536);\n"\
"	uvec4 h = uvec4(q.x, q.x + 1u, q.x, q.x + 1u) * 1597334677u ^ uvec4(q.y, q.y, q.y + 1u, q.y + 1u) * 3812015801u;\n"\
"	h = (h ^ (h >> 16u)) * 2246822519u;\n"\
"	vec4 v = vec4(h ^ (h >> 13u)) * (1.0 / 4294967296.0);\n"\
"	return mix(mix(v.x, v.y, f.x), mix(v.z, v.w, f.x), f.y);\n"\
"}\n"\
"\n"\
"// Shoreline foam (Water3.x: vr_water_foam, 0 off) on a level liquid seen from above, where it meets walls, steps and\n"\
"// things standing in it. How far the shore is: rim, from the swells' mesh (1 + the distance to the pool's rim in units,\n"\
"// 0 unknown: brush entities' liquids), and with the scene's distances (CausticsScale.w) how far behind the surface\n"\
"// the scene is along the view ray (a step, an object in the water, a wall seen at a slant). Water: white foam, its\n"\
"// edge broken by a crawling noise, wider where the swells push against the shore, and faint whitecaps on the swells'\n"\
"// crests; slime: a dark green scum; lava: a hot yellow rim at the rock (to bloom), a darker crust beyond. All in the\n"\
"// world (both eyes alike), fading out from 600 to 1000 units. tex the texture's colour, light the light on it (1\n"\
"// Quake's full); lit: the world program's (the unlit water's foam is as bright as its texture). alpha: its opacity.\n"\
"vec3 LiquidFoam(vec3 c, vec3 tex, vec3 light, vec3 pos, vec3 facing, float h, float rim, uint kind, bool lit, inout float alpha)\n"\
"{\n"\
"	if (Water3.x <= 0. || kind == 3u || facing.z < 0.7 || rim < 0.) // (rim < 0: a surface under more of the liquid)\n"\
"		return c;\n"\
"	float dist = distance(pos, EyePos);\n"\
"	float fade = 1.0 - smoothstep(600.0, 1000.0, dist);\n"\
"	if (fade <= 0.)\n"\
"		return c;\n"\
"	float amp = Water2.w * (kind == 1u ? 1.3 : kind == 2u ? 0.6 : 1.0);\n"\
"	float swell = amp > 0. ? (LiquidSwellHere.y > 0. ? LiquidSwellHere.x : LiquidSwell(pos.xy, kind).z) / amp : 0.0; // -1 .. 1\n"\
"	float width = (kind == 1u ? 8.0 : kind == 2u ? 7.0 : 10.0) * (0.5 + 0.5 * Water3.x) * (1.0 + 0.3 * swell + 0.1 * clamp(h, -1.0, 1.0));\n"\
"	float d = rim > 0. ? rim - 1.0 : 1e4;\n"\
"	if (CausticsScale.w != 0.)\n"\
"	{\n"\
"		float w = 1.0 / gl_FragCoord.w; // the surface's distance along the view\n"\
"		float behind = LiquidSceneDistanceHere() - w;\n"\
"		if (behind >= 0.) // (less: something in front, beside it)\n"\
"			d = min(d, behind * dist / w);\n"\
"	}\n"\
"	float f = 1.0 - d / width; // 1 at the shore, 0 width away\n"\
"	bool crest = kind == 4u && swell > 0.75;\n"\
"	if (f <= -1. && !crest) // (to twice the width: flecks drifting off it)\n"\
"		return c;\n"\
"	float t = Time * (kind == 1u ? 0.25 : kind == 2u ? 0.4 : 1.0);\n"\
"	vec2 p = pos.xy;\n"\
"	float n = LiquidNoise(p * 0.19 + vec2(t * 0.31, -t * 0.23)) * 0.6 + LiquidNoise(p * 0.47 - vec2(t * 0.47, t * 0.38)) * 0.4;\n"\
"	float fine = LiquidNoise(p * 0.6 + vec2(t * 0.7, t * 0.5));\n"\
"	float foam = smoothstep(n - 0.12, n + 0.12, f) * (0.6 + 0.4 * fine); // the foam reaches further where the noise is low\n"\
"	foam = max(foam, smoothstep(0.78, 0.97, f)); // a line at the shore itself\n"\
"	foam = max(foam, smoothstep(0.7, 0.85, fine) * smoothstep(0.45, 0.6, n) * 0.5 * clamp(f + 1.0, 0.0, 1.0)); // and flecks\n"\
"	if (crest)\n"\
"		foam = max(foam, smoothstep(0.75, 1.0, swell) * smoothstep(0.55, 0.8, n) * fine * 0.35);\n"\
"	foam *= fade * min(Water3.x, 1.0);\n"\
"	float b = lit ? min(dot(light, vec3(1.0 / 3.0)), 1.3) : clamp(dot(tex, vec3(0.3, 0.59, 0.11)) * 3.0 + 0.2, 0.4, 1.0);\n"\
"	if (kind == 1u) // lava: hot where it touches the rock, a cooler crust beyond\n"\
"	{\n"\
"		float hot = smoothstep(n * 0.7 + 0.2, n * 0.7 + 0.45, f) * (0.75 + 0.25 * sin(Time * 2.3 + n * 9.0));\n"\
"		c *= 1.0 - 0.45 * foam * (1.0 - hot) * smoothstep(0.3, 0.7, fine);\n"\
"		return c + vec3(1.2, 0.55, 0.12) * (hot * fade * min(Water3.x, 1.0) * (0.6 + 0.6 * Water2.x));\n"\
"	}\n"\
"	vec3 col = kind == 2u ? vec3(0.28, 0.34, 0.1) * (0.7 + 0.3 * fine) : vec3(0.85, 0.9, 0.92);\n"\
"	alpha = mix(alpha, 1.0, foam * 0.9);\n"\
"	return mix(c, col * b, foam * (kind == 2u ? 0.75 : 0.85));\n"\
"}\n"\
"\n"\
"// Caustics: the light on what is under water (the liquid a cell out from the surface, towards the eye) dappled by the\n"\
"// waves above, 1 elsewhere; two warped\n"\
"// sine lattices, bright along their lines, in the world (stretched down walls, weaker there: facing the surface's normal).\n"\
"float LiquidCaustics(vec3 pos, vec3 facing)\n"\
"{\n"\
"	if (Water2.y <= 0.)\n"\
"		return 1.0;\n"\
"	float wet = texture(LiquidVolume, (pos + facing * CausticsOrigin.w - CausticsOrigin.xyz) * CausticsScale.xyz).r; // a cell out\n"\
"	if (wet <= 0.004)\n"\
"		return 1.0;\n"\
"	vec2 p = (pos.xy + pos.z * vec2(0.35, 0.25)) * 0.09;\n"\
"	float t = Time * 0.7;\n"\
"	vec2 s = sin(p + 0.6 * sin(p.yx * vec2(1.3, 1.1) + vec2(t, t * 1.3)));\n"\
"	float l1 = 1.0 - abs(s.x + s.y) * 0.5;\n"\
"	vec2 p2 = mat2(0.8, 0.6, -0.6, 0.8) * p * 1.31 + vec2(3.1, 1.7);\n"\
"	s = sin(p2 + 0.6 * sin(p2.yx * vec2(1.3, 1.1) + vec2(t * 1.2 + 2.0, t * 1.56 + 2.0)));\n"\
"	float l2 = 1.0 - abs(s.x + s.y) * 0.5;\n"\
"	float c = pow(max(l1, l2), 8.0);\n"\
"	return 1.0 + wet * Water2.y * (0.4 + 0.6 * abs(facing.z)) * (c * 1.6 - 0.5);\n"\
"}\n"\
"\n"\

// the world vertex shader's Quake VR outputs
#define QVR_WORLD_VS_OUTPUTS \
"	layout(location=11) flat out uvec2 out_nmsampler; // QVR\n" \
"#endif\n" \
"layout(location=10) flat out float out_glow; // QVR\n" \
"layout(location=12) flat out float out_pdepth; // QVR: parallax mapping\n" \
"layout(location=13) flat out vec4 out_uvclamp; // QVR\n" \
"layout(location=14) flat out vec4 out_detail; // QVR: detail textures (vr/vr_detail.cpp)\n" \
"layout(location=15) flat out float out_aoself; // QVR: dynamic ambient occlusion (vr/vr_ao.cpp)\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	layout(location=20) out float out_rim; // QVR: 1 + the distance to the shore (the swells' mesh), 0 unknown: the foam\n"

// the world fragment shader's Quake VR inputs
#define QVR_WORLD_FS_INPUTS \
"	layout(location=11) flat in uvec2 in_nmsampler; // QVR\n" \
"#endif\n" \
"layout(location=10) flat in float in_glow; // QVR\n" \
"layout(location=12) flat in float in_pdepth; // QVR: parallax mapping\n" \
"layout(location=13) flat in vec4 in_uvclamp; // QVR\n" \
"layout(location=14) flat in vec4 in_detail; // QVR: detail textures (vr/vr_detail.cpp)\n" \
"layout(location=15) flat in float in_aoself; // QVR: dynamic ambient occlusion's own group (vr/vr_ao.cpp)\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	layout(location=20) in float in_rim; // QVR: the shoreline foam's distance (LiquidFoam)\n"

// detail, parallax, specular anti-aliasing, the baked light's bumps
#define QVR_WORLD_FS_FUNCTIONS \
DETAIL_FUNCTIONS /* QVR */ \
PARALLAX_FUNCTIONS /* QVR */ \
SPECULAR_AA_FUNCTIONS /* QVR */ \
"// QVR: the screen derivatives of the baked light's brightness lum, for its bumps (BakedBump): the lightmap's slope\n" \
"// here, from its luxels in full precision, times how its coordinates change across the screen. dFdx of the filtered\n" \
"// light itself was 0 between steps of the filter's 8-bit weights (1/256 of a luxel) and of the 8-bit light, and a\n" \
"// whole step where one fell between two pixels: up close, a grid of dark dashes and rings of them round lights\n" \
"// (round 15). The slope is the luxels' central differences blended across the cell, so it has no jump at the luxel\n" \
"// lines (the bilinear light's own slope has: a line in the bumps' light every 16 units); a luxel past the cell whose\n" \
"// difference disagrees with the cell's own (another face's light beside it in the atlas) isn't used. The luxels are\n" \
"// gathered (4 reads for the 4 x 4 around, a read per style): their green, relative, for the brightness's.\n" \
"vec4 LightmapGreen(vec2 uv, float ofs) // 4 luxels' green, all styles: textureGather's order (x 0 1, y 1 1, z 1 0, w 0 0)\n" \
"{\n" \
"	if (in_styles.y < 0.)\n" \
"		return in_styles.x * textureGather(LMTex, uv, 1);\n" \
"	if (in_styles.z < 0.)\n" \
"		return in_styles.x * textureGather(LMTex, uv, 1) + in_styles.y * textureGather(LMTex, uv + vec2(ofs, 0.), 1);\n" \
"	uv.x += ofs; // 3 or 4 styles: the greens of each, in the second block\n" \
"	return in_styles.x * textureGather(LMTex, uv, 0) + in_styles.y * textureGather(LMTex, uv, 1) +\n" \
"		in_styles.z * textureGather(LMTex, uv, 2) + in_styles.w * textureGather(LMTex, uv, 3);\n" \
"}\n" \
"float LuxelSlope(float outside, float inside, float lum) // at a luxel: the central difference, if its far side agrees\n" \
"{\n" \
"	return abs(outside - inside) <= 0.5 * (abs(outside) + abs(inside)) + max(0.1 * lum, 0.008) ? 0.5 * (outside + inside) : inside;\n" \
"}\n" \
"// QVR: the baked light's real direction at the pixel (deluxemaps: vr_deluxemap, ShadowFlags 128), from the map's\n" \
"// .lux (r_brush.c, GL_FillSurfaceLux): x along the texture's s axis on the face, y the normal n crossed with it, z n.\n" \
"// That axis is the gradient of the texture coordinate over the face, from the derivatives the bumps' frame comes\n" \
"// from: exact on flat faces, the same in both eyes, turning with a rotating brush model. Filtered, the direction is\n" \
"// shorter where its luxels disagree (light from several sides), and leans less. w 0: none (faces without, items).\n" \
"vec4 LuxDirection(vec2 lmuv, vec3 n, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	vec4 lux = textureLod(LuxTex, lmuv, 0.);\n" \
"	float det = dot(n, cross(dpdx, dpdy));\n" \
"	vec3 gs = (cross(dpdy, n) * duvdx.x + cross(n, dpdx) * duvdy.x) * (det < 0. ? -1.0 : 1.0); // along s\n" \
"	if (lux.a < 0.5 || dot(gs, gs) < 1e-30)\n" \
"		return vec4(0.);\n" \
"	vec3 t = normalize(gs), d = lux.xyz * 2.0 - 1.0;\n" \
"	return vec4(t * d.x + cross(n, t) * d.y + n * d.z, 1.0);\n" \
"}\n" \
"// The light's direction l for the bumps: at most 50 degrees off the normal n (the shading divides by dot(n, l): a\n" \
"// light along the wall would divide by nothing; at 60 the lit sides of ridges clipped white; the guess leans 45).\n" \
"vec3 LuxLight(vec3 n, vec3 l)\n" \
"{\n" \
"	float c = max(dot(n, l), 1e-6); // (straight down or none: n, as atan(0, 0) gave)\n" \
"	vec3 t = l - n * dot(n, l);\n" \
"	float s2 = dot(t, t);\n" \
"	// its angle from the normal, atan(s, c), past 50 degrees (0.8727): tan compared without the trigonometry\n" \
"	if (s2 * (0.6427605 * 0.6427605) >= c * c * (0.7660672 * 0.7660672))\n" \
"		return n * 0.6427605 + t * (0.7660672 * inversesqrt(max(s2, 1e-8)));\n" \
"	return (n * c + t) * inversesqrt(c * c + s2); // (cos a, sin a) = (c, s) / |(c, s)|\n" \
"}\n" \
"vec2 LightmapLumDerivs(float lum)\n" \
"{\n" \
"	vec2 size = vec2(textureSize(LMTex, 0));\n" \
"	vec2 p = in_lmuv * size - 0.5; // in luxels, from the first's centre\n" \
"	vec2 dpx = dFdx(p), dpy = dFdy(p);\n" \
"	vec2 c = floor(p), f = p - c; // the cell: luxels c (a) to c + 1 (e)\n" \
"	vec4 q0 = LightmapGreen((c + vec2(0., 0.)) / size, in_lmofs); // the 4 x 4 luxels c - 1 to c + 2, in 2 x 2 quads\n" \
"	vec4 q1 = LightmapGreen((c + vec2(2., 0.)) / size, in_lmofs);\n" \
"	vec4 q2 = LightmapGreen((c + vec2(0., 2.)) / size, in_lmofs);\n" \
"	vec4 q3 = LightmapGreen((c + vec2(2., 2.)) / size, in_lmofs);\n" \
"	float a = q0.y, b = q1.x, d = q2.z, e = q3.w; // the cell's: a b, and d e a row on\n" \
"	float gx0 = mix(LuxelSlope(a - q0.x, b - a, a), LuxelSlope(q1.y - b, b - a, b), f.x);\n" \
"	float gx1 = mix(LuxelSlope(d - q2.w, e - d, d), LuxelSlope(q3.z - e, e - d, e), f.x);\n" \
"	float gy0 = mix(LuxelSlope(a - q0.z, d - a, a), LuxelSlope(q2.y - d, d - a, d), f.y);\n" \
"	float gy1 = mix(LuxelSlope(b - q1.w, e - b, b), LuxelSlope(q3.x - e, e - b, e), f.y);\n" \
"	vec2 g = vec2(mix(gx0, gx1, f.y), mix(gy0, gy1, f.x)); // per luxel\n" \
"	float green = mix(mix(a, b, f.x), mix(d, e, f.x), f.y);\n" \
"	g *= green > 1e-3 ? lum * LightTweak.x / green : 0.0; // relative, as the brightness's (and its contrast's power)\n" \
"	return vec2(dot(g, dpx), dot(g, dpy));\n" \
"}\n"

// detail textures close by; a liquid's unlit texture
#define QVR_WORLD_FS_DETAIL \
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) " && !DITHER\n" \
"	if (Detail.x > 0. && in_detail.z > 0.) // QVR: detail textures close by (vr/vr_detail.cpp); not on fullbright texels (alpha 0)\n" \
"	{\n" \
"		float detail = DetailFactor(in_detail, puv, duvdx, duvdy, distance(in_pos, EyePos));\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_SOLID) "\n" \
"		detail = mix(1.0, detail, result.a);\n" \
"#endif\n" \
"		result.rgb *= detail;\n" \
"	}\n" \
"#endif\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	vec3 liquid_tex = result.rgb; // QVR: unlit (lava glows)\n" \
"#endif\n"

// light contrast, normal maps in the baked light, specular
#define QVR_WORLD_FS_LIGHT \
"	if (LightTweak.x != 1.) // QVR: contrast about Quake's full light (vr_light_contrast): darker shade, lamps as bright\n" \
"		total_light = 0.5 * pow(max(total_light, vec3(0.)) * 2.0, vec3(LightTweak.x));\n" \
"\n" \
"	// QVR: the normal bent by the normal map (vr_normalmaps), for the baked light and the dynamic lights\n" \
"	vec3 bumped = facing;\n" \
"	vec3 luxdir = vec3(0.); // QVR: the baked light's real direction (deluxemaps), for its bumps and sheen; 0: none\n" \
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	if (LightTweak.w > 0.)\n" \
"	{\n" \
"		bumped = BumpedNormal(NormalTex, puv, duvdx, duvdy, dpdx, dpdy, facing);\n" \
"		if (LightTweak.y > 0.)\n" \
"		{\n" \
"			vec4 lux = (ShadowFlags & 128u) != 0u ? LuxDirection(lmuv, facing, duvdx, duvdy, dpdx, dpdy) : vec4(0.);\n" \
"			if (lux.w > 0.) // QVR: from where the light comes from (as BakedBump: as bright as before on the flat)\n" \
"			{\n" \
"				luxdir = LuxLight(facing, lux.xyz);\n" \
"				total_light *= max(mix(1.0, max(dot(bumped, luxdir), 0.0) / dot(facing, luxdir), LightTweak.y), 0.0);\n" \
"			}\n" \
"			else\n" \
"			{\n" \
"				float lum = dot(total_light, vec3(1.0 / 3.0));\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n" \
"				vec2 dlum = vec2(0.); // no derivatives after the discard\n" \
"#else\n" \
"				vec2 dlum = LightmapLumDerivs(lum); // QVR: not dFdx(lum): stepped (see there)\n" \
"#endif\n" \
"				total_light *= BakedBump(facing, bumped, dpdx, dpdy, lum, dlum); // guessed from where it brightens\n" \
"			}\n" \
"		}\n" \
"	}\n" \
"	total_light *= LiquidCaustics(in_pos, facing); // QVR: under water (vr_water_caustics)\n" \
"	total_light *= DynamicAO(in_pos, AONormal(facing, luxdir), in_aoself, in_coord, in_depth); // QVR: what moves darkens the baked light near it (vr/vr_ao.cpp)\n" \
"#else\n" \
"	bumped = LiquidNormal(waves, facing); // QVR: the waves', for the lights' glints\n" \
"#endif\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n" \
"	SpecularAA(0.0); // QVR: the normal map's spread only (no derivatives after the discard)\n" \
"#else\n" \
"	SpecularAA(Parallax.w > 0. ? NormalSpread(bumped) : 0.0); // QVR: the sheen's lobe widened by the bumps under the pixel (vr_specular_aa)\n" \
"#endif\n" \
"	// QVR: the baked light's sheen, from its real direction (deluxemaps), as the dynamic lights' (vr_specular, the lobe\n" \
"	// SpecularAA sets) at a quarter of their strength: a lamp's glint on the bumps near it (at half, rough walls by\n" \
"	// a lamp looked hazy, 7 to 12% brighter)\n" \
"	if (luxdir != vec3(0.) && LightTweak.z > 0. && dot(bumped, luxdir) > 0.)\n" \
"	{\n" \
"		vec3 h = normalize(luxdir + normalize(EyePos - in_pos));\n" \
"		specular_light += total_light * (0.5 * LightTweak.z * SpecLobe.y) * pow(max(dot(bumped, h), 0.0), SpecLobe.x);\n" \
"	}\n" \
"\n"

// a light's shadow and spot cone
#define QVR_WORLD_FS_LIGHT_SHADOW \
"					if (l.shadow.w != 0.) // QVR: a map light's shadow of moving things\n" \
"					{\n" \
"						total_light *= MapLightShadow(l, in_pos, facing, total_light);\n" \
"						continue;\n" \
"					}\n" \
"					if (darkplaces) // QVR: DarkPlaces' falloff, colours brighter than 1 (vr_dlight_falloff)\n" \
"					{\n" \
"						float d = distance(l.origin, in_pos);\n" \
"						if (d >= l.radius)\n" \
"							continue;\n" \
"						float lit = DarkPlacesAtten(d, l.radius) * LightShadow(l, in_pos, facing);\n" \
"						if (lit <= 0.)\n" \
"							continue;\n" \
"						dynamic_light += lit * 0.5 * LightAngleDP(l, in_pos, bumped) * l.color; // halved: the lightmap is doubled below\n" \
"						specular_light += lit * LightSpecular(l, in_pos, bumped, EyePos) * l.color;\n" \
"						continue;\n" \
"					}\n"

// force grab's glow; the liquid's look
#define QVR_WORLD_FS_GLOW_LIQUID \
"	if (in_glow > 0.) // QVR: force grab's glow (vr/vr_fgfx.cpp)\n" \
"	{\n" \
"		float rim = 1.0 - abs(dot(facing, normalize(EyePos - in_pos)));\n" \
"		result.rgb += SceneTone.yzw * in_glow * (pow(rim, 2.0) * 1.1 + 0.12); // QVR: the player's hue\n" \
"	}\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	float liquid_alpha = in_alpha; // QVR: the liquid's look (vr_water_*)\n" \
"	result.rgb = LiquidShade(liquid_tex, result.rgb, total_light, in_pos, bumped, facing, waves.z, liquid, liquid_alpha);\n" \
"	result.rgb = LiquidFoam(result.rgb, liquid_tex, total_light, in_pos, facing, waves.z, in_rim, liquid, true, liquid_alpha);\n" \
"#endif\n"

// alpha to coverage; the liquid's alpha and refraction
#define QVR_WORLD_FS_ALPHA \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n" \
"	if (a2c)\n" \
"		result.a = clamp(coverage, 0.0, 1.0); // QVR: the samples it covers (GL_SAMPLE_ALPHA_TO_ONE writes 1)\n" \
"#endif\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	result.a = liquid_alpha; // QVR: and what is behind it bent by the waves\n" \
"	result = LiquidRefract(result, in_pos, bumped, facing, liquid);\n" \
"#endif\n"

// the recoil steadied (ZeroBlend), the poses' occlusion
#define QVR_ALIAS_VS_FUNCTIONS \
"#if POSEVERTTYPE == 1 || POSEVERTTYPE == 2\n" \
"vec3 ZeroBlend(vec3 pos, InstanceData inst) { return pos; }\n" \
"#else\n" \
"// QVR: the recoil steadied (vr/vr_render.cpp): the pose blended towards frame 0 (Padding: the zero pose times the\n" \
"// vertex count, the blend * 255 in the top byte). A muzzle flash's vertex (the top bit of its normal's 4th byte,\n" \
"// gl_mesh.c) keeps the flash's size: moved only as far as the gun vertex it rides on (its 4th bytes) is steadied.\n" \
"vec3 PackedPos(uint index)\n" \
"{\n" \
"	return vec3((PackedPosNor[index].xxx >> uvec3(0, 8, 16)) & 255u);\n" \
"}\n" \
"vec3 ZeroBlend(vec3 pos, InstanceData inst)\n" \
"{\n" \
"	if (inst.Padding == 0)\n" \
"		return pos;\n" \
"	uint zero = uint(inst.Padding) & 0xFFFFFFu;\n" \
"	float blend = float(uint(inst.Padding) >> 24) / 255.0;\n" \
"	uvec2 z = PackedPosNor[zero + uint(gl_VertexID)];\n" \
"	if ((z.y & 0x80000000u) == 0u)\n" \
"		return mix(pos, vec3((z.xxx >> uvec3(0, 8, 16)) & 255u), blend);\n" \
"	uint ref = (z.x >> 24) | ((z.y >> 16) & 0x7F00u);\n" \
"	vec3 gun = mix(PackedPos(uint(inst.Pose1) + ref), PackedPos(uint(inst.Pose2) + ref), inst.Blend);\n" \
"	return pos - (gun - PackedPos(zero + ref)) * blend;\n" \
"}\n" \
"#endif\n" \
"\n" \
"#if POSEVERTTYPE == 0 // PV_QUAKE1\n" \
"// QVR: the model's own occlusion at this vertex in a pose (vr/vr_ao.cpp: 1 open), in its position's 4th byte\n" \
"// (gl_mesh.c); a muzzle flash's vertex keeps its gun vertex there (the flag in its normal's 4th byte): none.\n" \
"float PoseAO(uint pose)\n" \
"{\n" \
"	uvec2 data = PackedPosNor[pose + gl_VertexID];\n" \
"	return (data.y & 0x80000000u) != 0u ? 1.0 : float(data.x >> 24) * (1.0 / 255.0);\n" \
"}\n" \
"#else\n" \
"float PoseAO(uint pose) { return 1.0; }\n" \
"#endif\n" \
"\n"

// the alias vertex shader's Quake VR outputs
#define QVR_ALIAS_VS_OUTPUTS \
"layout(location=3) out vec3 out_nor; // QVR: per-pixel dynamic lights (vr/vr_lighting.cpp)\n" \
"layout(location=4) out float out_depth; // QVR\n" \
"layout(location=5) noperspective out vec2 out_coord; // QVR\n" \
"layout(location=6) flat out float out_glow; // QVR\n" \
"layout(location=7) flat out float out_fbboost; // QVR\n" \
"layout(location=8) flat out float out_pdepth; // QVR: parallax mapping\n" \
"layout(location=9) flat out vec4 out_bumplight; // QVR: xyz towards the model's light (world), w how much the bumps shade it\n" \
"layout(location=10) flat out int out_instance; // QVR: its directional ambient (Ambient) for the fragment shader\n" \
"layout(location=11) out vec3 out_morphpos; // QVR: a gun morphing into its other ammo's model (vr/vr_render.cpp): where in its own units\n" \
"layout(location=12) flat out float out_morph; // QVR: and how far (Ambient[2].w; 0 not morphing)\n" \
"layout(location=13) out float out_vao; // QVR: the model's own occlusion here (vr/vr_ao.cpp), times its strength: 1 none\n"

// the model's light direction and shade (vr_model_light_parity), its outputs
#define QVR_ALIAS_VS_SHADE \
"	vec3 lightdir = normalize(mix(vec3(0.70710678, 0.0, 0.70710678), inst.LightDir.xyz, inst.LightDir.w)); // QVR: vr/vr_modellight.cpp\n" \
"	vec3 shadevector = orientation * lightdir;\n" \
"	out_instance = gl_InstanceID; // QVR\n" \
"	out_morph = inst.Ambient[2].w; // QVR\n" \
"	out_vao = inst.AO.y > 0. ? max(1.0 - inst.AO.y * (1.0 - mix(PoseAO(inst.Pose1), PoseAO(inst.Pose2), inst.Blend)), 0.0) : 1.0; // QVR\n" \
"	out_morphpos = lerpedPos * vec3(inst.Ambient[3].w, inst.Ambient[4].w, inst.Ambient[5].w); // QVR: the model's scale\n" \
"	float dirkeep = inst.Ambient[1].w; // QVR: the share of the directional shading the ambient cube leaves (1 without it)\n" \
"	out_bumplight = vec4(lightdir, (abs(inst.Glow.y) - 1.0) * dirkeep); // QVR: Glow.y: +-(1 + the bumps' strength), + on a par with the world\n" \
"	float dot1, dot2;\n" \
"	if (inst.Glow.y > 0.) // QVR: on a par with the world (vr_model_light_parity): 0.6 .. 1.4 by the normal, on average the light given\n" \
"	{\n" \
"		dot1 = 1.0 + 0.4 * dot(pose1.nor, shadevector);\n" \
"		dot2 = 1.0 + 0.4 * dot(pose2.nor, shadevector);\n" \
"	}\n" \
"	else\n" \
"	{\n" \
"		dot1 = r_avertexnormal_dot(pose1.nor, shadevector);\n" \
"		dot2 = r_avertexnormal_dot(pose2.nor, shadevector);\n" \
"	}\n" \
"	float shade = mix(inst.Glow.y > 0. ? 1.0 : 1.18, mix(dot1, dot2, inst.Blend), dirkeep); // QVR: towards its average over the sphere\n"

// wounds painted on models (vr/vr_wounds.cpp)
#define QVR_WOUND_PAINT_FS \
"// QVR: wounds painted on models (vr/vr_wounds.cpp): the model drawn into its mask, laid out by its skin's coordinates\n" \
"// (alias_vertex_shader with WOUNDPAINT; in_pos: the world, as the model is now), each texel given the most any splat\n" \
"// paints at the model's surface there (blended by max): r blood, g char, b wetness, a heat. Distances are the world's,\n" \
"// so a splat goes across the skin's seams as it goes across the model.\n" \
"layout(location=0) in vec2 in_texcoord;\n" \
"layout(location=2) in vec3 in_pos;\n" \
"layout(location=3) in vec3 in_nor;\n" \
"\n" \
"// Five vectors a splat: 0 its centre, radius; 1 its axis (unit), shape; 2 what it paints (r g b a, the most);\n" \
"// 3 a direction it is drawn out along (or 0), its seed; 4 how deep it reaches along the axis, the least the surface\n" \
"// faces the axis, how far drawn out (a wound's) or its noise's scale (a burn's) or patchiness (a liquid's), a\n" \
"// wound's run down (0 none).\n" \
"layout(location=0) uniform int NumSplats;\n" \
"layout(location=1) uniform vec4 Splats[80];\n" \
"\n" \
"layout(location=0) out vec4 out_fragcolor;\n" \
"\n" \
"float PaintHash(float n)\n" \
"{\n" \
"	return fract(sin(n * 12.9898 + 4.1414) * 43758.5453);\n" \
"}\n" \
"\n" \
"float PaintHash3(vec3 p)\n" \
"{\n" \
"	p = fract(p * 0.3183099 + 0.1) * 17.0;\n" \
"	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));\n" \
"}\n" \
"\n" \
"float PaintNoise(vec3 x)\n" \
"{\n" \
"	vec3 i = floor(x), f = fract(x);\n" \
"	f = f * f * (3.0 - 2.0 * f);\n" \
"	vec2 o = vec2(0.0, 1.0);\n" \
"	return mix(mix(mix(PaintHash3(i + o.xxx), PaintHash3(i + o.yxx), f.x), mix(PaintHash3(i + o.xyx), PaintHash3(i + o.yyx), f.x), f.y),\n" \
"		mix(mix(PaintHash3(i + o.xxy), PaintHash3(i + o.yxy), f.x), mix(PaintHash3(i + o.xyy), PaintHash3(i + o.yyy), f.x), f.y), f.z);\n" \
"}\n" \
"\n" \
"// A wound (shape 0): a star of blood round the axis (its core darkest), a few drops spattered round it, and a run\n" \
"// dripping down the surface from it. 1 at the middle, 0.5 out to the star's tips.\n" \
"float PaintWound(vec3 v, vec3 nrm, vec4 s0, vec4 s1, vec4 s3, vec4 s4)\n" \
"{\n" \
"	vec3 a = s1.xyz;\n" \
"	if (dot(nrm, a) < s4.y)\n" \
"		return 0.0;\n" \
"	float h = dot(v, a);\n" \
"	float val = 0.0;\n" \
"	float r0 = s0.w;\n" \
"	float seed = s3.w;\n" \
"	vec3 q = v - h * a;\n" \
"	vec3 e = s3.xyz - a * dot(s3.xyz, a);\n" \
"	e = dot(e, e) > 1e-4 ? normalize(e) : normalize(cross(a, abs(a.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0)));\n" \
"	vec2 k = vec2(dot(q, e) / max(s4.z, 1.0), dot(q, cross(a, e)));\n" \
"	float r = length(k);\n" \
"	if (abs(h) <= s4.x)\n" \
"	{\n" \
"		// 6..9 spikes of their own widths and lengths round a round middle, the edge ragged\n" \
"		float spikes = 6.0 + floor(PaintHash(seed) * 4.0);\n" \
"		float sa = (atan(k.y, k.x) / 6.2831853 + 0.5) * spikes + PaintHash(seed + 1.7);\n" \
"		float si = mod(floor(sa), spikes);\n" \
"		float width = 0.55 + 0.4 * PaintHash(seed + si * 3.3);\n" \
"		float prof = max(1.0 - abs(fract(sa) - 0.5) / (0.5 * width), 0.0);\n" \
"		float len = 0.55 + 0.75 * PaintHash(seed + si * 7.13);\n" \
"		float rag = 1.0 + 0.25 * (PaintHash(seed + floor(sa * 5.0) * 1.31) - 0.5);\n" \
"		float rs = r0 * (0.5 + 0.5 * len * prof) * rag;\n" \
"		if (r < rs)\n" \
"			val = mix(0.5, 1.0, clamp(1.0 - r / (0.8 * r0), 0.0, 1.0));\n" \
"		for (int j = 0; j < 5; j++) // drops spattered round it\n" \
"		{\n" \
"			float fj = float(j);\n" \
"			if (PaintHash(seed + fj * 9.7) > 0.7)\n" \
"				continue;\n" \
"			float an = PaintHash(seed + fj * 3.1) * 6.2831853;\n" \
"			vec2 c = vec2(cos(an), sin(an)) * r0 * (1.1 + 0.9 * PaintHash(seed + fj * 5.3));\n" \
"			if (length(k - c) < r0 * (0.1 + 0.12 * PaintHash(seed + fj * 1.9)))\n" \
"				val = max(val, 0.55);\n" \
"		}\n" \
"	}\n" \
"	if (s4.w > 0.0) // the run: down the surface, thinning\n" \
"	{\n" \
"		vec3 side = cross(vec3(0.0, 0.0, 1.0), a);\n" \
"		float sl = length(side);\n" \
"		float down = -v.z;\n" \
"		if (sl > 0.3 && down > 0.0 && down < s4.w && abs(h) < s4.x + down * 0.6)\n" \
"		{\n" \
"			float width = max(r0 * 0.2 * (1.0 - 0.5 * down / s4.w), 0.45);\n" \
"			if (abs(dot(v, side / sl)) < width)\n" \
"				val = max(val, 0.55 + 0.25 * (1.0 - down / s4.w));\n" \
"		}\n" \
"	}\n" \
"	return val;\n" \
"}\n" \
"\n" \
"// A burn (shape 1: facing along the axis) or a blast (2: facing its centre): charred round the centre (fully to s4.x\n" \
"// from it), ragged, none at the radius.\n" \
"float PaintBurn(vec3 v, vec3 p, vec3 nrm, vec4 s0, vec4 s1, vec4 s3, vec4 s4, bool blast)\n" \
"{\n" \
"	float d = length(v);\n" \
"	float face = blast ? dot(nrm, -v / max(d, 1e-3)) : dot(nrm, s1.xyz);\n" \
"	if (face < s4.y || d >= s0.w)\n" \
"		return 0.0;\n" \
"	vec3 x = p * s4.z + s3.w;\n" \
"	float n = PaintNoise(x) * 0.65 + PaintNoise(x * 2.3 + 5.1) * 0.35;\n" \
"	float t = clamp((s0.w - d) / max(s0.w - s4.x, 1e-3), 0.0, 1.0); // 1 within s4.x of the centre, 0 at the radius\n" \
"	return clamp(t * 1.1 + (n - 0.5) * 2.2 - 0.35, 0.0, 1.0); // blotchy: holes, and deeper patches\n" \
"}\n" \
"\n" \
"// A liquid (shape 3): all under its surface (s0.z; ragged), the deeper the more (fully s0.w under it); s4.z > 0: only\n" \
"// in patches (lava, slime eating in).\n" \
"float PaintLiquid(vec3 p, vec4 s0, vec4 s3, vec4 s4)\n" \
"{\n" \
"	float under = s0.z - p.z + (PaintNoise(p * 0.45 + s3.w) - 0.5) * 3.0;\n" \
"	if (under < 0.0)\n" \
"		return 0.0;\n" \
"	float val = clamp(0.5 + 0.5 * under / s0.w, 0.0, 1.0);\n" \
"	if (s4.z > 0.0)\n" \
"	{\n" \
"		float n = PaintNoise(p * 0.3 + s3.w) * 0.6 + PaintNoise(p * 0.9 + 3.7) * 0.4;\n" \
"		val *= step(s4.z, n);\n" \
"	}\n" \
"	return val;\n" \
"}\n" \
"\n" \
"void main()\n" \
"{\n" \
"	vec3 p = in_pos;\n" \
"	// A triangle folded away to nothing (a posed body's parts hidden inside others: its skin still takes its texels)\n" \
"	// is not the model's surface: nothing painted there.\n" \
"	if (dot(dFdx(p), dFdx(p)) + dot(dFdy(p), dFdy(p)) < 1e-6)\n" \
"		discard;\n" \
"	vec3 nrm = normalize(in_nor);\n" \
"	vec4 total = vec4(0.0);\n" \
"	for (int i = 0; i < NumSplats; i++)\n" \
"	{\n" \
"		vec4 s0 = Splats[i * 5], s1 = Splats[i * 5 + 1], s2 = Splats[i * 5 + 2], s3 = Splats[i * 5 + 3], s4 = Splats[i * 5 + 4];\n" \
"		vec3 v = p - s0.xyz;\n" \
"		int shape = int(s1.w + 0.5);\n" \
"		float val;\n" \
"		if (shape == 0)\n" \
"			val = PaintWound(v, nrm, s0, s1, s3, s4);\n" \
"		else if (shape == 3)\n" \
"			val = PaintLiquid(p, s0, s3, s4);\n" \
"		else\n" \
"			val = PaintBurn(v, p, nrm, s0, s1, s3, s4, shape == 2);\n" \
"		total = max(total, s2 * val);\n" \
"	}\n" \
"	out_fragcolor = total;\n" \
"}\n"

// the frame data, lights and functions the alias fragment shader uses
#define QVR_ALIAS_FS_HEADER \
ALIAS_FRAMEDATA_BUFFER /* QVR */ \
LIGHT_BUFFER /* QVR */ \
LIGHT_CLUSTER_IMAGE("readonly") /* QVR */ \
SHADOW_FUNCTIONS /* QVR */ \
AO_FUNCTIONS /* QVR */ \
PARALLAX_FUNCTIONS /* QVR */ \
SPECULAR_AA_FUNCTIONS /* QVR */

// the alias fragment shader's Quake VR inputs
#define QVR_ALIAS_FS_INPUTS \
"layout(location=3) in vec3 in_nor; // QVR\n" \
"layout(location=4) in float in_depth; // QVR\n" \
"layout(location=5) noperspective in vec2 in_coord; // QVR\n" \
"layout(location=6) flat in float in_glow; // QVR\n" \
"layout(location=7) flat in float in_fbboost; // QVR\n" \
"layout(location=8) flat in float in_pdepth; // QVR: parallax mapping's depth in units (vr_parallax_models; 0 off; negative: an authored map's, vr_parallax_authored)\n" \
"layout(location=9) flat in vec4 in_bumplight; // QVR: xyz towards the model's light, w how much the bumps shade it\n" \
"layout(location=10) flat in int in_instance; // QVR: for its Ambient\n" \
"layout(location=11) in vec3 in_morphpos; // QVR: a gun's morph (vr/vr_render.cpp)\n" \
"layout(location=12) flat in float in_morph; // QVR\n" \
"layout(location=13) in float in_vao; // QVR: the model's own occlusion (vr/vr_ao.cpp)\n"

// per-pixel lights, normal maps, ambient, wounds, morphs
#define QVR_ALIAS_FS_FUNCTIONS \
"// QVR: dynamic lights per pixel (vr/vr_lighting.cpp), shaded by the angle (the normal bent by the normal map) and\n" \
"// shadowed, as a multiplier of the texture (Quake adds radius - distance to the model's light, which is divided by\n" \
"// 200, doubled for overbright models; by 128 on a par with the world); `spec` gets their sheen. n: the smooth normal.\n" \
"vec3 ModelDynamicLights(vec3 n, vec3 bumped, out vec3 spec)\n" \
"{\n" \
"	spec = vec3(0.);\n" \
"	if ((ShadowFlags & 8u) == 0u || NumLights == 0u)\n" \
"		return vec3(0.);\n" \
"	ivec3 cluster_coord;\n" \
"	cluster_coord.x = int(floor(in_coord.x));\n" \
"	cluster_coord.y = int(floor(in_coord.y));\n" \
"	cluster_coord.z = int(floor(log2(in_depth) * ZLogScale + ZLogBias));\n" \
"	uvec2 clusterdata = imageLoad(LightClusters, cluster_coord).xy;\n" \
"	if ((clusterdata.x | clusterdata.y) == 0u)\n" \
"		return vec3(0.);\n" \
"	vec3 pos = in_pos + EyePos;\n" \
"	bool darkplaces = (ShadowFlags & 16u) != 0u;\n" \
"	float unit = (ShadowFlags & 32u) != 0u ? 1.0 / 128.0 : Fog.w < 0. ? 2.0 / 200.0 : 1.0 / 200.0; // the sign of Fog.w: overbright models\n" \
"	float angleScale = darkplaces ? 1.0 : 2.0; // LightAngle doubles the lit part (over its ambient 0.3)\n" \
"	vec3 toEye = normalize(EyePos - pos);\n" \
"	vec3 total = vec3(0.);\n" \
"	// LightShadow, LightAngle(DP) and LightSpecular (SHADOW_FUNCTIONS) inlined, the light's distance and direction\n" \
"	// worked out once for all three (and the direction to the eye once for all lights)\n" \
"	for (uint i = 0u, ofs = 0u; i < 2u; i++, ofs += 32u)\n" \
"	{\n" \
"		uint mask = clusterdata[i];\n" \
"		while (mask != 0u)\n" \
"		{\n" \
"			int j = findLSB(mask);\n" \
"			mask ^= 1u << j;\n" \
"			Light l = Lights[ofs + j];\n" \
"			if (l.shadow.w != 0.)\n" \
"				continue;\n" \
"			vec3 tl = l.origin - pos;\n" \
"			float d2 = dot(tl, tl);\n" \
"			if (d2 >= l.radius * l.radius)\n" \
"				continue;\n" \
"			float inv = inversesqrt(max(d2, 1e-12));\n" \
"			float d = d2 * inv;\n" \
"			vec3 dir = tl * inv;\n" \
"			float lit = (darkplaces ? DarkPlacesAtten(d, l.radius) : (l.radius - d) * unit) * (1.0 - smoothstep(0.0, 1.0, l.spot.w + dot(l.spot.xyz, dir))); // SpotCone\n" \
"			if (lit > 0. && l.shadow.z > 0.)\n" \
"				lit *= l.shadow2.x > 0. ? SpotShadow(l, pos, n) : ShadowLookup(ShadowAtlas, l.shadow.xyz, ShadowOffset(-tl, n, l.shadow.z));\n" \
"			if (lit <= 0.)\n" \
"				continue;\n" \
"			float ndl = dot(bumped, dir);\n" \
"			float ambient = darkplaces ? l.minlight : 0.3;\n" \
"			total += lit * mix(1.0, ambient + (1.0 - ambient) * angleScale * max(ndl, 0.0), DlightAngle) * l.color;\n" \
"			if (LightTweak.z > 0. && ndl > 0.)\n" \
"				spec += lit * (pow(max(dot(bumped, normalize(dir + toEye)), 0.0), SpecLobe.x) * SpecLobe.y * LightTweak.z) * l.color;\n" \
"		}\n" \
"	}\n" \
"	return total;\n" \
"}\n" \
"\n" \
"// QVR: the bumps on the model's own light (in_bumplight.w: vr_normalmap_models), from its direction (vr_modellight's\n" \
"// or the fixed one): brighter where a bump leans towards the light, darker where it leans away. The lean is the bent\n" \
"// normal's part along the surface, which averages out over the skin: as bright as before on the whole (like the\n" \
"// world's bumps in the baked light). Weaker on the side facing away, which the light's direction hardly reaches.\n" \
"float ModelBumpShade(vec3 n, vec3 bumped)\n" \
"{\n" \
"	if (in_bumplight.w <= 0.)\n" \
"		return 1.0;\n" \
"	vec3 l = in_bumplight.xyz;\n" \
"	float lean = dot(bumped - n * dot(bumped, n), l);\n" \
"	return max(1.0 + lean * in_bumplight.w * (0.3 + 0.7 * smoothstep(-0.5, 0.5, dot(n, l))), 0.0);\n" \
"}\n" \
"\n" \
"// QVR: directional ambient (vr/vr_ambient.cpp, vr_model_ambient_dir): the light around the model from six sides\n" \
"// (+X -X +Y -Y +Z -Z, each over the model's own light: 1 on average), shaded by the normal squared (an ambient\n" \
"// cube, as Half-Life 2's).\n" \
"// AmbientCube(n): the light around the model reaching a unit world-space normal n, relative to the model's own\n" \
"// light (in_color): 1 on average over the sphere, > 1 towards lit surroundings, < 1 towards shade; vec3(1.0) when\n" \
"// off (vr_model_ambient_dir 0). Stable name: other effects (rim light) may tint with it.\n" \
"vec3 AmbientCube(vec3 n)\n" \
"{\n" \
"	vec4 a0 = instances[in_instance].Ambient[0];\n" \
"	if (a0.w <= 0.)\n" \
"		return vec3(1.0);\n" \
"	vec3 n2 = n * n;\n" \
"	vec3 c = n2.x * instances[in_instance].Ambient[n.x >= 0. ? 0 : 1].xyz\n" \
"		+ n2.y * instances[in_instance].Ambient[n.y >= 0. ? 2 : 3].xyz\n" \
"		+ n2.z * instances[in_instance].Ambient[n.z >= 0. ? 4 : 5].xyz;\n" \
"	return mix(vec3(1.0), c, a0.w);\n" \
"}\n" \
"\n" \
"// QVR: the ambient cube at the normal leaning towards the bumped one by the model's bump strength (held weapons half).\n" \
"vec3 ModelAmbient(vec3 n, vec3 bumped)\n" \
"{\n" \
"	float bumps = clamp(abs(instances[in_instance].Glow.y) - 1.0, 0.0, 1.0);\n" \
"	return AmbientCube(normalize(mix(n, bumped, bumps)));\n" \
"}\n" \
"\n" \
"// QVR: rim light and reflections (vr/vr_envmap.cpp; the instance's Surface). EnvCube: the world round the hands.\n" \
"layout(binding=14) uniform samplerCube EnvCube;\n" \
"\n" \
"// QVR: how much a skin's texel looks like metal: greys and blue-greys (Quake's palette rows 0-15, 32-47, the\n" \
"// green-greys), not warm colours (skin, wood, leather, rust) nor saturated ones, nor black.\n" \
"float MetalMask(vec3 c)\n" \
"{\n" \
"	float mx = max(c.r, max(c.g, c.b));\n" \
"	float sat = (mx - min(c.r, min(c.g, c.b))) / max(mx, 1e-3);\n" \
"	float grey = c.r > c.b + 0.02 ? 1.0 - smoothstep(0.06, 0.14, sat) : 1.0 - smoothstep(0.22, 0.4, sat);\n" \
"	return grey * smoothstep(0.03, 0.12, mx);\n" \
"}\n" \
"\n" \
"// QVR: added to the lit colour. The rim light: a fresnel falloff round the edges facing away from this eye, in the\n" \
"// model's light from behind it (its ambient cube away from the eye). The reflections: the world's cube in the\n" \
"// mirrored eye direction (the normal half bent by the bumps), by fresnel (metal: the skin's colour straight on, white\n" \
"// at grazing angles) and the metal mask, darker where the model is dark. lit: the texel's lit share (not fullbright).\n" \
"vec3 ModelRimReflect(vec3 n, vec3 bumped, vec3 skin, float lit)\n" \
"{\n" \
"	vec4 s = instances[in_instance].Surface;\n" \
"	vec3 add = vec3(0.);\n" \
"#if ALPHATEST\n" \
"	lit = 1.0;\n" \
"#endif\n" \
"	if ((s.x <= 0. && s.y <= 0.) || lit <= 0.)\n" \
"		return add;\n" \
"	vec3 v = normalize(-in_pos);\n" \
"	if (s.x > 0.)\n" \
"	{\n" \
"		float f = 1.0 - clamp(dot(n, v), 0.0, 1.0);\n" \
"		vec3 back = n - v;\n" \
"		back *= inversesqrt(max(dot(back, back), 1e-6));\n" \
"		add += min(in_color.rgb, vec3(1.0)) * min(AmbientCube(back), vec3(1.5)) * (pow(f, 4.0) * s.x * 0.6) * skin; // capped: in bright light (outdoors, the sky behind) models glowed\n" \
"	}\n" \
"	float metal = s.y > 0. ? MetalMask(skin) * s.y : 0.;\n" \
"	if (s.w > 0.) // vr_weapon_reflections 2: the cube as a sharp mirror (a test)\n" \
"		return textureLod(EnvCube, reflect(-v, n), 0.).rgb * 2.0 - skin * in_color.rgb;\n" \
"	if (metal > 0.)\n" \
"	{\n" \
"		vec3 nb = normalize(mix(n, bumped, 0.5));\n" \
"		float ndv = clamp(dot(nb, v), 0.0, 1.0);\n" \
"		vec3 env = textureLod(EnvCube, reflect(-v, nb), s.z).rgb;\n" \
"		vec3 f0 = clamp(mix(vec3(0.5), skin * 1.5, 0.5), 0.0, 1.0);\n" \
"		vec3 fres = f0 + (1.0 - f0) * pow(1.0 - ndv, 5.0);\n" \
"		float dark = smoothstep(0.02, 0.35, dot(in_color.rgb, vec3(0.3333)));\n" \
"		add += env * fres * (metal * dark * 1.5);\n" \
"	}\n" \
"	return add * lit;\n" \
"}\n" \
"\n" \
"// QVR: a gun morphing into its other ammo's model (vr/vr_render.cpp: in_morph + coming in, - going out, 2 * kind +\n" \
"// progress): the two models' surfaces split by a noise in the models' shared units, the one coming in where it is\n" \
"// under the progress, the other where it is over; a glowing seam between them, in the kind's colour (the lava's\n" \
"// orange, the multi-rockets' yellow, the plasma's blue). False where this model is not drawn (yet or any more).\n" \
"float MorphHash(vec3 p)\n" \
"{\n" \
"	p = fract(p * 0.3183099 + 0.1) * 17.0;\n" \
"	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));\n" \
"}\n" \
"float MorphNoise(vec3 x)\n" \
"{\n" \
"	vec3 i = floor(x), f = fract(x);\n" \
"	f = f * f * (3.0 - 2.0 * f);\n" \
"	vec2 o = vec2(0.0, 1.0);\n" \
"	return mix(mix(mix(MorphHash(i + o.xxx), MorphHash(i + o.yxx), f.x), mix(MorphHash(i + o.xyx), MorphHash(i + o.yyx), f.x), f.y),\n" \
"		mix(mix(MorphHash(i + o.xxy), MorphHash(i + o.yxy), f.x), mix(MorphHash(i + o.xyy), MorphHash(i + o.yyy), f.x), f.y), f.z);\n" \
"}\n" \
"// QVR: wounds painted on the model (vr/vr_wounds.cpp: vr_wounds, vr_wounds_burns, vr_wounds_wet): its mask, in the\n" \
"// skin's layout (r blood, g char, b wetness, a heat: a fresh burn's embers), read at the skin's texel (the mask's on a\n" \
"// skin finer than twice it) and shown with a 4x4 ordered dither over the values' edges: sharp-edged, chunky marks on\n" \
"// the skin's own grid, in Quake's palette colours, as the skins' painted ones.\n" \
"layout(binding=13) uniform sampler2DArray WoundMasks;\n" \
"\n" \
"struct Wounds\n" \
"{\n" \
"	float blood;   // covered by blood (0 or 1)\n" \
"	float burn;	// charred\n" \
"	float wet;	 // wet\n" \
"	vec3 look;	 // the texel's colour with them\n" \
"	float gloss;   // a wet sheen's strength\n" \
"	float flatten; // how much of the normal map's bumps they hide\n" \
"	vec3 glow;	 // embers (unlit)\n" \
"};\n" \
"\n" \
"float WoundHash(vec2 p)\n" \
"{\n" \
"	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n" \
"}\n" \
"\n" \
"Wounds WoundsAt(vec2 uv, vec3 skin)\n" \
"{\n" \
"	Wounds w;\n" \
"	w.blood = 0.0; w.burn = 0.0; w.wet = 0.0; w.look = skin; w.gloss = 0.0; w.flatten = 0.0; w.glow = vec3(0.0);\n" \
"	vec4 wi = instances[in_instance].Wound; // x its layer + 1 (0 none), yz its size in texels, w the time\n" \
"	if (wi.x < 0.5)\n" \
"		return w;\n" \
"	vec2 size = vec2(textureSize(Tex, 0));\n" \
"	vec2 grid = all(lessThanEqual(size, wi.yz * 2.0)) ? size : wi.yz;\n" \
"	vec2 t = floor(clamp(uv, 0.0, 0.99999) * grid);\n" \
"	vec4 m = texelFetch(WoundMasks, ivec3(ivec2((t + 0.5) / grid * wi.yz), int(wi.x) - 1), 0);\n" \
"	if (m == vec4(0.0))\n" \
"		return w;\n" \
"	ivec2 it = ivec2(t) & 3;\n" \
"	const float bayer[16] = float[16](0., 8., 2., 10., 12., 4., 14., 6., 3., 11., 1., 9., 15., 7., 13., 5.);\n" \
"	float d = (bayer[it.y * 4 + it.x] + 0.5) / 16.0;\n" \
"	float h = WoundHash(t);\n" \
"	w.burn = step(0.16 + 0.28 * d, m.g);\n" \
"	w.blood = step(0.2 + 0.24 * d, m.r);\n" \
"	w.wet = step(0.06 + 0.4 * d, m.b);\n" \
"	float lum = dot(skin, vec3(0.3, 0.59, 0.11));\n" \
"	// char: the skin's shading darkened and browned; the deepest near black (Quake's palette 16..17)\n" \
"	vec3 charred = m.g > 0.72 ? mix(vec3(15., 11., 7.), vec3(31., 23., 15.), h) / 255.0\n" \
"							  : vec3(0.26, 0.2, 0.15) * (0.35 + 0.9 * lum) * (0.8 + 0.4 * h);\n" \
"	// blood: Quake's reds, as the body's painted wounds: as the monsters' own (palette 69..75): 69 in the middle, 71 round it, 74 at the tips and runs\n" \
"	vec3 red = vec3(m.r > 0.82 ? 47. : m.r > 0.62 ? 63. : 87., 0., 0.) / 255.0;\n" \
"	vec3 c = mix(skin, charred, w.burn);\n" \
"	c = mix(c, red * (0.75 + 0.5 * lum), w.blood); // a little of the skin's shading through it\n" \
"	c *= 1.0 - 0.5 * w.wet; // wet: darker\n" \
"	w.look = c;\n" \
"	w.gloss = w.blood * 0.55;\n" \
"	w.flatten = max(w.blood * 0.75, w.wet * 0.5);\n" \
"	// embers: a fresh burn glows in its cracks, flickering (Quake's fullbright oranges, 232..236)\n" \
"	if (m.a > 0.0 && w.burn > 0.0)\n" \
"	{\n" \
"		float flick = WoundHash(t + floor(wi.w * 7.0) * 1.37);\n" \
"		w.glow = step(0.85, h) * step(0.6, m.g) * step(flick, m.a) * mix(vec3(183., 51., 15.), vec3(219., 127., 59.), flick) / 255.0 * (0.8 * m.a);\n" \
"	}\n" \
"	return w;\n" \
"}\n" \
"\n" \
"// QVR: a wound's wet sheen (blood, water) in the model's own light, from its direction: a tight highlight, and a little\n" \
"// at grazing angles.\n" \
"vec3 WoundSheen(Wounds w, vec3 n)\n" \
"{\n" \
"	if (w.gloss <= 0.0 && w.wet <= 0.0)\n" \
"		return vec3(0.0);\n" \
"	vec3 v = normalize(-in_pos);\n" \
"	vec3 l = normalize(in_bumplight.xyz);\n" \
"	float s = pow(max(dot(n, normalize(l + v)), 0.0), 48.0);\n" \
"	float f = pow(1.0 - clamp(dot(n, v), 0.0, 1.0), 4.0);\n" \
"	return min(in_color.rgb, vec3(1.0)) * (w.gloss * s * 0.35 + w.wet * (pow(max(dot(n, normalize(l + v)), 0.0), 24.0) * 0.15 + f * 0.05));\n" \
"}\n" \
"\n" \
"bool Morph(out vec3 seam)\n" \
"{\n" \
"	seam = vec3(0.0);\n" \
"	if (in_morph == 0.0)\n" \
"		return true;\n" \
"	float a = abs(in_morph);\n" \
"	float kind = floor(a * 0.5);\n" \
"	float cut = (a - 2.0 * kind) * 1.1 - 0.05;\n" \
"	float n = 0.6 * MorphNoise(in_morphpos * 0.45) + 0.4 * MorphNoise(in_morphpos * 1.3 + 7.3);\n" \
"	float d = n - cut;\n" \
"	vec3 hot = kind > 1.5 ? vec3(0.5, 0.8, 2.4) : kind > 0.5 ? vec3(2.2, 1.4, 0.35) : vec3(2.4, 0.8, 0.15);\n" \
"	seam = hot * 0.8 * (1.0 - smoothstep(0.0, 0.03, abs(d)));\n" \
"	return in_morph > 0.0 ? d < 0.0 : d >= 0.0;\n" \
"}\n" \
"\n"

// parallax occlusion mapping on the skin's heights
#define QVR_ALIAS_FS_PARALLAX \
"	// QVR: parallax occlusion mapping (vr_parallax_models) on the skin's heights, in the triangle's own frame, with\n" \
"	// fewer steps than the world's; read with the surface's mip level (the moved coordinates jump at occlusions)\n" \
"	if (in_pdepth != 0.) // < 0: an authored map's heights (real steep relief: all the world's steps)\n" \
"	{\n" \
"		vec3 n = normalize(cross(dpdx, dpdy));\n" \
"		n = dot(n, in_pos) > 0. ? -n : n; // facing the eye (in_pos is from it)\n" \
"		// sooner out towards the outlines than on the world (from 50 to 70 degrees off the triangle; an authored map's real,\n" \
"		// steep relief from 37 to 60): a model's\n" \
"		// curved sides are seen at grazing angles all round, where the skin would slide the most\n" \
"		float pdepth = abs(in_pdepth) * (in_pdepth < 0. ? smoothstep(0.5, 0.8, -dot(normalize(in_pos), n)) : smoothstep(0.34, 0.64, -dot(normalize(in_pos), n)));\n" \
"		if (pdepth > 0.)\n" \
"			uv = ParallaxUV(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, n, in_pos, pdepth, in_pdepth < 0. ? max(Parallax.z, 8.0) : max(Parallax.z * 0.5, 4.0), vec4(0.));\n" \
"	}\n"

// wounds, normal maps, occlusion, the model's light
#define QVR_ALIAS_FS_LIGHT \
"	Wounds wounds = WoundsAt(uv, result.rgb); // QVR: wounds painted on the model (vr/vr_wounds.cpp)\n" \
"	result.rgb = wounds.look;\n" \
"#if ALPHATEST\n" \
"	vec3 emissive = vec3(0.); // QVR\n" \
"#else\n" \
"	vec3 emissive = result.rgb * (1.0 - result.a); // QVR: fullbright texels, unlit (ALPHABRIGHT skins keep them in the alpha)\n" \
"#endif\n" \
"	// QVR: the normal bent by the skin's normal map (vr_normalmaps), for the model's own light and the dynamic lights\n" \
"	vec3 n = normalize(in_nor);\n" \
"	float bumpk = instances[in_instance].AO.z; // QVR: its normal map's strength: a made one's LightTweak.w, an authored one's own (vr_normalmap_authored)\n" \
"	vec3 bumped = bumpk > 0. && (in_bumplight.w > 0. || NumLights > 0u || instances[in_instance].Ambient[0].w > 0.) ? BumpedNormalK(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, n, bumpk) : n;\n" \
"	if (wounds.flatten > 0.) // QVR: blood and water fill the bumps\n" \
"		bumped = normalize(mix(bumped, n, wounds.flatten));\n" \
"#if ALPHATEST\n" \
"	SpecularAA(0.0); // QVR: the normal map's spread only (no derivatives after the discard)\n" \
"#else\n" \
"	SpecularAA(Parallax.w > 0. ? NormalSpread(bumped) : 0.0); // QVR: the sheen's lobe widened by the bumps and curves under the pixel (vr_specular_aa)\n" \
"#endif\n" \
"	vec3 skin = result.rgb; // QVR: for the rim light and reflections\n" \
"	vec3 spec; // QVR\n" \
"	// QVR: ambient occlusion (vr/vr_ao.cpp): the model's own and that of what moves near it darken its own light;\n" \
"	// dynamic lights get half of its own (in the log: its square root), shadows stand for the rest\n" \
"	float occlusion = in_vao * DynamicAO(in_pos + EyePos, AONormal(n, in_bumplight.xyz), instances[in_instance].AO.x, in_coord, in_depth);\n" \
"	vec3 light = in_color.rgb * ModelBumpShade(n, bumped) * ModelAmbient(n, bumped) * occlusion + ModelDynamicLights(n, bumped, spec) * sqrt(in_vao); // QVR\n"

// fullbrights, wounds' glow, force grab's glow, morphs
#define QVR_ALIAS_FS_GLOW \
"	// QVR: the held weapons' dim fullbrights (sights) shine brighter, bright ones stay (vr/vr_emissive.cpp)\n" \
"	fullbright *= 1.0 - max(wounds.blood, wounds.burn); // QVR: covered by a wound\n" \
"	vec3 glowing = fullbright + emissive;\n" \
"	result.rgb += fullbright + glowing * (in_fbboost * (1.0 - max(glowing.r, max(glowing.g, glowing.b))));\n" \
"	result.rgb += wounds.glow; // QVR: a fresh burn's embers\n" \
"	if (in_glow != 0.) // QVR: force grab's glow round the edges (vr/vr_fgfx.cpp); negative, the counter's (vr/vr_meleehud.cpp)\n" \
"	{\n" \
"		float rim = 1.0 - abs(dot(normalize(in_nor), normalize(-in_pos)));\n" \
"		vec3 glowColor = in_glow > 0. ? SceneTone.yzw : vec3(1.0, 0.66, 0.2); // QVR: the player's hue; the counter's gold\n" \
"		result.rgb += glowColor * abs(in_glow) * (pow(rim, 2.0) * 1.1 + (in_glow > 0. ? 0.08 : 0.12));\n" \
"	}\n" \
"	result.rgb += morphSeam; // QVR: a morph's glowing seam\n" \
"	if (!morphShown) // QVR: this model is not there yet (or any more) in a morph\n" \
"		discard;\n"

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
