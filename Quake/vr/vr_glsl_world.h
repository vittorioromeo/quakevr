// vr_glsl_world.h -- Quake VR's GLSL, the world shaders' pieces: wounds on boxes, decals, permanent decals, light: part of vr_glsl.h (included there, in this order; macros
// only, spliced into gl_shaders.h's shaders by name).

#pragma once

// the world vertex shader's Quake VR outputs
#define QVR_WORLD_VS_OUTPUTS \
"	layout(location=11) flat out uvec4 out_nmsampler; // QVR: the normal map, the specular map\n" \
"#endif\n" \
"layout(location=10) flat out float out_glow; // QVR\n" \
"layout(location=12) flat out float out_pdepth; // QVR: parallax mapping\n" \
"layout(location=13) flat out vec4 out_uvclamp; // QVR\n" \
"layout(location=14) flat out vec4 out_detail; // QVR: detail textures (vr/vr_detail.cpp)\n" \
"layout(location=15) flat out float out_aoself; // QVR: dynamic ambient occlusion (vr/vr_ao.cpp)\n" \
"layout(location=16) flat out vec4 out_extmat; // QVR: the external maps' numbers (vr/vr_extmaps.cpp)\n" \
"layout(location=17) out vec3 out_boxpos; // QVR: a held prop's blood (BoxWounds): where in its box, a unit its largest side\n" \
"layout(location=18) flat out vec4 out_wound; // QVR: its box mask (Instance.wound)\n" \
"layout(location=19) flat out vec4 out_retro; // QVR: retro textures (vr_retro.h): xy the texture's Quake size, z the set\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	layout(location=20) out float out_rim; // QVR: 1 + the distance to the shore (the swells' mesh), 0 unknown: the foam\n"

// the world fragment shader's Quake VR inputs
#define QVR_WORLD_FS_INPUTS \
"	layout(location=11) flat in uvec4 in_nmsampler; // QVR: the normal map, the specular map\n" \
"#endif\n" \
"layout(location=10) flat in float in_glow; // QVR\n" \
"layout(location=12) flat in float in_pdepth; // QVR: parallax mapping\n" \
"layout(location=13) flat in vec4 in_uvclamp; // QVR\n" \
"layout(location=14) flat in vec4 in_detail; // QVR: detail textures (vr/vr_detail.cpp)\n" \
"layout(location=15) flat in float in_aoself; // QVR: dynamic ambient occlusion's own group (vr/vr_ao.cpp)\n" \
"layout(location=16) flat in vec4 in_extmat; // QVR: the external maps' numbers (vr/vr_extmaps.cpp)\n" \
"layout(location=17) in vec3 in_boxpos; // QVR: a held prop's blood (BoxWounds)\n" \
"layout(location=18) flat in vec4 in_wound; // QVR: its box mask: x its layer + 1 (0 none), yz its size in texels, w the blood's opacity\n" \
"layout(location=19) flat in vec4 in_retro; // QVR: retro textures (vr_retro.h): xy the texture's Quake size, z the set\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	layout(location=20) in float in_rim; // QVR: the shoreline foam's distance (LiquidFoam)\n"

// QVR: blood on a brush model you held (vr/vr_wounds.cpp, "Blood on your props": an ammo or health box, an explosive
// box): its mask has no skin to follow, so it is laid out on its box, six cells (3 x 2: +x -x +y -y +z -z), each the side
// of the box facing that way, its two other axes across it over the box's largest side (q: where in the box from its
// centre, a unit that side). A surface reads the cell of the way it faces most (its normal's largest axis, in the
// model's own frame) and of the side of the centre it is on. Read and dithered as the monsters' masks (WoundsAt): sharp,
// chunky, Quake's reds. x blood, y char, z wet (0 or 1), w the blood's red.
#define QVR_BOX_WOUNDS \
"layout(binding=13) uniform sampler2DArray WoundMasks; // QVR: (vr/vr_wounds.cpp)\n" \
"vec4 BoxWounds(vec3 q, vec4 wi)\n" \
"{\n" \
"	vec3 n = abs(cross(dFdx(q), dFdy(q)));\n" \
"	int a = n.x >= n.y && n.x >= n.z ? 0 : n.y >= n.z ? 1 : 2;\n" \
"	int r = a * 2 + (q[a] >= 0.0 ? 0 : 1);\n" \
"	vec2 uv = (a == 0 ? q.yz : a == 1 ? q.xz : q.xy) + 0.5;\n" \
"	vec2 c = (vec2(float(r % 3), float(r / 3)) + clamp(uv, 0.0, 0.999)) / vec2(3.0, 2.0);\n" \
"	ivec2 t = ivec2(c * wi.yz);\n" \
"	vec4 m = texelFetch(WoundMasks, ivec3(t, int(wi.x) - 1), 0);\n" \
"	if (m.rgb == vec3(0.0))\n" \
"		return vec4(0.0);\n" \
"	ivec2 it = t & 3;\n" \
"	const float bayer[16] = float[16](0., 8., 2., 10., 12., 4., 14., 6., 3., 11., 1., 9., 15., 7., 13., 5.);\n" \
"	float d = (bayer[it.y * 4 + it.x] + 0.5) / 16.0;\n" \
"	return vec4(step(0.2 + 0.24 * d, m.r), step(0.16 + 0.28 * d, m.g), step(0.06 + 0.4 * d, m.b),\n" \
"		(m.r > 0.82 ? 47. : m.r > 0.62 ? 63. : 87.) / 255.0);\n" \
"}\n"

// QVR: a held prop's blood (BoxWounds) on its texture, before its light: char, blood (as much as vr_wounds_blood_alpha
// leaves of the texture), wet darker; its fullbrights covered
#define QVR_WORLD_FS_WOUNDS \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_SOLID) "\n" \
"	if (in_wound.x > 0.5)\n" \
"	{\n" \
"		vec4 bw = BoxWounds(in_boxpos, in_wound);\n" \
"		float lum = dot(result.rgb, vec3(0.3, 0.59, 0.11));\n" \
"		result.rgb = mix(result.rgb, vec3(0.26, 0.2, 0.15) * (0.35 + 0.9 * lum), bw.y);\n" \
"		result.rgb = mix(result.rgb, vec3(bw.w, 0.0, 0.0) * (0.75 + 0.5 * lum), bw.x * in_wound.w);\n" \
"		result.rgb *= 1.0 - 0.5 * bw.z;\n" \
"		fullbright *= 1.0 - max(bw.x, bw.y);\n" \
"	}\n" \
"#endif\n"

