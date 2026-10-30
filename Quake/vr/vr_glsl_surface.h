// vr_glsl_surface.h -- Quake VR's GLSL, relief (parallax), specular anti-aliasing, detail textures, liquids: part of vr_glsl.h (included there, in this order; macros
// only, spliced into gl_shaders.h's shaders by name).

#pragma once

// parallax occlusion mapping (vr_parallax): the texture coordinates where the ray from the eye through this
// pixel (ray: from the eye to it; n: the surface's normal, facing the eye) meets the height field under the surface
// (tex's alpha: 1 the surface, 0 `depth` units deep; the instance's: the world's, an item box's or a model's).
// The ray is walked in steps (`steps` at most looking straight on times a half, at grazing angles times 2, where its
// run along the surface is long; no more than 1.5 for each texel of the height field it crosses at the mip level the
// reads use, 1 when refined; 4 at least: small boxes and models cross few), then the step it crossed the height field
// in is refined (Parallax2.x times: regula falsi, Illinois: relief mapping's binary search's sureness, the secant's
// speed), so the hit slides smoothly as the eye moves instead of jumping a layer at a time. The texture's axes on the
// surface are the gradients of its coordinates, from the derivatives the bumps' frame is made from: exact on flat faces
// and a model's triangles; each eye walks its own ray. It fades out over the last quarter of Parallax.y units away, and
// at grazing angles (vr_parallax_grazing: from Parallax2.y to Parallax2.z, cosines; none when both are 0); the shift
// along the surface is at most PARALLAX_STRETCH times the depth (the ray steeper beyond: the relief stays, the smear
// doesn't grow without bound). None where the whole shift is under a third of a pixel. `uvclamp` (an item box's
// texture_t): the rays stay in the part of the texture the face shows, the shift shrinking towards its edges, instead
// of reading past them (the texture's unused rest, or its other side).
#define PARALLAX_FUNCTIONS \
QVR_ZFIX_FUNCTION \
"#define PARALLAX_STRETCH 8.0\n"\
"// How far along the eye's ray (dist from it to the surface; cosa its cosine to the surface's normal) the ray walks\n"\
"// below the surface, to the height field's bottom (0: none). Also the world's depth pre-pass's bound on it.\n"\
"float ParallaxReach(float dist, float cosa, float depth)\n"\
"{\n"\
"	if (dist >= Parallax.y || depth <= 0.)\n"\
"		return 0.;\n"\
"	float fade = 1.0 - smoothstep(Parallax.y * 0.75, Parallax.y, dist);\n"\
"	if (Parallax2.y > 0.)\n"\
"		fade *= smoothstep(Parallax2.z, Parallax2.y, cosa);\n"\
"	return depth * fade * min(1.0 / max(cosa, 1e-3), PARALLAX_STRETCH);\n"\
"}\n"\
"// pixel depth offset (vr_parallax_depth_write): ParallaxUV's hit, how far past the surface along the eye's ray (0:\n"\
"// none); ParallaxFragDepth the depth of the point that far along it (dir, a unit) from p, as the view's matrix puts\n"\
"// it (zfix: with the vertex shader's polygon offset, QVR_ZFixDepth), never nearer than the surface's own for a hit\n"\
"// below it (s > 0); s < 0: a raised hit, over the surface (ParallaxPlane), nearer\n"\
"float ParallaxDist = 0.;\n"\
"float ParallaxFragDepth(mat4 viewproj, vec3 p, vec3 dir, float s, bool zfix)\n"\
"{\n"\
"	vec4 clip = viewproj * vec4(p + dir * s, 1.0);\n"\
"	float ndc = (zfix ? QVR_ZFixDepth(clip.z, clip.w) : clip.z) / clip.w;\n"\
"#if REVERSED_Z\n"\
"	float d = gl_DepthRange.near + gl_DepthRange.diff * ndc;\n"\
"	return s > 0. ? min(d, gl_FragCoord.z) : d;\n"\
"#else\n"\
"	float d = gl_DepthRange.near + gl_DepthRange.diff * (ndc * 0.5 + 0.5);\n"\
"	return s > 0. ? max(d, gl_FragCoord.z) : d;\n"\
"#endif\n"\
"}\n"\
"// Where the surface's polygon sits in the relief, 0..1 (0: at its top, the relief carved down from it; 1 its bottom,\n"\
"// the relief raised over it), at most at the texture's average height. Set before ParallaxUV (0 unless the world's\n"\
"// shader sets it: 1, Parallax3.x): the ray is walked from where it crosses the relief's top, that share of the depth\n"\
"// over the surface\n"\
"float ParallaxPlane = 0.;\n"\
"vec2 ParallaxUV(sampler2D tex, vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, vec3 ray, float depth,\n"\
"	float steps, vec4 uvclamp)\n"\
"{\n"\
"	ParallaxDist = 0.;\n"\
"	float dist = length(ray);\n"\
"	ray /= max(dist, 1e-3);\n"\
"	float cosa = -dot(ray, n); // n faces the eye\n"\
"	float det = dot(n, cross(dpdx, dpdy));\n"\
"	float reach = ParallaxReach(dist, cosa, depth); // along the ray; times sin(a) along the surface\n"\
"	if (reach * reach < 0.11 * max(dot(dpdx, dpdx), dot(dpdy, dpdy)) || abs(det) < 1e-12)\n"\
"		return uv;\n"\
"	vec3 gu = (cross(dpdy, n) * duvdx.x + cross(n, dpdx) * duvdy.x) / det; // the coordinates' change per unit\n"\
"	vec3 gv = (cross(dpdy, n) * duvdx.y + cross(n, dpdx) * duvdy.y) / det;\n"\
"	vec3 shift = (ray + n * cosa) * reach; // along the surface, from the relief's top to its bottom\n"\
"	vec2 duv = vec2(dot(shift, gu), dot(shift, gv));\n"\
"	// the walk from uv - duv * plane (the top) to uv + duv * (1 - plane); the surface never below the texture's average\n"\
"	// height (its smallest mip's), so a flat one (all at the top) or one mostly at the top isn't raised over the props\n"\
"	float plane = ParallaxPlane > 0. ? min(ParallaxPlane, 1.0 - textureLod(tex, uv, 16.0).a) : 0.;\n"\
"	if (uvclamp.z > uvclamp.x || uvclamp.w > uvclamp.y)\n"\
"	{\n"\
"		vec2 margin = 0.5 / vec2(textureSize(tex, 0));\n"\
"		vec2 base = floor(uv - (uvclamp.xy + uvclamp.zw) * 0.5 + 0.5); // the whole textures the face is moved by\n"\
"		vec2 lo = uvclamp.xy + base + margin - uv, hi = uvclamp.zw + base - margin - uv; // how far it may go\n"\
"		float k = 1.0;\n"\
"		for (int e = 0; e < 2; e++) // both ends of the walk\n"\
"		{\n"\
"			vec2 d = duv * (e == 0 ? 1.0 - plane : -plane);\n"\
"			if (uvclamp.z > uvclamp.x && abs(d.x) > 1e-7)\n"\
"				k = min(k, max((d.x > 0. ? hi.x : lo.x) / d.x, 0.));\n"\
"			if (uvclamp.w > uvclamp.y && abs(d.y) > 1e-7)\n"\
"				k = min(k, max((d.y > 0. ? hi.y : lo.y) / d.y, 0.));\n"\
"		}\n"\
"		duv *= k;\n"\
"		reach *= k; // the hit as far from the surface as the texture shows (none at the face's edges)\n"\
"	}\n"\
"	uv -= duv * plane; // where the ray crosses the relief's top; ParallaxDist from the surface: less reach * plane\n"\
"	float over_surface = reach * plane;\n"\
"	vec2 size = vec2(textureSize(tex, 0));\n"\
"	float px = length(duvdx * size), py = length(duvdy * size); // the pixel's footprint in texels, its two ways\n"\
"	float lod = max(log2(max(min(px, py), max(px, py) * 0.125)), 0.); // the mip level read (8x anisotropic)\n"\
"	float texels = length(duv * size) * exp2(-lod); // the texels the ray crosses there\n"\
"	steps = min(ceil(steps * mix(2.0, 0.5, cosa)), 64.0);\n"\
"	steps = min(steps, max(4.0, ceil(texels * (Parallax2.x > 0. ? 1.0 : 1.5))));\n"\
"	float stepsize = 1.0 / steps;\n"\
"	float h = 1.0 - textureGrad(tex, uv, duvdx, duvdy).a; // how deep the height field is, 0..1\n"\
"	if (h <= 0.)\n"\
"	{\n"\
"		ParallaxDist = -over_surface;\n"\
"		return uv;\n"\
"	}\n"\
"	float ray_depth = 0., prev = h; // prev: how far over the height field the ray was, the step before\n"\
"	// four steps' reads at a time (independent: the GPU overlaps them), then the steps taken in order as before (the\n"\
"	// same reads and result; up to 3 past the hit read for nothing): ROUND21.md, Shader optimizations: retro and parallax\n"\
"	bool done = false;\n"\
"	for (int i = 1; i <= 64 && !done; i += 4)\n"\
"	{\n"\
"		float hs[4];\n"\
"		for (int j = 0; j < 4; j++)\n"\
"			hs[j] = 1.0 - textureGrad(tex, uv + duv * (float(i + j) * stepsize), duvdx, duvdy).a;\n"\
"		for (int j = 0; j < 4; j++)\n"\
"		{\n"\
"			prev = h - ray_depth;\n"\
"			ray_depth = float(i + j) * stepsize;\n"\
"			h = hs[j];\n"\
"			if (h <= ray_depth || float(i + j) >= steps)\n"\
"			{\n"\
"				done = true;\n"\
"				break;\n"\
"			}\n"\
"		}\n"\
"	}\n"\
"	float over = h - ray_depth; // <= 0: under it\n"\
"	if (over > 0.)\n"\
"	{\n"\
"		ParallaxDist = reach - over_surface;\n"\
"		return uv + duv; // the bottom\n"\
"	}\n"\
"	// refined between the ray's depths a (over it: fa > 0) and b (under it: fb <= 0): regula falsi, the Illinois way\n"\
"	// (the end kept twice running halved, so the bracket shrinks from both sides); the last guess not read\n"\
"	float a = ray_depth - stepsize, b = ray_depth, fa = prev, fb = over;\n"\
"	int refine = int(Parallax2.x), side = 0;\n"\
"	for (int k = 0; k < 8 && k < refine; k++)\n"\
"	{\n"\
"		float t = a + (b - a) * fa / max(fa - fb, 1e-5);\n"\
"		float ft = 1.0 - textureGrad(tex, uv + duv * t, duvdx, duvdy).a - t;\n"\
"		if (ft > 0.)\n"\
"		{\n"\
"			a = t;\n"\
"			fa = ft;\n"\
"			if (side > 0)\n"\
"				fb *= 0.5;\n"\
"			side = 1;\n"\
"		}\n"\
"		else\n"\
"		{\n"\
"			b = t;\n"\
"			fb = ft;\n"\
"			if (side < 0)\n"\
"				fa *= 0.5;\n"\
"			side = -1;\n"\
"		}\n"\
"	}\n"\
"	float t = a + (b - a) * fa / max(fa - fb, 1e-5);\n"\
"	ParallaxDist = reach * t - over_surface;\n"\
"	return uv + duv * t;\n"\
"}\n"\
"\n"\

