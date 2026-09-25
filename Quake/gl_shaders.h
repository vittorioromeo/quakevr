/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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

////////////////////////////////////////////////////////////////
//
// Compile-time options
//
////////////////////////////////////////////////////////////////

#define LINEAR_SPACE_OIT			0
#define SHOW_ACTIVE_LIGHT_CLUSTERS	0
#define SHOW_WORLD_NORMALS			0

////////////////////////////////////////////////////////////////
//
// GUI
//
////////////////////////////////////////////////////////////////

static const char gui_vertex_shader[] =
"layout(location=0) in vec2 in_pos;\n"
"layout(location=1) in vec2 in_uv;\n"
"layout(location=2) in vec4 in_color;\n"
"\n"
"layout(location=0) centroid out vec2 out_uv;\n"
"layout(location=1) centroid out vec4 out_color;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = vec4(in_pos, 0.0, 1.0);\n"
"	out_uv = in_uv;\n"
"	out_color = in_color;\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char gui_fragment_shader[] =
"layout(binding=0) uniform sampler2D Tex;\n"
"\n"
"layout(location=0) centroid in vec2 in_uv;\n"
"layout(location=1) centroid in vec4 in_color;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	out_fragcolor = texture(Tex, in_uv) * in_color;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// View blend
//
////////////////////////////////////////////////////////////////

static const char viewblend_vertex_shader[] =
"void main()\n"
"{\n"
"	ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
"	gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char viewblend_fragment_shader[] =
"layout(location=0) uniform vec4 Color;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	out_fragcolor = Color;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// View warp/scale
//
////////////////////////////////////////////////////////////////

static const char warpscale_vertex_shader[] =
"layout(location=0) out vec2 out_uv;\n"
"\n"
"void main()\n"
"{\n"
"	ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
"	out_uv = vec2(v) * 2.0;\n"
"	gl_Position = vec4(out_uv * 2.0 - 1.0, 0.0, 1.0);\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char warpscale_fragment_shader[] =
"layout(binding=0) uniform sampler2D Tex;\n"
"\n"
"layout(location=0) uniform vec4 UVScaleWarpTime; // xy=Scale z=Warp w=Time\n"
"layout(location=1) uniform vec4 BlendColor;\n"
"\n"
"layout(location=0) in vec2 in_uv;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	vec2 uv = in_uv;\n"
"	vec2 uv_scale = UVScaleWarpTime.xy;\n"
"\n"
"#if WARP\n"
"	float time = UVScaleWarpTime.w;\n"
"	float aspect = dFdy(uv.y) / dFdx(uv.x);\n"
"	vec2 warp_amp = UVScaleWarpTime.zz;\n"
"	warp_amp.y *= aspect;\n"
"	uv = warp_amp + uv * (1.0 - 2.0 * warp_amp); // remap to safe area\n"
"	uv += warp_amp * sin(vec2(uv.y / aspect, uv.x) * (3.14159265 * 8.0) + time);\n"
"#endif // WARP\n"
"\n"
"	out_fragcolor = texture(Tex, uv * uv_scale);\n"
"	out_fragcolor.rgb = mix(out_fragcolor.rgb, BlendColor.rgb, BlendColor.a);\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Postprocess (dithering, palettization, gamma/contrast)
//
////////////////////////////////////////////////////////////////

#define PALETTE_BUFFER \
"layout(std430, binding=0) restrict readonly buffer PaletteBuffer\n"\
"{\n"\
"	uint Palette[256];\n"\
"};\n"\
"\n"\
"uvec3 UnpackRGB8(uint c)\n"\
"{\n"\
"	return uvec3(c, c >> 8, c >> 16) & 255u;\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define NOISE_FUNCTIONS \
"// ALU-only 16x16 Bayer matrix\n"\
"float bayer01(ivec2 coord)\n"\
"{\n"\
"	coord &= 15;\n"\
"	coord.y ^= coord.x;\n"\
"	uint v = uint(coord.y | (coord.x << 8));	// 0  0  0  0 | x3 x2 x1 x0 |  0  0  0  0 | y3 y2 y1 y0\n"\
"	v = (v ^ (v << 2)) & 0x3333;				// 0  0 x3 x2 |  0  0 x1 x0 |  0  0 y3 y2 |  0  0 y1 y0\n"\
"	v = (v ^ (v << 1)) & 0x5555;				// 0 x3  0 x2 |  0 x1  0 x0 |  0 y3  0 y2 |  0 y1  0 y0\n"\
"	v |= v >> 7;								// 0 x3  0 x2 |  0 x1  0 x0 | x3 y3 x2 y2 | x1 y1 x0 y0\n"\
"	v = bitfieldReverse(v) >> 24;				// 0  0  0  0 |  0  0  0  0 | y0 x0 y1 x1 | y2 x2 y3 x3\n"\
"	return float(v) * (1.0/256.0);\n"\
"}\n"\
"\n"\
"float bayer(ivec2 coord)\n"\
"{\n"\
"	return bayer01(coord) - 0.5;\n"\
"}\n"\
"\n"\
"// Hash without Sine\n"\
"// https://www.shadertoy.com/view/4djSRW \n"\
/* Copyright (c)2014 David Hoskins.\
\
Permission is hereby granted, free of charge, to any person obtaining a copy\
of this software and associated documentation files (the "Software"), to deal\
in the Software without restriction, including without limitation the rights\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\
copies of the Software, and to permit persons to whom the Software is\
furnished to do so, subject to the following conditions:\
\
The above copyright notice and this permission notice shall be included in all\
copies or substantial portions of the Software.\
\
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\
SOFTWARE.*/\
"float whitenoise01(vec2 p)\n"\
"{\n"\
"	vec3 p3 = fract(vec3(p.xyx) * .1031);\n"\
"	p3 += dot(p3, p3.yzx + 33.33);\n"\
"	return fract((p3.x + p3.y) * p3.z);\n"\
"}\n"\
"\n"\
"float whitenoise(vec2 p)\n"\
"{\n"\
"	return whitenoise01(p) - 0.5;\n"\
"}\n"\
"\n"\
"// Convert uniform distribution to triangle-shaped distribution\n"\
"// Input in [0..1], output in [-1..1]\n"\
"// Based on https://www.shadertoy.com/view/4t2SDh \n"\
"float tri(float x)\n"\
"{\n"\
"	float orig = x * 2.0 - 1.0;\n"\
"	uint signbit = floatBitsToUint(orig) & 0x80000000u;\n"\
"	x = sqrt(abs(orig)) - 1.;\n"\
"	x = uintBitsToFloat(floatBitsToUint(x) ^ signbit);\n"\
"	return x;\n"\
"}\n"\
"\n"\
"#define DITHER_NOISE(uv) tri(bayer01(ivec2(uv)))\n"\
"#define SCREEN_SPACE_NOISE() DITHER_NOISE(floor(gl_FragCoord.xy)+0.5)\n"\
"#define SUPPRESS_BANDING() bayer(ivec2(gl_FragCoord.xy))\n"\

////////////////////////////////////////////////////////////////

static const char postprocess_vertex_shader[] =
"void main()\n"
"{\n"
"	ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
"	gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char postprocess_fragment_shader[] =
"layout(binding=0) uniform sampler2D GammaTexture;\n"
"layout(binding=1) uniform usampler3D PaletteLUT;\n"
"\n"
PALETTE_BUFFER
NOISE_FUNCTIONS
"\n"
"layout(location=0) uniform vec4 Params;\n"
"layout(binding=2) uniform sampler2D BloomTexture; // QVR: an eye's glow (vr_bloom.cpp)\n"
"layout(location=1) uniform float BloomStrength; // QVR\n"
"layout(binding=6) uniform sampler2D WaterScene; // QVR: GammaTexture read smoothly, under water (vr/vr_water.cpp)\n"
"layout(location=2) uniform vec4 WaterParams; // QVR: time, wobble and blur (of the height; both 0: not under water)\n"
"layout(location=3) uniform vec4 WaterProj; // QVR: the projection: ndc x = x + y * left / forward, ndc y = z + w * up / forward\n"
"layout(location=4) uniform vec3 WaterFwd; // QVR: the eye's axes in the world\n"
"layout(location=5) uniform vec3 WaterLeft; // QVR\n"
"layout(location=6) uniform vec3 WaterUp; // QVR\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	float gamma = Params.x;\n"
"	float contrast = Params.y;\n"
"	float scale = Params.z;\n"
"	float dither = Params.w;\n"
"	out_fragcolor = texelFetch(GammaTexture, ivec2(gl_FragCoord), 0);\n"
"	vec2 buv = gl_FragCoord.xy / vec2(textureSize(GammaTexture, 0)); // QVR\n"
"	if (WaterParams.y + WaterParams.z > 0.0) // QVR: under water: a slow wobble of the direction looked in, in the world\n"
"	{ // (the same in both eyes, fixed to the world as the head turns), and a little blur\n"
"		vec2 size = vec2(textureSize(GammaTexture, 0));\n"
"		vec2 ndc = buv * 2.0 - 1.0;\n"
"		vec3 dir = normalize(WaterFwd + WaterLeft * ((ndc.x - WaterProj.x) / WaterProj.y) + WaterUp * ((ndc.y - WaterProj.z) / WaterProj.w));\n"
"		float t = WaterParams.x;\n"
"		vec2 w = vec2(sin(dot(dir, vec3(6.1, 2.3, 4.7)) + t * 1.1) + 0.5 * sin(dot(dir, vec3(-3.7, 8.3, 2.9)) - t * 1.6),\n"
"			sin(dot(dir, vec3(2.9, -5.9, 6.3)) + t * 0.9) + 0.5 * sin(dot(dir, vec3(7.7, 1.9, -4.9)) + t * 1.4));\n"
"		vec2 aspect = vec2(size.y / size.x, 1.0);\n"
"		buv = clamp(buv + w * (WaterParams.y * aspect), vec2(0.0), vec2(1.0));\n"
"		vec2 b = max(WaterParams.z, 0.5 / size.y) * aspect;\n"
"		out_fragcolor.rgb = (texture(WaterScene, buv + b).rgb + texture(WaterScene, buv - b).rgb +\n"
"			texture(WaterScene, buv + vec2(b.x, -b.y)).rgb + texture(WaterScene, buv + vec2(-b.x, b.y)).rgb) * 0.25;\n"
"	}\n"
"	if (BloomStrength > 0.0) // QVR: smoothed up from a quarter of the size by four bilinear taps\n"
"	{\n"
"		vec2 bt = 0.5 / vec2(textureSize(BloomTexture, 0));\n"
"		out_fragcolor.rgb += (texture(BloomTexture, buv + vec2(-bt.x, -bt.y)).rgb + texture(BloomTexture, buv + vec2(bt.x, -bt.y)).rgb +\n"
"			texture(BloomTexture, buv + vec2(-bt.x, bt.y)).rgb + texture(BloomTexture, buv + vec2(bt.x, bt.y)).rgb) * (0.25 * BloomStrength);\n"
"	}\n"
"#if PALETTIZE == 1\n"
"	vec2 noiseuv = floor(gl_FragCoord.xy * scale) + 0.5;\n"
"	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"	out_fragcolor.rgb += DITHER_NOISE(noiseuv) * dither;\n"
"	out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"#endif // PALETTIZE == 1\n"
"#if PALETTIZE\n"
"	ivec3 clr = ivec3(clamp(out_fragcolor.rgb, 0., 1.) * 127. + 0.5);\n"
"	uint remap = Palette[texelFetch(PaletteLUT, clr, 0).x];\n"
"	out_fragcolor.rgb = vec3(UnpackRGB8(remap)) * (1./255.);\n"
"#else\n"
"	out_fragcolor.rgb *= contrast;\n"
"	out_fragcolor = vec4(pow(out_fragcolor.rgb, vec3(gamma)), 1.0);\n"
"#endif // PALETTIZE\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Common shader snippets
//
////////////////////////////////////////////////////////////////

