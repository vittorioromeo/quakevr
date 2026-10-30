// vr_glsl_alias.h -- Quake VR's GLSL, the model (alias) shaders' pieces: dents, skinning, wound paint, light, glow: part of vr_glsl.h (included there, in this order; macros
// only, spliced into gl_shaders.h's shaders by name).

#pragma once

// the recoil steadied (ZeroBlend), the poses' occlusion
// flesh dents (vr/vr_dents.cpp): a body's dents, 3 rows each from its instance's Dent.x: the centre in the model's rest
// pose (units) and the radius; the struck surface's rest normal and the depth; a blade's long axis and how drawn out
// along it (1 round). Each pushes in as a smooth bowl (1 - t^2)^2 (t its distance in radii, squashed along the axis),
// only where the surface faces its own (an arm lying on the chest in the rest pose isn't dented with it); the deepest
// counts. Read by the alias vertex shader (the vertices pushed in) and fragment shader (the crater, its bowl's light).
#define QVR_DENT_FUNCTIONS \
"layout(std430, binding=12) restrict readonly buffer DentBuffer { vec4 Dents[]; };\n" \
"// How far the dents push in at rest point p (rest normal rn); t the nearest one's distance in its radii (out to\n" \
"// `reach` of them, shrunk where the surface turns away), g the depth's gradient (rest space).\n" \
"float DentDepth(vec3 p, vec3 rn, uint first, uint count, float reach, out float t, out vec3 g)\n" \
"{\n" \
"	float best = 0.0;\n" \
"	t = 1e9;\n" \
"	g = vec3(0.0);\n" \
"	for (uint k = 0u; k < count; k++)\n" \
"	{\n" \
"		uint i = first + 3u * k;\n" \
"		vec4 c = Dents[i], m = Dents[i + 1u], a = Dents[i + 2u];\n" \
"		float face = smoothstep(-0.1, 0.45, dot(rn, m.xyz));\n" \
"		vec3 d = p - c.xyz;\n" \
"		float squash = 1.0 - 1.0 / max(a.w, 1.0);\n" \
"		vec3 q = d - a.xyz * (dot(d, a.xyz) * squash);\n" \
"		float rr = c.w * c.w;\n" \
"		float off = dot(q, m.xyz); // off its surface (in front of it, behind it): flattened, a gun held before the chest left out\n" \
"		float t2 = (dot(q, q) + 3.0 * off * off) / rr;\n" \
"		if (face <= 0.0 || t2 >= reach * reach)\n" \
"			continue;\n" \
"		t = min(t, sqrt(t2) / face);\n" \
"		if (t2 < 1.0)\n" \
"		{\n" \
"			float k1 = 1.0 - t2;\n" \
"			float h = m.w * face * k1 * k1;\n" \
"			if (h > best)\n" \
"			{\n" \
"				best = h;\n" \
"				g = (-4.0 * m.w * face * k1 / rr) * (q - a.xyz * (dot(q, a.xyz) * squash) + 3.0 * off * m.xyz);\n" \
"			}\n" \
"		}\n" \
"	}\n" \
"	return best;\n" \
"}\n"

// flesh dents (vr/vr_dents.cpp): each vertex measured in the rest pose (a .mdl's pose 0 in units; a skeletal model's
// bind pose: a ragdoll's is that same pose 0) and pushed in along its normal as drawn, after every pose, smoothing,
// rebuild and kick: the dents go with the body however it moves. Times the vertex's flesh (its model's table, from the
// body's header row before its dents: 0 a weapon, never dented or painted; out_restnor.w for the fragment shader).
#define QVR_ALIAS_VS_DENTS \
"	out_restpos = vec3(0.0);\n" \
"	out_restnor = vec4(0.0);\n" \
"	if (inst.Dent.y > 0.5)\n" \
"	{\n" \
"#if POSEVERTTYPE == 1\n" \
"		PoseVertex rest = PoseVertex(in_pos, in_nor.xyz);\n" \
"#else\n" \
"		PoseVertex rest = GetPoseVertex(0u);\n" \
"#endif\n" \
"		float flesh = 1.0;\n" \
"		uint fleshAt = uint(Dents[uint(inst.Dent.x) - 1u].x);\n" \
"		if (fleshAt > 0u)\n" \
"		{\n" \
"			uint fv = fleshAt - 1u + uint(gl_VertexID);\n" \
"			flesh = Dents[fv >> 2u][fv & 3u];\n" \
"		}\n" \
"		out_restpos = rest.pos * inst.DentScale.xyz + vec3(inst.Dent.zw, inst.DentScale.w);\n" \
"		out_restnor = vec4(normalize(rest.nor), flesh);\n" \
"		float dent_t;\n" \
"		vec3 dent_g;\n" \
"		float push = flesh > 0.0 ? flesh * DentDepth(out_restpos, out_restnor.xyz, uint(inst.Dent.x), uint(inst.Dent.y), 1.0, dent_t, dent_g) : 0.0;\n" \
"		if (push > 0.0)\n" \
"		{\n" \
"			vec3 dn = mix(pose1.nor, pose2.nor, inst.Blend);\n" \
"			lerpedPos -= dn * (push * inversesqrt(max(dot(dn, dn), 1e-6))) / inst.DentScale.xyz;\n" \
"		}\n" \
"	}\n"