// QVR: a brush model drawn into its box mask (BoxWounds; vr/vr_wounds.cpp): no vertices, 36 (two triangles a side of its
// box, gl_VertexID), each side over its cell, with where it is in the world and its normal for the wound paint's
// fragment shader (QVR_WOUND_PAINT_FS). The box's sides stand for its surfaces (an item's box is its box).
#define QVR_BOX_WOUND_PAINT_VS \
"layout(location=81) uniform vec4 BoxWorld[3]; // its world matrix (transposed 4x3, as the world's Instance.mat)\n" \
"layout(location=84) uniform vec4 BoxCentre; // xyz its box's centre (the model's frame), w its largest side\n" \
"layout(location=85) uniform vec4 BoxHalf; // xyz half its sides\n" \
"\n" \
"layout(location=0) out vec2 out_texcoord;\n" \
"layout(location=2) out vec3 out_pos;\n" \
"layout(location=3) out vec3 out_nor;\n" \
"\n" \
"void main()\n" \
"{\n" \
"	const vec2 corner[6] = vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));\n" \
"	int r = gl_VertexID / 6;\n" \
"	vec2 uv = corner[gl_VertexID % 6];\n" \
"	int a = r / 2;\n" \
"	float s = (r % 2) == 0 ? 1.0 : -1.0;\n" \
"	vec2 w = (uv - 0.5) * BoxCentre.w;\n" \
"	vec3 q = a == 0 ? vec3(s * BoxHalf.x, w) : a == 1 ? vec3(w.x, s * BoxHalf.y, w.y) : vec3(w, s * BoxHalf.z);\n" \
"	vec3 nrm = a == 0 ? vec3(s, 0.0, 0.0) : a == 1 ? vec3(0.0, s, 0.0) : vec3(0.0, 0.0, s);\n" \
"	mat4x3 world = transpose(mat3x4(BoxWorld[0], BoxWorld[1], BoxWorld[2]));\n" \
"	mat3 rot = mat3(world[0], world[1], world[2]);\n" \
"	out_pos = rot * (BoxCentre.xyz + q) + world[3];\n" \
"	out_nor = normalize(rot * nrm);\n" \
"	out_texcoord = uv;\n" \
"	vec2 c = (vec2(float(r % 3), float(r / 3)) + uv) / vec2(3.0, 2.0);\n" \
"	gl_Position = vec4(c * 2.0 - 1.0, 0.0, 1.0);\n" \
"}\n"