#define FRAMEDATA_BUFFER \
"layout(std140, binding=0) uniform FrameDataUBO\n"\
"{\n"\
"	mat4	ViewProj;\n"\
"	vec4	Fog;\n"\
"	vec4	SkyFog;\n"\
"	vec3	WindDir;\n"\
"	float	WindPhase;\n"\
"	float	ScreenDither;\n"\
"	float	TextureDither;\n"\
"	float	ShadowBias; // QVR: vr/vr_lighting.cpp\n"\
"	float	DlightAngle; // QVR\n"\
"	vec3	EyePos;\n"\
"	float	Time;\n"\
"	float	ZLogScale;\n"\
"	float	ZLogBias;\n"\
"	uint	NumLights;\n"\
"	uint	ShadowFlags; // QVR\n"\
"	vec4	LightTweak; // QVR: lightmap contrast, the normal maps' share of the baked light, specular intensity, normal map strength\n"\
"	vec4	Parallax; // QVR: parallax mapping: depth in units (0 off), the distance it ends at, the most steps, unused\n"\
"	vec4	Water; // QVR: liquids (vr/vr_water.cpp): waves, fresnel, refraction (0: no scene to read), glints\n"\
"	vec4	Water2; // QVR: lava glow, caustics (0 off), the eye in a liquid (1), unused\n"\
"	vec4	CausticsOrigin; // QVR: xyz where the liquid volume (LiquidVolume) starts, in the world; w its cell size\n"\
"	vec4	CausticsScale; // QVR: xyz one over its size in units\n"\
"};\n"\
"\n"\
"vec3 ApplyFog(vec3 clr, vec3 p)\n"\
"{\n"\
"	float fog = exp2(-Fog.w * dot(p, p));\n"\
"	fog = clamp(fog, 0.0, 1.0);\n"\
"	return mix(Fog.rgb, clr, fog);\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define LIGHT_BUFFER \
"#define LIGHT_TILES_X " QS_STRINGIFY (LIGHT_TILES_X) "\n"\
"#define LIGHT_TILES_Y " QS_STRINGIFY (LIGHT_TILES_Y) "\n"\
"#define LIGHT_TILES_Z " QS_STRINGIFY (LIGHT_TILES_Z) "\n"\
"#define MAX_LIGHTS    " QS_STRINGIFY (MAX_DLIGHTS)   "\n"\
"\n"\
"struct Light\n"\
"{\n"\
"	vec3	origin;\n"\
"	float	radius;\n"\
"	vec3	color;\n"\
"	float	minlight;\n"\
"	vec4	shadow; // QVR: vr/vr_lighting.cpp\n"\
"	vec4	shadow2; // QVR\n"\
"	vec4	spot; // QVR: a spot light's cone (zero: a point light)\n"\
"};\n"\
"\n"\
"layout(std430, binding=0) restrict readonly buffer LightBuffer\n"\
"{\n"\
"	float	LightStyles[" QS_STRINGIFY (MAX_LIGHTSTYLES) "];\n"\
"	Light	Lights[];\n"\
"};\n"\
"\n"\
"float GetLightStyle(int index)\n"\
"{\n"\
"	float result;\n"\
"	if (index < " QS_STRINGIFY (MAX_LIGHTSTYLES) ")\n"\
"		result = LightStyles[index];\n"\
"	else\n"\
"		result = 1.0;\n"\
"	return result;\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define LIGHT_CLUSTER_IMAGE(mode) \
"layout(rg32ui, binding=0) uniform " mode " uimage3D LightClusters;\n"\

////////////////////////////////////////////////////////////////

// QVR: the frame data the alias shaders read (vr/vr_lighting.cpp); its own names, since the alias
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
"};\n"\
"\n"\

// QVR: shadows and per-pixel light (vr/vr_lighting.cpp). Needs the frame data and LIGHT_BUFFER.
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
"const vec3 ShadowRight[6] = vec3[6](vec3(0.,-1.,0.), vec3(0.,1.,0.), vec3(1.,0.,0.), vec3(-1.,0.,0.), vec3(0.,1.,0.), vec3(0.,-1.,0.));\n"\
"const vec3 ShadowUp[6] = vec3[6](vec3(0.,0.,1.), vec3(0.,0.,1.), vec3(0.,0.,1.), vec3(0.,0.,1.), vec3(1.,0.,0.), vec3(1.,0.,0.));\n"\
"\n"\
"float ShadowTap(sampler2DShadow atlas, vec2 texel, vec2 inv, float ref)\n"\
"{\n"\
"	return texture(atlas, vec3(texel * inv, ref));\n"\
"}\n"\
"\n"\
"// The filtered compare at texel (atlas texels) against the reversed depth ref: 1, 4, 9 or 16 taps.\n"\
"float ShadowFilter(sampler2DShadow atlas, vec2 texel, float ref)\n"\
"{\n"\
"	vec2 inv = 1.0 / vec2(textureSize(atlas, 0));\n"\
"	uint filt = ShadowFlags & 3u;\n"\
"	if (filt == 0u)\n"\
"		return ShadowTap(atlas, texel, inv, ref);\n"\
"	if (filt == 1u)\n"\
"		return 0.25 * (ShadowTap(atlas, texel + vec2(-0.5, -0.5), inv, ref) + ShadowTap(atlas, texel + vec2(0.5, -0.5), inv, ref)\n"\
"			+ ShadowTap(atlas, texel + vec2(-0.5, 0.5), inv, ref) + ShadowTap(atlas, texel + vec2(0.5, 0.5), inv, ref));\n"\
"	float r = filt == 2u ? 1.0 : 1.5;\n"\
"	float sum = 0.;\n"\
"	for (float y = -r; y <= r + 0.01; y += 1.0)\n"\
"		for (float x = -r; x <= r + 0.01; x += 1.0)\n"\
"			sum += ShadowTap(atlas, texel + vec2(x, y), inv, ref);\n"\
"	return sum / ((2.0 * r + 1.0) * (2.0 * r + 1.0));\n"\
"}\n"\
"\n"\
"// How much of a light reaches the point d away from it, 0..1, from its faces at tile (xy origin, z face size).\n"\
"float ShadowLookup(sampler2DShadow atlas, vec3 tile, vec3 d)\n"\
"{\n"\
"	vec3 a = abs(d);\n"\
"	int face;\n"\
"	float z;\n"\
"	if (a.x >= a.y && a.x >= a.z) { face = d.x > 0. ? 0 : 1; z = a.x; }\n"\
"	else if (a.y >= a.z) { face = d.y > 0. ? 2 : 3; z = a.y; }\n"\
"	else { face = d.z > 0. ? 4 : 5; z = a.z; }\n"\
"	z = max(z, SHADOW_NEAR);\n"\
"	float size = tile.z;\n"\
"	float k = 1.0 - 2.0 * SHADOW_BORDER / size;\n"\
"	vec2 st = vec2(dot(d, ShadowRight[face]), dot(d, ShadowUp[face])) / z;\n"\
"	vec2 texel = tile.xy + vec2(float(face % 3), float(face / 3)) * size + (st * (0.5 * k) + 0.5) * size;\n"\
"	return ShadowFilter(atlas, texel, SHADOW_NEAR / z * (1.0 + 0.002 * ShadowBias));\n"\
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
"// A dynamic light's sheen towards the eye (DarkPlaces' r_shadow_gloss 2): Blinn's, exponent 32, LightTweak.z strong.\n"\
"float LightSpecular(Light l, vec3 pos, vec3 n, vec3 eye)\n"\
"{\n"\
"	if (LightTweak.z <= 0.)\n"\
"		return 0.0;\n"\
"	vec3 dir = normalize(l.origin - pos);\n"\
"	if (dot(n, dir) <= 0.)\n"\
"		return 0.0;\n"\
"	vec3 h = normalize(dir + normalize(eye - pos));\n"\
"	return pow(max(dot(n, h), 0.0), 32.0) * LightTweak.z;\n"\
"}\n"\
"\n"\
"// The normal n bent by a normal map (tangent space, green up; LightTweak.w deepens it), in the frame the texture\n"\
"// lies in on the surface: a cotangent frame from the derivatives of the position and texture coordinates, exact on\n"\
"// the world's flat faces, the same in both eyes. The derivatives come from the caller (taken before any discard).\n"\
"vec3 BumpedNormal(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n)\n"\
"{\n"\
"	vec3 dp2perp = cross(dpdy, n);\n"\
"	vec3 dp1perp = cross(n, dpdx);\n"\
"	vec3 t = dp2perp * duvdx.x + dp1perp * duvdy.x;\n"\
"	vec3 b = dp2perp * duvdx.y + dp1perp * duvdy.y;\n"\
"	float k = inversesqrt(max(max(dot(t, t), dot(b, b)), 1e-24));\n"\
"	vec2 m = textureGrad(tex, uv, duvdx, duvdy).xy * 2.0 - 1.0; // x and y (RG8): z makes it unit length\n"\
"	float z = sqrt(max(1.0 - dot(m, m), 0.0025));\n"\
"	m *= LightTweak.w;\n"\
"	return normalize((t * m.x - b * m.y) * k + n * z);\n"\
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
"	vec3 o = ShadowOffset(d, n, l.shadow.z);\n"\
"	float world = ShadowLookup(ShadowStatic, vec3(l.shadow2.xy, l.shadow.z), o);\n"\
"	if (world <= 0.)\n"\
"		return 1.0;\n"\
"	float blocked = world * (1.0 - ShadowLookup(ShadowAtlas, l.shadow.xyz, o));\n"\
"	float lum = max(lit.r, max(lit.g, lit.b));\n"\
"	return 1.0 - clamp(given / max(lum, 1e-3), 0.0, 1.0) * blocked * l.color.x;\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// QVR: parallax occlusion mapping (vr_parallax): the texture coordinates where the ray from the eye through this
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

#define DRAW_ELEMENTS_INDIRECT_COMMAND \
"struct DrawElementsIndirectCommand\n"\
"{\n"\
"	uint	count;\n"\
"	uint	instanceCount;\n"\
"	uint	firstIndex;\n"\
"	uint	baseVertex;\n"\
"	uint	baseInstance;\n"\
"};\n"\

////////////////////////////////////////////////////////////////

#define WORLD_DRAW_BUFFER \
DRAW_ELEMENTS_INDIRECT_COMMAND \
"\n"\
"layout(std430, binding=1) buffer DrawIndirectBuffer\n"\
"{\n"\
"	DrawElementsIndirectCommand cmds[];\n"\
"};\n"\

////////////////////////////////////////////////////////////////

#define WORLD_CALLDATA_BUFFER \
"struct Call\n"\
"{\n"\
"	uint	flags;\n"\
"	float	wateralpha;\n"\
"#if BINDLESS\n"\
"	uvec2	txhandle;\n"\
"	uvec2	fbhandle;\n"\
"	uvec2	nmhandle; // QVR: the normal map (vr_normalmaps)\n"\
"#else\n"\
"	int		baseinstance;\n"\
"	int		padding;\n"\
"#endif // BINDLESS\n"\
"	vec4	uvclamp; // QVR: parallax mapping's rays stay in s .x to .z, t .y to .w, repeating (none where .z <= .x)\n"\
"};\n"\
"const uint\n"\
"	CF_USE_POLYGON_OFFSET = 1u,\n"\
"	CF_USE_FULLBRIGHT = 2u,\n"\
"	CF_NOLIGHTMAP = 4u\n"\
";\n"\
"\n"\
"layout(std430, binding=1) restrict readonly buffer CallBuffer\n"\
"{\n"\
"	Call call_data[];\n"\
"};\n"\
"\n"\
"#if BINDLESS\n"\
"	#define GET_INSTANCE_ID(call) (gl_BaseInstanceARB + gl_InstanceID)\n"\
"#else\n"\
"	#define GET_INSTANCE_ID(call) (call.baseinstance + gl_InstanceID)\n"\
"#endif\n"\

////////////////////////////////////////////////////////////////

#define WORLD_INSTANCEDATA_BUFFER \
"struct Instance\n"\
"{\n"\
"	vec4	mat[3];\n"\
"	float	alpha;\n"\
"	float	glow; // QVR: the force grab glow (vr/vr_fgfx.cpp)\n"\
"	float	parallax; // QVR: parallax mapping's depth in units (0 off)\n"\
"};\n"\
"\n"\
"layout(std430, binding=2) restrict readonly buffer InstanceBuffer\n"\
"{\n"\
"	Instance instance_data[];\n"\
"};\n"\
"\n"\
"vec3 Transform(vec3 p, Instance instance)\n"\
"{\n"\
"	mat4x3 world = transpose(mat3x4(instance.mat[0], instance.mat[1], instance.mat[2]));\n"\
"	return mat3(world[0], world[1], world[2]) * p + world[3];\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define WORLD_VERTEX_BUFFER \
"layout(location=0) in vec3 in_pos;\n"\
"layout(location=1) in vec4 in_uv;\n"\
"layout(location=2) in float in_lmofs;\n"\
"layout(location=3) in ivec4 in_styles;\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define BINDLESS_VERTEX_HEADER \
"#if BINDLESS\n"\
"	#extension GL_ARB_shader_draw_parameters : require\n"\
"	#define DRAW_ID			gl_DrawIDARB\n"\
"#else\n"\
"	layout(location=0) uniform int DrawID;\n"\
"	#define DRAW_ID			DrawID\n"\
"#endif\n"\
"\n"\

////////////////////////////////////////////////////////////////

#define OIT_OUTPUT(output_name) \
"#define OUT_COLOR " QS_STRINGIFY (output_name) "\n"\
"#if OIT\n"\
"	vec4 OUT_COLOR;\n"\
"	layout(location=0) out vec4 out_accum;\n"\
"	layout(location=1) out float out_reveal;\n"\
"\n"\
"	vec3 GammaToLinear(vec3 v)\n"\
"	{\n"\
"#if " QS_STRINGIFY (LINEAR_SPACE_OIT) "\n"\
"		return v*v;\n"\
"#else\n"\
"		return v;\n"\
"#endif\n"\
"	}\n"\
"\n"\
"	void main_body();\n"\
"\n"\
"	void main()\n"\
"	{\n"\
"		main_body();\n"\
"		OUT_COLOR = clamp(OUT_COLOR, 0.0, 1.0);\n"\
"		vec4 color = vec4(GammaToLinear(OUT_COLOR.rgb), OUT_COLOR.a);\n"\
"		float z = 1./gl_FragCoord.w;\n"\
"#if " QS_STRINGIFY (LINEAR_SPACE_OIT) "\n"\
"		float weight = clamp(color.a * color.a * 0.03 / (1e-5 + pow(z/2e5, 2.0)), 1e-2, 3e3);\n"\
"#else\n"\
"		float weight = clamp(color.a * color.a * 0.03 / (1e-5 + pow(z/1e7, 1.0)), 1e-2, 3e3);\n"\
"#endif\n"\
"		out_accum = vec4(color.rgb, color.a * weight);\n"\
"		out_accum.rgb *= out_accum.a;\n"\
"		out_reveal = color.a;\n"\
"	}\n"\
"\n"\
"	#define main main_body\n"\
"#else\n"\
"	layout(location=0) out vec4 OUT_COLOR;\n"\
"#endif // OIT\n"\