#define QVR_ALIAS_VS_FUNCTIONS \
"#if POSEVERTTYPE == 1 || POSEVERTTYPE == 2\n" \
"vec3 ZeroBlend(vec3 pos, InstanceData inst) { return pos; }\n" \
"#else\n" \
"// QVR: the recoil steadied (vr/vr_render.cpp): the pose blended towards frame 0 (Padding: the zero pose times the\n" \
"// vertex count, the blend * 255 in the top byte). A muzzle flash's vertex (the top bit of its normal's 4th byte,\n" \
"// gl_mesh.c) keeps the flash's size: moved only as far as the gun vertex it rides on (its 4th bytes) is steadied.\n" \
"vec3 PackedPos(uint index)\n" \
"{\n" \
"	return Dequantised(vec3((PackedPosNor[index].xxx >> uvec3(0, 8, 16)) & 255u), index); // dequantised (vr_mdldequant.cpp)\n" \
"}\n" \
"vec3 ZeroBlend(vec3 pos, InstanceData inst)\n" \
"{\n" \
"	if (inst.Padding == 0)\n" \
"		return pos;\n" \
"	uint zero = uint(inst.Padding) & 0xFFFFFFu;\n" \
"	float blend = float(uint(inst.Padding) >> 24) / 255.0;\n" \
"	uvec2 z = PackedPosNor[zero + uint(gl_VertexID)];\n" \
"	if ((z.y & 0x80000000u) == 0u)\n" \
"		return mix(pos, PackedPos(zero + uint(gl_VertexID)), blend);\n" \
"	uint ref = (z.x >> 24) | ((z.y >> 16) & 0x7F00u);\n" \
"	vec3 gun = mix(PackedPos(uint(inst.Pose1) + ref), PackedPos(uint(inst.Pose2) + ref), inst.Blend);\n" \
"	return pos - (gun - PackedPos(zero + ref)) * blend;\n" \
"}\n" \
"#endif\n" \
"\n" \
"#if POSEVERTTYPE == 1 // PV_IQM\n" \
"int HeaviestBone() // QVR: the vertex's bone of the most weight (its side's, vr/vr_wounds.cpp; its part's, vr/vr_retro.cpp)\n" \
"{\n" \
"	int bone = in_indices.x;\n" \
"	float most = in_weights.x;\n" \
"	if (in_weights.y > most) { bone = in_indices.y; most = in_weights.y; }\n" \
"	if (in_weights.z > most) { bone = in_indices.z; most = in_weights.z; }\n" \
"	if (in_weights.w > most) { bone = in_indices.w; }\n" \
"	return bone;\n" \
"}\n" \
"#endif\n" \
"\n" \
"#if POSEVERTTYPE == 0 // PV_QUAKE1\n" \
"// QVR: r_lerpmodels 3's limbs (vr/vr_animsmooth.cpp): each vertex's place in its rig's rest pose (model units), its bone\n" \
"// plus half how much it swings with it (0..0.5: its fit's residual), and each instance's bones' swings (3 rows each: the\n" \
"// bone's rigid motion between the poses less its chord, in the poses' units), added to the spline's place.\n" \
"layout(std430, binding=8) restrict readonly buffer SmoothRestBuffer { vec4 SmoothRest[]; };\n" \
"layout(std430, binding=9) restrict readonly buffer SmoothSwingBuffer { vec4 SmoothSwing[]; };\n" \
"vec3 RigSwing(uint base)\n" \
"{\n" \
"	vec4 r = SmoothRest[4u * uint(gl_VertexID)];\n" \
"	float bone = floor(r.w);\n" \
"	uint i = base + uint(bone) * 3u;\n" \
"	vec4 p = vec4(r.xyz, 1.0);\n" \
"	return (r.w - bone) * 2.0 * vec3(dot(SmoothSwing[i], p), dot(SmoothSwing[i + 1u], p), dot(SmoothSwing[i + 2u], p));\n" \
"}\n" \
"// QVR: a living monster's hit reaction (vr/vr_hitreact.cpp): its bones' kicks (3 rows each, after its swings), every\n" \
"// vertex moved whole with its bone.\n" \
"vec3 RigKick(uint base)\n" \
"{\n" \
"	vec4 r = SmoothRest[4u * uint(gl_VertexID)];\n" \
"	uint i = base + uint(floor(r.w)) * 3u;\n" \
"	vec4 p = vec4(r.xyz, 1.0);\n" \
"	return vec3(dot(SmoothSwing[i], p), dot(SmoothSwing[i + 1u], p), dot(SmoothSwing[i + 2u], p));\n" \
"}\n" \
"// QVR: an armed humanoid's arms (vr/vr_agarms.cpp, vr_agarms): its vertex weighed between its arm's bones (the rest\n" \
"// row's fourth: x 1 + side * 4 + its bone and its blend to the next, y its share of the body as animated, z its rest\n" \
"// normal packed, w its arm rows' offset from `base`), each bone a turn and a move of the rest place (model units; the\n" \
"// rows: origin and on-share, 1 / scale, six bones' quaternion and move, the four joints' rest places, the chest, the\n" \
"// shoulders' rest places, the clavicles' quaternion and move; w's fraction its clavicle's share). Two bones are\n" \
"// blended about their joint (the turns interpolated: no gap, no shrinking), the hand rigid.\n" \
"vec3 QvrQRot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }\n" \
"vec3 QvrOctNormal(float pk)\n" \
"{\n" \
"	float a = floor(pk / 4096.0);\n" \
"	vec2 e = vec2(a, pk - a * 4096.0) * (2.0 / 4095.0) - 1.0;\n" \
"	vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));\n" \
"	float t = max(-n.z, 0.0);\n" \
"	n.x += n.x >= 0.0 ? -t : t;\n" \
"	n.y += n.y >= 0.0 ? -t : t;\n" \
"	return normalize(n);\n" \
"}\n" \
"vec3 RigArm(uint base, vec3 pos, inout vec3 nor)\n" \
"{\n" \
"	vec4 s = SmoothRest[4u * uint(gl_VertexID) + 3u];\n" \
"	if (s.x < 0.5 || s.x > 8.5)\n" \
"		return pos;\n" \
"	uint b = base + uint(s.w);\n" \
"	vec4 info = SmoothSwing[b];\n" \
"	if (info.w <= 0.0)\n" \
"		return pos;\n" \
"	float code = s.x - 1.0;\n" \
"	float side = floor(code * 0.25);\n" \
"	float c = code - side * 4.0;\n" \
"	float bone = min(floor(c), 2.0);\n" \
"	float f = c - bone;\n" \
"	uint k = uint(side * 3.0 + bone);\n" \
"	vec4 qa = SmoothSwing[b + 2u + 2u * k];\n" \
"	vec3 ta = SmoothSwing[b + 3u + 2u * k].xyz;\n" \
"	vec3 r = SmoothRest[4u * uint(gl_VertexID)].xyz;\n" \
"	vec3 n0 = QvrOctNormal(s.z);\n" \
"	vec3 p;\n" \
"	vec3 n;\n" \
"	// y: 2 * round(frame share * 1000) + body share. The body near the arms moves as the chest, not as its frames\n" \
"	// (which swing the shoulders with the animation's arms); further off, more as its frames.\n" \
"	float fq = floor(s.y * 0.5);\n" \
"	float body = 1.0 - (1.0 - (s.y - 2.0 * fq)) * info.w;\n" \
"	vec3 invS = SmoothSwing[b + 1u].xyz;\n" \
"	vec4 qc = SmoothSwing[b + 18u];\n" \
"	vec3 framed = pos / invS + info.xyz;\n" \
"	vec3 chest = QvrQRot(qc, r) + SmoothSwing[b + 19u].xyz;\n" \
"	// the shoulder's top on its clavicle by w's fraction (vr_agarms_clavicle; rows 22.. each side's turn and move)\n" \
"	float kc = fract(s.w) * (1.0 / 0.99);\n" \
"	if (kc > 0.001)\n" \
"	{\n" \
"		vec4 qk = SmoothSwing[b + 22u + 2u * uint(side)];\n" \
"		if (dot(qk, qc) < 0.0)\n" \
"			qk = -qk;\n" \
"		chest = mix(chest, QvrQRot(qk, r) + SmoothSwing[b + 23u + 2u * uint(side)].xyz, kc);\n" \
"		qc = normalize(mix(qc, qk, kc));\n" \
"	}\n" \
"	vec3 bodyAt = mix(framed, chest + (fq * 0.001) * (framed - chest), info.w);\n" \
"	if (body >= 0.9995)\n" \
"		return (bodyAt - info.xyz) * invS;\n" \
"	// the IK stretch (the upper arm's move row's w: the stretch - 1; vr_agarms.cpp stretchedRest): the rest place\n" \
"	// stretched along the upper arm about the shoulder and the forearm about the elbow, the blend's joint on its bone\n" \
"	vec3 rs = r;\n" \
"	vec3 jOff = vec3(0.0);\n" \
"	float sg = SmoothSwing[b + 3u + 6u * uint(side)].w;\n" \
"	if (sg != 0.0 && bone < 1.5)\n" \
"	{\n" \
"		vec3 S0 = SmoothSwing[b + 20u + uint(side)].xyz;\n" \
"		vec3 E0 = SmoothSwing[b + 14u + uint(side * 2.0)].xyz;\n" \
"		vec3 W0 = SmoothSwing[b + 15u + uint(side * 2.0)].xyz;\n" \
"		float lu = length(E0 - S0);\n" \
"		float lf = length(W0 - E0);\n" \
"		vec3 u0 = (E0 - S0) / max(lu, 1e-4);\n" \
"		vec3 f0 = (W0 - E0) / max(lf, 1e-4);\n" \
"		float tf = clamp(dot(r - E0, f0), 0.0, lf);\n" \
"		if (bone < 0.5)\n" \
"		{\n" \
"			float tu = clamp(dot(r - S0, u0), 0.0, lu);\n" \
"			rs = r + sg * ((tu + (lu - tu) * f) * u0 + (f * tf) * f0);\n" \
"			jOff = (sg * lu) * u0;\n" \
"		}\n" \
"		else\n" \
"		{\n" \
"			rs = r + (sg * (tf + (lf - tf) * f)) * f0;\n" \
"			jOff = (sg * lf) * f0;\n" \
"		}\n" \
"	}\n" \
"	if (bone < 0.5 && f < 0.0005 && body > 0.0005)\n" \
"	{\n" \
"		// the shoulder: the upper arm's turn and the chest's blended about the shoulder (no flap as the arm rises), plus\n" \
"		// the body's own move off the chest by the body's share\n" \
"		if (dot(qc, qa) < 0.0)\n" \
"			qa = -qa;\n" \
"		vec4 q = normalize(mix(qa, qc, body));\n" \
"		vec3 s0 = SmoothSwing[b + 20u + uint(side)].xyz;\n" \
"		p = QvrQRot(q, rs - s0) + QvrQRot(qa, s0) + ta + body * (bodyAt - chest);\n" \
"		nor = mix(QvrQRot(q, n0) * max(length(nor), 1e-3), nor, body);\n" \
"		return (p - info.xyz) * invS;\n" \
"	}\n" \
"	if (f > 0.0005 && bone < 1.5)\n" \
"	{\n" \
"		vec4 qb = SmoothSwing[b + 4u + 2u * k];\n" \
"		if (dot(qa, qb) < 0.0)\n" \
"			qb = -qb;\n" \
"		vec4 q = normalize(mix(qa, qb, f));\n" \
"		vec3 j0 = SmoothSwing[b + 14u + uint(side * 2.0 + bone)].xyz + jOff;\n" \
"		p = QvrQRot(q, rs - j0) + QvrQRot(qa, j0) + ta;\n" \
"		n = QvrQRot(q, n0);\n" \
"	}\n" \
"	else\n" \
"	{\n" \
"		p = QvrQRot(qa, rs) + ta;\n" \
"		n = QvrQRot(qa, n0);\n" \
"	}\n" \
"	nor = mix(n * max(length(nor), 1e-3), nor, body);\n" \
"	return (mix(p, bodyAt, body) - info.xyz) * invS;\n" \
"}\n" \
"// QVR: an armed humanoid's legs in a crouch (vr/vr_aglegs.cpp, vr_aglegs): its vertex weighed between its leg's bones\n" \
"// (the rest row's fourth: x 9 + side * 4 + its bone (thigh, shin, foot) and its blend to the next, y its hip's share on\n" \
"// the pelvis, z its rest normal packed, w its leg rows' offset from `base`), each bone a turn and a move of the rest\n" \
"// place (model units; the rows: origin and on-share, 1 / scale, six bones' quaternion and move, the knees' and ankles'\n" \
"// rest places, the pelvis as drawn, the hips' rest places). Blended about their joint, the foot rigid.\n" \
"vec3 RigLeg(uint base, vec3 pos, inout vec3 nor)\n" \
"{\n" \
"	vec4 s = SmoothRest[4u * uint(gl_VertexID) + 3u];\n" \
"	if (s.x < 8.5)\n" \
"		return pos;\n" \
"	uint b = base + uint(s.w);\n" \
"	vec4 info = SmoothSwing[b];\n" \
"	if (info.w <= 0.0)\n" \
"		return pos;\n" \
"	float code = s.x - 9.0;\n" \
"	float side = floor(code * 0.25);\n" \
"	float c = code - side * 4.0;\n" \
"	float bone = min(floor(c), 2.0);\n" \
"	float f = c - bone;\n" \
"	uint k = uint(side * 3.0 + bone);\n" \
"	vec4 qa = SmoothSwing[b + 2u + 2u * k];\n" \
"	vec3 ta = SmoothSwing[b + 3u + 2u * k].xyz;\n" \
"	vec3 r = SmoothRest[4u * uint(gl_VertexID)].xyz;\n" \
"	vec3 invS = SmoothSwing[b + 1u].xyz;\n" \
"	vec3 framed = pos / invS + info.xyz;\n" \
"	vec4 q = qa;\n" \
"	vec3 j = vec3(0.0);\n" \
"	if (f > 0.0005 && bone < 1.5)\n" \
"	{\n" \
"		vec4 qb = SmoothSwing[b + 4u + 2u * k];\n" \
"		if (dot(qa, qb) < 0.0)\n" \
"			qb = -qb;\n" \
"		q = normalize(mix(qa, qb, f));\n" \
"		j = SmoothSwing[b + 14u + uint(side * 2.0 + bone)].xyz;\n" \
"	}\n" \
"	else if (bone < 0.5 && s.y > 0.0005)\n" \
"	{\n" \
"		vec4 qp = SmoothSwing[b + 18u];\n" \
"		if (dot(qa, qp) < 0.0)\n" \
"			qp = -qp;\n" \
"		q = normalize(mix(qa, qp, s.y));\n" \
"		j = SmoothSwing[b + 20u + uint(side)].xyz;\n" \
"	}\n" \
"	vec3 p = QvrQRot(q, r - j) + QvrQRot(qa, j) + ta;\n" \
"	nor = mix(nor, QvrQRot(q, QvrOctNormal(s.z)) * max(length(nor), 1e-3), info.w);\n" \
"	return (mix(framed, p, info.w) - info.xyz) * invS;\n" \
"}\n" \
"// QVR: vr_anim_smooth_rebuild: 9 rows a bone (its swing, its motion now from its rest place to the poses' bytes, its\n" \
"// turn with its share in the first's w); each vertex's rebuild place on its bone (units) and share, its normal so: the\n" \
"// vertex (and normal) moved from the spline's (`pos`, swung) to its bone's by both shares.\n" \
"vec3 RigRebuild(uint base, vec3 pos, inout vec3 nor)\n" \
"{\n" \
"	uint v = 4u * uint(gl_VertexID);\n" \
"	vec4 r = SmoothRest[v];\n" \
"	float bone = floor(r.w);\n" \
"	uint i = base + uint(bone) * 9u;\n" \
"	vec4 p = vec4(r.xyz, 1.0);\n" \
"	pos += (r.w - bone) * 2.0 * vec3(dot(SmoothSwing[i], p), dot(SmoothSwing[i + 1u], p), dot(SmoothSwing[i + 2u], p));\n" \
"	vec4 q = SmoothRest[v + 1u];\n" \
"	float k = q.w * SmoothSwing[i + 6u].w;\n" \
"	if (k > 0.0)\n" \
"	{\n" \
"		vec4 c = vec4(q.xyz, 1.0);\n" \
"		vec3 n = SmoothRest[v + 2u].xyz;\n" \
"		pos = mix(pos, vec3(dot(SmoothSwing[i + 3u], c), dot(SmoothSwing[i + 4u], c), dot(SmoothSwing[i + 5u], c)), k);\n" \
"		nor = mix(nor, vec3(dot(SmoothSwing[i + 6u].xyz, n), dot(SmoothSwing[i + 7u].xyz, n), dot(SmoothSwing[i + 8u].xyz, n)), k);\n" \
"	}\n" \
"	return pos;\n" \
"}\n" \
"\n" \
"// QVR: the model's own occlusion at this vertex in a pose (vr/vr_ao.cpp: 1 open), in its position's 4th byte\n" \
"// (gl_mesh.c); a muzzle flash's vertex keeps its gun vertex there (the flag in its normal's 4th byte): none.\n" \
"float PoseAO(uint pose)\n" \
"{\n" \
"	uvec2 data = PackedPosNor[pose + gl_VertexID];\n" \
"	return (data.y & 0x80000000u) != 0u ? 1.0 : float(data.x >> 24) * (1.0 / 255.0);\n" \
"}\n" \
"#elif POSEVERTTYPE == 1 // PV_IQM\n" \
"// QVR: a ragdoll's skinned model's occlusion (vr/vr_ragdoll.cpp: its .mdl's over all its poses), negated in its normal's\n" \
"// 4th byte; 0 (every other skeletal model): none.\n" \
"float PoseAO(uint pose) { return in_nor.w < 0.0 ? -in_nor.w : 1.0; }\n" \
"#else\n" \
"float PoseAO(uint pose) { return 1.0; }\n" \
"#endif\n" \
"\n"

