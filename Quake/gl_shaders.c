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

#include "quakedef.h"
#include "gl_shaders.h"
#include "q_ctype.h"

glprogs_t glprogs;
static GLuint gl_programs[256]; // QVR: 256 (the multiview twins)
static GLuint gl_current_program;
static int gl_num_programs;
static int gl_shader_ab; // QVR: tests: QVR_SHADER_AB in every shader (vr_shader_reload)

// QVR: single-pass stereo (vr/vr_stereo_single.cpp, docs/vr-port/STEREO.md): each graphics program's sources, for its
// multiview twin (the same sources compiled with QVR_MULTIVIEW), made when single-pass stereo turns on
// (GL_MultiviewPrepare).
typedef struct {
	GLuint		program;
	GLuint		twin;		// 0: not made (yet)
	qboolean	tried;		// made, or failed to
	qboolean	ported;		// a ported pass's program (its name)
	qboolean	pending;	// its twin compiling in the background (GL_MultiviewPending)
	int			missing;	// used in the multiview pass, its twin failed (GL_MultiviewStats)
	const char	*sources[2];
	char		macros[1024];
	char		name[128];
} glprogsource_t;
static glprogsource_t gl_progsources[160];
static int gl_num_progsources;
static int gl_multiview_compile; // GL_CreateShader: the twin's header (1); a failure is reported, not fatal
static int gl_multiview_missing; // programs used in the multiview pass without a twin (VR_StereoSingleInfo)
static qboolean gl_multiview_unprepared; // a ported program made since GL_MultiviewPrepare (its twin not yet)
#ifndef GL_COMPLETION_STATUS_KHR
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif
static qboolean gl_multiview_async; // GL_CreateShader, GL_CreateProgramFromShaders: a twin not waited for
static int gl_multiview_pending; // twins compiling in the background (GL_MultiviewPending)
qboolean gl_multiview_draw; // GL_UseProgram: the multiview pass draws (twins)

/*
=============
GL_InitError
=============
*/
static void GL_InitError (const char *message, ...)
{
	const char *fmt;
	char buf[4096];
	size_t len;
	va_list argptr;

	va_start (argptr, message);
	q_vsnprintf (buf, sizeof (buf), message, argptr);
	va_end (argptr);

	len = strlen (buf);
	while (len && q_isspace (buf[len - 1]))
		buf[--len] = '\0';

	fmt = 
		"Your system appears to meet the minimum requirements,\n"
		"however an error was encountered during OpenGL initialization.\n"
		"This could be caused by a driver or an engine bug.\n"
		"Please report this issue, including the following details:\n"
		"\n"
		"%s\n"
		"\n"
		"Engine:	Ironwail " IRONWAIL_VER_STRING " (%d-bit)\n"
		"OpenGL:	%s\n"
		"GPU:   	%s\n"
		"Vendor:	%s\n"
#if defined(_WIN32)
		"\n"
		"(Note: you can press Ctrl+C to copy this text to clipboard)"
#endif
	;

	Sys_Error (
		fmt,
		buf,
		(int) sizeof (void *) * 8,
		gl_version,
		gl_renderer,
		gl_vendor
	);
}

/*
=============
AppendString
=============
*/
static qboolean AppendString (char **dst, const char *dstend, const char *str, int len)
{
	int avail = dstend - *dst;
	if (len < 0)
		len = Q_strlen (str);
	if (len + 1 > avail)
		return false;
	memcpy (*dst, str, len);
	(*dst)[len] = 0;
	*dst += len;
	return true;
}