// QVR: the decals on the world's own surfaces (vr/vr_decals.cpp, vr_decals_world), applied in its shader as DOOM (2016)
// and its clustered forward decals do: each pixel finds the decals over it and puts them on its texture, before its
// light, where the parallax mapping moved it (so they follow the relief as the texture does, and take the light, the
// sheen and the fog as it does). The decals are found in a grid over the world, 32 units a cell, hashed into buckets
// (DecalGrid: [0] the buckets' mask, then each bucket's list's start << 8 | its length (255 at most: the marks whose
// cells there reach their rectangle, each listed in all its buckets or none: vr_decals.cpp worldBuckets), then the lists). A decal is on a face turned towards its normal (60 degrees at most) whose plane passes within n.w of its
// middle, over its rectangle (projected along its normal), as clipToWorld laid the meshes. Spreading (a pool, a run), darkening (drying), showing
// and fading are worked out from its age, as appendDecal's; with retro textures on decals, read through their set: m = its texel (premultiplied, times its colour) + 1 - its
// alpha, on the texture as the meshes' modulating blend put it on the lit scene (multiplications: in any order).
#define QVR_DECAL_FUNCTIONS \
"struct Decal\n" \
"{\n" \
"	vec4	centre; // xyz its middle, w its cell in the atlas (8 x 5)\n" \
"	vec4	u; // xyz its length's way (a unit), w half its length\n" \
"	vec4	v; // xyz its width's way, w half its width\n" \
"	vec4	n; // xyz the normal of the surface it was laid on, w how far from its plane a surface takes it\n" \
"	vec4	time; // x when it shows (DecalClock.x's clock), y seconds it spreads over (0: at once), z from this part of its size, w how much darker it gets (+2: spreading from its -u end, a run)\n" \
"	vec4	carve; // x its crater's depth in units (0: flat), y its radius, z its rim's height over its depth, w its dark core's part of the radius + 2 x its shape (0 a pit, 1 a nail's hole, 2 a blast's crater)\n" \
"};\n" \
"layout(std430, binding=3) restrict readonly buffer DecalBuffer\n" \
"{\n" \
"	Decal Decals[];\n" \
"};\n" \
"layout(std430, binding=4) restrict readonly buffer DecalGridBuffer\n" \
"{\n" \
"	uint DecalGrid[];\n" \
"};\n" \
"layout(binding=14) uniform sampler2D DecalAtlas;\n" \
"\n" \
"// How far along the surface the parallax mapping moved this pixel's texture coordinates (by duv), in the world.\n" \
"vec3 ParallaxShiftWorld(vec2 duv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;\n" \
"	if (abs(det) < 1e-12)\n" \
"		return vec3(0.0);\n" \
"	float a = (duv.x * duvdy.y - duv.y * duvdy.x) / det;\n" \
"	float b = (duvdx.x * duv.y - duvdx.y * duv.x) / det;\n" \
"	return dpdx * a + dpdy * b;\n" \
"}\n" \
"// ... and back: a move along the surface (in the world) as one of its texture coordinates (none if the frame is flat).\n" \
"vec2 WorldShiftUV(vec3 s, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	float xx = dot(dpdx, dpdx), xy = dot(dpdx, dpdy), yy = dot(dpdy, dpdy);\n" \
"	float det = xx * yy - xy * xy;\n" \
"	if (abs(det) < 1e-20)\n" \
"		return vec2(0.0);\n" \
"	float sx = dot(dpdx, s), sy = dot(dpdy, s);\n" \
"	return duvdx * ((sx * yy - sy * xy) / det) + duvdy * ((sy * xx - sx * xy) / det);\n" \
"}\n" \
"\n" \
"// QVR: the carved decals (vr_decal_carve). A round mark's ragged edge: its radius's factor along dir (a few harmonics of\n" \
"// the angle, made from the direction itself: no atan), turned by rot (each mark's own), `amount` of it.\n" \
"float CarveRag(vec2 dir, vec2 rot, float amount)\n" \
"{\n" \
"	float c = dir.x * rot.x - dir.y * rot.y, s = dir.x * rot.y + dir.y * rot.x;\n" \
"	return 1.0 + amount * (0.08 * (4.0 * c * c * c - 3.0 * c) + 0.06 * (2.0 * c * s) + 0.04 * (3.0 * s - 4.0 * s * s * s));\n" \
"}\n" \
"// A round mark's height at x (its radius 1): k.y its depth, k.z its rim's height, k.w its shape (0 a pit: a bowl; 1 a\n" \
"// nail's hole: steep walls; 2 a blast's crater: wide and smooth; 3 a blunt blow's dent: shallow, rounded).\n" \
"float CarveProfile(float x, vec4 k)\n" \
"{\n" \
"	float x2 = x * x, b = max(1.0 - x2, 0.0);\n" \
"	float bowl = k.w < 0.5 ? b : k.w < 1.5 ? sqrt(max(1.0 - x2 * x2, 0.0)) : k.w < 2.5 ? b * (1.0 - 0.35 * x2) : b * sqrt(b);\n" \
"	float rr = (x - 1.0) / (k.w > 2.5 ? 0.4 : k.w > 1.5 ? 0.35 : 0.25);\n" \
"	return -k.y * bowl + k.z * exp(-rr * rr);\n" \
"}\n" \
"// A blade's groove (shape 4) at q: along u its length (k.x * e each way), across v its width (k.x each way), its depth\n" \
"// and width tapering to its ends, a rounded V across. facet: a straight V, the taper in 3 steps each way.\n" \
"float CarveGroove(vec2 q, vec4 k, float e, bool facet)\n" \
"{\n" \
"	float xs = abs(q.x) / (k.x * e), ta;\n" \
"	if (facet)\n" \
"	{\n" \
"		float xf = xs * 3.0, x0 = floor(xf) * (1.0 / 3.0), x1 = x0 + 1.0 / 3.0;\n" \
"		ta = mix(sqrt(max(1.0 - x0 * x0, 0.0)), sqrt(max(1.0 - x1 * x1, 0.0)), fract(xf));\n" \
"	}\n" \
"	else\n" \
"		ta = sqrt(max(1.0 - xs * xs, 0.0));\n" \
"	float yn = abs(q.y) / (k.x * (0.3 + 0.7 * ta));\n" \
"	float v = max(1.0 - yn, 0.0);\n" \
"	v = facet ? v : v * (2.0 - v);\n" \
"	float rr = (yn - 1.0) / 0.35;\n" \
"	return (-k.y * v + k.z * exp(-rr * rr)) * ta;\n" \
"}\n" \
"// A carved mark's height over the surface at q (in units, about its middle, along its u and v), k: its radius (a\n" \
"// groove's half width), depth, rim's height and shape (CarveProfile's, 4 a groove e times as long as wide); rot turns its\n" \
"// ragged edge. facet (vr_decal_carve_style 2): low-poly, flat facets: 8 sides round, 3 rings out (a polygon's corners on\n" \
"// the ragged edge, the height linear across each ring of each side).\n" \
"float CarveHeight(vec2 q, vec4 k, vec2 rot, float e, bool facet)\n" \
"{\n" \
"	if (k.w > 3.5)\n" \
"		return CarveGroove(q, k, e, facet);\n" \
"	float amount = k.w > 2.5 ? 0.5 : 1.0;\n" \
"	if (facet)\n" \
"	{\n" \
"		const float sec = 0.78539816;\n" \
"		float i0 = floor(atan(q.y, q.x == 0.0 && q.y == 0.0 ? 1.0 : q.x) / sec) * sec;\n" \
"		vec2 d0 = vec2(cos(i0), sin(i0)), d1 = vec2(d0.x - d0.y, d0.x + d0.y) * 0.70710678; // (the next corner: 45 degrees on)\n" \
"		vec2 v0 = d0 * (k.x * CarveRag(d0, rot, amount)), v1 = d1 * (k.x * CarveRag(d1, rot, amount));\n" \
"		float x = (q.x * (v1.y - v0.y) + q.y * (v0.x - v1.x)) / (v0.x * v1.y - v0.y * v1.x); // q = a v0 + b v1: x = a + b\n" \
"		float xf = x * 3.0, x0 = floor(xf) * (1.0 / 3.0);\n" \
"		return mix(CarveProfile(x0, k), CarveProfile(x0 + 1.0 / 3.0, k), fract(xf));\n" \
"	}\n" \
"	float r = length(q);\n" \
"	vec2 dir = r > 1e-4 ? q / r : vec2(1.0, 0.0);\n" \
"	return CarveProfile(r / (k.x * CarveRag(dir, rot, amount)), k);\n" \
"}\n" \
"// The retro style (vr_decal_carve_style 1): q moved to the middle of the wall's texel block it is in (toB: the mark's\n" \
"// u, v to the texture's blocks; b0: the blocks at its middle; fromB back).\n" \
"vec2 CarveSnap(vec2 q, bool snap, mat2 toB, mat2 fromB, vec2 b0)\n" \
"{\n" \
"	return snap ? fromB * (floor(toB * q + b0) + 0.5 - b0) : q;\n" \
"}\n" \
"// The height the ray meets: snapped, a block's height is its middle's, in steps of a quarter of the depth.\n" \
"float CarveSample(vec2 q, vec4 k, vec2 rot, float e, bool facet, bool snap, mat2 toB, mat2 fromB, vec2 b0)\n" \
"{\n" \
"	if (!snap)\n" \
"		return CarveHeight(q, k, rot, e, facet);\n" \
"	float st = max(k.y * 0.25, 0.05);\n" \
"	return floor(CarveHeight(CarveSnap(q, true, toB, fromB, b0), k, rot, e, false) / st + 0.5) * st;\n" \
"}\n" \
"\n" \
"// The first carved decal at p (where the parallax mapping put the surface; q its own point, facing its normal, dpdx and\n" \
"// dpdy its derivatives across the screen): the eye's ray marched into its crater (parallax occlusion: steps down from\n" \
"// its rim's top to its floor, the hit refined between the last two), as many steps as the ray crosses pixels (fewer far\n" \
"// off, 3 at least, DecalCarve.z at most). shift: how far along the surface the hit is from p; bend: its normal there\n" \
"// less the decal's; dark: its colour's factor (deeper darker, its core darker still); light: where the baked light is\n" \
"// guessed to come from (its v: turned towards the light that reaches it, vr_decals.cpp). Flat (false): none here,\n" \
"// beyond DecalCarve.w, or one spanning a pixel or two (it flattens as it shrinks, and as it fades out). tuv: the\n" \
"// texture's coordinates at p, duvdx and duvdy their derivatives, grid: its texel blocks across it (the retro style's;\n" \
"// DecalCarve.z's 128s: the style, 0 smooth, 1 retro, 2 faceted, 3 retro on retro textures' blocks, else smooth).\n" \
"bool CarveAt(vec3 p, vec3 q, vec3 facing, vec3 dpdx, vec3 dpdy, vec2 tuv, vec2 duvdx, vec2 duvdy, vec2 grid, out vec3 shift, out vec3 bend, out float dark, out vec3 light)\n" \
"{\n" \
"	shift = vec3(0.0);\n" \
"	bend = vec3(0.0);\n" \
"	dark = 1.0;\n" \
"	light = facing;\n" \
"	ivec3 c = ivec3(floor(p * (1.0 / 32.0)));\n" \
"	uint h = (uint(c.x) * 73856093u) ^ (uint(c.y) * 19349663u) ^ (uint(c.z) * 83492791u);\n" \
"	uint mask = DecalGrid[0];\n" \
"	uint bucket = DecalGrid[1u + (h & mask)];\n" \
"	uint first = 2u + mask + (bucket >> 8), count = bucket & 255u;\n" \
"	if (count == 0u)\n" \
"		return false;\n" \
"	vec3 eye = p - EyePos;\n" \
"	float dist = length(eye);\n" \
"	float fade = 1.0 - smoothstep(0.5 * DecalCarve.w, DecalCarve.w, dist);\n" \
"	if (fade <= 0.0)\n" \
"		return false;\n" \
"	vec3 view = eye / max(dist, 1e-3);\n" \
"	float foot = max(max(length(dpdx), length(dpdy)), 1e-4); // units a pixel\n" \
"	float style = floor(DecalCarve.z * (1.0 / 128.0)), maxSteps = DecalCarve.z - 128.0 * style;\n" \
"	if (style > 2.5)\n" \
"		style = Retro > 0 && RetroP0.w > 0.0 && RetroFar < 1.0 ? 1.0 : 0.0;\n" \
"	bool facet = style > 1.5, retro = style > 0.5 && style < 1.5 && grid.x > 0.0 && grid.y > 0.0;\n" \
"	for (uint k = 0u; k < count; k++)\n" \
"	{\n" \
"		uint at = DecalGrid[first + k];\n" \
"		vec4 cv = Decals[at].carve;\n" \
"		if (cv.x <= 0.0) // flat (blood, a scorch of lightning): the early way out\n" \
"			continue;\n" \
"		vec4 n4 = Decals[at].n, c4 = Decals[at].centre;\n" \
"		if (dot(facing, n4.xyz) < 0.5 || abs(dot(c4.xyz - q, facing)) > n4.w)\n" \
"			continue;\n" \
"		vec4 u4 = Decals[at].u, v4 = Decals[at].v;\n" \
"		vec3 off = p - c4.xyz;\n" \
"		vec2 q0 = vec2(dot(off, u4.xyz), dot(off, v4.xyz));\n" \
"		float R = cv.y;\n" \
"		float el = cv.w > 7.5 ? max(u4.w / max(v4.w, 1e-3), 1.0) : 1.0; // a groove's length over its width\n" \
"		vec2 sc = vec2(1.0 / el, 1.0), q0s = q0 * sc;\n" \
"		float age = DecalClock.x - Decals[at].time.x;\n" \
"		float a = clamp((DecalClock.y - age) / 5.0, 0.0, 1.0);\n" \
"		float strength = fade * a * smoothstep(1.0, 3.0, R / foot); // (flattening far off, small on the screen, fading out)\n" \
"		float D = cv.x * DecalCarve.x * strength;\n" \
"		if (age < 0.0 || D < 0.02 || dot(q0s, q0s) > R * R * 9.0)\n" \
"			continue;\n" \
"		float rimH = D * cv.z * DecalCarve.y;\n" \
"		float shape = floor(cv.w * 0.5);\n" \
"		vec4 kk = vec4(R, D, rimH, shape);\n" \
"		vec3 n = n4.xyz;\n" \
"		float vz = min(dot(view, n), -0.12); // into it (grazing rays: their reach bounded)\n" \
"		vec2 slope = vec2(dot(view, u4.xyz), dot(view, v4.xyz)) / -vz; // along it per unit down\n" \
"		// the ray from the rim's top to the floor: missing the crater and its rim (1.6 radii), flat here\n" \
"		vec2 ra = q0 - slope * rimH, rb = q0 + slope * D, ab = rb - ra;\n" \
"		vec2 near = (ra + ab * clamp(-dot(ra, ab) / max(dot(ab, ab), 1e-8), 0.0, 1.0)) * sc;\n" \
"		if (dot(near, near) > R * R * 2.56)\n" \
"			continue;\n" \
"		float ang = fract(sin(dot(c4.xyz, vec3(12.9898, 78.233, 37.719))) * 43758.5453) * 6.2831853;\n" \
"		vec2 rot = vec2(cos(ang), sin(ang));\n" \
"		// The retro style: the heights block by block, the wall's texel blocks (a block a pixel and a half wide or more)\n" \
"		bool snap = false;\n" \
"		mat2 toB = mat2(1.0), fromB = mat2(1.0);\n" \
"		vec2 b0 = vec2(0.0);\n" \
"		if (retro)\n" \
"		{\n" \
"			toB = mat2(WorldShiftUV(u4.xyz, duvdx, duvdy, dpdx, dpdy) * grid, WorldShiftUV(v4.xyz, duvdx, duvdy, dpdx, dpdy) * grid);\n" \
"			float bd = abs(determinant(toB));\n" \
"			snap = bd > 1e-8 && inversesqrt(bd) > foot * 1.5;\n" \
"			if (snap)\n" \
"			{\n" \
"				fromB = inverse(toB);\n" \
"				b0 = tuv * grid - toB * q0;\n" \
"			}\n" \
"		}\n" \
"		float steps = clamp(ceil(length(ab) / foot * 0.5) + 2.0, 3.0, max(maxSteps, 3.0));\n" \
"		float ds = (D + rimH) / steps;\n" \
"		float s0 = -rimH, g0 = s0 + CarveSample(ra, kk, rot, el, facet, snap, toB, fromB, b0), s1 = s0, g1 = g0; // g: the ray's depth past the surface's\n" \
"		for (float i = 1.0; i <= steps; i += 1.0)\n" \
"		{\n" \
"			s1 = -rimH + ds * i;\n" \
"			g1 = s1 + CarveSample(q0 + slope * s1, kk, rot, el, facet, snap, toB, fromB, b0);\n" \
"			if (g1 >= 0.0)\n" \
"				break;\n" \
"			s0 = s1;\n" \
"			g0 = g1;\n" \
"		}\n" \
"		float sh = g1 > g0 && !snap ? mix(s0, s1, clamp(-g0 / (g1 - g0), 0.0, 1.0)) : s1;\n" \
"		vec2 qh = q0 + slope * sh;\n" \
"		vec2 qn = CarveSnap(qh, snap, toB, fromB, b0); // (snapped: the block's normal, its middle's)\n" \
"		float e = max(R * 0.06, 0.02);\n" \
"		float h0 = CarveHeight(qn, kk, rot, el, facet);\n" \
"		float hh = snap ? CarveSample(qh, kk, rot, el, facet, snap, toB, fromB, b0) : h0;\n" \
"		vec2 g = vec2(CarveHeight(qn + vec2(e, 0.0), kk, rot, el, facet) - h0, CarveHeight(qn + vec2(0.0, e), kk, rot, el, facet) - h0) / e;\n" \
"		bend = normalize(n - u4.xyz * g.x - v4.xyz * g.y) - n;\n" \
"		shift = u4.xyz * (qh.x - q0.x) + v4.xyz * (qh.y - q0.y);\n" \
"		float deep = clamp(-hh / D, 0.0, 1.0);\n" \
"		float core = cv.w - 2.0 * shape;\n" \
"		dark = 1.0 - 0.45 * deep * deep;\n" \
"		if (core > 0.0)\n" \
"			dark *= mix(0.22, 1.0, smoothstep(core * 0.55, core, length(qn) / R));\n" \
"		dark = mix(1.0, dark, strength);\n" \
"		light = normalize(n * 0.6 + v4.xyz * 0.8);\n" \
"		return true;\n" \
"	}\n" \
"	return false;\n" \
"}\n" \
"\n" \
"// The decals' colour on the texture at p (where the parallax mapping put it; q: the surface's own point there, facing\n" \
"// its normal, dpdx and dpdy its derivatives across the screen).\n" \
"vec3 DecalsAt(vec3 p, vec3 q, vec3 facing, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	vec3 m = vec3(1.0);\n" \
"	ivec3 c = ivec3(floor(p * (1.0 / 32.0)));\n" \
"	uint h = (uint(c.x) * 73856093u) ^ (uint(c.y) * 19349663u) ^ (uint(c.z) * 83492791u);\n" \
"	uint mask = DecalGrid[0];\n" \
"	uint bucket = DecalGrid[1u + (h & mask)];\n" \
"	uint first = 2u + mask + (bucket >> 8), count = bucket & 255u;\n" \
"	const vec2 inset = vec2(0.5 / 2048.0, 0.5 / 1280.0);\n" \
"	const vec2 cellsize = vec2(1.0 / 8.0, 1.0 / 5.0);\n" \
"	const vec2 span = cellsize - 2.0 * inset;\n" \
"	for (uint k = 0u; k < count; k++)\n" \
"	{\n" \
"		Decal d = Decals[DecalGrid[first + k]];\n" \
"		if (dot(facing, d.n.xyz) < 0.5 || abs(dot(d.centre.xyz - q, facing)) > d.n.w) // its middle within n.w of this face's plane\n" \
"			continue;\n" \
"		// outside its rectangle at its full size (it only spreads to it; a little larger, for rounding): no more worked out\n" \
"		vec3 off = p - d.centre.xyz;\n" \
"		if (abs(dot(off, d.u.xyz)) > d.u.w * 1.001 + 0.01 || abs(dot(off, d.v.xyz)) > d.v.w * 1.001 + 0.01)\n" \
"			continue;\n" \
"		float age = DecalClock.x - d.time.x;\n" \
"		float a = clamp((DecalClock.y - age) / 5.0, 0.0, 1.0);\n" \
"		if (age < 0.0 || a <= 0.0)\n" \
"			continue;\n" \
"		bool run = d.time.w >= 1.5;\n" \
"		float darken = run ? d.time.w - 2.0 : d.time.w;\n" \
"		float s = 1.0;\n" \
"		if (d.time.y > 0.0)\n" \
"		{\n" \
"			float t = 1.0 - min(1.0, age / d.time.y);\n" \
"			s = d.time.z + (1.0 - d.time.z) * (1.0 - t * t);\n" \
"		}\n" \
"		float su = max(s, 1e-3), sv = run ? 1.0 : su;\n" \
"		// where in it at its full size: drawn in towards where it spreads from (its middle; a run's top, its -u end)\n" \
"		float from = run ? -d.u.w : 0.0;\n" \
"		vec2 st = vec2((from + (dot(off, d.u.xyz) - from) / su) / d.u.w, dot(off, d.v.xyz) / (sv * d.v.w)) * 0.5 + 0.5;\n" \
"		if (any(lessThan(st, vec2(0.0))) || any(greaterThan(st, vec2(1.0))))\n" \
"			continue;\n" \
"		int cell = int(d.centre.w + 0.5);\n" \
"		vec2 at = vec2(float(cell % 8), float(cell / 8)) * cellsize + inset + st * span;\n" \
"		vec2 k2 = 0.5 * span / vec2(su * d.u.w, sv * d.v.w);\n" \
"		vec2 gx = vec2(dot(dpdx, d.u.xyz), dot(dpdx, d.v.xyz)) * k2;\n" \
"		vec2 gy = vec2(dot(dpdy, d.u.xyz), dot(dpdy, d.v.xyz)) * k2;\n" \
"		// with retro textures on decals (vr_retro): as the meshes' shader read them, the atlas's whole size its Quake\n" \
"		// texels; blocky, pulled to the palette, premultiplied (not in the programs made without them, NO_DECAL_RETRO:\n" \
"		// R_ChooseBModelProgram's while the decals have no retro set; the code alone cost a quarter of the marks' time)\n" \
"#ifndef NO_DECAL_RETRO\n" \
"		if (DecalClock.w > 0.5)\n" \
"		{\n" \
"			// the world's own retro set (its normal and specular maps are read after): kept, the decal's own used here\n" \
"			int world_retro = Retro;\n" \
"			vec4 world_p0 = RetroP0, world_p1 = RetroP1, world_p2 = RetroP2;\n" \
"			vec2 world_grid = RetroGrid;\n" \
"			float world_far = RetroFar;\n" \
"			Retro = 0;\n" \
"			RetroBegin(DecalClock.w, vec2(2048.0, 1280.0), gx, gy, dpdx, dpdy, facing);\n" \
"			if (Retro > 0)\n" \
"			{\n" \
"				float dk = 1.0 - darken * min(1.0, age / max(d.time.y, 8.0));\n" \
"				RetroLodReads = true;\n" \
"				vec4 q = RetroQuantPremul(RetroSample(DecalAtlas, at, gx, gy, false) * vec4(vec3(a * dk), a), floor(at * RetroGrid));\n" \
"				RetroLodReads = false;\n" \
"				m *= q.rgb + (1.0 - q.a);\n" \
"			}\n" \
"			bool decal_retro = Retro > 0;\n" \
"			Retro = world_retro;\n" \
"			RetroP0 = world_p0;\n" \
"			RetroP1 = world_p1;\n" \
"			RetroP2 = world_p2;\n" \
"			RetroGrid = world_grid;\n" \
"			RetroFar = world_far;\n" \
"			if (decal_retro)\n" \
"				continue;\n" \
"		}\n" \
"#endif\n" \
"		// filtered over the pixel's footprint by hand: the mip level of its narrower way, up to 4 reads along the longer\n" \
"		// (textureGrad's own, in llvmpipe, put a faint line along some rows of pixel quads)\n" \
"		vec2 tx = gx * vec2(2048.0, 1280.0), ty = gy * vec2(2048.0, 1280.0);\n" \
"		float lx = dot(tx, tx), ly = dot(ty, ty);\n" \
"		vec2 major = lx > ly ? gx : gy;\n" \
"		float lmajor = sqrt(max(lx, ly)), lminor = sqrt(min(lx, ly));\n" \
"		float reads = clamp(ceil(lmajor / max(lminor, 1e-4)), 1.0, 4.0);\n" \
"		float lod = log2(max(max(lminor, lmajor / reads), 1e-4));\n" \
"		vec4 texel = vec4(0.0);\n" \
"		for (float r = 0.0; r < reads; r += 1.0)\n" \
"			texel += textureLod(DecalAtlas, at + major * ((r + 0.5) / reads - 0.5), lod);\n" \
"		texel /= reads;\n" \
"		float dark = 1.0 - darken * min(1.0, age / max(d.time.y, 8.0));\n" \
"		m *= texel.rgb * (a * dark) + (1.0 - texel.a * a);\n" \
"	}\n" \
"	return m;\n" \
"}\n"

