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

// vr_retrolight.h -- retro lighting (vr_retrolight.cpp; docs/vr-port/ROUND21.md, "Retro lighting"): the GLSL the
// world and model shaders share (spliced into SHADOW_FUNCTIONS, vr_glsl.h). C and C++.
//
// Light in Quake's coarse look, to go with the retro textures: the baked light in a few levels (bands), optionally
// dithered between them; the lightmap read on its blocky grid; dynamic lights in levels and evaluated on a grid of
// blocks on the surface; shadows hard, blocky, in levels. Everything is fixed to the surface (the face's texel grid,
// the model's skin), never to the screen, so nothing swims as the head moves; grids turn smooth where their blocks get
// smaller than a pixel.
//
// The frame data's RetroLight[6] (all 0: off; vr_retrolight.cpp fills it):
//   [0] the world's baked light: levels a unit (0 none), their edges' softness (share of a level), dither, dither cell (texels)
//   [1] the lightmap's grid: on, its block (texels: 1 .. 16, 16 a luxel), grids' edge softness (pixels), dynamic lights' block on the world (texels, 0 none)
//   [2] the world's dynamic lights: levels a unit (0 none), the levels' spacing (a power: 1 even in light, 0.5 in brightness), -, 1: any of the world's on (the world shader's switch)
//   [3] shadows: filter (0 as set, 1 one tap, 2 the shadow map's texels), levels (0 smooth, 1 on/off), block (texels, 0 none), edges' softness
//   [4] the models' own light: levels a unit, softness, dither, dither cell (skin texels)
//   [5] the models' dynamic lights: levels a unit, block (skin texels, 0 none), 1: any of the models' on (their switch), -

#ifndef QVR_VR_RETROLIGHT_H
#define QVR_VR_RETROLIGHT_H