////////////////////////////////////////////////////////////////

// QVR: liquids (vr/vr_water.cpp): waves, fresnel, glints, refraction, lava's glow, and caustics on what is under water.
// Everything is a function of the world position and the time (the same in both eyes) and of the view vector. Needs
// the frame data. The kind is in a call's flags (bits 3-5): 1 lava, 2 slime, 3 teleport, 4 water.
#define LIQUID_FUNCTIONS \
"layout(binding=6) uniform sampler2D LiquidScene; // the opaque scene, while translucent liquids draw into the OIT buffers\n"\
"layout(binding=7) uniform sampler3D LiquidVolume; // where the water and slime are (1), a cell round them into walls\n"\
"\n"\
"uint LiquidKind(uint flags)\n"\
"{\n"\
"	return (flags >> 3) & 7u;\n"\
"}\n"\
"\n"\
"// Quake's warp of the liquids' texture coordinates; lava's slower with the waves on.\n"\
"vec2 LiquidWarp(vec2 uv, uint kind)\n"\
"{\n"\
"	float t = kind == 1u && Water.x > 0. ? Time * 0.35 : Time;\n"\
"	return uv * 2.0 + 0.125 * sin(uv.yx * (3.14159265 * 2.0) + t);\n"\
"}\n"\
"\n"\
"// The waves: a sum of sines over the plane with normal n, in world units along the world axes the plane lies in (t, b).\n"\
"// xy the height's gradient along t and b, z the height. Lava's are long and slow, slime's slower than water's.\n"\
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
"	return r * Water.x;\n"\
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
"		f = 0.02 + 0.98 * pow(1.0 - cosv, 3.0);\n"\
"		env = l * (kind == 2u ? vec3(0.16, 0.22, 0.11) : vec3(0.22, 0.26, 0.3));\n"\
"	}\n"\
"	fresnel = f * Water.y;\n"\
"	vec3 c = mix(lit, env, fresnel);\n"\
"	vec3 hv = normalize(normalize(facing + vec3(0.25, 0.15, 0.0)) + v);\n"\
"	float nh = max(dot(n, hv), 0.0);\n"\
"	float glint = (pow(nh, 200.0) * 2.0 + pow(nh, 24.0) * 0.08) * (0.15 + 0.85 * f) * (1.0 - Water2.z);\n"\
"	c += glint * Water.w * l * (kind == 2u ? vec3(0.7, 1.0, 0.5) : vec3(1.0));\n"\
"	alpha = mix(alpha * (1.0 - 0.3 * Water.y), 1.0, fresnel);\n"\
"	if (kind == 2u)\n"\
"		alpha = mix(alpha, 1.0, 0.3 * Water.y); // murky\n"\
"	return c;\n"\
"}\n"\
"\n"\
"// A translucent liquid over what is behind it, that bent by the waves (n, facing as above): read from the opaque scene\n"\
"// (Water.z: how far, 0: nothing to read), the liquid then opaque over it. The bend is a shift in the world, projected.\n"\
"vec4 LiquidRefract(vec4 c, vec3 pos, vec3 n, vec3 facing, uint kind)\n"\
"{\n"\
"	if (Water.z <= 0. || c.a >= 1. || kind == 1u || kind == 3u)\n"\
"		return c;\n"\
"	float dist = distance(pos, EyePos);\n"\
"	vec3 d = (n - facing) * (Water.z * min(dist * 0.08, 12.0) * (kind == 2u ? 0.5 : 1.0));\n"\
"	vec4 c0 = ViewProj * vec4(pos, 1.0);\n"\
"	vec4 c1 = ViewProj * vec4(pos + d, 1.0);\n"\
"	vec2 uv = gl_FragCoord.xy / vec2(textureSize(LiquidScene, 0)) + (c1.xy / c1.w - c0.xy / c0.w) * 0.5;\n"\
"	vec3 behind = texture(LiquidScene, uv).rgb;\n"\
"	return vec4(mix(behind, c.rgb, c.a), 1.0);\n"\
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

////////////////////////////////////////////////////////////////
//
// World
//
////////////////////////////////////////////////////////////////