// QVR: the permanent decals' layer (vr/vr_permdecals.cpp): a face's texels in its own plane, in pages of 30 x 30, each
// page's slot in the atlas (32 x 32: a texel of its neighbours' round it; 4 mip levels) found in the page table
// (PermDecalTable: each page's slot + 1, 0 none; a face's pages from in_grimepages.x, in_grimepages.y of them across +
// 4096 down). The texel is half the factor the marks baked there multiply the texture by (10 bits: 512 / 1023 is 1).
// Read at the pixel's footprint's mip level (the narrower way: its larger side), kept inside the slot at that level.
#define QVR_PERMDECAL_FUNCTIONS \
"layout(std430, binding=10) restrict readonly buffer PermDecalTableBuffer\n" \
"{\n" \
"	uint PermDecalTable[];\n" \
"};\n" \
"layout(binding=18) uniform sampler2DArray PermDecalAtlas; // (its layers: PermDecals.y squared slots each)\n" \
"\n" \
"vec4 PermDecalsAt(vec2 g, vec2 pages, vec2 dgdx, vec2 dgdy) // w 1: a page there\n" \
"{\n" \
"	float across = mod(pages.y, 4096.0), down = floor(pages.y / 4096.0);\n" \
"	vec2 page = clamp(floor(g * (1.0 / 30.0)), vec2(0.0), vec2(across, down) - 1.0);\n" \
"	uint e = PermDecalTable[uint(pages.x + 0.5) + uint(page.y * across + page.x + 0.5)];\n" \
"	if (e == 0u)\n" \
"		return vec4(1.0, 1.0, 1.0, 0.0);\n" \
"	uint row = uint(PermDecals.y + 0.5), perlayer = row * row;\n" \
"	uint slot = (e - 1u) % perlayer, layer = (e - 1u) / perlayer;\n" \
"	float lod = clamp(log2(max(max(length(dgdx), length(dgdy)), 1e-6)), 0.0, 3.0);\n" \
"	float rim = 0.5 * exp2(ceil(lod));\n" \
"	vec2 local = clamp(g - page * 30.0 + 1.0, vec2(rim), vec2(32.0 - rim));\n" \
"	vec2 at = (vec2(float(slot % row), float(slot / row)) * 32.0 + local) * PermDecals.z;\n" \
"	return vec4(textureLod(PermDecalAtlas, vec3(at, float(layer)), lod).rgb * (1023.0 / 512.0), 1.0);\n" \
"}\n" \
"// The layer as blocky as the texture under it: read at the middle of the texture's block or texel there, so a mark\n" \
"// has the surface's blocks. Retro textures' blocks (Retro > 0, snapping: their soft edges, smooth again far off), or\n" \
"// the texture's own texels where it is drawn nearest (PermDecals.x 2: Quake's own textures, their size in_retro.xy's;\n" \
"// 3: every texture); smooth otherwise. tuv: where the texture is read (parallax's), at the layer's texel g; lq: its\n" \
"// Quake size, tsize its own. The layer's texel at another point of the texture: both are laid along the face (affine\n" \
"// maps), their screen derivatives give one from the other.\n" \
"vec4 PermDecalsLook(vec2 g, vec2 pages, vec2 dgdx, vec2 dgdy, vec2 tuv, vec2 duvdx, vec2 duvdy, vec2 lq, vec2 tsize)\n" \
"{\n" \
"	vec4 here = PermDecalsAt(g, pages, dgdx, dgdy);\n" \
"	if (here.w < 0.5) // no page here (most of the world): nothing to snap (a page's marks reach a texel into the next)\n" \
"		return here;\n" \
"	vec2 grid = vec2(0.0);\n" \
"	float soft = 0.0, far = 0.0;\n" \
"	if (Retro > 0)\n" \
"	{\n" \
"		if (RetroP0.w > 0.0 && RetroFar < 1.0)\n" \
"		{\n" \
"			grid = RetroGrid;\n" \
"			soft = RetroP0.y;\n" \
"			far = RetroFar;\n" \
"		}\n" \
"	}\n" \
"	else if (PermDecals.x > 2.5 || (PermDecals.x > 1.5 && lq.x > 0.0 && all(lessThan(abs(tsize - lq), vec2(0.5)))))\n" \
"		grid = tsize;\n" \
"	mat2 uvm = mat2(duvdx, duvdy);\n" \
"	float det = determinant(uvm);\n" \
"	vec2 span = (abs(duvdx) + abs(duvdy)) * grid; // the blocks a pixel spans: a pixel or more, the smooth read (mipmapped) the same\n" \
"	if (grid.x <= 0.0 || abs(det) < 1e-14 || max(span.x, span.y) >= 1.0)\n" \
"		return here;\n" \
"	mat2 tog = mat2(dgdx, dgdy) * inverse(uvm);\n" \
"	// the blocks as RetroBlocks has them: one read inside a block, two or four over its edges (P0.y pixels wide)\n" \
"	vec2 p = tuv * grid - 0.5;\n" \
"	vec2 i = floor(p), f = p - i;\n" \
"	f = clamp((f - 0.5) / max(span * soft, vec2(1e-4)) + 0.5, 0.0, 1.0);\n" \
"	bvec2 edge = bvec2(f.x > 0.0 && f.x < 1.0, f.y > 0.0 && f.y < 1.0);\n" \
"	vec2 c = i + vec2(edge.x ? 0.0 : f.x, edge.y ? 0.0 : f.y), fe = vec2(edge.x ? f.x : 0.0, edge.y ? f.y : 0.0);\n" \
"	vec4 r = vec4(0.0);\n" \
"	for (int k = 0; k < 4; k++) // (a loop: one copy of the read in the program)\n" \
"	{\n" \
"		vec2 o = vec2(float(k & 1), float(k >> 1));\n" \
"		vec2 wo = mix(1.0 - fe, fe, o);\n" \
"		if (wo.x * wo.y <= 0.0)\n" \
"			continue;\n" \
"		r += PermDecalsAt(g + tog * ((c + o + 0.5) / grid - tuv), pages, dgdx, dgdy) * (wo.x * wo.y);\n" \
"	}\n" \
"	return far > 0.0 ? mix(r, here, far) : r;\n" \
"}\n"