/*
=============
GL_CreateShader
=============
*/
static GLuint GL_CreateShader (GLenum type, const char *source, const char *extradefs, const char *name)
{
	const char *strings[16];
	const char *typestr = NULL;
	char header[256];
	int numstrings = 0;
	GLint status;
	GLuint shader;

	switch (type)
	{
		case GL_VERTEX_SHADER:
			typestr = "vertex";
			break;
		case GL_FRAGMENT_SHADER:
			typestr = "fragment";
			break;
		case GL_COMPUTE_SHADER:
			typestr = "compute";
			break;
		default:
			Sys_Error ("GL_CreateShader: unknown type 0x%X for %s", type, name);
			break;
	}

	q_snprintf (header, sizeof (header),
		"#version 430\n"
		"\n"
		"#define BINDLESS %d\n"
		"#define REVERSED_Z %d\n"
		"#define QVR_SHADER_AB %d\n", // QVR: tests (vr_shader_reload)
		gl_bindless_able,
		gl_clipcontrol_able,
		gl_shader_ab
	);
	strings[numstrings++] = header;
	if (gl_multiview_compile) // QVR: a multiview twin (single-pass stereo): both eyes from each draw
		strings[numstrings++] = type == GL_VERTEX_SHADER ?
			"#extension GL_OVR_multiview2 : require\nlayout(num_views = 2) in;\n#define QVR_MULTIVIEW 1\n#define QVR_VIEW int(gl_ViewID_OVR)\n" :
			"#extension GL_OVR_multiview2 : require\n#define QVR_MULTIVIEW 1\n#define QVR_VIEW int(gl_ViewID_OVR)\n";

	if (extradefs && *extradefs)
		strings[numstrings++] = extradefs;
	strings[numstrings++] = source;

	VR_GLStep ("compile %s %s shader", name, typestr); // QVR: breadcrumbs (vr/vr_glsafe.cpp)
	shader = GL_CreateShaderFunc (type);
	GL_ObjectLabelFunc (GL_SHADER, shader, -1, name);
	GL_ShaderSourceFunc (shader, numstrings, strings, NULL);
	GL_CompileShaderFunc (shader);
	if (gl_multiview_async) // QVR: a twin made in the background: not waited for (GL_MultiviewPending)
		return shader;
	GL_GetShaderivFunc (shader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetShaderInfoLogFunc (shader, sizeof(infolog), NULL, infolog);
		if (gl_multiview_compile) // QVR: a twin that fails leaves its pass per eye
		{
			Con_Printf ("multiview twin of %s: %s shader:\n%s\n", name, typestr, infolog);
			gl_multiview_compile = 2;
			return shader;
		}
		GL_InitError ("Error compiling %s %s shader:\n\n%s", name, typestr, infolog);
	}

	return shader;
}

/*
=============
GL_CreateProgramFromShaders
=============
*/
static GLuint GL_CreateProgramFromShaders (const GLuint *shaders, int numshaders, const char *name)
{
	GLuint program;
	GLint status;

	program = GL_CreateProgramFunc ();
	GL_ObjectLabelFunc (GL_PROGRAM, program, -1, name);

	while (numshaders-- > 0)
	{
		GL_AttachShaderFunc (program, *shaders);
		GL_DeleteShaderFunc (*shaders);
		++shaders;
	}

	VR_GLStep ("link %s", name); // QVR: breadcrumbs (vr/vr_glsafe.cpp)
	GL_LinkProgramFunc (program);
	status = GL_TRUE;
	if (!gl_multiview_async) // QVR: a twin made in the background: its status later (GL_MultiviewPending)
		GL_GetProgramivFunc (program, GL_LINK_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetProgramInfoLogFunc (program, sizeof(infolog), NULL, infolog);
		if (gl_multiview_compile) // QVR: a twin
		{
			Con_Printf ("multiview twin of %s: link:\n%s\n", name, infolog);
			gl_multiview_compile = 2;
		}
		else
		GL_InitError ("Error linking %s program:\n\n%s", name, infolog);
	}

	if (gl_num_programs == countof(gl_programs))
		Sys_Error ("gl_programs overflow");
	gl_programs[gl_num_programs] = program;
	gl_num_programs++;

	return program;
}