// pixel depth offset (vr_parallax_depth_write, Parallax2.w): in the programs made with PDO 1 (the opaque world's
// glprogs.world_pdo and its pre-pass glprogs.world_depth_pdo, used only while it is on: a shader that writes
// gl_FragDepth writes it for every pixel, and under foveated rendering's coarse shading one value for each block of
// pixels, so the others keep the fixed-function depth), QVR_PDO is 1 and the shader writes gl_FragDepth on every
// path: the surface's own (gl_FragCoord.z), or a parallax hit's (ParallaxFragDepth): further, or nearer where
// the world's ParallaxPlane raises the relief over the surface, so depth_any (it was depth_less with reversed depth,
// depth_greater without, before the plane, round 22). The driver measured here (NVIDIA, 2026-10) shades every fragment
// of such a shader either way (depth_any, less and greater timed alike): the cost of vr_parallax_depth_write (ROUND21.md).
// Declaring the depth never nearer (depth_less/depth_greater) where nothing is raised was tried in round 22 and removed:
// this NVIDIA driver shaded the same fragments either way (vr_world_fragments; ROUND22.md, "Conservative depth for the
// parallax depth write", and "Removed perf attempts"). What it costs now: only the textures with relief, nearer than
// the parallax's reach, are drawn by these programs (r_world.c R_DrawSolidSplit; "Parallax depth write's cost").
#define QVR_PARALLAX_DEPTH_OUT \
"#ifndef PDO\n"\
"	#define PDO 0\n"\
"#endif\n"\
"#if PDO\n"\
"	#define QVR_PDO 1\n"\
"	layout(depth_any) out float gl_FragDepth;\n"\
"#else\n"\
"	#define QVR_PDO 0\n"\
"#endif\n"