// r_lerpmodels 3 (vr/vr_animsmooth.cpp): the vertex along a spline through four poses (the lerp's two, the one before and
// the one expected after: weights SmoothW, all on one pose at a pose itself), plus its bone's swing (a rigged monster's:
// limbs keep their length mid-frame); the normal along the same spline, given to both poses for the shading after.
#define QVR_ALIAS_VS_SMOOTH \
"#if POSEVERTTYPE != 1\n" \
"	if (inst.Smooth.w != 0)\n" \
"	{\n" \
"		PoseVertex pose0 = GetPoseVertex(uint(inst.Smooth.x));\n" \
"		PoseVertex pose3 = GetPoseVertex(uint(inst.Smooth.y));\n" \
"		vec4 w = inst.SmoothW;\n" \
"		lerpedPos = w.x * pose0.pos + w.y * pose1.pos + w.z * pose2.pos + w.w * pose3.pos;\n" \
"		vec3 smoothnor = w.x * pose0.nor + w.y * pose1.nor + w.z * pose2.nor + w.w * pose3.nor;\n" \
"#if POSEVERTTYPE == 0\n" \
"		if (inst.Smooth.z > 0)\n" \
"			lerpedPos += RigSwing(uint(inst.Smooth.z - 1));\n" \
"		else if (inst.Smooth.z < 0)\n" \
"			lerpedPos = RigRebuild(uint(-inst.Smooth.z - 1), lerpedPos, smoothnor);\n" \
"		if (inst.Smooth.w > 1)\n" \
"		{\n" \
"			lerpedPos += RigKick(uint(inst.Smooth.w - 2));\n" \
"			lerpedPos = RigArm(uint(inst.Smooth.w - 2), lerpedPos, smoothnor);\n" \
"			lerpedPos = RigLeg(uint(inst.Smooth.w - 2), lerpedPos, smoothnor);\n" \
"		}\n" \
"#endif\n" \
"		pose1.nor = smoothnor;\n" \
"		pose2.nor = smoothnor;\n" \
"	}\n" \
"#endif\n"

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
"layout(location=11) out vec3 out_morphpos; // QVR: where in the model's pose (byte) space: a gun's morph (times the model's scale, Ambient[3..5].w: vr/vr_render.cpp), an incandescent blade's mask (HotBlade: vr/vr_hotblade.cpp)\n" \
"layout(location=12) flat out float out_morph; // QVR: and how far (Ambient[2].w; 0 not morphing)\n" \
"layout(location=13) out float out_vao; // QVR: the model's own occlusion here (vr/vr_ao.cpp), times its strength: 1 none\n" \
"layout(location=14) flat out float out_woundside; // QVR: 1 on its right side (its wound mask's other layer: vr/vr_wounds.cpp)\n" \
"layout(location=15) flat out float out_retroset; // QVR: its retro textures' set (vr/vr_retro.h; your body's by part)\n" \
"layout(location=16) out vec3 out_restpos; // QVR: flesh dents (vr/vr_dents.cpp): where in the model's rest pose (units; zero: no dents)\n" \
"layout(location=17) out vec4 out_restnor; // QVR: ... its normal there, and its flesh (w: 0 a weapon)\n" \
"layout(location=18) out vec4 out_stump; // QVR: a ragdoll's stumps (StumpFlesh): xyz the rest pose's place (units), w how near a cut joint (1: inst.Stump.y; 2 none)\n"

// which side's wound mask a vertex reads (vr/vr_wounds.cpp): your body's arms and legs share their skin's texels,
// mirrored, so its mask is two layers, the left side's (with the middle's) and the right side's; a vertex's side is its
// heaviest bone's (InstanceData's WoundSide.xy: the right side's bones). A triangle's corners are all on one side.
#define QVR_ALIAS_VS_WOUNDSIDE \
"	out_woundside = 0.0;\n" \
"#if POSEVERTTYPE == 1\n" \
"	if (inst.WoundSide.x + inst.WoundSide.y > 0.5)\n" \
"	{\n" \
"		int bone = HeaviestBone();\n" \
"		uint bits = bone < 24 ? uint(inst.WoundSide.x) >> uint(bone) : bone < 48 ? uint(inst.WoundSide.y) >> uint(bone - 24) : 0u;\n" \
"		out_woundside = float(bits & 1u);\n" \
"	}\n" \
"#endif\n" \
"	out_stump = vec4(0.0, 0.0, 0.0, 2.0); // QVR: a ragdoll's stumps (inst.Stump: vr/vr_ragdoll.cpp stumpLook): how near a joint cut\n" \
"#if POSEVERTTYPE == 1\n" \
"	if (inst.Stump.x > 0.5) // ... its bone's vertices shrunk to it (neckShrink): any of them there, this one's too\n" \
"	{\n" \
"		uint cuts = uint(inst.Stump.x);\n" \
"		float near = 2.0;\n" \
"		for (uint c = 0u; c < 24u; c++)\n" \
"		{\n" \
"			if ((cuts & (1u << c)) == 0u)\n" \
"				continue;\n" \
"			mat3x4 sb = BonePoses[uint(inst.Pose1) + c];\n" \
"			float sk = length(vec3(sb[0].x, sb[1].x, sb[2].x));\n" \
"			if (sk < 1e-6) // (a hidden bone's: all of it at nothing)\n" \
"				continue;\n" \
"			vec3 joint = (transpose(sb) * vec4(bind_pos, 1.0)).xyz;\n" \
"			near = min(near, distance(lerpedPos, joint) / max(inst.Stump.y, 0.1));\n" \
"		}\n" \
"		out_stump = vec4(bind_pos, near);\n" \
"	}\n" \
"#endif\n"

// the retro textures' set of a vertex (vr/vr_retro.h): the instance's; your body's (RetroPart set) the first of its
// parts' sets (Hands, Arms, Torso, Legs) plus its heaviest bone's part (2 bits: RetroPart.xy the low, .zw the high).
#define QVR_ALIAS_VS_RETRO \
"	out_retroset = inst.Retro.x;\n" \
"#if POSEVERTTYPE == 1\n" \
"	if (inst.Retro.x > 0.5 && dot(inst.RetroPart, vec4(1.0)) > 0.5)\n" \
"	{\n" \
"		int bone = HeaviestBone();\n" \
"		uvec2 bits = bone < 24 ? uvec2(inst.RetroPart.xz) >> uint(bone) : bone < 48 ? uvec2(inst.RetroPart.yw) >> uint(bone - 24) : uvec2(0u);\n" \
"		out_retroset += float(bits.x & 1u) + 2.0 * float(bits.y & 1u);\n" \
"	}\n" \
"#endif\n"

// the model's light direction and shade (vr_model_light_parity), its outputs
#define QVR_ALIAS_VS_SHADE \
"	vec3 lightdir = normalize(mix(vec3(0.70710678, 0.0, 0.70710678), inst.LightDir.xyz, inst.LightDir.w)); // QVR: vr/vr_modellight.cpp\n" \
"	vec3 shadevector = orientation * lightdir;\n" \
"	out_instance = ALIAS_INSTANCE_ID; // QVR: its instance (a layered shadow draw's: FaceDraws)\n" \
"	out_morph = inst.Ambient[2].w; // QVR\n" \
"	out_vao = inst.AO.y > 0. ? max(1.0 - inst.AO.y * (1.0 - mix(PoseAO(inst.Pose1), PoseAO(inst.Pose2), inst.Blend)), 0.0) : 1.0; // QVR\n" \
"	out_morphpos = lerpedPos; // QVR: the pose's space (Morph scales it to the model's units)\n" \
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
"	if (s4.x > 0.0) val *= 1.0 - smoothstep(s4.x * 0.8, s4.x, distance(p, s3.xyz)); // bounded wrist smear\n" \
"	if (s4.z > 0.0)\n" \
"	{\n" \
"		float n = PaintNoise(p * 0.3 + s3.w) * 0.6 + PaintNoise(p * 0.9 + 3.7) * 0.4;\n" \
"		val *= step(s4.z, n);\n" \
"	}\n" \
"	return val;\n" \
"}\n" \
"\n" \
"// A spatter's drops in the cells of x (a lattice of unit cells): one in a cell by chance `chance`, round, a little\n" \
"// drawn out along `dir` (thrown that way), its middle darker (a thick drop's). 0 where none.\n" \
"float SpatterDrops(vec3 x, vec3 dir, float chance)\n" \
"{\n" \
"	vec3 c = floor(x);\n" \
"	if (PaintHash3(c + 0.37) >= chance)\n" \
"		return 0.0;\n" \
"	vec3 o = vec3(PaintHash3(c + 1.13), PaintHash3(c + 2.31), PaintHash3(c + 3.77)) * 0.4 + 0.3;\n" \
"	float r = mix(0.12, 0.3, PaintHash3(c + 4.91));\n" \
"	vec3 q = x - c - o;\n" \
"	float along = dot(q, dir);\n" \
"	float dd = sqrt(max(dot(q, q) - along * along, 0.0) + along * along / 2.4); // (drawn out half as long again)\n" \
"	return dd < r ? mix(0.5, 0.9, 1.0 - dd / r) : 0.0;\n" \
"}\n" \
"\n" \
"// A spatter (shape 4): blood thrown from its centre onto what faces it: all of it covered within s4.x of the centre,\n" \
"// past it a share s4.w x (s4.x / distance)^2 (none at the radius) as drops, s4.z cells a unit (big ones about a third\n" \
"// of a cell across, specks a third of that), where it is dense smeared together; s4.y the least the surface faces it.\n" \
"float PaintSpatter(vec3 v, vec3 p, vec3 nrm, vec4 s0, vec4 s3, vec4 s4)\n" \
"{\n" \
"	float d = length(v);\n" \
"	vec3 dir = v / max(d, 1e-3);\n" \
"	if (d >= s0.w || dot(nrm, -dir) < s4.y)\n" \
"		return 0.0;\n" \
"	float q = d / max(s4.x, 0.1);\n" \
"	float share = clamp(d <= s4.x ? 1.0 : s4.w / (q * q), 0.0, 1.0) * clamp((s0.w - d) / max(s0.w * 0.15, 1e-3), 0.0, 1.0);\n" \
"	vec3 x = p * s4.z + s3.w;\n" \
"	float val = max(SpatterDrops(x, dir, min(1.0, share * 6.0)), SpatterDrops(x * 2.7 + 11.3, dir, min(1.0, share * 3.0)) * 0.9);\n" \
"	if (share > 0.3) // dense: run together\n" \
"	{\n" \
"		float n = PaintNoise(x * 0.8) * 0.7 + PaintNoise(x * 2.3 + 3.7) * 0.3;\n" \
"		float cut = mix(0.75, 0.2, (share - 0.3) / 0.7);\n" \
"		if (n > cut)\n" \
"			val = max(val, mix(0.5, 0.95, clamp((n - cut) * 5.0, 0.0, 1.0)));\n" \
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
"		else if (shape == 4)\n" \
"			val = PaintSpatter(v, p, nrm, s0, s3, s4);\n" \
"		else\n" \
"			val = PaintBurn(v, p, nrm, s0, s1, s3, s4, shape == 2);\n" \
"		total = max(total, s2 * val);\n" \
"	}\n" \
"	out_fragcolor = total;\n" \
"}\n"