static const char world_vertex_shader[] =
BINDLESS_VERTEX_HEADER
FRAMEDATA_BUFFER
LIGHT_BUFFER
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
WORLD_VERTEX_BUFFER
"\n"
"layout(location=0) flat out uint out_flags;\n"
"layout(location=1) flat out float out_alpha;\n"
"layout(location=2) out vec3 out_pos;\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"	layout(location=3) centroid out vec2 out_uv;\n"
"#else\n"
"	layout(location=3) out vec2 out_uv;\n"
"#endif\n"
"layout(location=4) centroid out vec2 out_lmuv;\n"
"layout(location=5) out float out_depth;\n"
"layout(location=6) noperspective out vec2 out_coord;\n"
"layout(location=7) flat out vec4 out_styles;\n"
"layout(location=8) flat out float out_lmofs;\n"
"#if BINDLESS\n"
"	layout(location=9) flat out uvec4 out_samplers;\n"
"	layout(location=11) flat out uvec2 out_nmsampler; // QVR\n"
"#endif\n"
"layout(location=10) flat out float out_glow; // QVR\n"
"layout(location=12) flat out float out_pdepth; // QVR: parallax mapping\n"
"layout(location=13) flat out vec4 out_uvclamp; // QVR\n"
"\n"
"void main()\n"
"{\n"
"	Call call = call_data[DRAW_ID];\n"
"	int instance_id = GET_INSTANCE_ID(call);\n"
"	Instance instance = instance_data[instance_id];\n"
"	out_pos = Transform(in_pos, instance);\n"
"	gl_Position = ViewProj * vec4(out_pos, 1.0);\n"
"#if REVERSED_Z\n"
"	const float ZBIAS = -1./1024;\n"
"#else\n"
"	const float ZBIAS =  1./1024;\n"
"#endif\n"
"	if ((call.flags & CF_USE_POLYGON_OFFSET) != 0u)\n"
"		gl_Position.z += ZBIAS;\n"
"	out_uv = in_uv.xy;\n"
"	out_lmuv = in_uv.zw;\n"
"	out_depth = gl_Position.w;\n"
"	out_coord = (gl_Position.xy / gl_Position.w * 0.5 + 0.5) * vec2(LIGHT_TILES_X, LIGHT_TILES_Y);\n"
"	out_flags = call.flags;\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	out_alpha = instance.alpha < 0.0 ? call.wateralpha : instance.alpha;\n"
"#else\n"
"	out_alpha = instance.alpha < 0.0 ? 1.0 : instance.alpha;\n"
"#endif\n"
"	out_glow = instance.glow; // QVR\n"
"	out_pdepth = instance.parallax; // QVR\n"
"	out_uvclamp = call.uvclamp; // QVR\n"
"	out_styles.x = GetLightStyle(in_styles.x);\n"
"	if (in_styles.y == 255)\n"
"		out_styles.yzw = vec3(-1.);\n"
"	else if (in_styles.z == 255)\n"
"		out_styles.yzw = vec3(GetLightStyle(in_styles.y), -1., -1.);\n"
"	else\n"
"		out_styles.yzw = vec3\n"
"		(\n"
"			GetLightStyle(in_styles.y),\n"
"			GetLightStyle(in_styles.z),\n"
"			GetLightStyle(in_styles.w)\n"
"		);\n"
"	if ((call.flags & CF_NOLIGHTMAP) != 0u)\n"
"		out_styles.xy = vec2(1., -1.);\n"
"	out_lmofs = in_lmofs;\n"
"#if BINDLESS\n"
"	out_samplers.xy = call.txhandle;\n"
"	if ((call.flags & CF_USE_FULLBRIGHT) != 0u)\n"
"		out_samplers.zw = call.fbhandle;\n"
"	else\n"
"		out_samplers.zw = out_samplers.xy;\n"
"	out_nmsampler = call.nmhandle; // QVR\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char world_fragment_shader[] =
"#if BINDLESS\n"
"	#extension GL_ARB_bindless_texture : require\n"
"#else\n"
"	layout(binding=0) uniform sampler2D Tex;\n"
"	layout(binding=1) uniform sampler2D FullbrightTex;\n"
"	layout(binding=3) uniform sampler2D NormalTex; // QVR\n"
"#endif\n"
"layout(binding=2) uniform sampler2D LMTex;\n"
"\n"
FRAMEDATA_BUFFER
LIGHT_BUFFER
LIGHT_CLUSTER_IMAGE("readonly")
SHADOW_FUNCTIONS // QVR
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
NOISE_FUNCTIONS
LIQUID_FUNCTIONS // QVR
"\n"
"layout(location=0) flat in uint in_flags;\n"
"layout(location=1) flat in float in_alpha;\n"
"layout(location=2) in vec3 in_pos;\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"	layout(location=3) centroid in vec2 in_uv;\n"
"#else\n"
"	layout(location=3) in vec2 in_uv;\n"
"#endif\n"
"layout(location=4) centroid in vec2 in_lmuv;\n"
"layout(location=5) in float in_depth;\n"
"layout(location=6) noperspective in vec2 in_coord;\n"
"layout(location=7) flat in vec4 in_styles;\n"
"layout(location=8) flat in float in_lmofs;\n"
"#if BINDLESS\n"
"	layout(location=9) flat in uvec4 in_samplers;\n"
"	layout(location=11) flat in uvec2 in_nmsampler; // QVR\n"
"#endif\n"
"layout(location=10) flat in float in_glow; // QVR\n"
"layout(location=12) flat in float in_pdepth; // QVR: parallax mapping\n"
"layout(location=13) flat in vec4 in_uvclamp; // QVR\n"
"\n"
OIT_OUTPUT (out_fragcolor)
"\n"
PARALLAX_FUNCTIONS // QVR
"void main()\n"
"{\n"
"#if " QS_STRINGIFY (SHOW_WORLD_NORMALS) "\n"
"	out_fragcolor = vec4(0.5 + 0.5 * normalize(cross(dFdx(in_pos), dFdy(in_pos))), 0.75);\n"
"	return;\n"
"#endif\n"
"	vec3 fullbright = vec3(0.);\n"
"	vec2 uv = in_uv;\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	uint liquid = LiquidKind(in_flags); // QVR: the waves (vr_water_waves) bend the texture a little\n"
"	vec3 waves = LiquidWaves(in_pos, cross(dFdx(in_pos), dFdy(in_pos)), liquid);\n"
"	uv = LiquidWarp(uv, liquid) + waves.xy * vec2(0.02, -0.02);\n"
"#endif\n"
"	vec3 dpdx = dFdx(in_pos), dpdy = dFdy(in_pos); // QVR: the surface's frame, for the normal map (before any discard)\n"
"	vec2 duvdx = dFdx(uv), duvdy = dFdy(uv);\n"
"	vec3 facing = normalize(cross(dpdx, dpdy)); // QVR: the surface's normal, towards the viewer\n"
"	facing = dot(facing, EyePos - in_pos) < 0. ? -facing : facing;\n"
"	vec3 specular_light = vec3(0.); // QVR: dynamic lights' sheen, added over the texture\n"
"#if BINDLESS\n"
"	sampler2D NormalTex = sampler2D(in_nmsampler); // QVR\n"
"	sampler2D Tex = sampler2D(in_samplers.xy);\n"
"	sampler2D FullbrightTex = sampler2D(in_samplers.zw); // the texture itself without a fullbright one\n"
"#endif\n"
"	// QVR: parallax occlusion mapping (vr_parallax) moves where the textures are read; their mip level is still\n"
"	// the surface's (the moved coordinates jump where the height field hides itself)\n"
"	vec2 puv = uv;\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_SOLID) "\n"
"	bool parallax = in_pdepth > 0.; // the instance's depth: the world's, an item box's (vr_parallax_items)\n"
"	if (parallax)\n"
"		puv = ParallaxUV(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, facing, in_pos - EyePos, in_pdepth, Parallax.z, in_uvclamp);\n"
"#else\n"
"	const bool parallax = false;\n"
"#endif\n"
"	if ((in_flags & CF_USE_FULLBRIGHT) != 0u)\n"
"		fullbright = parallax ? textureGrad(FullbrightTex, puv, duvdx, duvdy).rgb : texture(FullbrightTex, uv).rgb;\n"
"	vec4 result;\n"
"#if DITHER >= 2\n"
"	if (parallax)\n"
"		result = textureGrad(Tex, puv, duvdx * 0.5, duvdy * 0.5); // QVR: a mip bias of -1\n"
"	else\n"
"		result = texture(Tex, uv, -1.0);\n"
"#elif DITHER\n"
"	if (parallax)\n"
"		result = textureGrad(Tex, puv, duvdx * 0.70710678, duvdy * 0.70710678); // QVR: -0.5\n"
"	else\n"
"		result = texture(Tex, uv, -0.5);\n"
"#else\n"
"	if (parallax)\n"
"		result = textureGrad(Tex, puv, duvdx, duvdy); // QVR\n"
"	else\n"
"		result = texture(Tex, uv);\n"
"#endif\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"	// Note: for alpha-tested surfaces we need to compute the plane equation before discard is called,\n"
"	// otherwise we would get incorrect results for edge pixels due to invalid derivatives.\n"
"	vec4 plane;\n"
"	plane.xyz = normalize(cross(dFdx(in_pos), dFdy(in_pos)));\n"
"	plane.w = dot(in_pos, plane.xyz);\n"
"	if (result.a < 0.666)\n"
"		discard;\n"
"#endif\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	vec3 liquid_tex = result.rgb; // QVR: unlit (lava glows)\n"
"#endif\n"
"\n"
"	vec2 lmuv = in_lmuv;\n"
"#if DITHER\n"
"	vec2 lmsize = vec2(textureSize(LMTex, 0).xy) * 16.;\n"
"	lmuv = (floor(lmuv * lmsize) + 0.5) / lmsize;\n"
"#endif // DITHER\n"
"	vec4 lm0 = textureLod(LMTex, lmuv, 0.);\n"
"	vec3 total_light;\n"
"	if (in_styles.y < 0.) // single style fast path\n"
"		total_light = in_styles.x * lm0.xyz;\n"
"	else\n"
"	{\n"
"		vec4 lm1 = textureLod(LMTex, vec2(lmuv.x + in_lmofs, lmuv.y), 0.);\n"
"		if (in_styles.z < 0.) // 2 styles\n"
"		{\n"
"			total_light =\n"
"				in_styles.x * lm0.xyz +\n"
"				in_styles.y * lm1.xyz;\n"
"		}\n"
"		else // 3 or 4 lightstyles\n"
"		{\n"
"			vec4 lm2 = textureLod(LMTex, vec2(lmuv.x + in_lmofs * 2., lmuv.y), 0.);\n"
"			total_light = vec3\n"
"			(\n"
"				dot(in_styles, lm0),\n"
"				dot(in_styles, lm1),\n"
"				dot(in_styles, lm2)\n"
"			);\n"
"		}\n"
"	}\n"
"\n"
"	if (LightTweak.x != 1.) // QVR: contrast about Quake's full light (vr_light_contrast): darker shade, lamps as bright\n"
"		total_light = 0.5 * pow(max(total_light, vec3(0.)) * 2.0, vec3(LightTweak.x));\n"
"\n"
"	// QVR: the normal bent by the normal map (vr_normalmaps), for the baked light and the dynamic lights\n"
"	vec3 bumped = facing;\n"
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	if (LightTweak.w > 0.)\n"
"	{\n"
"		bumped = BumpedNormal(NormalTex, puv, duvdx, duvdy, dpdx, dpdy, facing);\n"
"		if (LightTweak.y > 0.)\n"
"		{\n"
"			float lum = dot(total_light, vec3(1.0 / 3.0));\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"			vec2 dlum = vec2(0.); // no derivatives after the discard\n"
"#else\n"
"			vec2 dlum = vec2(dFdx(lum), dFdy(lum));\n"
"#endif\n"
"			total_light *= BakedBump(facing, bumped, dpdx, dpdy, lum, dlum);\n"
"		}\n"
"	}\n"
"	total_light *= LiquidCaustics(in_pos, facing); // QVR: under water (vr_water_caustics)\n"
"#else\n"
"	bumped = LiquidNormal(waves, facing); // QVR: the waves', for the lights' glints\n"
"#endif\n"
"\n"
"	if (NumLights > 0u)\n"
"	{\n"
"		uint i, ofs;\n"
"		ivec3 cluster_coord;\n"
"		cluster_coord.x = int(floor(in_coord.x));\n"
"		cluster_coord.y = int(floor(in_coord.y));\n"
"		cluster_coord.z = int(floor(log2(in_depth) * ZLogScale + ZLogBias));\n"
"		uvec2 clusterdata = imageLoad(LightClusters, cluster_coord).xy;\n"
"		if ((clusterdata.x | clusterdata.y) != 0u)\n"
"		{\n"
"#if " QS_STRINGIFY (SHOW_ACTIVE_LIGHT_CLUSTERS) "\n"
"			int cluster_idx = cluster_coord.x + cluster_coord.y * LIGHT_TILES_X + cluster_coord.z * LIGHT_TILES_X * LIGHT_TILES_Y;\n"
"			total_light = vec3(ivec3((cluster_idx + 1) * 0x45d9f3b) >> ivec3(0, 8, 16) & 255) / 255.0;\n"
"#endif // SHOW_ACTIVE_LIGHT_CLUSTERS\n"
"#if MODE != " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"			// For non-alpha-tested geometry it is safe to defer the plane equation computation until it is actually needed\n"
"			vec4 plane;\n"
"			plane.xyz = normalize(cross(dFdx(in_pos), dFdy(in_pos)));\n"
"			plane.w = dot(in_pos, plane.xyz);\n"
"#endif\n"
"			vec3 dynamic_light = vec3(0.);\n"
"			bool darkplaces = (ShadowFlags & 16u) != 0u; // QVR\n"
"			for (i = 0u, ofs = 0u; i < 2u; i++, ofs += 32u)\n"
"			{\n"
"				uint mask = clusterdata[i];\n"
"				while (mask != 0u)\n"
"				{\n"
"					int j = findLSB(mask);\n"
"					mask ^= 1u << j;\n"
"					Light l = Lights[ofs + j];\n"
"					if (l.shadow.w != 0.) // QVR: a map light's shadow of moving things\n"
"					{\n"
"						total_light *= MapLightShadow(l, in_pos, facing, total_light);\n"
"						continue;\n"
"					}\n"
"					if (darkplaces) // QVR: DarkPlaces' falloff, colours brighter than 1 (vr_dlight_falloff)\n"
"					{\n"
"						float d = distance(l.origin, in_pos);\n"
"						if (d >= l.radius)\n"
"							continue;\n"
"						float lit = DarkPlacesAtten(d, l.radius) * LightShadow(l, in_pos, facing);\n"
"						if (lit <= 0.)\n"
"							continue;\n"
"						dynamic_light += lit * 0.5 * LightAngleDP(l, in_pos, bumped) * l.color; // halved: the lightmap is doubled below\n"
"						specular_light += lit * LightSpecular(l, in_pos, bumped, EyePos) * l.color;\n"
"						continue;\n"
"					}\n"
"					// mimics R_AddDynamicLights, up to a point\n"
"					float rad = l.radius;\n"
"					float dist = dot(l.origin, plane.xyz) - plane.w;\n"
"					rad -= abs(dist);\n"
"					float minlight = l.minlight;\n"
"					if (rad < minlight)\n"
"						continue;\n"
"					vec3 local_pos = l.origin - plane.xyz * dist;\n"
"					minlight = rad - minlight;\n"
"					dist = length(in_pos - local_pos);\n"
"					float add = clamp((minlight - dist) / 16.0, 0.0, 1.0) * max(0., rad - dist) / 256.;\n"
"					if (add <= 0.) // QVR\n"
"						continue;\n"
"					add *= LightShadow(l, in_pos, facing); // QVR\n"
"					specular_light += add * 2.0 * LightSpecular(l, in_pos, bumped, EyePos) * l.color; // QVR\n"
"					add *= LightAngle(l, in_pos, bumped, 0.0); // QVR\n"
"					dynamic_light += add * l.color;\n"
"				}\n"
"			}\n"
"			if ((ShadowFlags & 20u) != 0u) // QVR: uncapped (DarkPlaces' never are)\n"
"				total_light += dynamic_light;\n"
"			else\n"
"			total_light += max(min(dynamic_light, 1. - total_light), 0.);\n"
"		}\n"
"	}\n"
"#if DITHER >= 2\n"
"	total_light = floor(total_light * 63. + 0.5) * (2./63.);\n"
"#else\n"
"	total_light *= 2.0;\n"
"#endif\n"
"#if MODE != " QS_STRINGIFY (WORLDSHADER_ALPHATEST) "\n"
"	result.rgb = mix(result.rgb, result.rgb * total_light, result.a);\n"
"#else\n"
"	result.rgb *= total_light;\n"
"#endif\n"
"	result.rgb += specular_light; // QVR\n"
"	result.rgb += fullbright;\n"
"	if (in_glow > 0.) // QVR: force grab's glow (vr/vr_fgfx.cpp)\n"
"	{\n"
"		float rim = 1.0 - abs(dot(normalize(cross(dFdx(in_pos), dFdy(in_pos))), normalize(EyePos - in_pos)));\n"
"		result.rgb += vec3(0.35, 0.65, 1.0) * in_glow * (pow(rim, 2.0) * 1.1 + 0.12);\n"
"	}\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	float liquid_alpha = in_alpha; // QVR: the liquid's look (vr_water_*)\n"
"	result.rgb = LiquidShade(liquid_tex, result.rgb, total_light, in_pos, bumped, facing, waves.z, liquid, liquid_alpha);\n"
"#endif\n"
"	result = clamp(result, 0.0, 1.0);\n"
"	result.rgb = ApplyFog(result.rgb, in_pos - EyePos);\n"
"\n"
"	result.a = in_alpha; // FIXME: This will make almost transparent things cut holes though heavy fog\n"
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n"
"	result.a = liquid_alpha; // QVR: and what is behind it bent by the waves\n"
"	result = LiquidRefract(result, in_pos, bumped, facing, liquid);\n"
"#endif\n"
"	out_fragcolor = result;\n"
"#if DITHER == 1\n"
"	vec3 dpos = fwidth(in_pos);\n"
"	float farblend = clamp(max(dpos.x, max(dpos.y, dpos.z)) * 0.5 - 0.125, 0., 1.);\n"
"	farblend *= farblend;\n"
"	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"	float luma = dot(out_fragcolor.rgb, vec3(.25, .625, .125));\n"
"	float nearnoise = tri(whitenoise01(lmuv * lmsize)) * luma * TextureDither;\n"
"	float farnoise = Fog.w > 0. ? SCREEN_SPACE_NOISE() * ScreenDither : 0.;\n"
"	out_fragcolor.rgb += mix(nearnoise, farnoise, farblend);\n"
"	out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"#endif // DITHER == 1\n"
"#if DITHER >= 2\n"
"	// nuke extra precision in 10-bit framebuffer\n"
"	out_fragcolor.rgb = floor(out_fragcolor.rgb * 255. + 0.5) * (1./255.);\n"
"#elif DITHER == 0\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Water
//
////////////////////////////////////////////////////////////////

static const char water_vertex_shader[] =
BINDLESS_VERTEX_HEADER
FRAMEDATA_BUFFER
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
WORLD_VERTEX_BUFFER
"\n"
"layout(location=0) flat out float out_alpha;"
"layout(location=1) out vec2 out_uv;\n"
"layout(location=2) out vec3 out_pos;\n"
"#if BINDLESS\n"
"	layout(location=3) flat out uvec2 out_sampler;\n"
"#endif\n"
"layout(location=4) flat out uint out_flags; // QVR: the liquid's kind\n"
"\n"
"void main()\n"
"{\n"
"	Call call = call_data[DRAW_ID];\n"
"	int instance_id = GET_INSTANCE_ID(call);\n"
"	Instance instance = instance_data[instance_id];\n"
"	vec3 pos = Transform(in_pos, instance);\n"
"	gl_Position = ViewProj * vec4(pos, 1.0);\n"
"	out_uv = in_uv.xy;\n"
"	out_pos = pos - EyePos;\n"
"	out_flags = call.flags; // QVR\n"
"	out_alpha = instance.alpha < 0.0 ? call.wateralpha : instance.alpha;\n"
"#if BINDLESS\n"
"	out_sampler = call.txhandle;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char water_fragment_shader[] =
"#if BINDLESS\n"
"	#extension GL_ARB_bindless_texture : require\n"
"#else\n"
"	layout(binding=0) uniform sampler2D Tex;\n"
"#endif\n"
"\n"
FRAMEDATA_BUFFER
NOISE_FUNCTIONS
LIQUID_FUNCTIONS // QVR
"\n"
"layout(location=0) flat in float in_alpha;\n"
"layout(location=1) in vec2 in_uv;\n"
"layout(location=2) in vec3 in_pos;\n"
"#if BINDLESS\n"
"	layout(location=3) flat in uvec2 in_sampler;\n"
"#endif\n"
"layout(location=4) flat in uint in_flags; // QVR\n"
"\n"
OIT_OUTPUT (out_fragcolor)
"\n"
"void main()\n"
"{\n"
"	vec3 pos = in_pos + EyePos; // QVR: the liquid's look (vr_water_*), in the world\n"
"	vec3 facing = normalize(cross(dFdx(pos), dFdy(pos)));\n"
"	facing = dot(facing, in_pos) > 0. ? -facing : facing;\n"
"	uint liquid = LiquidKind(in_flags);\n"
"	vec3 waves = LiquidWaves(pos, facing, liquid);\n"
"	vec2 uv = LiquidWarp(in_uv, liquid) + waves.xy * vec2(0.02, -0.02);\n"
"#if BINDLESS\n"
"	sampler2D Tex = sampler2D(in_sampler);\n"
"#endif\n"
"	vec4 result = texture(Tex, uv);\n"
"	vec3 n = LiquidNormal(waves, facing); // QVR\n"
"	float alpha = in_alpha;\n"
"	result.rgb = clamp(LiquidShade(result.rgb, result.rgb, vec3(1.0), pos, n, facing, waves.z, liquid, alpha), 0.0, 1.0);\n"
"	result.rgb = ApplyFog(result.rgb, in_pos);\n"
"	result.a *= alpha; // QVR\n"
"	result = LiquidRefract(result, pos, n, facing, liquid); // QVR\n"
"	out_fragcolor = result;\n"
"#if DITHER\n"
"	if (Fog.w > 0.)\n"
"	{\n"
"		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"		out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"	}\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Sky stencil mark
//
////////////////////////////////////////////////////////////////