// the opaque world's depth pre-pass with pixel depth offset (glprogs.world_depth_pdo, r_world.c): the shading pass
// then passes only where its depth is as far as the pre-pass's or nearer, so the pre-pass writes how far the
// parallax mapping's hits can be at most (ParallaxReach, a little more; the shading pass's hit is never further), not
// the walk itself; the surface's own depth where there is no parallax. Pixels whose hit is nearer than that bound
// shade again under nearer surfaces there (a few units), and the shading pass writes the hits' real depth.
#define QVR_WORLD_DEPTH_FS_MAIN \
"void main()\n"\
"{\n"\
"	gl_FragDepth = gl_FragCoord.z;\n"\
"	if (in_pdepth > 0. && Parallax2.w > 0.)\n"\
"	{\n"\
"		vec3 dpdx = dFdx(in_pos), dpdy = dFdy(in_pos); // the shading pass's normal and ray (ParallaxUV's)\n"\
"		vec3 n = normalize(cross(dpdx, dpdy));\n"\
"		n = dot(n, EyePos - in_pos) < 0. ? -n : n;\n"\
"		vec3 ray = in_pos - EyePos;\n"\
"		float dist = length(ray);\n"\
"		ray /= max(dist, 1e-3);\n"\
"		float reach = ParallaxReach(dist, -dot(ray, n), in_pdepth); // (also with ParallaxPlane: its texture's cap unknown here)\n"\
"		if (reach > 0.)\n"\
"			gl_FragDepth = ParallaxFragDepth(ViewProj, in_pos, normalize(in_pos - EyePos), reach * 1.01 + 0.01,\n"\
"				(in_flags & CF_USE_POLYGON_OFFSET) != 0u);\n"\
"	}\n"\
"}\n"

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
"	// the wavy edge (Foam[i].w, LiquidFoam): the rim raised up its wall in slow curves drifting along the shore, up to\n"\
"	// 0.6 of its units, fading out 12 units in; only up (no gap opens; a liquid seen opaque gets an uneven line too)\n"\
"	int li = kind == 1u ? 1 : kind == 2u ? 2 : 0;\n"\
"	float lift = 0.0;\n"\
"	if (Foam[li].w > 0. && Foam[li].x > 0. && Water3.x > 0. && kind != 3u && rim < 13.0)\n"\
"	{\n"\
"		float t = Time * 0.5;\n"\
"		float n = sin(pos.x * 0.11 + pos.y * 0.07 + t) * sin(pos.y * 0.13 - pos.x * 0.05 - t * 0.8) * 0.5 + 0.5;\n"\
"		lift = 0.6 * Foam[li].w * n * (1.0 - smoothstep(1.0, 13.0, rim)) * fade;\n"\
"	}\n"\
"	return vec3(pos.xy, pos.z + swell + rip + lift);\n"\
"}\n"\
"\n"\