// the frame data, lights and functions the alias fragment shader uses
#define QVR_ALIAS_FS_HEADER \
ALIAS_FRAMEDATA_BUFFER \
LIGHT_BUFFER \
LIGHT_CLUSTER_IMAGE("readonly") \
QVR_RETRO_GLSL(QS_STRINGIFY (QVR_RETRO_LUT_UNIT_ALIAS)) \
SHADOW_FUNCTIONS \
AO_FUNCTIONS \
PARALLAX_FUNCTIONS \
SPECULAR_AA_FUNCTIONS

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
"layout(location=11) in vec3 in_morphpos; // QVR: the pose's space: a gun's morph (vr/vr_render.cpp), an incandescent blade's mask\n" \
"layout(location=12) flat in float in_morph; // QVR\n" \
"layout(location=13) in float in_vao; // QVR: the model's own occlusion (vr/vr_ao.cpp)\n" \
"layout(location=14) flat in float in_woundside; // QVR: 1 on its right side (vr/vr_wounds.cpp: your body's mask, one a side)\n" \
"layout(location=15) flat in float in_retroset; // QVR: its retro textures' set (vr/vr_retro.h)\n" \
"layout(location=16) in vec3 in_restpos; // QVR: flesh dents (vr/vr_dents.cpp): where in the model's rest pose\n" \
"layout(location=17) in vec4 in_restnor; // QVR: ... its normal there, and its flesh (w: 0 a weapon)\n" \
"layout(location=18) in vec4 in_stump; // QVR: a stump's flesh (StumpFlesh): xyz the rest pose's place, w how near a cut joint (1 its reach)\n"

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
"	cluster_coord.z = max(int(floor(log2(in_depth) * ZLogScale + ZLogBias)), 0); /* nearer than the first slice (a held gun): in it */\n" \
"	uvec2 clusterdata = QVR_LOAD_CLUSTER(cluster_coord).xy;\n" \
"	if ((clusterdata.x | clusterdata.y) == 0u)\n" \
"		return vec3(0.);\n" \
"	vec3 pos = RetroLightAt(in_pos + EyePos, RetroLight[5].y); /* QVR: or its retro lighting block's (vr_retrolight.h) */\n" \
"	vec3 spos = RetroShadowAt(in_pos + EyePos, pos); /* ... where the shadows are looked up */\n" \
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
"				lit *= mix(RetroShadowQuant(l.shadow2.x > 0. ? SpotShadow(l, spos, n) : ShadowLookup(ShadowAtlas, l.shadow.xyz, ShadowOffset(spos - l.origin, n, l.shadow.z))), 1.0, l.shadow2.y);\n" \
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
"// the skin's own grid, in Quake's palette colours, as the skins' painted ones. Your own body's and hands' (the fine\n" \
"// masks, vr_wounds_own_res; the instance's Wound.x negative) are finer than their skin and read smoothly: soft edges,\n" \
"// the same colours, and a relief (char crusted and cracked, blood sunk into the skin).\n" \
"layout(binding=13) uniform sampler2DArray WoundMasks;\n" \
"layout(binding=15) uniform sampler2DArray WoundMasksFine;\n" \
"layout(binding=10) uniform sampler2DArray WoundBloodFine; // the blood on yours that isn't yours (spatter, gibs): healing leaves it (fine or chunky)\n" \
"\n" \
"struct Wounds\n" \
"{\n" \
"	float blood;   // covered by blood (0 or 1)\n" \
"	float burn;	// charred\n" \
"	float wet;	 // the wet coat's strength (0 dry .. 2)\n" \
"	vec3 look;	 // the texel's colour with them\n" \
"	float gloss;   // a wet sheen's strength\n" \
"	float flatten; // how much of the normal map's bumps they hide\n" \
"	vec3 glow;	 // embers (unlit)\n" \
"	vec2 slope;	// the relief's slope along the skin's u and v (units a whole skin across; the fine masks')\n" \
"};\n" \
"\n" \
"// QVR: a cut-off limb's stump on its body (vr/vr_ragdoll.cpp: the cut bone's vertices shrunk to its joint, the\n" \
"// triangles across the cut a cone closing it): Quake's gibs' flesh, the same recipe as a cut limb's cap\n" \
"// (vr/vr_limbmodel.cpp fleshColour: keep them alike): dark reds of the palette's blood ramp, blotches of brown meat and\n" \
"// pale sinew, dark pits, wet glints, chunky cells (2.5 a unit) of the rest pose (they stay put on the body); the bone\n" \
"// in the cone's middle (r: 0 there, 1 at its rim). Palette colours.\n" \
"uint StumpHash(ivec3 c)\n" \
"{\n" \
"	uint h = uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(c.z) * 83492791u;\n" \
"	h = (h ^ (h >> 13u)) * 0x5bd1e995u;\n" \
"	return h ^ (h >> 15u);\n" \
"}\n" \
"float StumpRand(ivec3 c)\n" \
"{\n" \
"	return float(StumpHash(c) & 0xFFFFFFu) / 16777216.0;\n" \
"}\n" \
"float StumpNoise(vec3 p)\n" \
"{\n" \
"	ivec3 i = ivec3(floor(p));\n" \
"	vec3 f = fract(p);\n" \
"	f = f * f * (3.0 - 2.0 * f);\n" \
"	float a = mix(mix(StumpRand(i), StumpRand(i + ivec3(1, 0, 0)), f.x), mix(StumpRand(i + ivec3(0, 1, 0)), StumpRand(i + ivec3(1, 1, 0)), f.x), f.y);\n" \
"	float b = mix(mix(StumpRand(i + ivec3(0, 0, 1)), StumpRand(i + ivec3(1, 0, 1)), f.x), mix(StumpRand(i + ivec3(0, 1, 1)), StumpRand(i + ivec3(1, 1, 1)), f.x), f.y);\n" \
"	return mix(a, b, f.z);\n" \
"}\n" \
"vec3 StumpFlesh(vec3 c, float r)\n" \
"{\n" \
"	ivec3 cell = ivec3(floor(c));\n" \
"	float w = StumpRand(cell);\n" \
"	float lv = 0.6 * StumpNoise(c * 0.2) + 0.4 * StumpNoise(c * 0.43 + 31.0);\n" \
"	float meat = StumpNoise(c * 0.27 + 71.0);\n" \
"	float bone = 0.14 + 0.06 * (StumpNoise(c * 0.5 + 5.0) - 0.5);\n" \
"	vec3 o;\n" \
"	float l = lv + (w - 0.5) * 0.2;\n" \
"	if (r < bone * 0.4)\n" \
"		o = w < 0.5 ? vec3(55., 43., 31.) : vec3(39., 31., 23.);\n" \
"	else if (r < bone)\n" \
"		o = w < 0.4 ? vec3(107., 87., 71.) : w < 0.75 ? vec3(95., 75., 59.) : vec3(83., 63., 51.);\n" \
"	else if (r < bone * 1.3)\n" \
"		o = w < 0.5 ? vec3(31., 0., 0.) : vec3(23., 0., 0.);\n" \
"	else if (l < 0.3)\n" \
"		o = w < 0.5 ? vec3(15., 11., 7.) : vec3(23., 0., 0.);\n" \
"	else if (w > 0.97 && lv > 0.45)\n" \
"		o = w > 0.985 ? vec3(119., 0., 0.) : vec3(103., 0., 0.);\n" \
"	else if (meat > 0.8 && w > 0.5)\n" \
"	{\n" \
"		int k = min(int((w - 0.5) * 6.0), 2);\n" \
"		o = k == 0 ? vec3(83., 55., 35.) : k == 1 ? vec3(99., 63., 43.) : vec3(111., 71., 51.);\n" \
"	}\n" \
"	else if (meat > 0.64)\n" \
"	{\n" \
"		int k = clamp(int((lv * 1.4 - 0.2 + (w - 0.5) * 0.3) * 5.0), 0, 4);\n" \
"		o = k == 0 ? vec3(35., 19., 7.) : k == 1 ? vec3(47., 23., 11.) : k == 2 ? vec3(59., 31., 15.) : k == 3 ? vec3(75., 35., 19.) : vec3(87., 43., 23.);\n" \
"	}\n" \
"	else\n" \
"		o = vec3(float(7 + 8 * clamp(int(1.0 + lv * lv * 12.0 + (w - 0.5) * 3.0), 1, 13)), 0.0, 0.0);\n" \
"	return o / 255.0;\n" \
"}\n" \
"\n" \
"float WoundHash(vec2 p)\n" \
"{\n" \
"	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n" \
"}\n" \
"\n" \
"// Smooth value noise on a grid of unit cells (the fine masks' char and embers).\n" \
"float WoundNoise(vec2 p)\n" \
"{\n" \
"	vec2 i = floor(p), f = fract(p);\n" \
"	f = f * f * (3.0 - 2.0 * f);\n" \
"	return mix(mix(WoundHash(i), WoundHash(i + vec2(1.0, 0.0)), f.x), mix(WoundHash(i + vec2(0.0, 1.0)), WoundHash(i + vec2(1.0)), f.x), f.y);\n" \
"}\n" \
"\n" \
"// A fine mask (wi.x: -(its layer + 1)) read bilinearly, kept inside its region (wi.yz texels from the corner). Your\n" \
"// body's right side reads its other layer, the last (vr/vr_wounds.cpp: its arms and legs share their skin's texels).\n" \
"vec4 WoundFine(vec2 uv, vec4 wi)\n" \
"{\n" \
"	ivec3 size = textureSize(WoundMasksFine, 0);\n" \
"	vec2 p = clamp(clamp(uv, 0.0, 1.0) * wi.yz, vec2(0.5), wi.yz - 0.5) / vec2(size.xy);\n" \
"	return texture(WoundMasksFine, vec3(p, in_woundside > 0.5 ? float(size.z - 1) : -wi.x - 1.0));\n" \
"}\n" \
"\n" \
"// The blood on a fine mask that isn't its own (WoundBloodFine: the same layers).\n" \
"float WoundOther(vec2 uv, vec4 wi)\n" \
"{\n" \
"	ivec3 size = textureSize(WoundBloodFine, 0);\n" \
"	vec2 p = clamp(clamp(uv, 0.0, 1.0) * wi.yz, vec2(0.5), wi.yz - 0.5) / vec2(size.xy);\n" \
"	return texture(WoundBloodFine, vec3(p, in_woundside > 0.5 ? float(size.z - 1) : -wi.x - 1.0)).r;\n" \
"}\n" \
"\n" \
"// A fine mask's relief in units: blood sunk into the skin (a little at its edge, most at a wound's core), char\n" \
"// crusted and cracked (FrameWater3.zw: their strengths).\n" \
"float WoundHeight(vec2 uv, vec4 wi)\n" \
"{\n" \
"	vec4 m = WoundFine(uv, wi);\n" \
"	float burn = smoothstep(0.16, 0.44, m.g);\n" \
"	float crust = WoundNoise(uv * 150.0) * 0.6 + WoundNoise(uv * 380.0 + 3.1) * 0.4;\n" \
"	float blood = smoothstep(0.2, 0.44, m.r) * 0.35 + smoothstep(0.5, 0.95, m.r) * 0.65;\n" \
"	return FrameWater3.z * burn * (1.0 - 0.6 * smoothstep(0.2, 0.44, m.r)) * (0.3 * crust - 0.1) - FrameWater3.w * 0.2 * blood; // (blood over char smooths it)\n" \
"}\n" \
"\n" \
"Wounds WoundsAt(vec2 uv, vec3 skin)\n" \
"{\n" \
"	Wounds w;\n" \
"	// the wet coat (vr/vr_wounds.cpp wetCoat): GloveShell.z its strength, all of it (w 1: a weapon) or where the mask is wet\n" \
"	float coat = instances[in_instance].GloveShell.z;\n" \
"	w.blood = 0.0; w.burn = 0.0; w.wet = instances[in_instance].GloveShell.w > 0.5 ? coat : 0.0; w.look = skin; w.gloss = 0.0; w.flatten = 0.0; w.glow = vec3(0.0); w.slope = vec2(0.0);\n" \
"	vec4 wi = instances[in_instance].Wound; // x its layer + 1 (0 none; negative: -(its layer + 1) in the fine masks), yz its size in texels, w the time\n" \
"	if (abs(wi.x) < 0.5)\n" \
"		return w;\n" \
"	bool fine = wi.x < 0.0;\n" \
"	vec4 m;\n" \
"	float h, spot, flick, soft;\n" \
"	if (fine) // smooth: the values' own ramps over their edges, noise for the char's colour and the embers\n" \
"	{\n" \
"		m = WoundFine(uv, wi);\n" \
"		m.r = max(m.r, WoundOther(uv, wi)); // (its relief its own wounds' alone: the rest lies on the skin)\n" \
"		if (m == vec4(0.0))\n" \
"			return w;\n" \
"		w.burn = smoothstep(0.16, 0.44, m.g);\n" \
"		w.blood = smoothstep(0.2, 0.44, m.r);\n" \
"		w.wet = max(w.wet, coat * smoothstep(0.004, 0.03, m.b)); // (wet at all: the coat's even, drying as a whole)\n" \
"		h = WoundNoise(uv * 128.0);\n" \
"		spot = smoothstep(0.7, 0.78, WoundNoise(uv * 420.0 + 7.7));\n" \
"		flick = WoundNoise(uv * 160.0 + floor(wi.w * 7.0) * 1.37);\n" \
"		soft = 1.0;\n" \
"		if (FrameWater3.z + FrameWater3.w > 0.0 && m.r + m.g > 0.0)\n" \
"		{\n" \
"			vec2 e = 1.5 / wi.yz; // central differences, a texel and a half each way (soft rims)\n" \
"			w.slope = vec2(WoundHeight(uv + vec2(e.x, 0.0), wi) - WoundHeight(uv - vec2(e.x, 0.0), wi),\n" \
"				WoundHeight(uv + vec2(0.0, e.y), wi) - WoundHeight(uv - vec2(0.0, e.y), wi)) / (2.0 * e);\n" \
"		}\n" \
"	}\n" \
"	else\n" \
"	{\n" \
"		vec2 size = vec2(textureSize(Tex, 0));\n" \
"		vec2 grid = all(lessThanEqual(size, wi.yz * 2.0)) ? size : wi.yz;\n" \
"		vec2 t = floor(clamp(uv, 0.0, 0.99999) * grid);\n" \
"		int layer = in_woundside > 0.5 ? textureSize(WoundMasks, 0).z - 1 : int(wi.x) - 1; // your body's right side: the pool's last layer\n" \
"		ivec2 at = ivec2((t + 0.5) / grid * wi.yz);\n" \
"		m = texelFetch(WoundMasks, ivec3(at, layer), 0);\n" \
"		float other = instances[in_instance].WoundSide.z; // yours: -(the layer + 1) of the blood on it not yours (healing leaves it)\n" \
"		if (other < -0.5)\n" \
"			m.r = max(m.r, texelFetch(WoundBloodFine, ivec3(at, in_woundside > 0.5 ? textureSize(WoundBloodFine, 0).z - 1 : int(-other) - 1), 0).r);\n" \
"		if (m == vec4(0.0))\n" \
"			return w;\n" \
"		ivec2 it = ivec2(t) & 3;\n" \
"		const float bayer[16] = float[16](0., 8., 2., 10., 12., 4., 14., 6., 3., 11., 1., 9., 15., 7., 13., 5.);\n" \
"		float d = (bayer[it.y * 4 + it.x] + 0.5) / 16.0;\n" \
"		h = WoundHash(t);\n" \
"		w.burn = step(0.16 + 0.28 * d, m.g);\n" \
"		w.blood = step(0.2 + 0.24 * d, m.r);\n" \
"		w.wet = max(w.wet, coat * step(0.008, m.b)); // (wet at all, no dither: the coat's even)\n" \
"		spot = step(0.85, h);\n" \
"		flick = WoundHash(t + floor(wi.w * 7.0) * 1.37);\n" \
"		soft = 0.0;\n" \
"	}\n" \
"	float lum = dot(skin, vec3(0.3, 0.59, 0.11));\n" \
"	// char: the skin's shading darkened and browned; the deepest near black (Quake's palette 16..17)\n" \
"	vec3 charred = mix(vec3(0.26, 0.2, 0.15) * (0.35 + 0.9 * lum) * (0.8 + 0.4 * h), mix(vec3(15., 11., 7.), vec3(31., 23., 15.), h) / 255.0,\n" \
"		soft > 0.0 ? smoothstep(0.66, 0.78, m.g) : step(0.7201, m.g));\n" \
"	// blood: Quake's reds, as the body's painted wounds: as the monsters' own (palette 69..75): 69 in the middle, 71 round it, 74 at the tips and runs\n" \
"	vec3 red = vec3(soft > 0.0 ? mix(mix(87., 63., smoothstep(0.58, 0.66, m.r)), 47., smoothstep(0.78, 0.86, m.r))\n" \
"		: m.r > 0.82 ? 47. : m.r > 0.62 ? 63. : 87., 0., 0.) / 255.0;\n" \
"	vec3 c = mix(skin, charred, w.burn);\n" \
"	c = mix(c, red * (0.75 + 0.5 * lum), w.blood * instances[in_instance].WoundSide.w); // a little of the skin's shading through it; the skin itself as much as vr_wounds_blood_alpha leaves\n" \
"	w.look = c; // (wet: darkened over the whole look after, QVR_ALIAS_FS_LIGHT)\n" \
"	w.gloss = w.blood * 0.55 * instances[in_instance].WoundSide.w;\n" \
"	w.flatten = w.blood * 0.75 * instances[in_instance].WoundSide.w;\n" \
"	// embers: a fresh burn glows in its cracks, flickering (Quake's fullbright oranges, 232..236)\n" \
"	if (m.a > 0.0 && w.burn > 0.0)\n" \
"		w.glow = spot * step(0.6, m.g) * step(flick, m.a) * mix(vec3(183., 51., 15.), vec3(219., 127., 59.), flick) / 255.0 * (0.8 * m.a);\n" \
"	return w;\n" \
"}\n" \
"\n" \
"// QVR: a wound's wet sheen (blood, water) in the model's own light, from its direction (no dynamic light): a tight\n" \
"// highlight, and a little at grazing angles; the wet coat's a thin film's, glossier and wider.\n" \
"vec3 WoundSheen(Wounds w, vec3 n)\n" \
"{\n" \
"	if (w.gloss <= 0.0 && w.wet <= 0.0)\n" \
"		return vec3(0.0);\n" \
"	vec3 v = normalize(-in_pos);\n" \
"	vec3 l = normalize(in_bumplight.xyz);\n" \
"	float ndh = max(dot(n, normalize(l + v)), 0.0);\n" \
"	float s = pow(ndh, 48.0);\n" \
"	float nv = 1.0 - clamp(dot(n, v), 0.0, 1.0);\n" \
"	float f = pow(nv, 4.0);\n" \
"	vec3 sheen = min(in_color.rgb, vec3(1.0)) * (w.gloss * s * 0.35 + w.wet * (s * 0.35 + pow(ndh, 12.0) * 0.08 + f * 0.12));\n" \
"	if (instances[in_instance].GloveShell.w > 1.5) // a brain chunk (VR_AliasWetCoat): its rim warm and red, the light come through the soft tissue\n" \
"		sheen += min(in_color.rgb, vec3(1.0)) * vec3(0.13, 0.03, 0.026) * (nv * nv * min(w.wet, 1.0));\n" \
"	return sheen;\n" \
"}\n" \
"\n" \
"// QVR: the normal n tilted by a relief's slope along the skin (WoundsAt's slope: units a uv unit), from the derivatives\n" \
"// of the position and the skin's coordinates: the relief's screen slopes, onto the surface (Mikkelsen's surface gradient).\n" \
"vec3 WoundBump(vec3 n, vec2 slope, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	vec3 r1 = cross(dpdy, n), r2 = cross(n, dpdx);\n" \
"	float det = dot(dpdx, r1);\n" \
"	if (abs(det) < 1e-12)\n" \
"		return n;\n" \
"	vec3 grad = (r1 * dot(slope, duvdx) + r2 * dot(slope, duvdy)) * sign(det);\n" \
"	return normalize(abs(det) * n - grad);\n" \
"}\n" \
"\n" \
"// QVR: a glove's leather grain (vr_gloves): pebbles between thin crevices, a cellular pattern at g (cells): x the\n" \
"// distance to the nearest crevice (F2 - F1), yz its slope (in cells), w the nearest pebble's own random 0..1 (its tone)\n" \
"vec4 LeatherGrain(vec2 g)\n" \
"{\n" \
"	vec2 i = floor(g), f = g - i;\n" \
"	float d1 = 8.0, d2 = 8.0, tone = 0.0;\n" \
"	vec2 v1 = vec2(1.0), v2 = vec2(1.0);\n" \
"	for (int y = -1; y <= 1; y++)\n" \
"		for (int x = -1; x <= 1; x++)\n" \
"		{\n" \
"			uvec2 q = uvec2(ivec2(i) + ivec2(x, y) + 4096);\n" \
"			uint h = (q.x * 1597334677u) ^ (q.y * 3812015801u);\n" \
"			h *= 1597334677u;\n" \
"			vec2 r = vec2(x, y) + 0.1 + 0.8 * vec2(uvec2(h, h * 747796405u) >> 8) * (1.0 / 16777216.0) - f;\n" \
"			float d = dot(r, r);\n" \
"			if (d < d1) { d2 = d1; v2 = v1; d1 = d; v1 = r; tone = float(h >> 24) / 255.0; }\n" \
"			else if (d < d2) { d2 = d; v2 = r; }\n" \
"		}\n" \
"	d1 = sqrt(d1); d2 = sqrt(d2);\n" \
"	return vec4(d2 - d1, v1 / max(d1, 1e-4) - v2 / max(d2, 1e-4), tone);\n" \
"}\n" \
"\n" \
"// QVR: a glove's leather where it covers (vr_gloves): its colour times its own relief, none of the skin's painted\n" \
"// detail (nails, creases): the skin mask's A (make_skin_masks.py: its seams and stitches, 0.5 flat; read at ax/ay a\n" \
"// texel along u/v for their slope), the hem's rolled edge (hem 0..1 over the edge band, dhem its slope a texel), and\n" \
"// a pebbled grain of its own, faded out where a pebble would be under ~2 pixels. Out: the colour's shade, the relief's\n" \
"// slope (in uv units, for WoundBump), its sheen's share. painted: a painted layer gives the grain (vr_glove_paint):\n" \
"// this one's finer and fainter, a texture under it.\n" \
"float GloveLeather(vec2 uv, vec2 duvdx, vec2 duvdy, vec2 ts, float a, float ax, float ay, float hem, vec2 dhem, bool painted, out vec2 slope, out float gloss)\n" \
"{\n" \
"	float cells = painted ? 1.1 : 0.55; // grain cells a texel: the hand's 1.5 texels a mm, a pebble ~1.2 mm\n" \
"	float grainK = painted ? 0.35 : 1.0;\n" \
"	float px = cells * max(length(duvdx * ts), length(duvdy * ts)); // cells a pixel\n" \
"	float fine = 1.0 - smoothstep(0.25, 0.6, px);\n" \
"	vec2 s = 1.6 * vec2(ax - a, ay - a); // the seams (A 0.5 flat; 1.6 texels its full range)\n" \
"	float roll = 0.6 * sin(3.14159 * hem);\n" \
"	s += 0.6 * 3.14159 * cos(3.14159 * hem) * dhem; // the hem rolled over\n" \
"	float pebble = 1.0, tone = 0.5;\n" \
"	if (fine > 0.0)\n" \
"	{\n" \
"		vec4 gr = LeatherGrain(uv * ts * cells);\n" \
"		float e = clamp(gr.x / 0.2, 0.0, 1.0);\n" \
"		pebble = e * (2.0 - e); // 0 in a crevice, 1 on a pebble's top\n" \
"		s += grainK * fine * 0.14 * 2.0 * (1.0 - e) * step(gr.x, 0.2) / 0.2 * gr.yz * cells;\n" \
"		pebble = mix(0.85, pebble, fine * grainK);\n" \
"		tone = mix(0.5, gr.w, fine * grainK);\n" \
"	}\n" \
"	slope = s * vec2(1.0, ts.y / ts.x);\n" \
"	float seam = clamp(1.0 + 1.8 * (a - 0.5), 0.3, 1.5); // grooves dark, stitches light\n" \
"	gloss = clamp(seam, 0.0, 1.0) * mix(0.35, 1.0, pebble) * (0.5 + 0.5 * hem);\n" \
"	return seam * (0.88 + 0.14 * pebble) * (0.96 + 0.08 * tone) * (1.0 - 0.3 * (1.0 - hem)) * (1.0 + 0.15 * roll);\n" \
"}\n" \
"\n" \
"// QVR: a glove's leather's sheen, soft and broad, in the model's own light (k: GloveLeather's gloss times its cover)\n" \
"vec3 GloveSheen(vec3 n, float k)\n" \
"{\n" \
"	if (k <= 0.0 || dot(in_bumplight.xyz, in_bumplight.xyz) <= 0.0)\n" \
"		return vec3(0.0);\n" \
"	vec3 v = normalize(-in_pos);\n" \
"	vec3 l = normalize(in_bumplight.xyz);\n" \
"	float s = pow(max(dot(n, normalize(l + v)), 0.0), 18.0);\n" \
"	float f = pow(1.0 - clamp(dot(n, v), 0.0, 1.0), 4.0);\n" \
"	return min(in_color.rgb, vec3(1.0)) * k * (s * 0.12 + f * 0.05);\n" \
"}\n" \
"\n" \
"// QVR: painted nails' gloss (vr_nail_gloss g 0..1; k the polish's cover): a clear coat's highlight in the model's own\n" \
"// light, sharper and brighter the glossier, and the light round the model mirrored in it (Fresnel: most at grazing angles)\n" \
"vec3 NailGloss(vec3 n, float k, float g)\n" \
"{\n" \
"	if (k <= 0.0 || g <= 0.0)\n" \
"		return vec3(0.0);\n" \
"	vec3 v = normalize(-in_pos);\n" \
"	float fres = 0.04 + 0.96 * pow(1.0 - clamp(dot(n, v), 0.0, 1.0), 5.0);\n" \
"	vec3 lit = min(in_color.rgb, vec3(1.0));\n" \
"	vec3 add = lit * min(AmbientCube(reflect(-v, n)), vec3(1.5)) * (fres * g);\n" \
"	if (dot(in_bumplight.xyz, in_bumplight.xyz) > 0.0)\n" \
"	{\n" \
"		float p = mix(16.0, 160.0, g);\n" \
"		float s = pow(max(dot(n, normalize(normalize(in_bumplight.xyz) + v)), 0.0), p) * (p + 8.0) / 25.0;\n" \
"		add += lit * (s * 0.1 * (0.3 + g));\n" \
"	}\n" \
"	return add * k;\n" \
"}\n" \
"\n" \
"// QVR: a glove's painted relief (vr_glove_relief): its normal map (vr_render.cpp gloveHeightToNormals; green up, z kept:\n" \
"// a mip's averaged normal) on n, in the frame BumpedNormalK uses, but read smooth as its paint is (no retro blocks)\n" \
"vec3 GloveBump(vec2 uv, vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy, vec3 n, float strength)\n" \
"{\n" \
"	vec3 r1 = cross(dpdy, n), r2 = cross(n, dpdx);\n" \
"	vec3 t = r1 * duvdx.x + r2 * duvdy.x, b = r1 * duvdx.y + r2 * duvdy.y;\n" \
"	t *= inversesqrt(max(dot(t, t), 1e-24));\n" \
"	b *= inversesqrt(max(dot(b, b), 1e-24));\n" \
"	vec3 s = textureGrad(GloveHeightTex, uv, duvdx, duvdy).xyz * 2.0 - 1.0;\n" \
"	return normalize((t * s.x - b * s.y) * strength + n * max(s.z, 0.05));\n" \
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
"	vec3 mpos = in_morphpos * vec3(instances[in_instance].Ambient[3].w, instances[in_instance].Ambient[4].w, instances[in_instance].Ambient[5].w); // the model's units\n" \
"	float n = 0.6 * MorphNoise(mpos * 0.45) + 0.4 * MorphNoise(mpos * 1.3 + 7.3);\n" \
"	float d = n - cut;\n" \
"	vec3 hot = kind > 1.5 ? vec3(0.5, 0.8, 2.4) : kind > 0.5 ? vec3(2.2, 1.4, 0.35) : vec3(2.4, 0.8, 0.15);\n" \
"	seam = hot * 0.8 * (1.0 - smoothstep(0.0, 0.03, abs(d)));\n" \
"	return in_morph > 0.0 ? d < 0.0 : d >= 0.0;\n" \
"}\n" \
"\n" \
QVR_DENT_FUNCTIONS \
"// QVR: flesh dents at this pixel (vr/vr_dents.cpp), measured in the rest pose: the crater's raw flesh (cover), the\n" \
"// bloody rim round it, its depth and the depth's gradient (rest space), the nearest crater's distance in its radii.\n" \
"struct Dented\n" \
"{\n" \
"	float cover, rim, depth, t;\n" \
"	vec3 grad;\n" \
"};\n" \
"Dented DentsAt()\n" \
"{\n" \
"	Dented o = Dented(0.0, 0.0, 0.0, 1e9, vec3(0.0));\n" \
"	vec4 di = instances[in_instance].Dent;\n" \
"	float flesh = smoothstep(0.15, 0.85, in_restnor.w); // (none on a weapon, and where flesh meets it, faded)\n" \
"	if (di.y < 0.5 || flesh <= 0.0)\n" \
"		return o;\n" \
"	o.depth = flesh * DentDepth(in_restpos, normalize(in_restnor.xyz), uint(di.x), uint(di.y), 1.5, o.t, o.grad);\n" \
"	o.grad *= flesh;\n" \
"	float tear = WoundNoise(in_restpos.xy * 1.3 + in_restpos.z * 0.71) * 0.65 + WoundNoise(in_restpos.yz * 3.7 + in_restpos.x * 0.53) * 0.35;\n" \
"	float edge = o.t + (tear - 0.5) * 0.45; // torn, not round\n" \
"	o.cover = (1.0 - smoothstep(0.62, 0.7, edge)) * flesh;\n" \
"	o.rim = (1.0 - smoothstep(0.75, 1.35, edge)) * (flesh - o.cover);\n" \
"	return o;\n" \
"}\n" \
"// Raw flesh: muscle in two reds, a little fat and sinew, darker and bloodier towards the crater's core.\n" \
"vec3 DentFlesh(vec3 p, float t)\n" \
"{\n" \
"	float a = WoundNoise(p.xy * 2.3 + p.z * 0.73), b = WoundNoise(p.yz * 4.1 + p.x * 0.41 + 5.1);\n" \
"	vec3 meat = mix(vec3(0.24, 0.035, 0.03), vec3(0.42, 0.11, 0.09), a);\n" \
"	meat = mix(meat, vec3(0.55, 0.36, 0.27), smoothstep(0.8, 0.95, b) * 0.5);\n" \
"	meat = mix(meat, vec3(0.5, 0.24, 0.19), smoothstep(0.5, 0.66, t) * 0.7); // the torn skin's pale lip\n" \
"	return mix(meat, vec3(0.06, 0.004, 0.004), (1.0 - smoothstep(0.0, 0.55, t)) * 0.85); // the hole's dark, bloody depth\n" \
"}\n" \
"// The normal n bent by a dent's bowl: its height (minus the depth) on screen through the rest pose's derivatives, onto\n" \
"// the surface (Mikkelsen's surface gradient, as WoundBump).\n" \
"vec3 DentBump(vec3 n, vec3 grad, vec3 drdx, vec3 drdy, vec3 dpdx, vec3 dpdy)\n" \
"{\n" \
"	vec3 r1 = cross(dpdy, n), r2 = cross(n, dpdx);\n" \
"	float det = dot(dpdx, r1);\n" \
"	if (abs(det) < 1e-12)\n" \
"		return n;\n" \
"	float hx = -dot(grad, drdx), hy = -dot(grad, drdy);\n" \
"	return normalize(abs(det) * n - sign(det) * (r1 * hx + r2 * hy));\n" \
"}\n" \
"\n"