// the world vertex shader's permanent decals (QVR_PERMDECAL_FUNCTIONS): their attribute and outputs
#define QVR_WORLD_VS_PERMDECALS \
"layout(location=6) in vec4 in_grime; // QVR: the permanent decals' layer (glvert_t.grime): the texel on its face, the face's first page, its pages (across + 4096 down; 0: none)\n" \
"layout(location=21) out vec2 out_grime; // QVR\n" \
"layout(location=22) flat out vec2 out_grimepages; // QVR\n"

// the world fragment shader's
#define QVR_WORLD_FS_PERMDECAL_INPUTS \
"layout(location=21) in vec2 in_grime; // QVR: the permanent decals' layer (QVR_PERMDECAL_FUNCTIONS)\n" \
"layout(location=22) flat in vec2 in_grimepages; // QVR\n"

// QVR: the carved decals (QVR_DECAL_FUNCTIONS' CarveAt; the solid world's, before its textures are read): where a
// crater's floor or wall is seen through the pixel the textures are read from instead (puv moved on, as the parallax
// mapping moves it: the decals and the permanent layer then follow it as they follow that), its normal bent for the
// lights (QVR_WORLD_FS_LIGHT), its colour darkened (QVR_WORLD_FS_DECALS).
#define QVR_WORLD_FS_CARVE \
"	bool carve_on = false;\n" \
"	vec3 carve_bend = vec3(0.), carve_light = vec3(0.);\n" \
"	float carve_dark = 1.;\n" \
"	if (DecalCarve.x > 0.0 && DecalClock.z > 0.0 && in_retro.w > 0.5)\n" \
"	{\n" \
"		vec3 carve_pos = parallax ? in_pos + ParallaxShiftWorld(puv - uv, duvdx, duvdy, dpdx, dpdy) : in_pos;\n" \
"		vec3 carve_shift;\n" \
"		vec2 carve_grid = Retro > 0 && RetroP0.w > 0.0 ? RetroGrid : in_retro.xy; // (the retro style's blocks: the retro textures', else the texels)\n" \
"		carve_on = CarveAt(carve_pos, in_pos, facing, dpdx, dpdy, puv, duvdx, duvdy, carve_grid, carve_shift, carve_bend, carve_dark, carve_light);\n" \
"		if (carve_on)\n" \
"		{\n" \
"			puv += WorldShiftUV(carve_shift, duvdx, duvdy, dpdx, dpdy);\n" \
"			parallax = true;\n" \
"		}\n" \
"	}\n"

