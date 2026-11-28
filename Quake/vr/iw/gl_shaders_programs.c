// QVR: the decal retro programs and the lazy program getters (world, alias forms, water, particles).
// Moved as it was out of gl_shaders.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
=============
GL_WorldDecalRetroProgram

QVR: the solid world's program with the decals' retro textures' code (vr_retro: the decals have a retro set), made the
first time it is needed. The others go without it (NO_DECAL_RETRO): its mere presence slowed the decals' loop by a
quarter (PERF_DECISIONS.md 15), and making these at the start would add to every start's shaders.
=============
*/
GLuint GL_WorldDecalRetroProgram (int oit, int dither, qboolean pdo)
{
	GLuint *program = pdo ? &glprogs.world_pdo_decal_retro[dither] : &glprogs.world_decal_retro[oit][dither];
	if (!*program)
	{
		if (pdo)
			*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world pdo decal retro|OIT 0; DITHER %d; MODE %d; PDO 1", dither, WORLDSHADER_SOLID);
		else
			*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world decal retro|OIT %d; DITHER %d; MODE %d%s", oit, dither, WORLDSHADER_SOLID, GL_WorldLiteDefines ());
	}
	return *program;
}

/*
=============
GL_WorldUnlitDecalRetroProgram -- QVR

qworld (the shader review's finding 1): the unlit world's solid program (glprogs.world_unlit: no lights' loop, the
dynamic lights from vr_gl_world_dlmap's texture) with the decals' retro textures' code. Without it the first retro
blood mark sent the whole solid world to GL_WorldDecalRetroProgram's lit program, the lights' loop compiled in and run
per pixel (the form the unlit world exists to avoid: QUEST_PLAN.md 6z), in every fight. Made with the decals' others
(R_PrewarmDecalPrograms); 0 where the unlit forms are not made (the PC).
=============
*/
GLuint GL_WorldUnlitDecalRetroProgram (int dither)
{
	GLuint *program = &glprogs.world_unlit_decal_retro[dither];
	if (!*program && GL_WorldUnlitForms ())
		*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world unlit decal retro|OIT 0; DITHER %d; MODE %d; QVR_NOLIGHTS 1%s", dither, WORLDSHADER_SOLID, GL_WorldLiteDefines ());
	return *program;
}

/*
=============
GL_ShadersLazyStart, GL_WorldProgram, GL_AliasProgram, GL_AliasFormProgram, GL_WaterProgram, GL_ParticlesProgram -- QVR

The ES profile (the Quest) makes these groups' variants when they are first asked for, not all at the start: of its
~148 programs the Quest preset never uses about half (OIT's, the other dithers and model modes; 0.35-0.5 s each to
compile on Adreno, tens of seconds of a cold start). Its start set is the defaults' (no OIT, no dither, the standard
model mode); R_PrewarmSelectedPrograms makes the current settings' at each map's load; a setting changed in play
makes its variant at its first draw (once: the program cache keeps it). The PC makes them all at the start, as before
(gl_shaders_lazy 0: these only return what GL_CreateShadersAll made). Each is named and defined as it always was.
=============
*/
qboolean GL_ShadersLazyStart (int oit, int dither, int mode)
{
	return !gl_shaders_lazy || (oit == 0 && dither == 0 && mode == 0);
}

GLuint GL_WorldProgram (int oit, int dither, int mode)
{
	GLuint *program = &glprogs.world[oit][dither][mode];
	if (!*program)
		*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world|OIT %d; DITHER %d; MODE %d%s%s", oit, dither, mode,
			mode == WORLDSHADER_SOLID ? "; NO_DECAL_RETRO 1" : "", GL_WorldLiteDefines ()); // the decals' retro textures' code: GL_WorldDecalRetroProgram's; the ES lite form
	return *program;
}

GLuint GL_AliasProgram (int oit, int mode, int alphatest, int poseverttype)
{
	GLuint *program = &glprogs.alias[oit][mode][alphatest][poseverttype];
	if (!*program)
		*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias|OIT %d; MODE %d; ALPHATEST %d; POSEVERTTYPE %d%s",
			oit, mode, alphatest, poseverttype, GL_AliasLiteDefines ()); // the ES lite form
	return *program;
}

// The opaque models' forms are the ES profile's: without discard (vr_gl_alias_opaque: a tiler's early depth rejection;
// QUEST_PLAN.md 6p), and with the lite world's model bits, without the per-model features a batch leaves off (plain,
// vr_gl_alias_plain, 6r), keeping the wounds and dents (gore, vr_gl_alias_gore, 6zd) or the skin tone too (skin,
// vr_gl_alias_skin, 6ze).
qboolean GL_AliasFormAvailable (int form)
{
	if (!VR_GLESForm (VR_GLES_FORM_GLSL)) // pcopt: the PC's own GLSL with vr_gl_pc_forms 1: plain, gore and skin (opaque: their base, r_alias.c)
		return VR_GLLiteForms (VR_GL_PCFORM_MODELS) && ((gl_world_lite >> 9) & 1023) != 0;
	return form == ALIASFORM_OPAQUE || (gl_world_lite >> 9) != 0;
}

GLuint GL_AliasFormProgram (int form, int mode, int poseverttype)
{
	return GL_AliasFormProgramV (form, 0, mode, poseverttype);
}

GLuint GL_AliasFormProgramV (int form, int variant, int mode, int poseverttype)
{
	GLuint *program;
	if (!GL_AliasFormAvailable (form))
		return 0;
	variant = form == ALIASFORM_PLAIN || form == ALIASFORM_GORE || form == ALIASFORM_SKIN ? variant & GL_AliasFormVariants () : 0;
	switch (form)
	{
	case ALIASFORM_OPAQUE:
		program = &glprogs.alias_opaque[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias opaque|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s", mode, poseverttype, GL_AliasLiteDefines ());
		return *program;
	case ALIASFORM_PLAIN:
		program = &glprogs.alias_plain[variant][mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias plain|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefinesV (0, variant));
		return *program;
	case ALIASFORM_GORE:
		program = &glprogs.alias_gore[variant][mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias gore|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefinesV (VR_ALIAS_GORE_NEEDS, variant));
		return *program;
	case ALIASFORM_SKIN:
		program = &glprogs.alias_skin[variant][mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias skin|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefinesV (VR_ALIAS_SKIN_NEEDS, variant));
		return *program;
	case ALIASFORM_BODY: // (C5e: the skin form lit at its vertices, whatever vr_gl_alias_vlight says: VR_AliasBodyLight;
		// vr_gl_alias_body 1 only since C5h: 2 draws your body by its usual form, the loop skipped: r_alias.c BatchDlightsFlat)
		program = &glprogs.alias_body[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias body|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefines (VR_ALIAS_SKIN_NEEDS),
				(gl_world_lite & (1 << 28)) ? "" : "; QVR_APLAIN_VLIGHT 1");
		return *program;
	}
	return 0;
}

GLuint GL_WaterProgram (int oit, int dither)
{
	GLuint *program = &glprogs.water[oit][dither];
	if (!*program)
		*program = GL_CreateProgram (water_vertex_shader, water_fragment_shader, "water|OIT %d; DITHER %d", oit, dither);
	return *program;
}

GLuint GL_ParticlesProgram (int oit, int dither)
{
	GLuint *program = &glprogs.particles[oit][dither];
	if (!*program)
		*program = GL_CreateProgram (particles_vertex_shader, particles_fragment_shader, "particles|OIT %d; DITHER %d", oit, dither);
	return *program;
}