// a jointed hand on a bare forearm (vr/vr_view.cpp skinTint: SkinTint.w < 0): its end inside the arm (bind x below
// SkinTintRange.x) folded onto the wrist's middle line (SkinTintRange.yz), a cone within the arm however the wrist
// bends, never through its skin
#define QVR_ALIAS_VS_HANDFOLD \
"#if POSEVERTTYPE == 1\n" \
"	bind_pos = in_pos;\n" \
"	if (inst.SkinTint.w < 0.0 && in_pos.x < inst.SkinTintRange.x)\n" \
"		bind_pos.yz = inst.SkinTintRange.yz;\n" \
"#endif\n"

// bare forearms' ends tinted to the hand's skin (vr/vr_view.cpp skinTint): the arms' skin block's rows towards the wrist
// (SkinTintRange) times SkinTint's colour, more and more of it, all of it at the wrist where the hand comes out
#define QVR_ALIAS_FS_SKINTINT \
"	{\n" \
"		vec4 st = instances[in_instance].SkinTint, sr = instances[in_instance].SkinTintRange;\n" \
"		if (st.w > 0.0 && in_texcoord.x < sr.x && in_texcoord.y < sr.w)\n" \
"			result.rgb *= mix(vec3(1.0), st.rgb, st.w * smoothstep(sr.y, sr.z, in_texcoord.y));\n" \
"	}\n"

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
"	vec4 tone = instances[in_instance].SkinTone;\n" \
"	vec4 glove = instances[in_instance].Glove;\n" \
"	vec4 nails = instances[in_instance].Nails; // QVR: painted nails (vr_nails): the polish's colour, w 1 + its gloss (0 none)\n" \
"	vec2 leatherSlope = vec2(0.0); // QVR: a glove's leather (GloveLeather): its relief, its cover, its sheen\n" \
"	float leatherCover = 0.0, leatherGloss = 0.0;\n" \
"	float nailCover = 0.0; // QVR: the polish's cover (for its smooth coat and gloss: NailGloss)\n" \
"	if (tone.w > 0.0 || glove.w > 0.0 || nails.w > 0.0) // QVR: skin only (vr_skin_tone): the skin's share of the look toned, painted blood/char keep their colour,\n" \
"	{ // their soft edges blending in (no untoned ring); gloves (vr_gloves) over it, the skin toned where they leave it bare\n" \
"		float skinShare = (1.0 - wounds.burn) * (1.0 - wounds.blood * instances[in_instance].WoundSide.w);\n" \
"		vec4 mask = textureGrad(SkinMaskTex, uv, duvdx, duvdy);\n" \
"		float gmode = mod(glove.w, 4.0), gpaint = floor(glove.w * 0.25); // Glove.w: the channel, 4 times the painted layer's (vr_render.cpp glovePaintFlags)\n" \
"		float gm = gmode > 1.5 ? mask.b : gmode > 0.5 ? mask.g : 0.0; // make_skin_masks.py: 0.55 at the edge, 1 a hem inside\n" \
"		float cover = smoothstep(0.3, 0.5, gm), hem = clamp((gm - 0.55) / 0.45, 0.0, 1.0);\n" \
"		vec2 ts = vec2(textureSize(SkinMaskTex, 0));\n" \
"		vec4 mx = vec4(0.5), my = vec4(0.5);\n" \
"		vec2 dhem = vec2(0.0);\n" \
"		if (glove.w > 0.0 && gm > 0.3)\n" \
"		{\n" \
"			mx = textureGrad(SkinMaskTex, uv + vec2(1.0 / ts.x, 0.0), duvdx, duvdy);\n" \
"			my = textureGrad(SkinMaskTex, uv + vec2(0.0, 1.0 / ts.y), duvdx, duvdy);\n" \
"			dhem = (gmode > 1.5 ? vec2(mx.b, my.b) : vec2(mx.g, my.g)) - gm;\n" \
"			dhem = hem > 0.0 && hem < 1.0 ? dhem / 0.45 : vec2(0.0);\n" \
"		}\n" \
"		vec4 cuff = instances[in_instance].GloveCuff;\n" \
"		if (glove.w > 0.0 && cuff.x > 0.0 && uv.x < cuff.x && uv.y < cuff.w) // a bare forearm's cuff, by the skin's rows\n" \
"		{\n" \
"			float fw = max(abs(duvdx.y), abs(duvdy.y)) + 1e-6;\n" \
"			cover = smoothstep(-fw, fw, uv.y - cuff.y);\n" \
"			hem = clamp((uv.y - cuff.y) / (cuff.z - cuff.y), 0.0, 1.0);\n" \
"			dhem = vec2(0.0, hem > 0.0 && hem < 1.0 ? 1.0 / ((cuff.z - cuff.y) * ts.y) : 0.0);\n" \
"			mask.a = mx.a = my.a = 0.5; // plain leather\n" \
"			gpaint = 0.0; // (the hand's painted layer only)\n" \
"		}\n" \
"		if (gloveShell && mask.r > 0.5) // a glove's shell (vr_handrig.cpp gloveShells): leather all of it (its cut and its wall\n" \
"			cover = 1.0; // just off the mask's edge too: no skin-coloured lip), but where the skin's own blood shows through\n" \
"		cover *= step(0.5, glove.w);\n" \
"		vec3 toned = tone.w > 0.0 ? result.rgb * mix(vec3(1.0), tone.rgb, mask.r * (1.0 - cover)) : result.rgb;\n" \
"		if (!gloveShell && instances[in_instance].GloveShell.x > 0.0) // the skin by a glove's shell's edge in its shadow\n" \
"		{ // (~2.5 mm: the mask's coarser mip; a cuff's by its row)\n" \
"			vec2 gl = textureLod(SkinMaskTex, uv, 2.0).gb;\n" \
"			float near = cuff.x > 0.0 && uv.x < cuff.x && uv.y < cuff.w ? 1.0 - clamp((cuff.y - uv.y) / 0.0015, 0.0, 1.0) :\n" \
"				smoothstep(0.0, 0.3, gmode > 1.5 ? gl.y : gl.x);\n" \
"			toned *= 1.0 - 0.45 * near * (1.0 - cover);\n" \
"		}\n" \
"		if (nails.w > 0.0 && !gloveShell) // QVR: painted nails (vr_nails; make_nail_mask.py: the plates, a soft edge): the polish's\n" \
"		{ // colour, not toned (the skin round it is), the nail's own painted shading kept a little (0.44: its plate's mean); a glove over it hides it\n" \
"			float nm = textureGrad(NailTex, uv, duvdx, duvdy).r * (1.0 - cover);\n" \
"			if (nm > 0.0)\n" \
"			{\n" \
"				float shade = mix(1.0, clamp(dot(result.rgb, vec3(0.299, 0.587, 0.114)) / 0.44, 0.6, 1.4), 0.5);\n" \
"				toned = mix(toned, nails.rgb * shade, nm);\n" \
"				nailCover = nm * skinShare;\n" \
"			}\n" \
"		}\n" \
"		vec3 leather = glove.rgb;\n" \
"		if (cover > 0.0) // its own relief, none of the skin's painted detail (GloveLeather)\n" \
"		{\n" \
"			vec3 paint = vec3(0.5);\n" \
"			float wear = 0.0;\n" \
"			if (gpaint > 0.5) // QVR: its painted leather (vr_glove_paint; make_glove_paint.py): RGB its detail times the colour\n" \
"			{ // (0.5 as it is), A its wear; its relief (the normal map below) in place of the mask's seams (the painted ones over them)\n" \
"				vec4 gp = textureGrad(GloveTex, uv, duvdx, duvdy);\n" \
"				paint = gp.rgb;\n" \
"				wear = max(gp.a, 0.4 * (1.0 - hem)); // (the hem's edge rubbed too)\n" \
"				mask.a = mx.a = my.a = 0.5;\n" \
"			}\n" \
"			// worn: paler, greyer, matte (a black glove's scuffs grey, a brown one's tan)\n" \
"			float lum = dot(glove.rgb, vec3(0.299, 0.587, 0.114));\n" \
"			vec3 worn = mix(vec3(lum), glove.rgb, 0.45) * 1.6 + vec3(0.075, 0.068, 0.06);\n" \
"			leather = mix(glove.rgb, worn, wear) * paint * 2.0;\n" \
"			leather *= GloveLeather(uv, duvdx, duvdy, ts, mask.a, mx.a, my.a, hem, dhem, gpaint > 0.5, leatherSlope, leatherGloss);\n" \
"			leatherGloss *= 1.0 - 0.7 * wear;\n" \
"			leatherCover = cover * skinShare;\n" \
"		}\n" \
"		wounds.look += skinShare * (mix(toned, leather, cover) - result.rgb);\n" \
"	}\n" \
"	result.rgb = wounds.look;\n" \
"	if (in_stump.w < 1.1) // QVR: a cut-off limb's stump (StumpFlesh): the cone closing it and round it, its edge ragged\n" \
"	{\n" \
"		vec3 sc = in_stump.xyz * 2.5;\n" \
"		float cover = step(in_stump.w, 0.75 + 0.35 * StumpRand(ivec3(floor(sc)) + ivec3(9, 4, 2)));\n" \
"		result.rgb = mix(result.rgb, StumpFlesh(sc, in_stump.w), cover);\n" \
"		wounds.gloss = mix(wounds.gloss, 0.25, cover); // (wet, not glazed)\n" \
"		wounds.flatten = max(wounds.flatten, cover);\n" \
"	}\n" \
"	if (wounds.wet > 0.0 && instances[in_instance].GloveShell.w < 1.5) // QVR: the wet coat (vr/vr_wounds.cpp wetCoat): all of the look a little darker,\n" \
"	{ // evenly (not a fullbright texel's: unlit); w 2 (a brain chunk's own wetness: VR_AliasWetCoat) only shines\n" \
"		float wetDark = 0.3 * min(wounds.wet, 2.0);\n" \
"#if !ALPHATEST\n" \
"		wetDark *= result.a;\n" \
"#endif\n" \
"		result.rgb *= 1.0 - wetDark;\n" \
"		wounds.flatten = max(wounds.flatten, 0.35 * min(wounds.wet, 1.0)); // the film fills the bumps a little\n" \
"	}\n" \
"	Dented dent = DentsAt(); // QVR: flesh dents (vr/vr_dents.cpp): raw flesh in the crater, a bloody rim round it\n" \
"	if (dent.cover + dent.rim > 0.0)\n" \
"	{\n" \
"		result.rgb = mix(result.rgb, vec3(0.3, 0.02, 0.02) * (0.7 + 0.6 * dot(result.rgb, vec3(0.3, 0.59, 0.11))), dent.rim * 0.85);\n" \
"		result.rgb = mix(result.rgb, DentFlesh(in_restpos, dent.t), dent.cover);\n" \
"		wounds.blood = max(wounds.blood, max(dent.cover, dent.rim));\n" \
"		wounds.gloss = max(wounds.gloss, 0.6 * dent.cover + 0.45 * dent.rim);\n" \
"#if !ALPHATEST\n" \
"		result.a = mix(result.a, 1.0, max(dent.cover, dent.rim)); // (not a fullbright texel's: lit)\n" \
"#endif\n" \
"	}\n" \
"#if ALPHATEST\n" \
"	vec3 emissive = vec3(0.); // QVR\n" \
"#else\n" \
"	vec3 emissive = result.rgb * (1.0 - result.a); // QVR: fullbright texels, unlit (ALPHABRIGHT skins keep them in the alpha)\n" \
"#endif\n" \
"	// QVR: the normal bent by the skin's normal map (vr_normalmaps), for the model's own light and the dynamic lights\n" \
"	vec3 n = normalize(in_nor);\n" \
"	float bumpk = instances[in_instance].AO.z; // QVR: its normal map's strength: a made one's LightTweak.w, an authored one's own (vr_normalmap_authored)\n" \
"	vec3 bumped = bumpk > 0. && (in_bumplight.w > 0. || NumLights > 0u || instances[in_instance].Ambient[0].w > 0.) ? BumpedNormalK(NormalTex, uv, duvdx, duvdy, dpdx, dpdy, n, bumpk) : n;\n" \
"	if (leatherCover > 0.) // QVR: a glove's leather: the skin's bumps (creases, nails) under it gone, its own relief instead:\n" \
"	{ // its painted relief's normal map (vr_glove_relief; vr_render.cpp gloveHeightToNormals: one tap, mipmapped) and its grain\n" \
"		float relief = instances[in_instance].GloveShell.y;\n" \
"		vec3 lb = relief > 0.0 ? GloveBump(uv, duvdx, duvdy, dpdx, dpdy, n, relief) : n;\n" \
"		bumped = WoundBump(normalize(mix(bumped, lb, leatherCover)), leatherSlope * leatherCover, duvdx, duvdy, dpdx, dpdy);\n" \
"	}\n" \
"	if (gloveShell) // QVR: a glove's shell's wall (vr_handrig.cpp gloveShells; its vertices' normals the skin's): lit as\n" \
"	{ // the side it is (the facet's normal), darker (the leather's cut edge)\n" \
"		vec3 ng = normalize(cross(dpdx, dpdy));\n" \
"		ng = dot(ng, in_pos) > 0.0 ? -ng : ng;\n" \
"		float wall = smoothstep(0.55, 0.8, 1.0 - abs(dot(ng, n)));\n" \
"		bumped = normalize(mix(bumped, ng, wall));\n" \
"		result.rgb *= 1.0 - 0.3 * wall;\n" \
"	}\n" \
"	if (nailCover > 0.0) // QVR: painted nails: the polish a smooth coat over the nail's bumps\n" \
"		bumped = normalize(mix(bumped, n, 0.8 * nailCover));\n" \
"	if (wounds.flatten > 0.) // QVR: blood and water fill the bumps\n" \
"		bumped = normalize(mix(bumped, n, wounds.flatten));\n" \
"	if (wounds.slope != vec2(0.)) // QVR: your own wounds' relief (the fine masks): char crusted, blood sunk in\n" \
"		bumped = WoundBump(bumped, wounds.slope, duvdx, duvdy, dpdx, dpdy);\n" \
"	if (dent.depth > 0.0) // QVR: a flesh dent's bowl in the light (the mesh's own push is coarse: Quake's meshes are)\n" \
"		bumped = DentBump(normalize(mix(bumped, n, dent.cover)), dent.grad, drdx, drdy, dpdx, dpdy);\n" \
"#if ALPHATEST\n" \
"	SpecularAA(0.0); // QVR: the normal map's spread only (no derivatives after the discard)\n" \
"#else\n" \
"	SpecularAA(Parallax.w > 0. ? NormalSpread(bumped) : 0.0); // QVR: the sheen's lobe widened by the bumps and curves under the pixel (vr_specular_aa)\n" \
"#endif\n" \
"	vec3 skin = result.rgb; // QVR: for the rim light and reflections\n" \
"	vec3 spec; // QVR\n" \
"	// QVR: ambient occlusion (vr/vr_ao.cpp): the model's own and that of what moves near it darken its own light;\n" \
"	// dynamic lights get half of its own (in the log: its square root), shadows stand for the rest\n" \
"	// (in_vao clamped: under foveated rendering's coarse shading a fragment's inputs are interpolated at its block's\n" \
"	// centre, which can be past the triangle's edge, where in_vao runs on below 0: sqrt gave NaN, drawn white)\n" \
"	float vao = clamp(in_vao, 0.0, 1.0);\n" \
"	float occlusion = vao * DynamicAO(in_pos + EyePos, AONormal(n, in_bumplight.xyz), instances[in_instance].AO.x, in_coord, in_depth);\n" \
"	vec3 own_light = in_color.rgb * ModelBumpShade(n, bumped) * ModelAmbient(n, bumped) * occlusion; // QVR\n" \
"	if (dent.depth > 0.0) // QVR: a dent's bowl shaded by the model's light whatever the bumps' share of it: its far wall lit, its near one dark\n" \
"		own_light *= max(1.0 + 1.2 * dot(bumped - n * dot(bumped, n), in_bumplight.xyz * inversesqrt(max(dot(in_bumplight.xyz, in_bumplight.xyz), 1e-6))), 0.3);\n" \
"	vec3 dyn_light = ModelDynamicLights(n, bumped, spec) * sqrt(vao); // QVR\n" \
"	if (RetroLight[5].z > 0.) // QVR: retro lighting's levels (vr_retrolight.h; no derivatives after the discard)\n" \
"	{\n" \
"#if ALPHATEST\n" \
"		RetroLightModel(own_light, dyn_light, false);\n" \
"#else\n" \
"		RetroLightModel(own_light, dyn_light, true);\n" \
"#endif\n" \
"	}\n" \
"	vec3 light = own_light + dyn_light; // QVR\n"