static const char skystencil_vertex_shader[] =
BINDLESS_VERTEX_HEADER
FRAMEDATA_BUFFER
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
WORLD_VERTEX_BUFFER
"\n"
"void main()\n"
"{\n"
"	Call call = call_data[DRAW_ID];\n"
"	int instance_id = GET_INSTANCE_ID(call);\n"
"	Instance instance = instance_data[instance_id];\n"
"	gl_Position = ViewProj * vec4(Transform(in_pos, instance), 1.0);\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Sky layers
//
////////////////////////////////////////////////////////////////

static const char sky_layers_vertex_shader[] =
BINDLESS_VERTEX_HEADER
FRAMEDATA_BUFFER
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
WORLD_VERTEX_BUFFER
"\n"
"layout(location=0) out vec3 out_dir;\n"
"#if BINDLESS\n"
"	layout(location=1) flat out uvec4 out_samplers;\n"
"#endif\n"
"\n"
"void main()\n"
"{\n"
"	Call call = call_data[DRAW_ID];\n"
"	int instance_id = GET_INSTANCE_ID(call);\n"
"	Instance instance = instance_data[instance_id];\n"
"	vec3 pos = Transform(in_pos, instance);\n"
"	gl_Position = ViewProj * vec4(pos, 1.0);\n"
"	out_dir = pos - EyePos;\n"
"	out_dir.z *= 3.0; // flatten the sphere\n"
"#if BINDLESS\n"
"	out_samplers.xy = call.txhandle;\n"
"	out_samplers.zw = call.fbhandle;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char sky_layers_fragment_shader[] =
"#if BINDLESS\n"
"	#extension GL_ARB_bindless_texture : require\n"
"#else\n"
"	layout(binding=0) uniform sampler2D SolidLayer;\n"
"	layout(binding=1) uniform sampler2D AlphaLayer;\n"
"#endif\n"
"\n"
FRAMEDATA_BUFFER
NOISE_FUNCTIONS
"\n"
"layout(location=0) in vec3 in_dir;\n"
"#if BINDLESS\n"
"	layout(location=1) flat in uvec4 in_samplers;\n"
"#endif\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"#if BINDLESS\n"
"	sampler2D SolidLayer = sampler2D(in_samplers.xy);\n"
"	sampler2D AlphaLayer = sampler2D(in_samplers.zw);\n"
"#endif\n"
"	vec2 uv = normalize(in_dir).xy * (189.0 / 64.0);\n"
"	vec4 result = texture(SolidLayer, uv + Time / 16.0);\n"
"	vec4 layer = texture(AlphaLayer, uv + Time / 8.0);\n"
"	result.rgb = mix(result.rgb, layer.rgb, layer.a);\n"
"	result.rgb = mix(result.rgb, SkyFog.rgb, SkyFog.a);\n"
"	out_fragcolor = result;\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Skybox cubemap
//
////////////////////////////////////////////////////////////////

static const char sky_cubemap_vertex_shader[] =
BINDLESS_VERTEX_HEADER
FRAMEDATA_BUFFER
WORLD_CALLDATA_BUFFER
WORLD_INSTANCEDATA_BUFFER
WORLD_VERTEX_BUFFER
"\n"
"layout(location=0) out vec3 out_dir;\n"
"\n"
"void main()\n"
"{\n"
"	Call call = call_data[DRAW_ID];\n"
"	int instance_id = GET_INSTANCE_ID(call);\n"
"	Instance instance = instance_data[instance_id];\n"
"	vec3 pos = Transform(in_pos, instance);\n"
"	gl_Position = ViewProj * vec4(pos, 1.0);\n"
"	out_dir.x = -(pos.y - EyePos.y);\n"
"	out_dir.y =  (pos.z - EyePos.z);\n"
"	out_dir.z =  (pos.x - EyePos.x);\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char sky_cubemap_fragment_shader[] =
FRAMEDATA_BUFFER
NOISE_FUNCTIONS
"\n"
"layout(binding=2) uniform samplerCube Skybox;\n"
"\n"
"layout(location=0) in vec3 in_dir;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"#if ANIM\n"
"	float t1 = WindPhase;\n"
"	float t2 = fract(t1) - 0.5;\n"
"	float blend = abs(t1 * 2.0);\n"
"	vec3 dir = normalize(in_dir);\n"
"	vec4 base = texture(Skybox, in_dir);\n"
"	vec4 layer1 = texture(Skybox, dir + t1 * WindDir);\n"
"	vec4 layer2 = texture(Skybox, dir + t2 * WindDir);\n"
"	layer1.a *= 1.0 - blend;\n"
"	layer2.a *= blend;\n"
"	layer1.rgb *= layer1.a;\n"
"	layer2.rgb *= layer2.a;\n"
"	vec4 combined = layer1 + layer2;\n"
"	out_fragcolor = vec4(base.rgb * (1.0 - combined.a) + combined.rgb, 1);\n"
"#else\n"
"	out_fragcolor = texture(Skybox, in_dir);\n"
"#endif\n"
"	out_fragcolor.rgb = mix(out_fragcolor.rgb, SkyFog.rgb, SkyFog.a);\n"
"#if DITHER\n"
"	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"	out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"	out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Skybox side
//
////////////////////////////////////////////////////////////////

static const char sky_boxside_vertex_shader[] =
"layout(location=0) uniform mat4 MVP;\n"
"layout(location=1) uniform vec3 EyePos;\n"
"\n"
"layout(location=0) in vec3 in_dir;\n"
"layout(location=1) in vec2 in_uv;\n"
"\n"
"layout(location=0) out vec3 out_dir;\n"
"layout(location=1) out vec2 out_uv;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = MVP * vec4(EyePos + in_dir, 1.0);\n"
"	gl_Position.z = gl_Position.w; // map to far plane\n"
"	out_dir = in_dir;\n"
"	out_uv = in_uv;\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char sky_boxside_fragment_shader[] =
"layout(binding=0) uniform sampler2D Tex;\n"
"\n"
NOISE_FUNCTIONS
"\n"
"layout(location=2) uniform vec4 Fog;\n"
"layout(location=3) uniform float ScreenDither;\n"
"\n"
"layout(location=0) in vec3 in_dir;\n"
"layout(location=1) in vec2 in_uv;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	out_fragcolor = texture(Tex, in_uv);\n"
"	out_fragcolor.rgb = mix(out_fragcolor.rgb, Fog.rgb, Fog.w);\n"
"#if DITHER\n"
"	out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"	out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"	out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Alias models
//
////////////////////////////////////////////////////////////////

#define ALIAS_INSTANCE_BUFFER \
"struct InstanceData\n"\
"{\n"\
"	vec4	WorldMatrix[3];\n"\
"	vec4	LightColor; // xyz=LightColor w=Alpha\n"\
"	int		Pose1;\n"\
"	int		Pose2;\n"\
"	float	Blend;\n"\
"	int		Padding;\n"\
"	vec4	LightDir; // QVR: xyz towards the model's light, w how much it replaces the fixed direction\n"\
"	vec4	Glow; // QVR: x the force grab glow (vr/vr_fgfx.cpp), y shaded on a par with the world (vr_model_light_parity), z the fullbright boost (vr/vr_emissive.cpp), w parallax mapping's depth in units\n"\
"};\n"\
"\n"\
"layout(std430, binding=1) restrict readonly buffer InstanceBuffer\n"\
"{\n"\
"	mat4	ViewProj;\n"\
"	vec3	EyePos;\n"\
"	vec4	Fog;\n"\
"	float	ScreenDither;\n"\
"	InstanceData instances[];\n"\
"};\n"\

////////////////////////////////////////////////////////////////

