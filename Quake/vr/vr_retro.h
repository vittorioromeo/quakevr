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

// vr_retro.h -- retro textures (vr_retro.cpp; docs/vr-port/ROUND21.md, "Retro textures"): the GLSL the world and model
// shaders share (gl_shaders.h, vr_glsl.h) and the sizes the C++ side fills. C and C++.
//
// High-resolution textures (QRP, external packs) are drawn in Quake's chunky look, in the shader that reads them (not
// a post-effect over the image): the texture is read on a coarser grid of blocks (a block: Block Size of the texture's
// own Quake texels, or of world units), each block the average of the texels under it (the mip level of its size, read
// at its centre), its edges sharp but anti-aliased over a pixel or so (no crawl as the head moves), back to plain
// mipmapping where blocks get smaller than a pixel; optionally pulled to Quake's 256 colours (with an ordered dither
// fixed to the blocks, not the screen), the bump and specular maps smooth, blocky or in between.
//
// Each kind of thing drawn (World; later the others) has its own set of settings: a set in the RetroUBO (binding 3),
// chosen per draw call (Call.retro) or instance (Instance.retro), 0 none.

#ifndef QVR_VR_RETRO_H
#define QVR_VR_RETRO_H

#define QVR_RETRO_MAX_SETS 64       // sets in the block (0: none; the categories', then the overrides')
#define QVR_RETRO_UBO_BINDING 3     // the uniform block's binding (0 frame data, 1 the light clusters' input, 2 AO)
#define QVR_RETRO_LUT_UNIT_WORLD 10 // the palette's table (gl_palette_lut, Ironwail's 128^3 nearest-colour index) in the world shader
#define QVR_RETRO_LUT_UNIT_ALIAS 3  // ... in the model shader (free there)
#define QVR_RETRO_LUT_UNIT_GFX 2    // ... in vr_gfx's (decals, Quake VR's particles: vr_gfx_gl.cpp)
#define QVR_RETRO_LUT_UNIT_SPRITE 3 // ... in the sprites' and Quake's particles' (r_sprite.c, r_part.c)