/*
====================
GL_CreateProgramFromSources
====================
*/
static GLuint GL_CreateProgramFromSources (int count, const GLchar **sources, const GLenum *types, const char *name, va_list argptr)
{
	char macros[1024];
	char eval[256];
	char *pipe;
	int i, realcount;
	GLuint shaders[2];

	if (count <= 0 || count > 2)
		Sys_Error ("GL_CreateProgramFromSources: invalid source count (%d)", count);

	q_vsnprintf (eval, sizeof (eval), name, argptr);
	macros[0] = 0;

	pipe = strchr (name, '|');
	if (pipe) // parse symbol list and generate #defines
	{
		char *dst = macros;
		char *dstend = macros + sizeof (macros);
		char *src = eval + 1 + (pipe - name);

		while (*src == ' ')
			src++;

		while (*src)
		{
			char *srcend = src + 1;
			while (*srcend && *srcend != ';')
				srcend++;

			if (!AppendString (&dst, dstend, "#define ", 8) ||
				!AppendString (&dst, dstend, src, srcend - src) ||
				!AppendString (&dst, dstend, "\n", 1))
				Sys_Error ("GL_CreateProgram: symbol overflow for %s", eval);

			src = srcend;
			while (*src == ';' || *src == ' ')
				src++;
		}

		AppendString (&dst, dstend, "\n", 1);
	}

	name = eval;

	realcount = 0;
	for (i = 0; i < count; i++)
		if (sources[i])
			shaders[realcount++] = GL_CreateShader (types[i], sources[i], macros, name);

	{ // QVR: a graphics program's sources kept for its multiview twin (GL_MultiviewProgram)
		GLuint program = GL_CreateProgramFromShaders (shaders, realcount, name);
		if (count == 2 && !gl_multiview_compile && gl_num_progsources < (int) countof (gl_progsources))
		{
			glprogsource_t *src = &gl_progsources[gl_num_progsources++];
			memset (src, 0, sizeof (*src));
			src->program = program;
			src->sources[0] = sources[0];
			src->sources[1] = sources[1];
			q_strlcpy (src->macros, macros, sizeof (src->macros));
			q_strlcpy (src->name, name, sizeof (src->name));
			// the ported passes' (STEREO.md): the world's solid and fence surfaces and its depth pre-pass, the alias models
			src->ported = !strncmp (name, "world|", 6) || !strncmp (name, "world pdo|", 10) || !strncmp (name, "world depth", 11) ||
				!strncmp (name, "world decal retro|", 18) || !strncmp (name, "world pdo decal retro|", 22) || !strncmp (name, "alias|", 6);
			gl_multiview_unprepared = gl_multiview_unprepared || src->ported;
		}
		return program;
	}
}

/*
====================
GL_MultiviewMake -- QVR

The multiview twin of `src`'s program compiled and linked; `async`: not waited for (GL_MultiviewPending tells when it
is done, and whether it failed).
====================
*/
static void GL_MultiviewMake (glprogsource_t *src, qboolean async)
{
	GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
	GLuint shaders[2];
	int j, realcount;
	src->tried = true;
	gl_multiview_compile = 1;
	gl_multiview_async = async;
	for (j = 0, realcount = 0; j < 2; j++)
		if (src->sources[j])
			shaders[realcount++] = GL_CreateShader (types[j], src->sources[j], src->macros, src->name);
	if (gl_multiview_compile == 1)
		src->twin = GL_CreateProgramFromShaders (shaders, realcount, src->name);
	else
		for (j = 0; j < realcount; j++)
			GL_DeleteShaderFunc (shaders[j]);
	if (gl_multiview_compile != 1)
		src->twin = 0;
	src->pending = async && src->twin;
	gl_multiview_pending += src->pending;
	gl_multiview_compile = 0;
	gl_multiview_async = false;
}

/*
====================
GL_MultiviewProgram -- QVR

The multiview twin of `program` (single-pass stereo); `program` itself when it has none (a pass not ported, or a twin
that failed to compile or is still compiling: counted in gl_multiview_missing). The twins are made ahead
(GL_MultiviewPrepare): one made here is a program made since.
====================
*/
static GLuint GL_MultiviewProgram (GLuint program)
{
	int i;
	for (i = 0; i < gl_num_progsources; i++)
	{
		glprogsource_t *src = &gl_progsources[i];
		if (src->program != program)
			continue;
		if (!src->ported)
			break;
		if (!src->tried)
			GL_MultiviewMake (src, false);
		if (src->twin && !src->pending)
			return src->twin;
		gl_multiview_missing++;
		src->missing++;
		break;
	}
	return program; // (a compute program, or one of Quake VR's own)
}