static const char alias_vertex_shader[] =
ALIAS_INSTANCE_BUFFER
"\n"
"struct PoseVertex\n"
"{\n"
"	vec3 pos;\n"
"	vec3 nor;\n"
"};\n"
"\n"
"#if POSEVERTTYPE == 1 // PV_IQM (Skeletal)\n"
"	layout(location=0) in vec3 in_pos;\n"
"	layout(location=1) in vec4 in_nor;\n"
"	layout(location=2) in vec2 in_uv;\n"
"	layout(location=3) in vec4 in_weights;\n"
"	layout(location=4) in ivec4 in_indices;\n"
"\n"
"	layout(std430, binding=2) restrict readonly buffer PoseBuffer\n"
"	{\n"
"		mat3x4 BonePoses[];\n"
"	};\n"
"\n"
"	PoseVertex GetPoseVertex(uint pose)\n"
"	{\n"
"		mat3x4 blendmat = BonePoses[pose + in_indices.x] * in_weights.x;\n"
"		blendmat += BonePoses[pose + in_indices.y] * in_weights.y;\n"
"		if (in_weights.z + in_weights.w > 0.0)\n"
"		{\n"
"			blendmat += BonePoses[pose + in_indices.z] * in_weights.z;\n"
"			blendmat += BonePoses[pose + in_indices.w] * in_weights.w;\n"
"		}\n"
"		mat4x3 anim = transpose(blendmat);\n"
"		return PoseVertex((anim * vec4(in_pos, 1.0)).xyz, (anim * vec4(in_nor.xyz, 0.0)).xyz);\n"
"	}\n"
"\n"
"#else // PV_QUAKE1 || PV_MD3\n"
"	layout (location = 0) in vec2 in_uv;\n"
"\n"
"	layout(std430, binding=2) restrict readonly buffer BlendShapeBuffer\n"
"	{\n"
"		uvec2 PackedPosNor[];\n"
"	};\n"
"\n"
"	PoseVertex GetPoseVertex (uint pose)\n"
"	{\n"
"		uvec2 data = PackedPosNor[pose + gl_VertexID];\n"
"#if POSEVERTTYPE == 2 // PV_MD3\n"
"		PoseVertex ret;\n"
"		ret.pos = vec3((ivec3(data.xxy >> uvec3(0, 16, 0)) & 65535) - 32768);\n"
"		vec2 spherical = vec2((data.yy >> uvec2(16, 24)) & 255) * (2.0 * 3.14159265 / 255.0);\n"
"		float sinlat = sin(spherical.x);\n"
"		float coslat = cos(spherical.x);\n"
"		float sinlng = sin(spherical.y);\n"
"		float coslng = cos(spherical.y);\n"
"		ret.nor.x = coslng * sinlat;\n"
"		ret.nor.y = sinlng * sinlat;\n"
"		ret.nor.z = coslat;\n"
"		return ret;\n"
"#else // PV_QUAKE1\n"
"		return PoseVertex(vec3((data.xxx >> uvec3(0, 8, 16)) & 255), unpackSnorm4x8(data.y).xyz);\n"
"#endif // POSEVERTTYPE\n"
"	}\n"
"\n"
"#endif // POSEVERTTYPE check\n"
"\n"
"float r_avertexnormal_dot(vec3 vertexnormal, vec3 dir) // from MH \n"
"{\n"
"	float d = dot(vertexnormal, dir);\n"
"	if (d < 0.0)\n"
"		return 1.0 + d * (13.0 / 44.0);\n"
"	else\n"
"		return 1.0 + d;\n"
"}\n"
"\n"
"#if MODE == " QS_STRINGIFY (ALIASSHADER_NOPERSP) "\n"
"	layout(location=0) noperspective out vec2 out_texcoord;\n"
"#else\n"
"	layout(location=0) out vec2 out_texcoord;\n"
"#endif\n"
"layout(location=1) out vec4 out_color;\n"
"layout(location=2) out vec3 out_pos;\n"
"layout(location=3) out vec3 out_nor; // QVR: per-pixel dynamic lights (vr/vr_lighting.cpp)\n"
"layout(location=4) out float out_depth; // QVR\n"
"layout(location=5) noperspective out vec2 out_coord; // QVR\n"
"layout(location=6) flat out float out_glow; // QVR\n"
"layout(location=7) flat out float out_fbboost; // QVR\n"
"layout(location=8) flat out float out_pdepth; // QVR: parallax mapping\n"
"\n"
"void main()\n"
"{\n"
"	InstanceData inst = instances[gl_InstanceID];\n"
"	out_glow = inst.Glow.x; // QVR\n"
"	out_fbboost = inst.Glow.z; // QVR\n"
"	out_pdepth = inst.Glow.w; // QVR: parallax mapping's depth (vr_parallax_models)\n"
"	out_texcoord = in_uv;\n"
"	PoseVertex pose1 = GetPoseVertex(inst.Pose1);\n"
"	PoseVertex pose2 = GetPoseVertex(inst.Pose2);\n"
"	mat4x3 worldmatrix = transpose(mat3x4(inst.WorldMatrix[0], inst.WorldMatrix[1], inst.WorldMatrix[2]));\n"
"	vec3 lerpedPos = mix(pose1.pos, pose2.pos, inst.Blend);\n"
"	if (inst.Padding != 0) // QVR: blend towards frame 0 (vr/vr_render.cpp)\n"
"		lerpedPos = mix(lerpedPos, GetPoseVertex(uint(inst.Padding) & 0xFFFFFFu).pos, float(uint(inst.Padding) >> 24) / 255.0);\n"
"	vec3 lerpedVert = (worldmatrix * vec4(lerpedPos, 1.0)).xyz;\n"
"	gl_Position = ViewProj * vec4(lerpedVert, 1.0);\n"
"	out_pos = lerpedVert - EyePos;\n"
"	out_nor = mat3(worldmatrix) * mix(pose1.nor, pose2.nor, inst.Blend); // QVR\n"
"	out_depth = gl_Position.w; // QVR\n"
"	out_coord = (gl_Position.xy / gl_Position.w * 0.5 + 0.5) * vec2(" QS_STRINGIFY (LIGHT_TILES_X) ", " QS_STRINGIFY (LIGHT_TILES_Y) "); // QVR\n"
"	// transform world X and Z axes to local space\n"
"	mat3 orientation = mat3(normalize(worldmatrix[0].xyz), normalize(worldmatrix[1].xyz), normalize(worldmatrix[2].xyz));\n"
"	orientation = transpose(orientation);\n"
"	vec3 shadevector = orientation * normalize(mix(vec3(0.70710678, 0.0, 0.70710678), inst.LightDir.xyz, inst.LightDir.w)); // QVR: vr/vr_modellight.cpp\n"
"	float dot1, dot2;\n"
"	if (inst.Glow.y != 0.) // QVR: on a par with the world (vr_model_light_parity): 0.6 .. 1.4 by the normal, on average the light given\n"
"	{\n"
"		dot1 = 1.0 + 0.4 * dot(pose1.nor, shadevector);\n"
"		dot2 = 1.0 + 0.4 * dot(pose2.nor, shadevector);\n"
"	}\n"
"	else\n"
"	{\n"
"		dot1 = r_avertexnormal_dot(pose1.nor, shadevector);\n"
"		dot2 = r_avertexnormal_dot(pose2.nor, shadevector);\n"
"	}\n"
"	out_color = clamp(inst.LightColor * vec4(vec3(mix(dot1, dot2, inst.Blend)), 1.0), 0.0, 1.0);\n"
"	uint overbright = floatBitsToUint(Fog.w) >> 31;\n"
"	out_color.rgb = ldexp(out_color.rgb, ivec3(overbright));\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char alias_fragment_shader[] =
ALIAS_INSTANCE_BUFFER
ALIAS_FRAMEDATA_BUFFER // QVR
LIGHT_BUFFER // QVR
LIGHT_CLUSTER_IMAGE("readonly") // QVR
SHADOW_FUNCTIONS // QVR
PARALLAX_FUNCTIONS // QVR
NOISE_FUNCTIONS
"\n"
"layout(binding=0) uniform sampler2D Tex;\n"
"layout(binding=1) uniform sampler2D FullbrightTex;\n"
"layout(binding=2) uniform sampler2D NormalTex; // QVR\n"
"\n"
"#if MODE == " QS_STRINGIFY (ALIASSHADER_NOPERSP) "\n"
"	layout(location=0) noperspective in vec2 in_texcoord;\n"
"#else\n"
"	layout(location=0) in vec2 in_texcoord;\n"
"#endif\n"
"layout(location=1) in vec4 in_color;\n"
"layout(location=2) in vec3 in_pos;\n"
"layout(location=3) in vec3 in_nor; // QVR\n"
"layout(location=4) in float in_depth; // QVR\n"
"layout(location=5) noperspective in vec2 in_coord; // QVR\n"
"layout(location=6) flat in float in_glow; // QVR\n"
"layout(location=7) flat in float in_fbboost; // QVR\n"
"layout(location=8) flat in float in_pdepth; // QVR: parallax mapping's depth in units (vr_parallax_models; 0 off)\n"
"\n"
OIT_OUTPUT (out_fragcolor)
"\n"
"// QVR: dynamic lights per pixel (vr/vr_lighting.cpp), shaded by the angle (the normal bent by the normal map) and\n"
"// shadowed, as a multiplier of the texture (Quake adds radius - distance to the model's light, which is divided by\n"
"// 200, doubled for overbright models; by 128 on a par with the world); `spec` gets their sheen.\n"
"vec3 ModelDynamicLights(vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, out vec3 spec)\n"
"{\n"
"	spec = vec3(0.);\n"
"	if ((ShadowFlags & 8u) == 0u || NumLights == 0u)\n"
"		return vec3(0.);\n"
"	ivec3 cluster_coord;\n"
"	cluster_coord.x = int(floor(in_coord.x));\n"
"	cluster_coord.y = int(floor(in_coord.y));\n"
"	cluster_coord.z = int(floor(log2(in_depth) * ZLogScale + ZLogBias));\n"
"	uvec2 clusterdata = imageLoad(LightClusters, cluster_coord).xy;\n"
"	if ((clusterdata.x | clusterdata.y) == 0u)\n"
"		return vec3(0.);\n"
"	vec3 pos = in_pos + EyePos;\n"
"	vec3 n = normalize(in_nor);\n"
"	vec3 bumped = LightTweak.w > 0. ? BumpedNormal(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, n) : n;\n"
"	bool darkplaces = (ShadowFlags & 16u) != 0u;\n"
"	float unit = (ShadowFlags & 32u) != 0u ? 1.0 / 128.0 : Fog.w < 0. ? 2.0 / 200.0 : 1.0 / 200.0; // the sign of Fog.w: overbright models\n"
"	vec3 total = vec3(0.);\n"
"	for (uint i = 0u, ofs = 0u; i < 2u; i++, ofs += 32u)\n"
"	{\n"
"		uint mask = clusterdata[i];\n"
"		while (mask != 0u)\n"
"		{\n"
"			int j = findLSB(mask);\n"
"			mask ^= 1u << j;\n"
"			Light l = Lights[ofs + j];\n"
"			if (l.shadow.w != 0.)\n"
"				continue;\n"
"			float d = distance(l.origin, pos);\n"
"			if (d >= l.radius)\n"
"				continue;\n"
"			float lit = (darkplaces ? DarkPlacesAtten(d, l.radius) : (l.radius - d) * unit) * LightShadow(l, pos, n);\n"
"			if (lit <= 0.)\n"
"				continue;\n"
"			total += lit * (darkplaces ? LightAngleDP(l, pos, bumped) : LightAngle(l, pos, bumped, 0.3)) * l.color;\n"
"			spec += lit * LightSpecular(l, pos, bumped, EyePos) * l.color;\n"
"		}\n"
"	}\n"
"	return total;\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	vec2 uv = in_texcoord;\n"
"	vec3 dpdx = dFdx(in_pos), dpdy = dFdy(in_pos); // QVR: for the normal map (before any discard)\n"
"	vec2 duvdx = dFdx(uv), duvdy = dFdy(uv);\n"
"#if MODE == " QS_STRINGIFY (ALIASSHADER_NOPERSP) "\n"
"	uv -= 0.5 / vec2(textureSize(Tex, 0).xy);\n"
"	vec4 result = textureLod(Tex, uv, 0.);\n"
"#else\n"
"	// QVR: parallax occlusion mapping (vr_parallax_models) on the skin's heights, in the triangle's own frame, with\n"
"	// fewer steps than the world's; read with the surface's mip level (the moved coordinates jump at occlusions)\n"
"	if (in_pdepth > 0.)\n"
"	{\n"
"		vec3 n = normalize(cross(dpdx, dpdy));\n"
"		n = dot(n, in_pos) > 0. ? -n : n; // facing the eye (in_pos is from it)\n"
"		uv = ParallaxUV(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, n, in_pos, in_pdepth, max(Parallax.z * 0.5, 4.0), vec4(0.));\n"
"	}\n"
"	vec4 result = textureGrad(Tex, uv, duvdx, duvdy);\n"
"#endif\n"
"#if ALPHATEST\n"
"	if (result.a < 0.666)\n"
"		discard;\n"
"#endif\n"
"#if ALPHATEST\n"
"	vec3 emissive = vec3(0.); // QVR\n"
"#else\n"
"	vec3 emissive = result.rgb * (1.0 - result.a); // QVR: fullbright texels, unlit (ALPHABRIGHT skins keep them in the alpha)\n"
"#endif\n"
"	vec3 spec; // QVR\n"
"	vec3 light = in_color.rgb + ModelDynamicLights(uv, duvdx, duvdy, dpdx, dpdy, spec); // QVR\n"
"#if ALPHATEST\n"
"	result.rgb *= light;\n"
"#else\n"
"	result.rgb = mix(result.rgb, result.rgb * light, result.a);\n"
"#endif\n"
"	result.rgb += spec; // QVR\n"
"#if POSEVERTTYPE == 2 \n"
"	result.a *= in_color.a;\n"
"#else\n"
"	result.a = in_color.a;\n"
"#endif\n"
"#if MODE == " QS_STRINGIFY (ALIASSHADER_NOPERSP) "\n"
"	vec3 fullbright = textureLod(FullbrightTex, uv, 0.).rgb;\n"
"#else\n"
"	vec3 fullbright = textureGrad(FullbrightTex, uv, duvdx, duvdy).rgb; // QVR: uv moved by parallax mapping\n"
"#endif\n"
"	// QVR: the held weapons' dim fullbrights (sights) shine brighter, bright ones stay (vr/vr_emissive.cpp)\n"
"	vec3 glowing = fullbright + emissive;\n"
"	result.rgb += fullbright + glowing * (in_fbboost * (1.0 - max(glowing.r, max(glowing.g, glowing.b))));\n"
"	if (in_glow > 0.) // QVR: force grab's glow round the edges (vr/vr_fgfx.cpp)\n"
"	{\n"
"		float rim = 1.0 - abs(dot(normalize(in_nor), normalize(-in_pos)));\n"
"		result.rgb += vec3(0.35, 0.65, 1.0) * in_glow * (pow(rim, 2.0) * 1.1 + 0.08);\n"
"	}\n"
"	result.rgb = clamp(result.rgb, 0.0, 1.0);\n"
"	float fog = exp2(abs(Fog.w) * -dot(in_pos, in_pos));\n"\
"	fog = clamp(fog, 0.0, 1.0);\n"
"	result.rgb = mix(Fog.rgb, result.rgb, fog);\n"
"	out_fragcolor = result;\n"
"#if MODE == " QS_STRINGIFY (ALIASSHADER_DITHER) " || MODE == " QS_STRINGIFY (ALIASSHADER_NOPERSP) "\n"
"	// Note: sign bit is used as overbright flag\n"
"	if (abs(Fog.w) > 0.)\n"
"	{\n"
"		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"		out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"	}\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Sprites
//
////////////////////////////////////////////////////////////////

static const char sprites_vertex_shader[] =
FRAMEDATA_BUFFER
"\n"
"layout(location=0) in vec3 in_pos;\n"
"layout(location=1) in vec2 in_uv;\n"
"\n"
"layout(location=0) out vec2 out_uv;\n"
"layout(location=1) out vec3 out_pos;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = ViewProj * vec4(in_pos, 1.0);\n"
"	out_pos = in_pos - EyePos;\n"
"	out_uv = in_uv;\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char sprites_fragment_shader[] =
FRAMEDATA_BUFFER
NOISE_FUNCTIONS
"\n"
"layout(binding=0) uniform sampler2D Tex;\n"
"\n"
"layout(location=0) in vec2 in_uv;\n"
"layout(location=1) in vec3 in_pos;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	vec4 result = texture(Tex, in_uv);\n"
"	if (result.a < 0.666)\n"
"		discard;\n"
"	result.rgb = ApplyFog(result.rgb, in_pos);\n"
"	out_fragcolor = result;\n"
"#if DITHER\n"
"	if (Fog.w > 0.)\n"
"	{\n"
"		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"		out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"	}\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Particles
//
////////////////////////////////////////////////////////////////