// QVR: the decals on the world's own surfaces (QVR_DECAL_FUNCTIONS) on its texture, before its light: the world's
// instance only (Instance.retro.w, R_InitBModelInstance), not brush models (the meshes were laid on the world alone);
// its fullbrights and sheen covered as much. Then the permanent decals' layer (QVR_PERMDECAL_FUNCTIONS), the world's
// faces and its brush models' (the faces laid out: 4096 + 1 at least, both counts; where the parallax mapping moved the
// texture to)
#define QVR_WORLD_FS_DECALS \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_SOLID) "\n" \
"	vec3 decal_mod = vec3(carve_dark); // (1: not carved here, QVR_WORLD_FS_CARVE)\n" \
"	result.rgb *= carve_dark;\n" \
"	fullbright *= carve_dark;\n" \
"	if (DecalClock.z > 0.0 && in_retro.w > 0.5)\n" \
"	{\n" \
"		vec3 decal_pos = parallax ? in_pos + ParallaxShiftWorld(puv - uv, duvdx, duvdy, dpdx, dpdy) : in_pos;\n" \
"		vec3 marks = DecalsAt(decal_pos, in_pos, facing, dpdx, dpdy);\n" \
"		result.rgb *= marks;\n" \
"		fullbright *= marks;\n" \
"		decal_mod *= marks;\n" \
"	}\n" \
"	vec2 grime_dx = dFdx(in_grime), grime_dy = dFdy(in_grime);\n" \
"	if (PermDecals.x > 0.0 && in_grimepages.y > 4096.5)\n" \
"	{\n" \
"		vec2 grime = parallax ? in_grime + ParallaxShiftWorld(puv - uv, duvdx, duvdy, vec3(grime_dx, 0.0), vec3(grime_dy, 0.0)).xy : in_grime;\n" \
"		vec4 perm = PermDecalsLook(grime, in_grimepages, grime_dx, grime_dy, puv, duvdx, duvdy, in_retro.xy, vec2(textureSize(Tex, 0)));\n" \
"		result.rgb *= perm.rgb;\n" \
"		fullbright *= perm.rgb;\n" \
"		decal_mod *= perm.rgb;\n" \
"		if (PermDecals.w > 0.5 && perm.w > 0.5) // vr_permdecal_show: the pages unlit, green, their marks on them\n" \
"		{\n" \
"			result.rgb = vec3(0.0);\n" \
"			fullbright = perm.rgb * vec3(0.6, 1.0, 0.6);\n" \
"		}\n" \
"	}\n" \
"#else\n" \
"	const vec3 decal_mod = vec3(1.0);\n" \
"#endif\n"