/*
====================
GL_MultiviewPrepare -- QVR

Every ported program's twin begun now (single-pass stereo turned on, or on at start: vr/vr_stereo_single.cpp), so none
is compiled mid-game the first time its pass draws. `async` (the driver has GL_KHR_parallel_shader_compile): compiled
by the driver's threads in the background; until GL_MultiviewPending says they are all done, the frames are two-pass.
Returns how many were begun (0: all were already).
====================
*/
int GL_MultiviewPrepare (int async)
{
	int i, made = 0;
	if (!gl_multiview_unprepared)
		return 0;
	gl_multiview_unprepared = false;
	for (i = 0; i < gl_num_progsources; i++)
	{
		glprogsource_t *src = &gl_progsources[i];
		if (src->ported && !src->tried)
		{
			GL_MultiviewMake (src, async != 0);
			made++;
		}
	}
	return made;
}

/*
====================
GL_MultiviewPending -- QVR

How many twins are still compiling in the background (GL_MultiviewPrepare). Those done are checked: a twin that failed
is reported and dropped (its pass then draws per eye in no frame: the multiview pass counts it as missing).
====================
*/
int GL_MultiviewPending (void)
{
	int i;
	if (!gl_multiview_pending)
		return 0;
	gl_multiview_pending = 0;
	for (i = 0; i < gl_num_progsources; i++)
	{
		glprogsource_t *src = &gl_progsources[i];
		GLint done = GL_TRUE, status = GL_TRUE;
		if (!src->pending)
			continue;
		GL_GetProgramivFunc (src->twin, GL_COMPLETION_STATUS_KHR, &done);
		if (!done)
		{
			gl_multiview_pending++;
			continue;
		}
		src->pending = false;
		GL_GetProgramivFunc (src->twin, GL_LINK_STATUS, &status);
		if (status != GL_TRUE)
		{
			char infolog[1024];
			memset (infolog, 0, sizeof (infolog));
			GL_GetProgramInfoLogFunc (src->twin, sizeof (infolog), NULL, infolog);
			Con_Printf ("multiview twin of %s: link:\n%s\n", src->name, infolog);
			src->twin = 0;
		}
	}
	return gl_multiview_pending;
}

/*
====================
GL_MultiviewStats -- QVR

The twins made so far, and the programs the multiview pass used without one (since the last call).
====================
*/
void GL_MultiviewStats (int *twins, int *missing)
{
	int i;
	*twins = 0;
	for (i = 0; i < gl_num_progsources; i++)
	{
		if (gl_progsources[i].twin)
			++*twins;
		if (gl_progsources[i].missing)
			Con_Printf ("  %s: its twin failed, %d uses\n", gl_progsources[i].name, gl_progsources[i].missing);
		gl_progsources[i].missing = 0;
	}
	*missing = gl_multiview_missing;
	gl_multiview_missing = 0;
}

/*
====================
GL_CreateProgram

Compiles and returns GLSL program.
====================
*/
static FUNC_PRINTF(3,4) GLuint GL_CreateProgram (const GLchar *vertSource, const GLchar *fragSource, const char *name, ...)
{
	const GLchar *sources[2] = {vertSource, fragSource};
	GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
	va_list argptr;
	GLuint program;

	va_start (argptr, name);
	program = GL_CreateProgramFromSources (2, sources, types, name, argptr);
	va_end (argptr);

	return program;
}

/*
====================
GL_CreateComputeProgram

Compiles and returns GLSL program.
====================
*/
static FUNC_PRINTF(2,3) GLuint GL_CreateComputeProgram (const GLchar *source, const char *name, ...)
{
	GLenum type = GL_COMPUTE_SHADER;
	va_list argptr;
	GLuint program;

	va_start (argptr, name);
	program = GL_CreateProgramFromSources (1, &source, &type, name, argptr);
	va_end (argptr);

	return program;
}

/*
====================
GL_UseProgram
====================
*/
void GL_UseProgram (GLuint program)
{
	if (gl_multiview_draw && program) // QVR: the multiview pass (single-pass stereo): the program's twin
		program = GL_MultiviewProgram (program);
	if (program == gl_current_program)
		return;
	gl_current_program = program;
	GL_UseProgramFunc (program);
}

