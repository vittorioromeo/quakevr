// vr_glsl_lighting.h -- Quake VR's GLSL, shadows and per-pixel light, ambient occlusion, gl_zfix's push back: part of vr_glsl.h (included there, in this order; macros
// only, spliced into gl_shaders.h's shaders by name).

#pragma once

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
QVR_RETROLIGHT_GLSL /* QVR: retro lighting (vr_retrolight.h) */ \
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
"	if (RetroLight[3].x > 0.) // QVR: retro lighting's hard shadows: one tap; 2: on the shadow map's texels (blocky)\n"\
"	{\n"\
"		filt = 0u;\n"\
"		if (RetroLight[3].x > 1.5)\n"\
"			texel = floor(texel) + 0.5;\n"\
"	}\n"\
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
"// A spot light's cone at a point, dir the unit direction from it to the light: 1 inside its inner cone, smoothly\n"\
"// down to 0 at its outer one; 1 for a point light (spot: xyz the direction / (cos inner - cos outer), w cos inner /\n"\
"// (cos inner - cos outer); zero for a point light).\n"\
"float SpotCone(Light l, vec3 dir)\n"\
"{\n"\
"	return 1.0 - smoothstep(0.0, 1.0, l.spot.w + dot(l.spot.xyz, dir));\n"\
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
"// A virtual light follows exactly the movement transform. Its straight ray in the destination\n"\
"// represents the folded source/exit path; attenuation therefore keeps its full travelled distance.\n"\
"float PortalLightPath(Light l, vec3 pos)\n"\
"{\n"\
"    if (l.gatelo.w <= 0.0) return 1.0;\n"\
"    float a = dot(l.origin, l.gateplane.xyz) - l.gateplane.w;\n"\
"    float b = dot(pos, l.gateplane.xyz) - l.gateplane.w;\n"\
"    if (a >= 0.0 || b <= 0.0) return 0.0;\n"\
"    vec3 entry = mix(l.origin, pos, -a / (b - a));\n"\
"    vec4 p = vec4(entry, 1.0), light = vec4(l.origin, 1.0);\n"\
"    vec3 sourceEntry = vec3(dot(l.gateinverse[0], p), dot(l.gateinverse[1], p), dot(l.gateinverse[2], p));\n"\
"    if (any(lessThan(sourceEntry, l.gatelo.xyz - 0.1)) || any(greaterThan(sourceEntry, l.gatehi.xyz + 0.1))) return 0.0;\n"\
"    vec3 sourceLight = vec3(dot(l.gateinverse[0], light), dot(l.gateinverse[1], light), dot(l.gateinverse[2], light));\n"\
"    return ShadowLookup(ShadowAtlas, l.gateshadow.xyz, sourceEntry - sourceLight);\n"\
"}\n"\
"\n"\
"// A dynamic light's shadow at pos (normal n), times its cone there (SpotCone: 1 for a point light). l.shadow2.y: the share\n"\
"// its shadow lets through (fading in or out: vr_lighting.cpp's selectDlights; 0 a full shadow).\n"\
"float LightShadow(Light l, vec3 pos, vec3 n, float cone)\n"\
"{\n"\
"	cone *= PortalLightPath(l, pos);\n"\
"	if (l.shadow.z <= 0. || cone <= 0.)\n"\
"		return cone;\n"\
"	if (l.shadow2.x > 0.)\n"\
"		return cone * mix(RetroShadowQuant(SpotShadow(l, pos, n)), 1.0, l.shadow2.y); // QVR: in retro lighting's levels\n"\
"	return cone * mix(RetroShadowQuant(ShadowLookup(ShadowAtlas, l.shadow.xyz, ShadowOffset(pos - l.origin, n, l.shadow.z))), 1.0, l.shadow2.y);\n"\
"}\n"\
"\n"\
"// Quake's dynamic lights ignore the angle they reach a surface at: DlightAngle blends in Lambert's (ndl: the normal\n"\
"// dotted with the unit direction to the light).\n"\
"float LightAngle(float ndl, float ambient)\n"\
"{\n"\
"	return mix(1.0, ambient + (1.0 - ambient) * 2.0 * max(ndl, 0.0), DlightAngle);\n"\
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
"float LightAngleDP(Light l, float ndl)\n"\
"{\n"\
"	return mix(1.0, l.minlight + (1.0 - l.minlight) * max(ndl, 0.0), DlightAngle);\n"\
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
"// A dynamic light's sheen towards the eye (DarkPlaces' r_shadow_gloss 2): Blinn's, exponent 32, LightTweak.z strong;\n"\
"// dir and toEye the unit directions to the light and to the eye, ndl dot(n, dir).\n"\
"float LightSpecular(vec3 n, float ndl, vec3 dir, vec3 toEye)\n"\
"{\n"\
"	if (LightTweak.z <= 0. || ndl <= 0.)\n"\
"		return 0.0;\n"\
"	vec3 h = normalize(dir + toEye);\n"\
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
"	vec4 s = RetroAux(tex, uv, duvdx, duvdy); // QVR: smooth, or the retro textures' blocks (vr_retro.h)\n"\
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
"vec3 BumpedNormal(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, float multiplier)\n"\
"{\n"\
"	return BumpedNormalFrame(tex, uv, duvdx, duvdy, dpdx, dpdy, n, LightTweak.w * multiplier, false);\n"\
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
"	return RetroShadowQuant(1.0 - clamp(given / max(lum, 1e-3), 0.0, 1.0) * blocked * l.color.x); // QVR: in retro lighting's levels\n"\
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
"			if (vis <= 0.1) // at the floor: the occluders left (each a share of what is left) keep it there\n"\
"				return 0.1;\n"\
"		}\n"\
"	}\n"\
"	return max(vis, 0.1);\n"\
"}\n"\
"\n"\

////////////////////////////////////////////////////////////////

// gl_zfix (CF_USE_POLYGON_OFFSET: brush entities, so that one flush with a wall loses to it): the clip-space depth
// (z, w) of a point pushed back 1/4096 of its distance, whatever the near plane. Ironwail added a constant 1/1024 to
// clip z, which pushes a point back (that / the near plane) of its distance with reversed Z, half that without:
// 1/4096 at the desktop's near plane of 4 units, but 1% at the headset's 0.1 (vr_nearclip), so that a button 8 units
// proud of its panel went behind it from 800 units away (NOTES.md vrfiringrange_2026-10-06_13-11-00). Moving clip z
// towards the far plane's (0 reversed, w otherwise) by a fraction moves the point back by that fraction (to first
// order) with any near plane, and keeps the depths' order (portals' oblique near planes too).
#define QVR_ZFIX_FRACTION "(1./4096.)"
#define QVR_ZFIX_FUNCTION \
"float QVR_ZFixDepth(float z, float w)\n"\
"{\n"\
"#if REVERSED_Z\n"\
"	return z * (1. - " QVR_ZFIX_FRACTION ");\n"\
"#else\n"\
"	return mix(z, w, " QVR_ZFIX_FRACTION ");\n"\
"#endif\n"\
"}\n"