static const char particles_vertex_shader[] =
FRAMEDATA_BUFFER
"\n"
"layout(location=0) in vec3 in_pos;\n"
"layout(location=1) in vec4 in_color;\n"
"\n"
"layout(location=0) out vec2 out_uv;\n"
"layout(location=1) out vec4 out_color;\n"
"layout(location=2) out vec3 out_pos;\n"
"\n"
"layout(location=0) uniform vec3 Params;\n"
"#define ProjScale	Params.xy\n"
"#define UVScale	Params.z\n"
"\n"
"void main()\n"
"{\n"
"	// figure the current corner: (-1, -1), (-1, 1), (1, -1) or (1, 1)\n"
"	uvec2 flipsign = uvec2(gl_VertexID, gl_VertexID >> 1) << 31;\n"
"	vec2 corner = uintBitsToFloat(floatBitsToUint(-1.0) ^ flipsign);\n"
"\n"
"	// project the center of the particle\n"
"	gl_Position = ViewProj * vec4(in_pos, 1.0);\n"
"\n"
"	// hack a scale up to keep particles from disappearing\n"
"	float depthscale = max(1.0 + gl_Position.w * 0.004, 1.08);\n"
"\n"
"	// perform the billboarding\n"
"	gl_Position.xy += ProjScale * uintBitsToFloat(floatBitsToUint(vec2(depthscale)) ^ flipsign);\n"
"\n"
"	out_pos = in_pos - EyePos; // FIXME: use corner position\n"
"	out_uv = corner * UVScale;\n"
"	out_color = in_color;\n"
"#if OIT\n"
"	out_color.a *= 0.9;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char particles_fragment_shader[] =
FRAMEDATA_BUFFER
NOISE_FUNCTIONS
"\n"
"layout(location=0) in vec2 in_uv;\n"
"layout(location=1) in vec4 in_color;\n"
"layout(location=2) in vec3 in_pos;\n"
"\n"
OIT_OUTPUT (out_fragcolor)
"\n"
"void main()\n"
"{\n"
"	out_fragcolor = in_color;\n"
"	out_fragcolor.rgb = ApplyFog(out_fragcolor.rgb, in_pos);\n"
"	float radius = length(in_uv);\n"
"	float pixel = fwidth(radius);\n"
"	out_fragcolor.a *= clamp((1. - radius) / pixel, 0., 1.);\n"
"#if DITHER\n"
"	if (Fog.w > 0.)\n"
"	{\n"
"		out_fragcolor.rgb = sqrt(out_fragcolor.rgb);\n"
"		out_fragcolor.rgb += SCREEN_SPACE_NOISE() * ScreenDither;\n"
"		out_fragcolor.rgb *= out_fragcolor.rgb;\n"
"	}\n"
"#else\n"
"	out_fragcolor.rgb += SUPPRESS_BANDING() * ScreenDither;\n"
"#endif\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Debug 3D
//
////////////////////////////////////////////////////////////////

static const char debug3d_vertex_shader[] =
FRAMEDATA_BUFFER
"\n"
"layout(location=0) in vec3 in_pos;\n"
"layout(location=1) in vec4 in_color;\n"
"\n"
"layout(location=0) out vec4 out_color;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = ViewProj * vec4(in_pos, 1.0);\n"
"	out_color = in_color;\n"
"}\n";

////////////////////////////////////////////////////////////////

static const char debug3d_fragment_shader[] =
"\n"
"layout(location=0) in vec4 in_color;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"void main()\n"
"{\n"
"	out_fragcolor = in_color;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// OIT resolve
//
////////////////////////////////////////////////////////////////

static const char oit_resove_vertex_shader[] =
"void main()\n"
"{\n"
"	ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
"	gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);\n"
"}\n";

static const char oit_resove_fragment_shader[] =
"layout(early_fragment_tests) in;\n"
"\n"
"#if MSAA\n"
"	#define Sampler				sampler2DMS\n"
"	#define FetchSample(s, c)	texelFetch(s, c, gl_SampleID)\n"
"#else\n"
"	#define Sampler				sampler2D\n"
"	#define FetchSample(s, c)	texelFetch(s, c, 0)\n"
"#endif\n"
"\n"
"layout(binding=0) uniform Sampler TexAccum;\n"
"layout(binding=1) uniform Sampler TexReveal;\n"
"\n"
"layout(location=0) out vec4 out_fragcolor;\n"
"\n"
"vec3 LinearToGamma(vec3 v)\n"
"{\n"
"#if " QS_STRINGIFY (LINEAR_SPACE_OIT) "\n"
"	return sqrt(clamp(v, 0.0, 1.0));\n"
"#else\n"
"	return clamp(v, 0.0, 1.0);\n"
"#endif\n"
"}\n"
"\n"
"// get the max value between three values\n"
"float max3(vec3 v)\n"
"{\n"
"	return max(max(v.x, v.y), v.z);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	ivec2 coords = ivec2(gl_FragCoord.xy);\n"
"	float revealage = FetchSample(TexReveal, coords).r;\n"
"	// Note: we're using the stencil buffer to discard pixels with no contribution\n"
"	//if (revealage >= 0.999)\n"
"	//	discard;\n"
"\n"
"	vec4 accumulation = FetchSample(TexAccum, coords);\n"
"	// suppress overflow\n"
"	if (isinf(max3(abs(accumulation.rgb))))\n"
"		accumulation.rgb = vec3(accumulation.a);\n"
"\n"
"	vec3 average_color = accumulation.rgb / max(accumulation.a, 1e-5);\n"
"	out_fragcolor = vec4(LinearToGamma(average_color), 1.0 - revealage);\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// COMPUTE SHADERS
//
////////////////////////////////////////////////////////////////
//
// Clear indirect draws
//
////////////////////////////////////////////////////////////////

static const char clear_indirect_compute_shader[] =
"layout(local_size_x=64) in;\n"
"\n"
WORLD_DRAW_BUFFER
"\n"
"void main()\n"
"{\n"
"	uint thread_id = gl_GlobalInvocationID.x;\n"
"	if (thread_id < cmds.length())\n"
"		cmds[thread_id].count = 0u;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Gather indirect draws
//
////////////////////////////////////////////////////////////////

static const char gather_indirect_compute_shader[] =
"layout(local_size_x=64) in;\n"
"\n"
DRAW_ELEMENTS_INDIRECT_COMMAND
"\n"
"layout(std430, binding=5) restrict readonly buffer DrawIndirectSrcBuffer\n"
"{\n"
"	DrawElementsIndirectCommand src_cmds[];\n"
"};\n"
"\n"
"layout(std430, binding=6) restrict writeonly buffer DrawIndirectDstBuffer\n"
"{\n"
"	DrawElementsIndirectCommand dst_cmds[];\n"
"};\n"
"\n"
"struct DrawRemap\n"
"{\n"
"	uint src_call;\n"
"	uint instance_data;\n"
"};\n"
"\n"
"layout(std430, binding=7) restrict readonly buffer DrawRemapBuffer\n"
"{\n"
"	DrawRemap remap_data[];\n"
"};\n"
"\n"
"#define MAX_INSTANCES " QS_STRINGIFY (MAX_BMODEL_INSTANCES) "u\n"
"\n"
"void main()\n"
"{\n"
"	uint thread_id = gl_GlobalInvocationID.x;\n"
"	uint num_calls = remap_data.length();\n"
"	if (thread_id >= num_calls)\n"
"		return;\n"
"	DrawRemap remap = remap_data[thread_id];\n"
"	DrawElementsIndirectCommand cmd = src_cmds[remap.src_call];\n"
"	cmd.baseInstance = remap.instance_data / MAX_INSTANCES;\n"
"	cmd.instanceCount = (remap.instance_data % MAX_INSTANCES) + 1u;\n"
"	if (cmd.count == 0u)\n"
"		cmd.instanceCount = 0u;\n"
"	dst_cmds[thread_id] = cmd;\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Cull/mark: leaf vis/frustum culling, surface backface culling,
// index buffer + draw indirect buffer updates
//
////////////////////////////////////////////////////////////////

static const char cull_mark_compute_shader[] =
"layout(local_size_x=64) in;\n"
"\n"
DRAW_ELEMENTS_INDIRECT_COMMAND
"\n"
"// Note: some old Intel drivers error out on shaders\n"
"// that perform atomic operations on SSBO struct members.\n"
"// As a work-around, we use uints and manual indexing.\n"
"layout(std430, binding=1) buffer DrawIndirectBuffer\n"
"{\n"
"	//DrawElementsIndirectCommand cmds[];\n"
"	uint rawcmds[];\n"
"};\n"
"\n"
"#define CMD_COUNT(base)			rawcmds[base]\n"
"#define CMD_INSTANCE_COUNT(base)	rawcmds[base + 1]\n"
"#define CMD_FIRST_INDEX(base)		rawcmds[base + 2]\n"
"#define CMD_BASE_VERTEX(base)		rawcmds[base + 3]\n"
"#define CMD_BASE_INSTANCE(base)	rawcmds[base + 4]\n"
"#define SIZEOF_CMD					5 /* uints */\n"
"\n"
"layout(std430, binding=2) restrict writeonly buffer IndexBuffer\n"
"{\n"
"	uint indices[];\n"
"};\n"
"\n"
"layout(std430, binding=3) restrict readonly buffer VisBuffer\n"
"{\n"
"	uint vis[];\n"
"};\n"
"\n"
"struct MarkSurface\n"
"{\n"
"	uint packedleafsky; // bit 0=sky; 1..31=leafindex\n"
"	uint surfindex;\n"
"};\n"
"\n"
"layout(std430, binding=4) restrict readonly buffer MarkSurfaceBuffer\n"
"{\n"
"	MarkSurface marksurfs[];\n"
"};\n"
"\n"
"struct Surface\n"
"{\n"
"	vec4	plane;\n"
"	uint	framecount;\n"
"	uint	texnum;\n"
"	uint	numedges;\n"
"	uint	firstvert;\n"
"	vec3	mins;\n"
"	uint	_pad0;\n"
"	vec3	maxs;\n"
"	uint	_pad1;\n"
"};\n"
"\n"
"// Same issue as above\n"
"layout(std430, binding=5) restrict buffer SurfaceBuffer\n"
"{\n"
"	//Surface surfaces[];\n"
"	uvec4 rawsurfaces[];\n"
"};\n"
"\n"
"#define SURF_PLANE(base)			uintBitsToFloat(rawsurfaces[base])\n"
"#define SURF_FRAMECOUNT(base)		rawsurfaces[base + 1].x\n"
"#define SURF_TEXNUM(base)			rawsurfaces[base + 1].y\n"
"#define SURF_NUMEDGES(base)		rawsurfaces[base + 1].z\n"
"#define SURF_FIRSTVERT(base)		rawsurfaces[base + 1].w\n"
"#define SURF_MINS(base)			uintBitsToFloat(rawsurfaces[base + 2].xyz)\n"
"#define SURF_MAXS(base)			uintBitsToFloat(rawsurfaces[base + 3].xyz)\n"
"#define SIZEOF_SURFACE				4 /* uvec4s */\n"
"\n"
"layout(std140, binding=1) uniform FrameCullUBO\n"
"{\n"
"	vec4	frustum[4];\n"
"	vec3	vieworg;\n"
"	uint	oldskyleaf;\n"
"	uint	framecount;\n"
"};\n"
"\n"
"void main()\n"
"{\n"
"	uint thread_id = gl_GlobalInvocationID.x;\n"
"	if (thread_id >= marksurfs.length())\n"
"		return;\n"
"	MarkSurface mark = marksurfs[thread_id];\n"
"\n"
"	// sky culling: when r_oldskyleaf is 0, surfaces inside a sky leaf are skipped\n"
"	if ((mark.packedleafsky & 1u) > oldskyleaf)\n"
"		return;\n"
"\n"
"	// vis culling\n"
"	uint leaf = mark.packedleafsky >> 1u;\n"
"	uint visible = vis[leaf >> 5u] & (1u << (leaf & 31u));\n"
"	if (visible == 0u)\n"
"		return;\n"
"\n"
"	uint surfbase = mark.surfindex * uint(SIZEOF_SURFACE);\n"
"\n"
"	// backface culling\n"
"	vec4 surfplane = SURF_PLANE(surfbase);\n"
"	if (dot(surfplane.xyz, vieworg) < surfplane.w)\n"
"		return;\n"
"\n"
"	// frustum culling\n"
"	vec3 mins = SURF_MINS(surfbase);\n"
"	vec3 maxs = SURF_MAXS(surfbase);\n"
"	for (uint i = 0u; i < 4u; i++)\n"
"	{\n"
"		vec4 plane = frustum[i];\n"
"		vec3 v;\n"
"		v.x = plane.x < 0.0 ? mins.x : maxs.x;\n"
"		v.y = plane.y < 0.0 ? mins.y : maxs.y;\n"
"		v.z = plane.z < 0.0 ? mins.z : maxs.z;\n"
"		if (dot(plane.xyz, v) < plane.w)\n"
"			return;\n"
"	}\n"
"\n"
"	// surfaces can appear in multiple leaves\n"
"	// check if this is the first time this surface has passed culling this frame\n"
"	if (atomicExchange(SURF_FRAMECOUNT(surfbase), framecount) == framecount)\n"
"		return;\n"
"\n"
"	// surface is visible, append its triangles to the index buffer\n"
"	// and update the draw command corresponding to its texture number\n"
"	uint texnum = SURF_TEXNUM(surfbase);\n"
"	uint numedges = SURF_NUMEDGES(surfbase);\n"
"	uint firstvert = SURF_FIRSTVERT(surfbase);\n"
"	uint cmdbase = texnum * uint(SIZEOF_CMD);\n"
"	// some bsps out there have faces with < 2 edges, which would cause underflow below\n"
"	numedges = max(numedges, 2u);\n"
"	uint ofs = CMD_FIRST_INDEX(cmdbase) + atomicAdd(CMD_COUNT(cmdbase), 3u * (numedges - 2u));\n"
"	for (uint i = 2u; i < numedges; i++)\n"
"	{\n"
"		indices[ofs++] = firstvert;\n"
"		indices[ofs++] = firstvert + i - 1u;\n"
"		indices[ofs++] = firstvert + i;\n"
"	}\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Light clustering
//
////////////////////////////////////////////////////////////////