#define LIQUID_FUNCTIONS \
"layout(binding=6) uniform sampler2D LiquidScene; // the opaque scene, while translucent liquids draw into the OIT buffers\n"\
"layout(binding=7) uniform sampler3D LiquidVolume; // where the water and slime are (1), a cell round them into walls\n"\
"layout(binding=8) uniform sampler2D LiquidDepth; // how far the opaque scene is (vr_water.cpp: half the size), with LiquidScene\n"\
"layout(binding=16) uniform samplerCube LiquidCube; // the room over the water (vr_envmap.cpp), each texel's distance in alpha\n"\
"layout(binding=17) uniform sampler2DArray PortalScene; // the view through a teleporter for this eye (vr_portals.cpp), by the eye's pixels\n"\
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
"// The room reflected at pos on a level liquid seen from above (vr_water_reflections; WaterCube: vr_envmap.cpp's water\n"\
"// cube, made over the surface at WaterCube2.x), n the waves' normal, v towards the eye, rough how much the waves\n"\
"// spread under the pixel: rgb the colour, a its share (0: none here, other heights, past WaterCube2.y from the\n"\
"// cube). The reflected ray is followed out to the cube's distances (its alpha; Szirmay-Kalos' distance impostors):\n"\
"// from the cube's centre the direction of where the ray meets the room is read, not the ray's own, so what is\n"\
"// reflected is where it is and the same in both eyes. Blurrier where the waves are rough under the pixel and far off.\n"\
"vec4 LiquidReflection(vec3 pos, vec3 n, vec3 v, float rough)\n"\
"{\n"\
"	if (WaterCube.w <= 0. || abs(pos.z - WaterCube2.x) > 6.0)\n"\
"		return vec4(0.);\n"\
"	vec3 o = pos - WaterCube.xyz;\n"\
"	float fade = 1.0 - smoothstep(WaterCube2.y * 0.6, WaterCube2.y, length(o.xy));\n"\
"	if (fade <= 0.)\n"\
"		return vec4(0.);\n"\
"	vec3 r = reflect(-v, n);\n"\
"	r = normalize(vec3(r.xy, max(r.z, 0.02))); // a wave's back bent below the plane: skimming it\n"\
"	float lod = clamp(WaterCube2.z + rough * 40.0 + log2(1.0 + distance(pos, EyePos) / 512.0), 0.0, WaterCube2.w);\n"\
"	float b = dot(o, r), oo = dot(o, o);\n"\
"	float t = textureLod(LiquidCube, r, lod).a;\n"\
"	for (int i = 0; i < 3; i++)\n"\
"	{\n"\
"		float d = textureLod(LiquidCube, o + r * t, lod).a; // the room's distance from the centre that way\n"\
"		float disc = b * b - oo + d * d; // where the ray is that far from the centre\n"\
"		t = disc > 0. ? max(-b + sqrt(disc), 0.) : t;\n"\
"	}\n"\
"	return vec4(textureLod(LiquidCube, o + r * t, lod).rgb, WaterCube.w * fade);\n"\
"}\n"\
"\n"\
"// A teleporter showing where it leads (vr_portals; PortalPlane, PortalMin, PortalMax: vr_portals.cpp's side shown in this\n"\
"// view): on that side's faces, seen from in front, the view through it (PortalScene: drawn for this eye, its pixels\n"\
"// these), wavering a little as the teleporter's texture does; a its share (0: not this side, or none). eye: the eye's\n"\
"// distance in front of its plane.\n"\
"vec4 LiquidPortal(vec3 pos, vec3 facing, out float eye)\n"\
"{\n"\
"    for (int i = 0; i < 8; ++i)\n"\
"    {\n"\
"        if (PortalMin[i].w <= 0. || dot(facing, PortalPlane[i].xyz) < 0.9 || abs(dot(pos, PortalPlane[i].xyz) - PortalPlane[i].w) > 2.0 ||\n"\
"            any(lessThan(pos, PortalMin[i].xyz - 2.0)) || any(greaterThan(pos, PortalMax[i].xyz + 2.0))) continue;\n"\
"        vec2 size = vec2(textureSize(PortalScene, 0).xy);\n"\
"        vec2 waver = vec2(sin(Time * 1.9 + dot(pos, vec3(0.071, 0.053, 0.089))), cos(Time * 1.6 + dot(pos, vec3(0.047, 0.083, 0.061))));\n"\
"        vec2 uv = (gl_FragCoord.xy * PortalUV.zw + PortalUV.xy) / size + waver * 0.0012;\n"\
"        eye = dot(EyePos, PortalPlane[i].xyz) - PortalPlane[i].w;\n"\
"        return vec4(texture(PortalScene, vec3(uv, float(i))).rgb, PortalMin[i].w);\n"\
"    }\n"\
"    eye = 0.;\n"\
"    return vec4(0.);\n"\
"}\n"\
"\n"\
"// A liquid's colour: tex its texture, lit that lit (light: by how much, 1 Quake's full light), n the waves' normal and\n"\
"// facing the flat one (towards the eye), h the waves' height. alpha: its opacity, in and out. Water and slime are more\n"\
"// see-through looking down and reflect a dim room colour at grazing angles (fresnel), glinting where the waves face a\n"\
"// light from above; lava glows, its hot parts brightest (to bloom), pulsing slowly; teleports shimmer.\n"\
"vec3 LiquidShade(vec3 tex, vec3 lit, vec3 light, vec3 pos, vec3 n, vec3 facing, float h, uint kind, inout float alpha)\n"\
"{\n"\
"	vec3 ndx = dFdx(n), ndy = dFdy(n); // QVR: the waves' spread under the pixel widens the glints (vr_specular_aa: Parallax.w)\n"\
"	float nspread = min(0.25 * (dot(ndx, ndx) + dot(ndy, ndy)), 0.09); // (also the reflection's blur)\n"\
"	float spread = nspread * Parallax.w;\n"\
"	if (kind == 1u)\n"\
"	{\n"\
"		float hot = smoothstep(0.2, 0.6, dot(tex, vec3(0.3, 0.59, 0.11)));\n"\
"		float pulse = 0.75 + 0.25 * sin(Time * 0.8 + h * 1.5 + dot(pos, vec3(0.011, 0.017, 0.0)));\n"\
"		return mix(lit, max(lit, tex), min(Water2.x, 1.0)) * (1.0 + Water2.x * (0.3 + 1.2 * hot * pulse));\n"\
"	}\n"\
"	if (kind == 3u)\n"\
"	{\n"\
"		vec3 shimmer = lit * (1.0 + 0.2 * sin(Time * 3.0 + h * 8.0) * min(Water.x, 1.0));\n"\
"		float eye;\n"\
"		vec4 through = LiquidPortal(pos, facing, eye); // QVR: where it leads (vr_portals), a little of its shimmer over it\n"\
"		if (through.a > 0.)\n"\
"		{\n"\
"			alpha = 1.0;\n"\
"			float nearFade = TeleportLook.z > 0. ? clamp(eye / TeleportLook.z, 0.0, 1.0) : 1.0; // QVR: gone at the crossing\n"\
"			return mix(shimmer, through.rgb, through.a * (1.0 - 0.12 * TeleportLook.y * nearFade));\n"\
"		}\n"\
"		alpha *= TeleportLook.y;\n"\
"		return shimmer;\n"\
"	}\n"\
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
"		if (facing.z > 0.9) // QVR: the room reflected (vr_water_reflections), slime's murkier and greener\n"\
"		{\n"\
"			vec4 room = LiquidReflection(pos, n, v, nspread);\n"\
"			env = mix(env, room.rgb * (kind == 2u ? vec3(0.55, 0.8, 0.4) : vec3(1.0)), min(room.a, 1.0));\n"\
"		}\n"\
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
"// crests; slime: a green scum; lava: a molten crust, hot at the rock (to bloom). Each liquid's own look (Foam[i],\n"\
"// FoamColor[i], FoamBreath[i]; i 0 water, 1 lava, 2 slime: vr_water_foam_*, vr_lava_foam_*, vr_slime_foam_*), and\n"\
"// its band breathing: wider and narrower in turn, each stretch of shore in its own phase (a slow noise in the world).\n"\
"// Its wavy edge (Foam[i].w units): the shore's line drawn back in drifting curves, the liquid see-through past it\n"\
"// (LiquidEdge; LiquidDisplace raises the rim alike, for liquids drawn opaque).\n"\
"// All in the world (both eyes alike), fading out from 600 to 1000 units. tex the texture's colour, light the light on\n"\
"// it (1 Quake's full); lit: the world program's (the unlit water's foam is as bright as its texture). alpha: its opacity.\n"\
"float LiquidEdge = 1.0; // the wavy edge's opacity here (LiquidFoam), the liquid's whole output's alpha times it\n"\
"vec3 LiquidFoam(vec3 c, vec3 tex, vec3 light, vec3 pos, vec3 facing, float h, float rim, uint kind, bool lit, inout float alpha)\n"\
"{\n"\
"	int li = kind == 1u ? 1 : kind == 2u ? 2 : 0;\n"\
"	vec4 look = Foam[li];\n"\
"	if (Water3.x <= 0. || look.x <= 0. || kind == 3u || facing.z < 0.7 || rim < 0.) // (rim < 0: a surface under more of the liquid)\n"\
"		return c;\n"\
"	float dist = distance(pos, EyePos);\n"\
"	float fade = 1.0 - smoothstep(600.0, 1000.0, dist);\n"\
"	if (fade <= 0.)\n"\
"		return c;\n"\
"	float amp = Water2.w * (kind == 1u ? 1.3 : kind == 2u ? 0.6 : 1.0);\n"\
"	float swell = amp > 0. ? (LiquidSwellHere.y > 0. ? LiquidSwellHere.x : LiquidSwell(pos.xy, kind).z) / amp : 0.0; // -1 .. 1\n"\
"	float width = (kind == 1u ? 8.0 : kind == 2u ? 7.0 : 10.0) * (0.5 + 0.5 * Water3.x) * (1.0 + 0.3 * swell + 0.1 * clamp(h, -1.0, 1.0)) * look.z; // (look.z: strength)\n"\
"	vec4 breath = FoamBreath[li];\n"\
"	if (breath.x > 0.) // breathing: a phase from a slow noise along the shore (z: how much it varies), a sine in time\n"\
"	{\n"\
"		float nb = LiquidNoise(pos.xy * 0.0125 + vec2(37.0 * float(li)));\n"\
"		float s = sin(6.2831853 * (Time * 0.25 * breath.y + 2.0 * breath.z * nb)) * (1.0 - 0.5 * breath.z + breath.z * nb);\n"\
"		width *= max(1.0 + breath.x * s, 0.1);\n"\
"	}\n"\
"	float d = rim > 0. ? rim - 1.0 : 1e4;\n"\
"	if (CausticsScale.w != 0.)\n"\
"	{\n"\
"		float w = 1.0 / gl_FragCoord.w; // the surface's distance along the view\n"\
"		float behind = LiquidSceneDistanceHere() - w;\n"\
"		if (behind >= 0.) // (less: something in front, beside it)\n"\
"			d = min(d, behind * dist / w);\n"\
"	}\n"\
"	if (look.w > 0. && d < look.w + 8.0) // the wavy edge (look.w units): the shore's line drawn back in slow curves\n"\
"	{ // drifting along it, wider where the swells push in; the liquid see-through past it (LiquidEdge), the foam following it\n"\
"		float te = Time * (kind == 1u ? 0.15 : kind == 2u ? 0.3 : 0.6);\n"\
"		vec2 q = pos.xy * (0.5 + 0.5 * look.y);\n"\
"		float e = LiquidNoise(q * 0.045 + vec2(te * 0.5, -te * 0.4)) * 0.65 + LiquidNoise(q * 0.13 - vec2(te * 0.7, te * 0.6)) * 0.35;\n"\
"		float shift = look.w * fade * clamp((e - 0.3) * 2.0 + 0.3 * swell, 0.0, 1.0);\n"\
"		float soft = 1.0 + dist * 0.003; // (wider far off: no shimmer)\n"\
"		d -= shift;\n"\
"		LiquidEdge = 1.0 - (1.0 - smoothstep(-soft, soft, d)) * min(shift / soft, 1.0);\n"\
"	}\n"\
"	float f = 1.0 - d / width; // 1 at the shore, 0 width away\n"\
"	bool crest = kind == 4u && swell > 0.75;\n"\
"	if (f <= -1. && !crest) // (to twice the width: flecks drifting off it)\n"\
"		return c;\n"\
"	float t = Time * (kind == 1u ? 0.25 : kind == 2u ? 0.4 : 1.0);\n"\
"	vec2 p = pos.xy * look.y; // (frequency: how finely it is broken up)\n"\
"	float n = LiquidNoise(p * 0.19 + vec2(t * 0.31, -t * 0.23)) * 0.6 + LiquidNoise(p * 0.47 - vec2(t * 0.47, t * 0.38)) * 0.4;\n"\
"	float fine = LiquidNoise(p * 0.6 + vec2(t * 0.7, t * 0.5));\n"\
"	float foam = smoothstep(n - 0.12, n + 0.12, f) * (0.6 + 0.4 * fine); // the foam reaches further where the noise is low\n"\
"	foam = max(foam, smoothstep(0.78, 0.97, f)); // a line at the shore itself\n"\
"	foam = max(foam, smoothstep(0.7, 0.85, fine) * smoothstep(0.45, 0.6, n) * 0.5 * clamp(f + 1.0, 0.0, 1.0)); // and flecks\n"\
"	if (crest)\n"\
"		foam = max(foam, smoothstep(0.75, 1.0, swell) * smoothstep(0.55, 0.8, n) * fine * 0.35);\n"\
"	fade *= look.x; // (opacity)\n"\
"	foam = min(foam * fade * min(Water3.x, 1.0), 1.0);\n"\
"	float b = lit ? min(dot(light, vec3(1.0 / 3.0)), 1.3) : clamp(dot(tex, vec3(0.3, 0.59, 0.11)) * 3.0 + 0.2, 0.4, 1.0);\n"\
"	vec3 col = FoamColor[li].rgb;\n"\
"	if (kind == 1u) // lava: a molten crust in its colour, glowing (unlit), darker in its cracks; hot where it touches the rock\n"\
"	{\n"\
"		float hot = smoothstep(n * 0.7 + 0.2, n * 0.7 + 0.45, f) * (0.75 + 0.25 * sin(Time * 2.3 + n * 9.0));\n"\
"		float glow = 0.6 + 0.6 * Water2.x;\n"\
"		c = mix(c, col * (0.55 + 0.45 * fine) * glow, foam * 0.85 * (1.0 - 0.5 * hot));\n"\
"		c *= 1.0 - 0.35 * foam * (1.0 - hot) * smoothstep(0.55, 0.85, 1.0 - fine);\n"\
"		return c + col * (hot * fade * min(Water3.x, 1.0) * glow);\n"\
"	}\n"\
"	if (kind == 2u)\n"\
"		col *= 0.7 + 0.3 * fine;\n"\
"	alpha = mix(alpha, 1.0, foam * 0.9);\n"\
"	return mix(c, col * b, foam * (kind == 2u ? 0.75 : 0.85));\n"\
"}\n"\
"\n"\
"// Blood in a liquid (BloodInfo.x clouds, 0 none: vr/vr_water.cpp's, vr_water_blood): dark red clouds on water (a\n"\
"// brown on slime; none on lava or teleports), spreading from where something bled into it, thickest at the centre,\n"\
"// their edges broken by a slowly drifting noise. On the surface (either side) over what is behind it: c the liquid as\n"\
"// drawn (refracted), lit by b (1 Quake's full light). The clouds' list is short (16) and each is two tests away.\n"\
"vec4 LiquidBlood(vec4 c, vec3 pos, vec3 facing, uint kind, float b)\n"\
"{\n"\
"	int n = int(BloodInfo.x);\n"\
"	if (n <= 0 || kind == 1u || kind == 3u || abs(facing.z) < 0.5)\n"\
"		return c;\n"\
"	float k = 0.0, nz = -1.0;\n"\
"	for (int i = 0; i < n; i++)\n"\
"	{\n"\
"		vec4 e = BloodAt[i];\n"\
"		vec2 d = pos.xy - e.xy;\n"\
"		float r2 = dot(d, d);\n"\
"		if (r2 >= e.w * e.w * 1.3 || abs(pos.z - e.z) > 40.0)\n"\
"			continue;\n"\
"		if (nz < 0.)\n"\
"		{\n"\
"			float t = Time * (kind == 2u ? 0.4 : 1.0);\n"\
"			vec2 p = pos.xy * 0.035 + vec2(t * 0.04, -t * 0.03);\n"\
"			nz = LiquidNoise(p) * 0.75 + LiquidNoise(p * 2.3 - vec2(t * 0.07, t * 0.05)) * 0.25;\n"\
"		}\n"\
"		float r = sqrt(r2) / e.w + (nz - 0.5) * 0.5;\n"\
"		float m = BloodAmp[i >> 2][i & 3] * (1.0 - smoothstep(0.3, 1.0, r)) * (1.15 - 0.3 * clamp(r, 0.0, 1.0));\n"\
"		k += m * (1.0 - min(k, 1.0));\n"\
"	}\n"\
"	if (k <= 0.)\n"\
"		return c;\n"\
"	k = min(k * 0.75, 0.88) * (1.0 - smoothstep(1200.0, 2000.0, distance(pos, EyePos)));\n"\
"	vec3 col = kind == 2u ? vec3(0.2, 0.09, 0.02) : vec3(0.32, 0.02, 0.015);\n"\
"	return vec4(mix(c.rgb, col * min(b, 1.1), k), mix(c.a, 1.0, k));\n"\
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