// fullbrights, wounds' glow, force grab's glow, morphs
#define QVR_ALIAS_FS_GLOW \
"	// QVR: the held weapons' dim fullbrights (sights) shine brighter, bright ones stay (vr/vr_emissive.cpp)\n" \
"	fullbright *= 1.0 - max(wounds.blood, wounds.burn); // QVR: covered by a wound\n" \
"	vec3 glowing = fullbright + emissive;\n" \
"	result.rgb += fullbright + glowing * (in_fbboost * (1.0 - max(glowing.r, max(glowing.g, glowing.b))));\n" \
"	result.rgb += wounds.glow; // QVR: a fresh burn's embers\n" \
"	if (in_glow > 3.5) // QVR: an incandescent blade (vr/vr_meleehud.cpp hotGlow): 4 + 2 x its colour (16 levels a channel: r + 16g + 256b) + its strength (0..2); the metal glows from within, its bright parts most and yellower\n" \
"	{\n" \
"		vec4 hb = instances[in_instance].HotBlade; // QVR: only the blade glows: a plane across it in the pose's space (vr/vr_hotblade.cpp bladePlane; zero: all of it)\n" \
"		float hmask = hb == vec4(0.0) ? 1.0 : clamp(dot(in_morphpos, hb.xyz) + hb.w, 0.0, 1.0);\n" \
"		float hv = in_glow - 4.0;\n" \
"		float hcode = floor(hv * 0.5);\n" \
"		float hs = (hv - 2.0 * hcode) * hmask * hmask * (3.0 - 2.0 * hmask);\n" \
"		vec3 hcolor = vec3(mod(hcode, 16.0), mod(floor(hcode / 16.0), 16.0), floor(hcode / 256.0)) / 15.0;\n" \
"		float hlum = dot(skin, vec3(0.3, 0.59, 0.11));\n" \
"		float hrim = 1.0 - abs(dot(normalize(in_nor), normalize(-in_pos)));\n" \
"		hcolor = mix(hcolor, vec3(1.0, 0.85, 0.55), clamp(hlum * hs - 0.2, 0.0, 0.45));\n" \
"		result.rgb = result.rgb * (1.0 - 0.3 * min(hs, 1.0)) + hcolor * hs * (0.1 + 0.9 * hlum + 0.45 * hrim * hrim);\n" \
"	}\n" \
"	else if (in_glow < -1.5) // QVR: a power-up's glow on your hands and body (vr/vr_view.cpp powerupGlow: Quad's blue, the pentagram's red, the biosuit's green): -2 - 2 x its colour (16 levels a channel: r + 16g + 256b) - its strength (0..1); a soft rim over a faint wash, none right at the eyes\n" \
"	{\n" \
"		float qv = -in_glow - 2.0;\n" \
"		float qcode = floor(qv * 0.5);\n" \
"		float qs = qv - 2.0 * qcode;\n" \
"		vec3 qcolor = vec3(mod(qcode, 16.0), mod(floor(qcode / 16.0), 16.0), floor(qcode / 256.0)) / 15.0;\n" \
"		float qrim = 1.0 - abs(dot(normalize(in_nor), normalize(-in_pos)));\n" \
"		float qnear = smoothstep(6.0, 16.0, length(in_pos));\n" \
"		result.rgb += qcolor * (qs * qnear * (pow(qrim, 2.5) * 1.4 + 0.08));\n" \
"	}\n" \
"	else if (in_glow != 0.) // QVR: force grab's glow round the edges (vr/vr_fgfx.cpp); negative, the counter's (vr/vr_meleehud.cpp)\n" \
"	{\n" \
"		float rim = 1.0 - abs(dot(normalize(in_nor), normalize(-in_pos)));\n" \
"		vec3 glowColor = in_glow > 1.5 ? vec3(1.0, 0.12, 0.06) : in_glow > 0. ? SceneTone.yzw : vec3(1.0, 0.66, 0.2); // QVR: the player's hue; the counter's gold; the toolgun remover's red (2..3: vr/vr_toolgun.cpp)\n" \
"		float glowStrength = in_glow > 1.5 ? in_glow - 2.0 : abs(in_glow);\n" \
"		result.rgb += glowColor * glowStrength * (pow(rim, 2.0) * 1.1 + (in_glow > 0. ? 0.08 : 0.12));\n" \
"	}\n" \
"	result.rgb += morphSeam; // QVR: a morph's glowing seam\n" \
"	if (!morphShown) // QVR: this model is not there yet (or any more) in a morph\n" \
"		discard;\n" \
"	if (in_color.a < 1.0 && BehindShownGate(in_pos + EyePos)) // QVR: see-through, behind a teleporter shown in this view\n" \
"		discard;\n"