/*
====================
GL_ClearCachedProgram

This must be called if you do anything that could make the cached program
invalid (e.g. manually binding, destroying the context).
====================
*/
void GL_ClearCachedProgram (void)
{
	gl_current_program = 0;
	GL_UseProgramFunc (0);
}

/*
=============
GL_CreateShaders
=============
*/
void GL_CreateShaders (void)
{
	int palettize, dither, mode, alphatest, warp, oit;

	glprogs.gui = GL_CreateProgram (gui_vertex_shader, gui_fragment_shader, "gui");
	glprogs.viewblend = GL_CreateProgram (viewblend_vertex_shader, viewblend_fragment_shader, "viewblend");
	for (warp = 0; warp < 2; warp++)
		glprogs.warpscale[warp] = GL_CreateProgram (warpscale_vertex_shader, warpscale_fragment_shader, "view warp/scale|WARP %d", warp);
	for (palettize = 0; palettize < 3; palettize++)
		glprogs.postprocess[palettize] = GL_CreateProgram (postprocess_vertex_shader, postprocess_fragment_shader, "postprocess|PALETTIZE %d", palettize);

	for (mode = 0; mode < 2; mode++)
		glprogs.oit_resolve[mode] = GL_CreateProgram (oit_resove_vertex_shader, oit_resove_fragment_shader, "oit resolve|MSAA %d", mode);

	for (oit = 0; oit < 2; oit++)
		for (dither = 0; dither < 3; dither++)
			for (mode = 0; mode < 3; mode++)
				glprogs.world[oit][dither][mode] = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world|OIT %d; DITHER %d; MODE %d%s", oit, dither, mode,
					mode == WORLDSHADER_SOLID ? "; NO_DECAL_RETRO 1" : ""); // QVR: the decals' retro textures' code: GL_WorldDecalRetroProgram's
	for (dither = 0; dither < 3; dither++) // QVR: the opaque world writing its parallax hits' depth (vr_parallax_depth_write)
		glprogs.world_pdo[dither] = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world pdo|OIT 0; DITHER %d; MODE %d; PDO 1; NO_DECAL_RETRO 1", dither, WORLDSHADER_SOLID);

	for (dither = 0; dither < 2; dither++)
	{
		for (oit = 0; oit < 2; oit++)
		{
			glprogs.water[oit][dither] = GL_CreateProgram (water_vertex_shader, water_fragment_shader, "water|OIT %d; DITHER %d", oit, dither);
			glprogs.particles[oit][dither] = GL_CreateProgram (particles_vertex_shader, particles_fragment_shader, "particles|OIT %d; DITHER %d", oit, dither);
		}
		for (mode = 0; mode < 2; mode++)
			glprogs.skycubemap[mode][dither] = GL_CreateProgram (sky_cubemap_vertex_shader, sky_cubemap_fragment_shader, "sky cubemap|ANIM %d; DITHER %d", mode, dither);
		glprogs.skylayers[dither] = GL_CreateProgram (sky_layers_vertex_shader, sky_layers_fragment_shader, "sky layers|DITHER %d", dither);
		glprogs.skyboxside[dither] = GL_CreateProgram (sky_boxside_vertex_shader, sky_boxside_fragment_shader, "skybox side|DITHER %d", dither);
		glprogs.sprites[dither] = GL_CreateProgram (sprites_vertex_shader, sprites_fragment_shader, "sprites|DITHER %d", dither);
	}
	glprogs.skystencil = GL_CreateProgram (skystencil_vertex_shader, NULL, "sky stencil");
	glprogs.world_depth = GL_CreateProgram (world_vertex_shader, NULL, "world depth|OIT 0; DITHER 0; MODE %d", WORLDSHADER_SOLID); // QVR
	glprogs.world_depth_pdo = GL_CreateProgram (world_vertex_shader, world_depth_fragment_shader, "world depth pdo|OIT 0; DITHER 0; MODE %d; PDO 1", WORLDSHADER_SOLID); // QVR: pixel depth offset (vr_parallax_depth_write)

	int poseverttype;
	for (oit = 0; oit < 2; oit++)
		for (mode = 0; mode < 3; mode++)
			for (alphatest = 0; alphatest < 2; alphatest++)
				for (poseverttype = 0; poseverttype < 3; poseverttype++) 
					glprogs.alias[oit][mode][alphatest][poseverttype] =
					GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias|OIT %d; MODE %d; ALPHATEST %d; POSEVERTTYPE %d", oit, mode, alphatest, poseverttype);
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR; MODE 0 (standard) in these: an undefined name in #if is an error in GLSL (strict drivers)
		glprogs.alias_depth[poseverttype] = GL_CreateProgram (alias_vertex_shader, NULL, "alias depth|MODE 0; POSEVERTTYPE %d", poseverttype);
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR: into all their faces at once (vr/vr_lighting.cpp)
		glprogs.alias_depth_layered[poseverttype] = gl_viewport_layer_able ?
			GL_CreateProgram (alias_vertex_shader, NULL, "alias depth layered|MODE 0; POSEVERTTYPE %d; LAYERED %d", poseverttype, gl_viewport_layer_able) : 0;
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR: wounds painted on models (vr/vr_wounds.cpp)
		glprogs.woundpaint[poseverttype] = GL_CreateProgram (alias_vertex_shader, wound_paint_fragment_shader, "wound paint|MODE 0; POSEVERTTYPE %d; WOUNDPAINT 1", poseverttype);
	glprogs.woundpaintbox = GL_CreateProgram (wound_paint_box_vertex_shader, wound_paint_fragment_shader, "wound paint box"); // QVR: a held brush model's (vr/vr_wounds.cpp)

	glprogs.debug3d = GL_CreateProgram (debug3d_vertex_shader, debug3d_fragment_shader, "debug3d");

	glprogs.clear_indirect = GL_CreateComputeProgram (clear_indirect_compute_shader, "clear indirect draw params");
	glprogs.gather_indirect = GL_CreateComputeProgram (gather_indirect_compute_shader, "indirect draw gather");
	glprogs.cull_mark = GL_CreateComputeProgram (cull_mark_compute_shader, "cull/mark");
	glprogs.cluster_lights = GL_CreateComputeProgram (cluster_lights_compute_shader, "light cluster");
	for (mode = 0; mode < 3; mode++)
		glprogs.palette_init[mode] = GL_CreateComputeProgram (palette_init_compute_shader, "palette init|MODE %d", mode);
	glprogs.palette_postprocess = GL_CreateComputeProgram (palette_postprocess_compute_shader, "palette postprocess");
}

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
			*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world decal retro|OIT %d; DITHER %d; MODE %d", oit, dither, WORLDSHADER_SOLID);
	}
	return *program;
}