// detail, parallax, specular anti-aliasing, the baked light's bumps, the decals
#define QVR_WORLD_FS_FUNCTIONS \
DETAIL_FUNCTIONS \
PARALLAX_FUNCTIONS \
SPECULAR_AA_FUNCTIONS \
QVR_BOX_WOUNDS \
QVR_DECAL_FUNCTIONS \
QVR_PERMDECAL_FUNCTIONS \
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
"// .lux (vr_modelload.cpp, VR_FillSurfaceLux): x along the texture's s axis on the face, y the normal n crossed with it, z n.\n" \
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
"	float lifted = green + AmbientLight.x * 0.5; // ... with the room's own fill light, the value the brightness above is the power of\n" \
"	g *= lifted > 1e-3 ? lum * LightTweak.x / lifted : 0.0; // relative, as the brightness's (and its contrast's power)\n" \
"	return vec2(dot(g, dpx), dot(g, dpy));\n" \
"}\n"

// detail textures close by; a liquid's unlit texture
#define QVR_WORLD_FS_DETAIL \
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) " && !DITHER\n" \
"	bool detail_on = Detail.x > 0. && in_detail.z > 0. && (Retro <= 0 || RetroP2.z > 0.); // QVR: detail textures close by (vr/vr_detail.cpp); none where a retro set keeps none of the grain\n" \
"#if MODE == " QS_STRINGIFY (WORLDSHADER_SOLID) "\n" \
"	detail_on = detail_on && result.a > 0.; // not on fullbright texels (alpha 0)\n" \
"#endif\n" \
"	if (detail_on) // (the mixes below, by 0..1, give a factor of 1 where they are by 0: the reads skipped there)\n" \
"	{\n" \
"		float detail = DetailFactor(in_detail, puv, duvdx, duvdy, distance(in_pos, EyePos));\n" \
"		if (Retro > 0) // QVR: retro textures keep as much of the grain as their set says (vr_retro.h)\n" \
"			detail = mix(1.0, detail, RetroP2.z);\n" \
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
"	if (LightTweak.x != 1. || AmbientLight.x > 0.) // QVR: the room's own fill light first (vr_ambient_light: a share of Quake's full light added to the lightmap), then the contrast about Quake's full light (vr_light_contrast): darker shade, lamps as bright\n" \
"		total_light = 0.5 * pow(max(total_light, vec3(0.)) * 2.0 + AmbientLight.x, vec3(LightTweak.x));\n" \
"\n" \
"	// QVR: the normal bent by the normal map (vr_normalmaps), for the baked light and the dynamic lights\n" \
"	vec3 bumped = facing;\n" \
"	vec3 luxdir = vec3(0.); // QVR: the baked light's real direction (deluxemaps), for its bumps and sheen; 0: none\n" \
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	if (LightTweak.w > 0.)\n" \
"	{\n" \
"		bumped = BumpedNormal(NormalTex, puv, duvdx, duvdy, dpdx, dpdy, facing, in_extmat.x);\n" \
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
"	if (carve_on) // QVR: a carved decal's crater (QVR_WORLD_FS_CARVE) in the baked light, from where it comes (deluxemaps), else\n" \
"	{ // from its side towards the light that reaches it; then its normal for the dynamic lights and the sheen\n" \
"		vec3 carve_n = normalize(facing + carve_bend);\n" \
"		vec4 carve_lux = (ShadowFlags & 128u) != 0u ? LuxDirection(lmuv, facing, duvdx, duvdy, dpdx, dpdy) : vec4(0.);\n" \
"		vec3 carve_l = carve_lux.w > 0. ? LuxLight(facing, carve_lux.xyz) : carve_light;\n" \
"		float carve_f = max(dot(carve_n, carve_l), 0.0) / max(dot(facing, carve_l), 0.2);\n" \
"		total_light *= mix(1.0, clamp(carve_f, 0.15, 1.8), 0.85);\n" \
"		bumped = normalize(bumped + carve_bend);\n" \
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
"#if MODE != " QS_STRINGIFY (WORLDSHADER_WATER) "\n" \
"	if ((in_flags & CF_SPECMAP) != 0u) // QVR: an external pack's specular map (vr/vr_extmaps.cpp): the sheen's colour and\n" \
"	{ // brightness per texel, its lobe narrowed by the .mat's hardness (as bright on the whole)\n" \
"		specular_map = RetroAux(SpecTex, puv, duvdx, duvdy).rgb * in_extmat.y; // (the retro textures' blocks: vr_retro.h)\n" \
"		float hard = SpecLobe.x * in_extmat.z;\n" \
"		SpecLobe = vec2(hard, SpecLobe.y * (hard + 1.0) / (SpecLobe.x + 1.0));\n" \
"	}\n" \
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

// a light's shadow and spot cone (rl_pos, rl_spos, rl_mpos: in_pos, or retro lighting's blocks: vr_retrolight.h;
// rl_spos_same: rl_spos is rl_pos; to_eye: the unit direction from rl_pos to the eye)
#define QVR_WORLD_FS_LIGHT_SHADOW \
"					if (l.shadow.w != 0.) // QVR: a map light's shadow of moving things\n" \
"					{\n" \
"						total_light *= MapLightShadow(l, rl_mpos, facing, total_light);\n" \
"						continue;\n" \
"					}\n" \
"					if (darkplaces) // QVR: DarkPlaces' falloff, colours brighter than 1 (vr_dlight_falloff)\n" \
"					{\n" \
"						vec3 tl = l.origin - rl_pos; // its distance and direction worked out once (as the models' loop)\n" \
"						float d2 = dot(tl, tl);\n" \
"						if (d2 >= l.radius * l.radius)\n" \
"							continue;\n" \
"						float inv = inversesqrt(max(d2, 1e-12));\n" \
"						vec3 dir = tl * inv;\n" \
"						float lit = DarkPlacesAtten(d2 * inv, l.radius) * LightShadow(l, rl_spos, facing, SpotCone(l, rl_spos_same ? dir : normalize(l.origin - rl_spos)));\n" \
"						if (lit <= 0.)\n" \
"							continue;\n" \
"						float ndl = dot(bumped, dir);\n" \
"						dynamic_light += lit * 0.5 * LightAngleDP(l, ndl) * l.color; // halved: the lightmap is doubled below\n" \
"						specular_light += lit * LightSpecular(bumped, ndl, dir, to_eye) * l.color;\n" \
"						continue;\n" \
"					}\n"

// force grab's glow; the liquid's look
#define QVR_WORLD_FS_GLOW_LIQUID \
"	if (in_glow > 0. && in_glow < 3.5) // QVR: force grab's glow (vr/vr_fgfx.cpp); 2..3 the toolgun remover's red (vr/vr_toolgun.cpp); (4 and over, an incandescent blade's: models only)\n" \
"	{\n" \
"		float rim = 1.0 - abs(dot(facing, normalize(EyePos - in_pos)));\n" \
"		vec3 glowColor = in_glow > 1.5 ? vec3(1.0, 0.12, 0.06) : SceneTone.yzw; // QVR: the player's hue\n" \
"		result.rgb += glowColor * (in_glow > 1.5 ? in_glow - 2.0 : in_glow) * (pow(rim, 2.0) * 1.1 + 0.12);\n" \
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
"	result = LiquidBlood(result, in_pos, facing, liquid, min(dot(total_light, vec3(1.0 / 3.0)), 1.3));\n" \
"	result.a *= LiquidEdge; // (the wavy edge: what is behind seen past it)\n" \
"#endif\n"