// The settings of a set (3 vec4s), as RetroSets holds them:
//   P0: x block size (> 0), y edge softness (pixels; 0 hard), z the blocks a pixel spans where it is plain mipmapping
//       again (it starts fading at half that), w 1 snapping (0: the texture as it was, only the palette and dither)
//   P1: x palette strength (0..1), y dither strength, z dither cell (blocks), w bumps (0 smooth, 1 the blocks')
//   P2: x 1 block colour = the average under it (its mip level; 0 the texel at its centre), y 1 block size in world
//       units (0: in the texture's own texels), z how much of the detail textures' grain stays (vr_detail), w 1: off (a
//       part of your body whose category is off: one draw, a set a part)
//
// RetroBegin picks the pixel's set (Retro, 0 none) and its grid; RetroSample reads a texture through it (colour,
// palette with quant), RetroAux a bump or specular map (blended by P1.w). Derivatives come from the caller (taken
// before any discard): only textureLod and textureGrad here.
#define QVR_RETRO_GLSL(lut) \
"layout(std140, binding=3) uniform RetroUBO\n" \
"{\n" \
"	vec4	RetroInfo; // x 1: on (vr_retro, not vr_retro_ab); y 1: the palette's table is there\n" \
"	uvec4	RetroPal[64]; // Quake's palette, RGB8 packed, four to a uvec4\n" \
"	vec4	RetroSets[192]; // QVR_RETRO_MAX_SETS sets of 3 (vr_retro.h)\n" \
"};\n" \
"layout(binding=" lut ") uniform usampler3D RetroLUT;\n" \
"int Retro = 0; // the pixel's set (0: none)\n" \
"vec4 RetroP0 = vec4(0.0), RetroP1 = vec4(0.0), RetroP2 = vec4(0.0);\n" \
"vec2 RetroGrid = vec2(1.0); // blocks across the texture (s, t)\n" \
"float RetroFar = 1.0; // 0: blocks; 1: plain mipmapping (blocks under a pixel)\n" \
"\n" \
"float RetroBayer2(vec2 a) { a = floor(a); return fract(dot(a, vec2(0.5, a.y * 0.75))); }\n" \
"float RetroBayer4(vec2 a) { return RetroBayer2(0.5 * a) * 0.25 + RetroBayer2(a); } // 0 to 15/16, a 4 x 4 cell's\n" \
"\n" \
"// set: the set's number; lq: the texture's own size in Quake texels; n: the surface's unit normal (world units only)\n" \
"void RetroBegin(float set, vec2 lq, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n)\n" \
"{\n" \
"	int s = int(set + 0.5);\n" \
"	if (s <= 0 || s >= 64 || RetroInfo.x <= 0.0 || lq.x <= 0.0 || lq.y <= 0.0)\n" \
"		return;\n" \
"	if (RetroSets[s * 3 + 2].w > 0.5) // off (a body part's set: vr_retro.cpp setRun)\n" \
"		return;\n" \
"	RetroP0 = RetroSets[s * 3];\n" \
"	RetroP1 = RetroSets[s * 3 + 1];\n" \
"	RetroP2 = RetroSets[s * 3 + 2];\n" \
"	float block = max(RetroP0.x, 0.01);\n" \
"	RetroGrid = lq / block;\n" \
"	if (RetroP2.y > 0.0) // world units: the texels a unit here (the texture's scale on the face), in eighths of an\n" \
"	{ // octave so that the grid is the same at every pixel of a face\n" \
"		float det = dot(n, cross(dpdx, dpdy));\n" \
"		if (abs(det) > 1e-12)\n" \
"		{\n" \
"			vec3 gs = (cross(dpdy, n) * duvdx.x + cross(n, dpdx) * duvdy.x) / det;\n" \
"			vec3 gt = (cross(dpdy, n) * duvdx.y + cross(n, dpdx) * duvdy.y) / det;\n" \
"			vec2 tpu = max(vec2(length(gs), length(gt)) * lq, vec2(1e-4));\n" \
"			tpu = exp2(floor(log2(tpu) * 8.0 + 0.5) * 0.125);\n" \
"			RetroGrid = lq / (block * tpu);\n" \
"		}\n" \
"	}\n" \
"	vec2 w = (abs(duvdx) + abs(duvdy)) * RetroGrid; // the blocks a pixel spans\n" \
"	float fade = max(RetroP0.z, 0.01);\n" \
"	RetroFar = smoothstep(0.5 * fade, fade, sqrt(w.x * w.y));\n" \
"	Retro = s;\n" \
"}\n" \
"\n" \
"// c pulled to the palette (by P1.x), dithered first by the 4 x 4 pattern of the blocks (cell: a block's index)\n" \
"vec3 RetroQuant(vec3 c, vec2 cell)\n" \
"{\n" \
"	if (RetroP1.x <= 0.0 || RetroInfo.y <= 0.0)\n" \
"		return c;\n" \
"	vec3 q = c;\n" \
"	float d = RetroP1.y * (1.0 - RetroFar); // none where the blocks are under a pixel (it would shimmer)\n" \
"	if (d > 0.0)\n" \
"		q += (RetroBayer4(floor(cell / max(RetroP1.z, 1.0))) - 0.46875) * (d * 0.0625);\n" \
"	uint i = texelFetch(RetroLUT, ivec3(clamp(q, 0.0, 1.0) * 127.0 + 0.5), 0).x;\n" \
"	uint p = RetroPal[i >> 2u][i & 3u];\n" \
"	return mix(c, vec3(uvec3(p, p >> 8u, p >> 16u) & 255u) * (1.0 / 255.0), RetroP1.x);\n" \
"}\n" \
"// A premultiplied colour (decals, particles, glows) pulled to the palette: its colour over its strength (its alpha,\n" \
"// or its brightest channel where it adds more than it covers), the strength kept\n" \
"vec4 RetroQuantPremul(vec4 c, vec2 cell)\n" \
"{\n" \
"	float k = max(c.a, max(c.r, max(c.g, c.b)));\n" \
"	if (k > 1e-4)\n" \
"		c.rgb = RetroQuant(c.rgb / k, cell) * k;\n" \
"	return c;\n" \
"}\n" \
"vec4 RetroTap(sampler2D tex, vec2 cell, float lod, bool quant)\n" \
"{\n" \
"	vec4 c = textureLod(tex, (cell + 0.5) / RetroGrid, lod);\n" \
"	if (quant)\n" \
"		c.rgb = RetroQuant(c.rgb, cell);\n" \
"	return c;\n" \
"}\n" \
"// The blocks: each its colour (the mip level of its size at its centre), the edges blended over P0.y pixels\n" \
"// (anti-aliased nearest: one read inside a block, two or four on its edges)\n" \
"vec4 RetroBlocks(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, bool quant)\n" \
"{\n" \
"	vec2 p = uv * RetroGrid - 0.5; // in blocks, from the first's centre\n" \
"	vec2 i = floor(p), f = p - i;\n" \
"	vec2 w = (abs(duvdx) + abs(duvdy)) * RetroGrid * RetroP0.y;\n" \
"	f = clamp((f - 0.5) / max(w, vec2(1e-4)) + 0.5, 0.0, 1.0);\n" \
"	vec2 size = vec2(textureSize(tex, 0));\n" \
"	vec2 tb = size / RetroGrid, ex = duvdx * size, ey = duvdy * size;\n" \
"	float lod = max(RetroP2.x > 0.0 ? log2(max(max(tb.x, tb.y), 1.0)) : 0.0, 0.5 * log2(max(max(dot(ex, ex), dot(ey, ey)), 1.0)));\n" \
"	bvec2 edge = bvec2(f.x > 0.0 && f.x < 1.0, f.y > 0.0 && f.y < 1.0);\n" \
"	vec2 c = i + vec2(edge.x ? 0.0 : f.x, edge.y ? 0.0 : f.y);\n" \
"	vec4 r = RetroTap(tex, c, lod, quant);\n" \
"	if (edge.x)\n" \
"		r = mix(r, RetroTap(tex, c + vec2(1.0, 0.0), lod, quant), f.x);\n" \
"	if (edge.y)\n" \
"	{\n" \
"		vec4 r2 = RetroTap(tex, c + vec2(0.0, 1.0), lod, quant);\n" \
"		if (edge.x)\n" \
"			r2 = mix(r2, RetroTap(tex, c + vec2(1.0, 1.0), lod, quant), f.x);\n" \
"		r = mix(r, r2, f.y);\n" \
"	}\n" \
"	return r;\n" \
"}\n" \
"vec4 RetroSmooth(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, bool quant)\n" \
"{\n" \
"	vec4 c = textureGrad(tex, uv, duvdx, duvdy);\n" \
"	if (quant)\n" \
"		c.rgb = RetroQuant(c.rgb, floor(uv * RetroGrid));\n" \
"	return c;\n" \
"}\n" \
"// A colour texture through the pixel's set (Retro > 0): blocks, faded to plain mipmapping far off\n" \
"vec4 RetroSample(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, bool quant)\n" \
"{\n" \
"	if (RetroP0.w <= 0.0 || RetroFar >= 1.0)\n" \
"		return RetroSmooth(tex, uv, duvdx, duvdy, quant);\n" \
"	vec4 c = RetroBlocks(tex, uv, duvdx, duvdy, quant);\n" \
"	return RetroFar > 0.0 ? mix(c, RetroSmooth(tex, uv, duvdx, duvdy, quant), RetroFar) : c;\n" \
"}\n" \
"// A bump or specular map: smooth, or blended towards the blocks by P1.w (textureGrad as before with no set)\n" \
"vec4 RetroAux(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy)\n" \
"{\n" \
"	vec4 s = textureGrad(tex, uv, duvdx, duvdy);\n" \
"	float k = Retro > 0 && RetroP0.w > 0.0 ? RetroP1.w * (1.0 - RetroFar) : 0.0;\n" \
"	return k > 0.0 ? mix(s, RetroBlocks(tex, uv, duvdx, duvdy, false), k) : s;\n" \
"}\n"

// The engine's calls (VR_RetroUpload, VR_RetroBind, VR_RetroCall, VR_RetroInstance) are declared in vr_api_render.h.

#endif // QVR_VR_RETRO_H