/*
=============
GL_DeleteShaders
=============
*/
void GL_DeleteShaders (void)
{
	int i;
	for (i = 0; i < gl_num_programs; i++)
	{
		GL_DeleteProgramFunc (gl_programs[i]);
		gl_programs[i] = 0;
	}
	gl_num_programs = 0;
	gl_num_progsources = 0; // QVR: (the twins were among gl_programs)
	gl_multiview_pending = 0; // QVR

	GL_UseProgramFunc (0);
	gl_current_program = 0;

	memset (&glprogs, 0, sizeof(glprogs));
}

/*
=============
GL_ReloadShaders_f

QVR: tests: "vr_shader_reload [n]" compiles the engine's shaders again (gl_shaders.h, vr_glsl.h, vr_retro.h...), with
QVR_SHADER_AB n (0 by default): a change under "#if QVR_SHADER_AB" compared with the code before it in one run, on
the same frame (e.g. paused), for image diffs and timings.
=============
*/
void GL_ReloadShaders_f (void)
{
	gl_shader_ab = Cmd_Argc () > 1 ? atoi (Cmd_Argv (1)) : 0;
	GL_DeleteShaders ();
	GL_CreateShaders ();
	Con_Printf ("shaders compiled again (QVR_SHADER_AB %d)\n", gl_shader_ab);
}