#define QVR_RETROLIGHT_GLSL \
"// QVR: retro lighting (vr/vr_retrolight.h). The pixel on its surface's grid, set by RetroLightBegin before any discard.\n" \
"vec2 RLTexel = vec2(0.0); // in Quake texels (the world: its lightmap's grid, as the texture's; models: the skin's)\n" \
"vec2 RLTexelDx = vec2(0.0), RLTexelDy = vec2(0.0); // its screen derivatives\n" \
"vec3 RLDs = vec3(0.0), RLDt = vec3(0.0); // world units a texel along the grid's s and t, on the surface\n" \
"float RLDither = 0.0; // the pixel's ordered dither offset (levels), faded where its cells get small on screen\n" \
"\n" \
"float RLBayer2(vec2 a) { a = floor(a); return fract(dot(a, vec2(0.5, a.y * 0.75))); }\n" \
"float RLBayer4(vec2 a) { return RLBayer2(0.5 * a) * 0.25 + RLBayer2(a); } // 0 .. 15/16 over a 4 x 4 cell\n" \
"\n" \
"// texel: the pixel in Quake texels on the surface's grid, dtx and dty its screen derivatives; dpdx, dpdy the\n" \
"// position's. group: its kind's levels, softness, dither, dither cell (RetroLight[0] or [4]).\n" \
"void RetroLightBegin(vec2 texel, vec2 dtx, vec2 dty, vec3 dpdx, vec3 dpdy, vec4 group)\n" \
"{\n" \
"	RLTexel = texel;\n" \
"	RLTexelDx = dtx;\n" \
"	RLTexelDy = dty;\n" \
"	float det = dtx.x * dty.y - dtx.y * dty.x; // the position along the grid's axes: dp = Ds ds + Dt dt\n" \
"	if (abs(det) > 1e-12)\n" \
"	{\n" \
"		RLDs = (dpdx * dty.y - dpdy * dtx.y) / det;\n" \
"		RLDt = (dpdy * dtx.x - dpdx * dty.x) / det;\n" \
"	}\n" \
"	if (group.z > 0.)\n" \
"	{\n" \
"		float size = max(group.w, 1.0);\n" \
"		float cells = max(length(dtx), length(dty)) / size; // cells a pixel\n" \
"		RLDither = (RLBayer4(texel / size) + (1.0 / 32.0 - 0.5)) * group.z * (1.0 - smoothstep(0.35, 0.7, cells));\n" \
"	}\n" \
"}\n" \
"\n" \
"// t (texels) moved towards the nearest centre of a grid of `block` texels (centres at (i + 0.5) * block): flat\n" \
"// blocks with a ramp `soft` pixels wide between neighbouring centres (0: nearest; never past the pixel's own place,\n" \
"// so a lightmap's read stays on its face); back to t where the blocks get smaller than about a pixel.\n" \
"vec2 RetroLightGrid(vec2 t, float block, float soft)\n" \
"{\n" \
"	vec2 c = t / block - 0.5, i = floor(c), f = c - i;\n" \
"	vec2 fp = vec2(length(vec2(RLTexelDx.x, RLTexelDy.x)), length(vec2(RLTexelDx.y, RLTexelDy.y))) / block; // blocks a pixel\n" \
"	vec2 w = clamp(max(soft * fp, (fp - 0.25) * 2.0), 1e-5, 1.0);\n" \
"	f = clamp((f - 0.5) / w + 0.5, 0.0, 1.0);\n" \
"	return (i + f + 0.5) * block;\n" \
"}\n" \
"\n" \
"// pos (the pixel's, on the surface) moved to its block's centre on the grid (block texels; 0: pos).\n" \
"vec3 RetroLightAt(vec3 pos, float block)\n" \
"{\n" \
"	if (block <= 0.)\n" \
"		return pos;\n" \
"	vec2 d = RetroLightGrid(RLTexel, block, RetroLight[1].z) - RLTexel;\n" \
"	return pos + RLDs * d.x + RLDt * d.y;\n" \
"}\n" \
"\n" \
"// Where a dynamic light's shadow is looked up: on the shadows' own grid (RetroLight[3].z), else at lit (the light's).\n" \
"vec3 RetroShadowAt(vec3 pos, vec3 lit)\n" \
"{\n" \
"	return RetroLight[3].z > 0. ? RetroLightAt(pos, RetroLight[3].z) : lit;\n" \
"}\n" \
"\n" \
"// The lightmap's coordinates on its blocky grid (RetroLight[1]): lmsize the atlas in Quake texels (luxels x 16).\n" \
"// RLTexel is the lightmap's grid less 8 (a texel's edges at whole numbers, as the texture's); a block of 16 is a\n" \
"// luxel, centred on it (nearest), smaller ones are texels' (Quake's software renderer lit each texel: 1).\n" \
"vec2 RetroLightmapUV(vec2 lmuv, vec2 lmsize)\n" \
"{\n" \
"	if (RetroLight[1].x <= 0.)\n" \
"		return lmuv;\n" \
"	float b = exp2(clamp(floor(log2(max(RetroLight[1].y, 1.0)) + 0.5), 0.0, 4.0)); // 1, 2, 4, 8 or 16\n" \
"	return RetroLightGrid(RLTexel + 8.0, b, RetroLight[1].z) / lmsize;\n" \
"}\n" \
"\n" \
"// The levels' spacing (RetroLight[2].y): 1 even in light (Quake's colormap), 0.5 even in brightness (more in the shade).\n" \
"float RetroLightSpacing() { return RetroLight[2].y > 0. ? RetroLight[2].y : 1.0; }\n" \
"\n" \
"// Light c in `steps` levels a unit of its brightness (its largest channel; the hue kept), with the pixel's dither;\n" \
"// soft: the levels' edges blurred over that share of a level; aa: a level's change across a pixel (fwidth; 0 none),\n" \
"// the least blur (no crawling edges).\n" \
"vec3 RetroLightQuant(vec3 c, float steps, float soft, float aa)\n" \
"{\n" \
"	float m = max(c.r, max(c.g, c.b));\n" \
"	if (steps <= 0. || m <= 1e-5)\n" \
"		return c;\n" \
"	float g = RetroLightSpacing();\n" \
"	float x = pow(m, g) * steps, y = x + RLDither, i = floor(y), f = y - i;\n" \
"	float w = min(max(soft, aa), 1.0);\n" \
"	float q = i + (w > 1e-4 ? smoothstep(0.5 - 0.5 * w, 0.5 + 0.5 * w, f) : step(0.5, f));\n" \
"	return c * (pow(max(q, 0.0) / steps, 1.0 / g) / m);\n" \
"}\n" \
"float RetroLightAA(vec3 c, float steps) // RetroLightQuant's aa (uniform control flow only)\n" \
"{\n" \
"	return steps > 0. ? fwidth(pow(max(max(c.r, max(c.g, c.b)), 1e-5), RetroLightSpacing()) * steps) : 0.0;\n" \
"}\n" \
"\n" \
"// A shadow's share of light (0 .. 1) in RetroLight[3].y levels (1: lit or not), with the pixel's dither.\n" \
"float RetroShadowQuant(float s)\n" \
"{\n" \
"	float n = RetroLight[3].y;\n" \
"	if (n <= 0. || s >= 1. || s <= 0.) // (fully lit or not: as it is, whatever the dither)\n" \
"		return s;\n" \
"	float y = s * n + RLDither, i = floor(y), f = y - i, w = min(RetroLight[3].w, 1.0);\n" \
"	return clamp((i + (w > 1e-4 ? smoothstep(0.5 - 0.5 * w, 0.5 + 0.5 * w, f) : step(0.5, f))) / n, 0.0, 1.0);\n" \
"}\n" \
"\n" \
"// The world's baked light and its dynamic lights (both before Quake's doubling) in their levels; lightmapped: the\n" \
"// face has a lightmap (not a liquid's or a sky's); aa: derivatives may be taken here.\n" \
"void RetroLightWorld(inout vec3 baked, inout vec3 dyn, bool lightmapped, bool aa)\n" \
"{\n" \
"	float aab = aa ? RetroLightAA(baked, RetroLight[0].x) : 0.0, aad = aa ? RetroLightAA(dyn, RetroLight[2].x) : 0.0;\n" \
"	if (lightmapped)\n" \
"		baked = RetroLightQuant(baked, RetroLight[0].x, RetroLight[0].y, aab);\n" \
"	dyn = RetroLightQuant(dyn, RetroLight[2].x, RetroLight[0].y, aad);\n" \
"}\n" \
"\n" \
"// A model's own light and its dynamic lights (1: the skin as it is, as the world's light doubled) in their levels.\n" \
"void RetroLightModel(inout vec3 own, inout vec3 dyn, bool aa)\n" \
"{\n" \
"	float so = RetroLight[4].x * 0.5, sd = RetroLight[5].x * 0.5;\n" \
"	float aao = aa ? RetroLightAA(own, so) : 0.0, aad = aa ? RetroLightAA(dyn, sd) : 0.0;\n" \
"	own = RetroLightQuant(own, so, RetroLight[4].y, aao);\n" \
"	dyn = RetroLightQuant(dyn, sd, RetroLight[4].y, aad);\n" \
"}\n" \
"\n"

#endif // QVR_VR_RETROLIGHT_H