static const char cluster_lights_compute_shader[] =
"layout(local_size_x=8, local_size_y=8, local_size_z=1) in;\n"
"\n"
FRAMEDATA_BUFFER
LIGHT_BUFFER
"\n"
LIGHT_CLUSTER_IMAGE("writeonly")
"\n"
"layout(std140, binding=1) uniform InputUBO\n"
"{\n"
"	mat4	TransposedProj;\n"
"	mat4	View;\n"
"};\n"
"\n"
"shared vec4 local_lights[MAX_LIGHTS]; // xyz = view space pos; w = radius\n"
"\n"
"vec4 cluster_planes[6]; // view space; facing outside\n"
"vec3 cluster_center;\n"
"vec3 cluster_half_size;\n"
"\n"
"vec4 ExtractFrustumPlane(int axis, float ndcval, float side)\n"
"{\n"
"	vec4 plane = TransposedProj[axis] - ndcval * TransposedProj[3];\n"
"	return inversesqrt(dot(plane.xyz, plane.xyz)) * side * plane;\n"
"}\n"
"\n"
"void ComputeClusterPlanes(uvec3 gid)\n"
"{\n"
"	const float TileSizeX = 2.0 / float(LIGHT_TILES_X);\n"
"	const float TileSizeY = 2.0 / float(LIGHT_TILES_Y);\n"
"	float x0 = -1.0 + float(gid.x) * TileSizeX;\n"
"	float y0 = -1.0 + float(gid.y) * TileSizeY;\n"
"	float z0 = exp2((float(gid.z) - ZLogBias) / ZLogScale);\n"
"	cluster_planes[0] = ExtractFrustumPlane(0, x0,             -1.0);      // left\n"
"	cluster_planes[1] = ExtractFrustumPlane(0, x0 + TileSizeX,  1.0);      // right\n"
"	cluster_planes[2] = ExtractFrustumPlane(1, y0,             -1.0);      // bottom\n"
"	cluster_planes[3] = ExtractFrustumPlane(1, y0 + TileSizeY,  1.0);      // top\n"
"	cluster_planes[4] = vec4(-1.0, 0.0, 0.0,  z0);                         // near\n"
"	cluster_planes[5] = vec4( 1.0, 0.0, 0.0, -z0 * exp2(1.0 / ZLogScale)); // far\n"
"}\n"
"\n"
"float PointPlaneDistance(vec3 p, vec4 plane)\n"
"{\n"
"	return dot(p, plane.xyz) + plane.w;\n"
"}\n"
"\n"
"vec3 IntersectDepthPlane(vec3 dir, float depth)\n"
"{\n"
"	return vec3(depth, (depth / dir.x) * dir.yz);\n"
"}\n"
"\n"
"void ComputeClusterExtents()\n"
"{\n"
"	vec3 bl = cross(cluster_planes[2].xyz, cluster_planes[0].xyz); // bottom-left\n"
"	vec3 tr = cross(cluster_planes[3].xyz, cluster_planes[1].xyz); // top-right\n"
"	float depth_near = cluster_planes[4].w;\n"
"	float depth_far = -cluster_planes[5].w;\n"
"	vec3 p0 = IntersectDepthPlane(bl, depth_near);\n"
"	vec3 p1 = IntersectDepthPlane(bl, depth_far);\n"
"	vec3 p2 = IntersectDepthPlane(tr, depth_near);\n"
"	vec3 p3 = IntersectDepthPlane(tr, depth_far);\n"
"	vec3 cluster_mins = vec3(depth_near, min(min(p0.yz, p1.yz), min(p2.yz, p3.yz)));\n"
"	vec3 cluster_maxs = vec3(depth_far,  max(max(p0.yz, p1.yz), max(p2.yz, p3.yz)));\n"
"	cluster_center = (cluster_mins + cluster_maxs) * 0.5;\n"
"	cluster_half_size = (cluster_maxs - cluster_mins) * 0.5;\n"
"}\n"
"\n"
"bool LightTouchesCluster(vec4 l)\n"
"{\n"
"#if 1\n"
"	vec3 delta = max(abs(l.xyz - cluster_center) - cluster_half_size, 0.0);\n"
"	if (dot(delta, delta) >= l.w * l.w)\n"
"		return false;\n"
"#endif\n"
"#if 0\n"
"	for (int i = 0; i < 6; i++)\n"
"		if (PointPlaneDistance(l.xyz, cluster_planes[i]) > l.w)\n"
"			return false;\n"
"#endif\n"
"	return true;\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	uvec3 gid = gl_GlobalInvocationID;\n"
"	if (any(greaterThanEqual(gid, uvec3(LIGHT_TILES_X, LIGHT_TILES_Y, LIGHT_TILES_Z))))\n"
"		return;\n"
"	uint numlights = NumLights;\n"
"	if (numlights == 0u)\n"
"	{\n"
"		imageStore(LightClusters, ivec3(gid), uvec4(0u));\n"
"		return;\n"
"	}\n"
"	uint groupsize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
"	uint numpasses = (numlights + (groupsize - 1u)) / groupsize;\n"
"	uint i, j, ofs;\n"
"	for (i = 0u, ofs = 0u; i < numpasses; i++, ofs += groupsize)\n"
"	{\n"
"		uint index = gl_LocalInvocationIndex + ofs;\n"
"		if (index < numlights)\n"
"		{\n"
"			Light l = Lights[index];\n"
"			vec3 center = l.origin;\n"
"			float radius = l.radius;\n"
"			float s = length(l.spot.xyz);\n"
"			if (s > 0.) // QVR: a spot light: the smallest sphere round its cone (out to its radius)\n"
"			{\n"
"				vec3 dir = l.spot.xyz / s;\n"
"				float c = clamp((l.spot.w - 1.0) / s, 0.0, 1.0); // the cosine of the outer cone\n"
"				float along = c >= 0.7071 ? l.radius / (2.0 * c) : l.radius * c;\n"
"				radius = c >= 0.7071 ? along : l.radius * sqrt(1.0 - c * c);\n"
"				center += dir * along;\n"
"				radius = min(radius * 1.01 + 1.0, l.radius);\n"
"			}\n"
"			local_lights[index] = vec4((View * vec4(center, 1.0)).xyz, radius);\n"
"		}\n"
"	}\n"
"	memoryBarrierShared();\n"
"	barrier();\n"
"\n"
"	ComputeClusterPlanes(gid);\n"
"	ComputeClusterExtents();\n"
"\n"
"	uint clustermask[MAX_LIGHTS / 32];\n"
"	for (i = 0u; i < clustermask.length(); i++)\n"
"		clustermask[i] = 0u;\n"
"	for (i = 0u; i < numlights; i++)\n"
"		if (LightTouchesCluster(local_lights[i]))\n"
"			clustermask[i >> 5u] |= 1u << (i & 31u);\n"
"	imageStore(LightClusters, ivec3(gid), uvec4(clustermask[0], clustermask[1], 0u, 0u));\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Palette initialization
//
////////////////////////////////////////////////////////////////

static const char palette_init_compute_shader[] =
"layout(local_size_x=128) in;\n"
"\n"
"layout(location=0) uniform int Offset;\n"
"\n"
PALETTE_BUFFER
"\n"
"layout(r8ui, binding=0) uniform writeonly uimage3D PaletteLUT;\n"
"\n"
"vec3 sRGBToLinearRGB(vec3 v)\n"
"{\n"
"	return mix(v / 12.92, pow((v + 0.055) / 1.055, vec3(2.4)), greaterThan(v, vec3(0.04045)));\n"
"}\n"
"\n"
"// A perceptual color space for image processing - Bjorn Ottosson\n"
"// https://bottosson.github.io/posts/oklab/ \n"
"vec3 LinearRGBToOKLab(vec3 c)\n"
"{\n"
"	float l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;\n"
"	float m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;\n"
"	float s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;\n"
"\n"
"	float l_ = pow(l, 1./3.);\n"
"	float m_ = pow(m, 1./3.);\n"
"	float s_ = pow(s, 1./3.);\n"
"\n"
"	return vec3(\n"
"		0.2104542553*l_ + 0.7936177850*m_ - 0.0040720468*s_,\n"
"		1.9779984951*l_ - 2.4285922050*m_ + 0.4505937099*s_,\n"
"		0.0259040371*l_ + 0.7827717662*m_ - 0.8086757660*s_\n"
"	);\n"
"}\n"
"\n"
"vec3 NormalizeColor(uvec3 clr8)\n"
"{\n"
"	vec3 clr = vec3(clr8) * (1./255.);\n"
"#if MODE >= 1\n"
"	clr = sRGBToLinearRGB(clr);\n"
"	#if MODE >= 2\n"
"		clr = LinearRGBToOKLab(clr);\n"
"	#endif\n"
"#endif\n"
"	return clr;\n"
"}\n"
"\n"
"float ColorDistanceSquared(vec3 c0, vec3 c1)\n"
"{\n"
"	vec3 delta = c1 - c0;\n"
"#if MODE == 1\n"
"	// Colour metric - Thiadmer Riemersma\n"
"	// https://www.compuphase.com/cmetric.htm \n"
"	float rmean = (c0.r + c1.r) * 0.5;\n"
"	return dot(delta * delta, vec3(2. + rmean, 4., 3. - rmean));\n"
"#elif MODE == 2\n"
"	delta.x *= 1.25; // lightness\n"
"	delta.z *= 1.5;  // blue-yellow\n"
"#endif\n"
"	return dot(delta, delta);\n"
"}\n"
"\n"
"shared vec3 palcolors[256];\n"
"\n"
"void main()\n"
"{\n"
"	uint groupsize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
"	uint numpasses = (256 + (groupsize - 1u)) / groupsize;\n"
"	uint i, ofs;\n"
"	for (i = 0u, ofs = 0u; i < numpasses; i++, ofs += groupsize)\n"
"	{\n"
"		uint idx = gl_LocalInvocationIndex + ofs;\n"
"		if (idx < 256u)\n"
"			palcolors[idx] = NormalizeColor(UnpackRGB8(Palette[idx]));\n"
"	}\n"
"	memoryBarrierShared();\n"
"	barrier();\n"
"\n"
"	uvec3 gid = gl_GlobalInvocationID + UnpackRGB8(uint(Offset));\n"
"	vec3 target = NormalizeColor((gid << 1) + (gid >> 6));\n"
"	uint bestidx = 0;\n"
"	float bestdist = 1e+32;\n"
"	for (i = 0u; i < 256u; i++)\n"
"	{\n"
"		vec3 candidate = palcolors[i];\n"
"		float dist = ColorDistanceSquared(target, candidate);\n"
"		if (dist < bestdist)\n"
"		{\n"
"			bestidx = i;\n"
"			bestdist = dist;\n"
"		}\n"
"	}\n"
"	imageStore(PaletteLUT, ivec3(gid), uvec4(bestidx, 0, 0, 0));\n"
"}\n";

////////////////////////////////////////////////////////////////
//
// Palette postprocess (color blending, gamma, contrast)
//
////////////////////////////////////////////////////////////////

static const char palette_postprocess_compute_shader[] =
"layout(local_size_x=64) in;\n"
"\n"
"layout(location=0) uniform vec2 GammmaContrast;\n"
"layout(location=1) uniform vec4 BlendColor;\n"
"\n"
PALETTE_BUFFER
"\n"
"layout(std430, binding=1) restrict writeonly buffer DstPaletteBuffer\n"
"{\n"
"	uint DstPalette[256];\n"
"};\n"
"\n"
"void main()\n"
"{\n"
"	float gamma = GammmaContrast.x;\n"
"	float contrast = GammmaContrast.y;\n"
"	uint idx = gl_GlobalInvocationID.x;\n"
"	if (idx >= 256u)\n"
"		return;\n"
"	vec3 color = vec3(UnpackRGB8(Palette[idx])) * (1./255.);\n"
"	color = mix(color, BlendColor.rgb, BlendColor.a);\n"
"	color *= contrast;\n"
"	color = pow(color, vec3(gamma));\n"
"	uvec3 dst = uvec3(clamp(color, 0., 1.) * 255. + .5);\n"
"	DstPalette[idx] = dst.r | (dst.g << 8) | (dst.b << 16) | 0xff000000u;\n"
"}\n";
