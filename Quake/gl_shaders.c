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
#include "vr/vr_api.h"
#include "vr/vr_api_render.h" // QVR: VR_PrecompileProgramsBegin, VR_PrecompileProgramsPoll

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
	GLuint		arraytwin;	// its twin with the world's textures in arrays (GL_WorldArraysProgram; 0: none)
	qboolean	arraytried;
	unsigned long long	twinkey;	// its multiview twin's program cache key, kept once its background link is done (0: none)
	char		macros[1024];
	char		name[128];
} glprogsource_t;
static glprogsource_t gl_progsources[160];
static int gl_num_progsources;
static int gl_progsources_dropped; // graphics programs made with gl_progsources full: no twin (GL_MultiviewCheck)
static int gl_multiview_compile; // GL_CreateShader: the twin's header (1); a failure is reported, not fatal
static int gl_multiview_missing; // programs used in the multiview pass without a twin (VR_StereoSingleInfo)
static qboolean gl_multiview_unprepared; // a ported program made since GL_MultiviewPrepare (its twin not yet)
#ifndef GL_COMPLETION_STATUS_KHR
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif
static qboolean gl_multiview_async; // GL_CreateShader, GL_CreateProgramFromShaders: a twin not waited for
static int gl_multiview_pending; // twins compiling in the background (GL_MultiviewPending)
static int gl_world_lite; // QVR: the world programs' lite form they were made with (VR_GLESWorldLite; GL_WorldLiteCheck)
qboolean gl_multiview_draw; // GL_UseProgram: the multiview pass draws (twins)
static int gl_arrays_compile; // QVR: GL_WorldArraysProgram's twin compiling (1; a failure reported, not fatal: 2)
static qboolean gl_shaders_lazy; // QVR: the ES profile makes some groups' variants when asked for (GL_ShadersLazyStart)
static qboolean gl_arrays_merge_tried; // QVR: GL_WorldArraysMergeProgram's made (or failed to be)
static FUNC_PRINTF(2,3) GLuint GL_CreateComputeProgram (const GLchar *source, const char *name, ...); // QVR: (GL_WorldArraysMergeProgram)

// QVR: GL_CreateShaders with the driver's parallel compile (gl_parallel_compile_able): every shader compiled and every
// program linked without waiting, their statuses read at its end in the same order (GL_DeferredCheck: the same errors).
typedef struct {
	GLuint		program;
	GLuint		shaders[2];
	int			numshaders;
	char		name[128];
	unsigned long long	key; // its binary kept once linked (the program cache; 0: not)
} gldeferred_t;
static gldeferred_t gl_deferred[256];
static int gl_num_deferred;
static qboolean gl_shaders_deferred;

// QVR: the program cache (vr/vr_diskcache.cpp VR_ProgCache*, vr_program_cache; QUEST_PLAN.md, C4d): a program's binary
// from the driver kept, given back the next start instead of compiling and linking its shaders.
static qboolean gl_progcache_on; // decided at GL_CreateShaders
static unsigned long long gl_progcache_key; // the program being linked's key (0: none, not kept)

static void GL_ProgramRegister (GLuint program)
{
	if (gl_num_programs == countof(gl_programs))
		Sys_Error ("gl_programs overflow");
	gl_programs[gl_num_programs] = program;
	gl_num_programs++;
}

static qboolean GL_ProgramBinaryInto (GLuint program, unsigned long long key, const char *name)
{ // QVR: program `program` (made, not linked) given its binary from the program cache; false: none, or refused
	unsigned format = 0;
	int length = 0;
	GLint status = GL_FALSE;
	double t0;
	const void *data = VR_ProgCacheFind (key, &format, &length);
	if (!data)
		return false;
	VR_GLStep ("program %s from its binary", name); // (breadcrumbs, vr/vr_glsafe.cpp)
	t0 = Sys_DoubleTime ();
	GL_ProgramBinaryFunc (program, (GLenum) format, data, length);
	GL_GetProgramivFunc (program, GL_LINK_STATUS, &status);
	VR_TimeAdd (status == GL_TRUE ? "  programs from their binaries (the program cache)" : "  program binaries refused (made again)",
		Sys_DoubleTime () - t0); // load timing (vr_startup_times)
	if (status != GL_TRUE) // (the driver refused it: made from the sources, its new binary kept)
	{
		VR_ProgCacheReject (key);
		return false;
	}
	return true;
}

static GLuint GL_ProgramFromCache (unsigned long long key, const char *name)
{
	GLuint program = GL_CreateProgramFunc (); // (a miss: deleted again, cheap)
	GL_ObjectLabelFunc (GL_PROGRAM, program, -1, name);
	if (!GL_ProgramBinaryInto (program, key, name))
	{
		GL_DeleteProgramFunc (program);
		return 0;
	}
	GL_ProgramRegister (program);
	return program;
}

static void GL_ProgramToCache (GLuint program, unsigned long long key)
{
	GLint length = 0;
	GLenum format = 0;
	GLsizei got = 0;
	void *data;
	GL_GetProgramivFunc (program, GL_PROGRAM_BINARY_LENGTH, &length);
	if (length <= 0)
		return;
	data = malloc (length);
	if (!data)
		return;
	GL_GetProgramBinaryFunc (program, length, &got, &format, data);
	if (got > 0)
		VR_ProgCacheAdd (key, format, data, got);
	free (data);
}

static int GL_ShaderStrings (GLenum type, const char *source, const char *extradefs, char *header, size_t headersize, const char **strings);

static unsigned long long GL_ProgramKey (unsigned long long key, GLenum type, const char *source, const char *extradefs)
{ // QVR: a shader's part of its program's key: its type and the very text the driver compiles (VR_GLShaderHash)
	const char *strings[16];
	char header[256], typestr[16];
	int numstrings = GL_ShaderStrings (type, source, extradefs, header, sizeof (header), strings);
	q_snprintf (typestr, sizeof (typestr), "%x", type);
	return VR_GLShaderHash (VR_ProgCacheKey (key, typestr), numstrings, strings);
}

/*
=============
GL_TwinProgramKey -- QVR

A twin's program cache key (an array twin, GL_WorldArraysProgram; a multiview twin, GL_MultiviewMake): called while
gl_arrays_compile or gl_multiview_compile is set, as GL_ShaderStrings reads it (the multiview header), over the text
the driver gets (`macros` with the twin's defines), the empty fragment shader too where it is added. 0 when the cache
is off. (Without it the twins were compiled at every start: the arrays' at each session's first map, seconds on the
Quest.)
=============
*/
static unsigned long long GL_TwinProgramKey (const GLchar *const *sources, const char *macros, qboolean emptyfrag)
{
	unsigned long long key = 0;
	if (!gl_progcache_on)
		return 0;
	if (sources[0])
		key = GL_ProgramKey (key, GL_VERTEX_SHADER, sources[0], macros);
	if (sources[1])
		key = GL_ProgramKey (key, GL_FRAGMENT_SHADER, sources[1], macros);
	if (emptyfrag)
		key = GL_ProgramKey (key, GL_FRAGMENT_SHADER, "void main() {}\n", macros);
	return key;
}

/*
=============
GL_StorageBlockLimits -- QVR

The driver's storage-block limits per stage, for a failed link's message: a link that fails on them (OpenGL ES may
offer few or none to the vertex and fragment stages; the scene's programs read several) says so, not only the
driver's log.
=============
*/
static const char *GL_StorageBlockLimits (void)
{
	GLint vs = 0, fs = 0, cs = 0;
	glGetIntegerv (GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &vs);
	glGetIntegerv (GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &fs);
	glGetIntegerv (GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &cs);
	return va ("the driver's storage blocks per stage: vertex %d, fragment %d, compute %d", vs, fs, cs);
}

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

	if (VR_GLESShaderError (buf)) // QVR: -glescompat: listed and counted, not fatal (every program checked in one run)
		return;

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
static int GL_ShaderStrings (GLenum type, const char *source, const char *extradefs, char *header, size_t headersize, const char **strings)
{ // QVR: a shader's strings (GL_CreateShader; the program cache's key, GL_ProgramKey)
	int numstrings = 0;
	q_snprintf (header, headersize,
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
	return numstrings;
}

static GLuint GL_CreateShader (GLenum type, const char *source, const char *extradefs, const char *name)
{
	const char *strings[16];
	const char *typestr = NULL;
	char header[256];
	int numstrings;
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

	numstrings = GL_ShaderStrings (type, source, extradefs, header, sizeof (header), strings);

	VR_GLStep ("compile %s %s shader", name, typestr); // QVR: breadcrumbs (vr/vr_glsafe.cpp)
	shader = GL_CreateShaderFunc (type);
	GL_ObjectLabelFunc (GL_SHADER, shader, -1, name);
	VR_GLShaderSource (shader, numstrings, strings); // QVR: -glescompat: as ES 3.2 GLSL (vr/vr_glsafe.cpp)
	GL_CompileShaderFunc (shader);
	if (gl_multiview_async) // QVR: a twin made in the background: not waited for (GL_MultiviewPending)
		return shader;
	if (gl_shaders_deferred && !gl_multiview_compile && !gl_arrays_compile) // QVR: its status read at GL_CreateShaders' end (GL_DeferredCheck)
		return shader;
	GL_GetShaderivFunc (shader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetShaderInfoLogFunc (shader, sizeof(infolog), NULL, infolog);
		if (gl_arrays_compile) // QVR: an array twin that fails leaves its pass on the bound path
		{
			Con_Printf ("array twin of %s: %s shader:\n%s\n", name, typestr, infolog);
			VR_GLESShaderError (NULL);
			gl_arrays_compile = 2;
			return shader;
		}
		if (gl_multiview_compile) // QVR: a twin that fails leaves its pass per eye
		{
			Con_Printf ("multiview twin of %s: %s shader:\n%s\n", name, typestr, infolog);
			VR_GLESShaderError (NULL); // (-glescompat: counted)
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
	gldeferred_t *deferred = NULL; // QVR: its statuses read at GL_CreateShaders' end (GL_DeferredCheck)

	program = GL_CreateProgramFunc ();
	GL_ObjectLabelFunc (GL_PROGRAM, program, -1, name);

	if (gl_shaders_deferred && !gl_multiview_compile && !gl_arrays_compile) // QVR
	{
		int i;
		if (gl_num_deferred == countof (gl_deferred) || numshaders > (int) countof (gl_deferred[0].shaders))
			Sys_Error ("gl_deferred overflow");
		deferred = &gl_deferred[gl_num_deferred++];
		deferred->program = program;
		deferred->numshaders = numshaders;
		for (i = 0; i < numshaders; i++)
			deferred->shaders[i] = shaders[i]; // (deleted below: kept while attached, their status still there to read)
		q_strlcpy (deferred->name, name, sizeof (deferred->name));
		deferred->key = gl_progcache_key; // QVR: its binary kept at GL_DeferredCheck
	}

	while (numshaders-- > 0)
	{
		GL_AttachShaderFunc (program, *shaders);
		GL_DeleteShaderFunc (*shaders);
		++shaders;
	}

	VR_GLStep ("link %s", name); // QVR: breadcrumbs (vr/vr_glsafe.cpp)
	if (gl_progcache_key) // QVR: its binary to be kept (GL_ProgramToCache)
		GL_ProgramParameteriFunc (program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	GL_LinkProgramFunc (program);
	status = GL_TRUE;
	if (!gl_multiview_async && !deferred) // QVR: a twin made in the background: its status later (GL_MultiviewPending); deferred: GL_DeferredCheck
		GL_GetProgramivFunc (program, GL_LINK_STATUS, &status);

	if (status != GL_TRUE)
	{
		char infolog[1024];
		memset(infolog, 0, sizeof(infolog));
		GL_GetProgramInfoLogFunc (program, sizeof(infolog), NULL, infolog);
		if (gl_arrays_compile) // QVR: an array twin
		{
			Con_Printf ("array twin of %s: link:\n%s\n", name, infolog);
			gl_arrays_compile = 2;
		}
		else if (gl_multiview_compile) // QVR: a twin
		{
			Con_Printf ("multiview twin of %s: link:\n%s\n", name, infolog);
			gl_multiview_compile = 2;
		}
		else
		GL_InitError ("Error linking %s program:\n\n%s\n\n(%s)", name, infolog, GL_StorageBlockLimits ()); // QVR: the limits
	}
	else if (gl_progcache_key && !deferred && !gl_multiview_async) // QVR: the program cache (deferred: GL_DeferredCheck)
		GL_ProgramToCache (program, gl_progcache_key);

	GL_ProgramRegister (program); // QVR

	return program;
}

/*
=============
GL_DeferredCheckOne -- QVR

A deferred program's statuses (gl_shaders_deferred; the queued set's, GL_ShadersPump): its shaders' compile, then its
link; a failure reported as GL_CreateShader and GL_CreateProgramFromShaders would have; its binary to the program cache.
=============
*/
static void GL_DeferredCheckOne (const gldeferred_t *d)
{
	int j;
	GLint status;
	char infolog[1024];
	for (j = 0; j < d->numshaders; j++)
	{
		GL_GetShaderivFunc (d->shaders[j], GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE)
		{
			GLint type = 0;
			GL_GetShaderivFunc (d->shaders[j], GL_SHADER_TYPE, &type);
			memset (infolog, 0, sizeof (infolog));
			GL_GetShaderInfoLogFunc (d->shaders[j], sizeof (infolog), NULL, infolog);
			GL_InitError ("Error compiling %s %s shader:\n\n%s", d->name,
				type == GL_VERTEX_SHADER ? "vertex" : type == GL_FRAGMENT_SHADER ? "fragment" : "compute", infolog);
		}
	}
	GL_GetProgramivFunc (d->program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE)
	{
		memset (infolog, 0, sizeof (infolog));
		GL_GetProgramInfoLogFunc (d->program, sizeof (infolog), NULL, infolog);
		GL_InitError ("Error linking %s program:\n\n%s\n\n(%s)", d->name, infolog, GL_StorageBlockLimits ()); // QVR: the limits
	}
	if (d->key) // QVR: the program cache
		GL_ProgramToCache (d->program, d->key);
}

/*
=============
The queued shader set -- QVR (QUEST_PLAN.md 6zi)

GL_CreateShadersAsync: the set's programs made (their GL names, as the renderer keeps them) but neither compiled nor
linked: each one queued. GL_ShadersPump, once a frame while the set is pending (host.c _Host_Frame, the loading
panel's frames: vr/vr_shaderload.cpp), hands them to the driver a few milliseconds' worth at a time (from the program
cache, or compiled and linked: by the driver's threads with GL_KHR_parallel_shader_compile, polled with
GL_COMPLETION_STATUS_KHR, never waited for; without it one by one, each compile on the frame that submits it), and
reads each one's statuses once the driver says it is done. Nothing draws while the set is pending: the frames show
the loading panel; the commands, the server and the client wait.
=============
*/
typedef struct {
	gldeferred_t	d;			// the program, its shaders (attached), its name, its cache key
	int				count;
	const GLchar	*sources[2];
	GLenum			types[2];
	qboolean		emptyfrag;	// an ES graphics program without a fragment shader: an empty one
	char			macros[1024];
	int				state;		// 0 queued, 1 with the driver, 2 done
	qboolean		cached;		// from the program cache
	int				bytes;		// its sources' size
	double			t0, ms;		// handed to the driver, how long until done
} glqueued_t;
static glqueued_t gl_queue[192];
static int gl_num_queue, gl_queue_submitted, gl_queue_done, gl_queue_compiling;
static qboolean gl_shaders_queueing; // GL_CreateProgramFromSources queues (GL_CreateShadersAsync)
static qboolean gl_queue_active; // a queued set pending (GL_ShadersPending)
static double gl_queue_t0, gl_queue_last, gl_queue_maxwork, gl_queue_maxgap; // its start, the last pump, the longest pump's work and gap between pumps (ms)
static double gl_queue_flushed; // the program cache's pack last written meanwhile (VR_ProgCacheFlush)
static double gl_queue_maxsubmit, gl_queue_maxcheck; // the longest submit, status check (ms)
static int gl_queue_frames;
static qboolean gl_vr_programs_pending; // VR_PrecompileProgramsBegin's programs compiling (VR_PrecompileProgramsPoll)

static GLuint GL_QueueProgram (int count, const GLchar **sources, const GLenum *types, const char *macros, const char *name, qboolean emptyfrag)
{
	glqueued_t *q;
	int i;
	if (gl_num_queue == (int) countof (gl_queue))
		Sys_Error ("gl_queue overflow");
	q = &gl_queue[gl_num_queue++];
	memset (q, 0, sizeof (*q));
	q->count = count;
	for (i = 0; i < count; i++)
	{
		q->sources[i] = sources[i];
		q->types[i] = types[i];
		q->bytes += sources[i] ? (int) strlen (sources[i]) : 0;
	}
	q->emptyfrag = emptyfrag;
	q_strlcpy (q->macros, macros, sizeof (q->macros));
	q_strlcpy (q->d.name, name, sizeof (q->d.name));
	q->d.program = GL_CreateProgramFunc ();
	GL_ObjectLabelFunc (GL_PROGRAM, q->d.program, -1, name);
	GL_ProgramRegister (q->d.program);
	return q->d.program;
}

static void GL_QueuedDone (glqueued_t *q)
{
	q->state = 2;
	q->ms = (Sys_DoubleTime () - q->t0) * 1000.0;
	gl_queue_done++;
	VR_StepProgress ("shaders %d/%d: %s", gl_queue_done, gl_num_queue, q->d.name);
}

static void GL_QueuedSubmit (glqueued_t *q)
{
	unsigned long long key = 0;
	GLuint shaders[2];
	int i, n = 0;
	const char *emptysource = "void main() {}\n";
	q->t0 = Sys_DoubleTime ();
	if (gl_progcache_on)
	{
		for (i = 0; i < q->count; i++)
			if (q->sources[i])
				key = GL_ProgramKey (key, q->types[i], q->sources[i], q->macros);
		if (q->emptyfrag)
			key = GL_ProgramKey (key, GL_FRAGMENT_SHADER, emptysource, q->macros);
		if (GL_ProgramBinaryInto (q->d.program, key, q->d.name))
		{
			q->cached = true;
			GL_QueuedDone (q);
			return;
		}
	}
	gl_shaders_deferred = true; // (no status read here: GL_DeferredCheckOne once done)
	for (i = 0; i < q->count; i++)
		if (q->sources[i])
			shaders[n++] = GL_CreateShader (q->types[i], q->sources[i], q->macros, q->d.name);
	if (q->emptyfrag)
		shaders[n++] = GL_CreateShader (GL_FRAGMENT_SHADER, emptysource, q->macros, q->d.name);
	gl_shaders_deferred = false;
	q->d.numshaders = n;
	q->d.key = key;
	for (i = 0; i < n; i++)
	{
		q->d.shaders[i] = shaders[i];
		GL_AttachShaderFunc (q->d.program, shaders[i]);
		GL_DeleteShaderFunc (shaders[i]); // (kept while attached, its status still there to read)
	}
	VR_GLStep ("link %s", q->d.name); // (breadcrumbs, vr/vr_glsafe.cpp)
	if (key)
		GL_ProgramParameteriFunc (q->d.program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	GL_LinkProgramFunc (q->d.program);
	q->state = 1;
	gl_queue_compiling++;
	if (!gl_parallel_compile_able) // (compiled by now, or the status read waits: one at a time)
	{
		GL_DeferredCheckOne (&q->d);
		gl_queue_compiling--;
		GL_QueuedDone (q);
	}
}

/*
====================
GL_CreateProgramFromSources
====================
*/
static GLuint GL_CreateProgramFromSources (int count, const GLchar **sources, const GLenum *types, const char *name, va_list argptr)
{
	char macros[1024];
	char eval[1024]; // QVR: as macros (a name's defines: the Quest preset's plain models' reach 335 characters)
	char *pipe;
	int i, realcount;
	GLuint shaders[2];

	if (count <= 0 || count > 2)
		Sys_Error ("GL_CreateProgramFromSources: invalid source count (%d)", count);

	// QVR: a name cut short lost its last defines without a word (a feature compiled in that was meant out; a define cut
	// before its value, a compile error): stop instead
	if (q_vsnprintf (eval, sizeof (eval), name, argptr) >= (int) sizeof (eval))
		Sys_Error ("GL_CreateProgram: the program's name and defines are over %d characters:\n%s", (int) sizeof (eval) - 1, eval);
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

	{ // QVR: a graphics program's sources kept for its multiview twin (GL_MultiviewProgram)
		// QVR: OpenGL ES links no graphics program without a fragment shader (the depth-only ones: the sky stencil, the
		// world's depth pre-pass): an empty one there (the Quest's Adreno refuses the link; QUEST_PLAN.md, C4b)
		const qboolean emptyfrag = count == 2 && types[0] == GL_VERTEX_SHADER && sources[0] && !sources[1] && VR_GLESForm (VR_GLES_FORM_GLSL);
		const char *emptysource = "void main() {}\n";
		unsigned long long key = 0;
		GLuint program = 0;
		const double perft0 = vr_perf_recording ? Sys_DoubleTime () : 0.0; // QVR: the perf recorder (a mid-game compile is a hitch)
		qboolean compiled = false;
		if (gl_shaders_queueing && !gl_multiview_compile && !gl_arrays_compile && !gl_multiview_async) // QVR: the queued set (GL_ShadersPump)
			program = GL_QueueProgram (count, sources, types, macros, name, emptyfrag);
		else if (gl_progcache_on && !gl_multiview_compile && !gl_arrays_compile && !gl_multiview_async) // QVR: the program cache
		{
			for (i = 0; i < count; i++)
				if (sources[i])
					key = GL_ProgramKey (key, types[i], sources[i], macros);
			if (emptyfrag)
				key = GL_ProgramKey (key, GL_FRAGMENT_SHADER, emptysource, macros);
			program = GL_ProgramFromCache (key, name);
		}
		if (!program)
		{
			realcount = 0;
			for (i = 0; i < count; i++)
				if (sources[i])
					shaders[realcount++] = GL_CreateShader (types[i], sources[i], macros, name);
			if (emptyfrag)
				shaders[realcount++] = GL_CreateShader (GL_FRAGMENT_SHADER, emptysource, macros, name);
			gl_progcache_key = key;
			program = GL_CreateProgramFromShaders (shaders, realcount, name);
			gl_progcache_key = 0;
			compiled = true;
		}
		if (vr_perf_recording && !gl_shaders_queueing)
			VR_PerfEvent (compiled ? VR_PERF_SHADER : VR_PERF_SHADERBIN, (Sys_DoubleTime () - perft0) * 1000.0, name);
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
		else if (count == 2 && !gl_multiview_compile)
			gl_progsources_dropped++;
		return program;
	}
}

/*
====================
GL_WorldArraysProgram -- QVR

The twin of world program `program` drawing with the world's textures in arrays (QVR_WORLD_ARRAYS: the bound path's
draw of many calls, vr/vr_worldarrays.cpp), made the first time it is asked for; 0 for a program without one (not a
world program, the liquids', or a twin that failed to compile: its pass stays on the bound path). `depthonly`: the
program reads no texture (the depth pre-pass): any call may join its draws.
====================
*/
GLuint GL_WorldArraysProgram (GLuint program, qboolean *depthonly)
{
	GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
	char macros[1024 + 64];
	GLuint shaders[2];
	glprogsource_t *src = NULL;
	int i, realcount;

	*depthonly = false;
	for (i = 0; i < gl_num_progsources && !src; i++)
		if (gl_progsources[i].program == program)
			src = &gl_progsources[i];
	if (!src || !program)
		return 0;
	*depthonly = src->sources[1] == NULL;
	if (src->arraytried)
		return src->arraytwin;
	src->arraytried = true;
	if (strncmp (src->name, "world|", 6) && strncmp (src->name, "world pdo|", 10) && strncmp (src->name, "world depth", 11) &&
		strncmp (src->name, "world decal retro|", 18) && strncmp (src->name, "world pdo decal retro|", 22))
		return 0;
	if (strstr (src->name, "MODE " QS_STRINGIFY (WORLDSHADER_WATER))) // the liquids' (translucent, in their order) stay as they are
		return 0;
	q_snprintf (macros, sizeof (macros), "%s#define QVR_WORLD_ARRAYS 1\n", src->macros);
	gl_arrays_compile = 1;
	{
		const qboolean emptyfrag = !src->sources[1] && VR_GLESForm (VR_GLES_FORM_GLSL);
		const unsigned long long key = GL_TwinProgramKey (src->sources, macros, emptyfrag);
		if (key && (src->arraytwin = GL_ProgramFromCache (key, src->name)) != 0) // (the program cache)
		{
			gl_arrays_compile = 0;
			return src->arraytwin;
		}
		gl_progcache_key = key; // (its binary kept once linked: GL_CreateProgramFromShaders)
	}
	for (i = 0, realcount = 0; i < 2; i++)
		if (src->sources[i])
			shaders[realcount++] = GL_CreateShader (types[i], src->sources[i], macros, src->name);
	if (realcount == 1 && VR_GLESForm (VR_GLES_FORM_GLSL)) // (the depth pre-pass's twin: an empty fragment shader, as above)
		shaders[realcount++] = GL_CreateShader (GL_FRAGMENT_SHADER, "void main() {}\n", macros, src->name);
	if (gl_arrays_compile == 1)
		src->arraytwin = GL_CreateProgramFromShaders (shaders, realcount, src->name);
	else
		for (i = 0; i < realcount; i++)
			GL_DeleteShaderFunc (shaders[i]);
	gl_progcache_key = 0;
	if (gl_arrays_compile != 1)
		src->arraytwin = 0;
	gl_arrays_compile = 0;
	return src->arraytwin;
}

int GL_WorldRigLightsOff (void) // QVR: the world's programs skip the rig's small lights (C4v; vr_rig_lights_world 0)
{
	return (gl_world_lite & (1 << 27)) != 0;
}

/*
====================
GL_WorldArraysMergeProgram -- QVR

The compute program putting a draw's calls' indices one after another (world_arrays_merge_compute_shader), made the
first time it is needed (only the bound path with the world's textures in arrays uses it).
====================
*/
GLuint GL_WorldArraysMergeProgram (void)
{
	if (!glprogs.world_arrays_merge && !gl_arrays_merge_tried)
	{
		GLuint program;
		gl_arrays_merge_tried = true;
		gl_arrays_compile = 1;
		program = GL_CreateComputeProgram (world_arrays_merge_compute_shader, "world arrays merge");
		glprogs.world_arrays_merge = gl_arrays_compile == 1 ? program : 0;
		gl_arrays_compile = 0;
	}
	return glprogs.world_arrays_merge;
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
	const qboolean emptyfrag = !src->sources[1] && VR_GLESForm (VR_GLES_FORM_GLSL);
	unsigned long long key;
	src->tried = true;
	gl_multiview_compile = 1;
	key = GL_TwinProgramKey (src->sources, src->macros, emptyfrag); // (the multiview header in: gl_multiview_compile)
	src->twinkey = 0;
	if (key && (src->twin = GL_ProgramFromCache (key, src->name)) != 0) // (the program cache)
	{
		src->pending = false;
		gl_multiview_compile = 0;
		return;
	}
	gl_multiview_async = async;
	gl_progcache_key = async ? 0 : key; // (a background link's binary is read when it is done: GL_MultiviewPending)
	src->twinkey = async ? key : 0;
	for (j = 0, realcount = 0; j < 2; j++)
		if (src->sources[j])
			shaders[realcount++] = GL_CreateShader (types[j], src->sources[j], src->macros, src->name);
	if (realcount == 1 && VR_GLESForm (VR_GLES_FORM_GLSL)) // (a vertex-only program: an empty fragment shader, as above)
		shaders[realcount++] = GL_CreateShader (GL_FRAGMENT_SHADER, "void main() {}\n", src->macros, src->name);
	if (gl_multiview_compile == 1)
		src->twin = GL_CreateProgramFromShaders (shaders, realcount, src->name);
	else
		for (j = 0; j < realcount; j++)
			GL_DeleteShaderFunc (shaders[j]);
	gl_progcache_key = 0;
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
		else if (src->twinkey) // QVR: its binary kept, now that it is linked (the program cache)
			GL_ProgramToCache (src->twin, src->twinkey);
		src->twinkey = 0;
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
GL_MultiviewCheck -- QVR

vr_multiview_check (vr/vr_stereo_single.cpp, docs/vr-port/CODE_STYLE.md, "Rendering stays multiview-ready"): every
graphics program's multiview twin, its pass ported or not, compiled and linked into a program thrown away after
(nothing kept, the real twins untouched). All are begun first and read after, so the driver's threads can compile them
side by side. `report` is called for each: its name, ported or not, built or not and the driver's log. Returns how many
graphics programs were made with no room left to keep their sources (they can have no twin).
====================
*/
int GL_MultiviewCheck (void (*report) (const char *name, int ported, int ok, const char *log, void *ctx), void *ctx)
{
	const GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
	GLuint programs[countof (gl_progsources)];
	GLuint shaders[countof (gl_progsources)][2];
	int i, j;

	gl_multiview_compile = 1;
	gl_multiview_async = true; // (GL_CreateShader: no status read there)
	for (i = 0; i < gl_num_progsources; i++)
	{
		const glprogsource_t *src = &gl_progsources[i];
		programs[i] = GL_CreateProgramFunc ();
		for (j = 0; j < 2; j++)
		{
			shaders[i][j] = src->sources[j] ? GL_CreateShader (types[j], src->sources[j], src->macros, src->name) : 0;
			if (shaders[i][j])
				GL_AttachShaderFunc (programs[i], shaders[i][j]);
		}
		GL_LinkProgramFunc (programs[i]);
	}
	gl_multiview_compile = 0;
	gl_multiview_async = false;

	for (i = 0; i < gl_num_progsources; i++)
	{
		char log[1024];
		GLint status = GL_FALSE;
		log[0] = 0;
		GL_GetProgramivFunc (programs[i], GL_LINK_STATUS, &status);
		for (j = 0; j < 2 && status != GL_TRUE && !log[0]; j++) // the first failing shader's log, else the link's
		{
			GLint compiled = GL_TRUE;
			if (!shaders[i][j])
				continue;
			GL_GetShaderivFunc (shaders[i][j], GL_COMPILE_STATUS, &compiled);
			if (compiled != GL_TRUE)
				GL_GetShaderInfoLogFunc (shaders[i][j], sizeof (log), NULL, log);
		}
		if (status != GL_TRUE && !log[0])
			GL_GetProgramInfoLogFunc (programs[i], sizeof (log), NULL, log);
		report (gl_progsources[i].name, gl_progsources[i].ported, status == GL_TRUE, log, ctx);
		for (j = 0; j < 2; j++)
			if (shaders[i][j])
				GL_DeleteShaderFunc (shaders[i][j]);
		GL_DeleteProgramFunc (programs[i]);
	}
	return gl_progsources_dropped;
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
	const double start = Sys_DoubleTime (); // QVR

	va_start (argptr, name);
	program = GL_CreateProgramFromSources (2, sources, types, name, argptr);
	va_end (argptr);

	// QVR: a program made in play is a hitch (a lazily made variant: GL_WorldDecalRetroProgram's translucent ones)
	if (cls.state == ca_connected && cls.signon == SIGNONS)
	{
		char macros[256];
		va_start (argptr, name);
		q_vsnprintf (macros, sizeof (macros), name, argptr);
		va_end (argptr);
		Con_DPrintf ("GL program made in play: %s, %.0f ms\n", macros, (Sys_DoubleTime () - start) * 1000.0);
	}

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
/*
=============
GL_DeferredCheck -- QVR

The deferred programs' statuses (gl_shaders_deferred), in the order they were made: each one's shaders' compile, then
its link; the first failure reported as GL_CreateShader and GL_CreateProgramFromShaders would have.
=============
*/
static void GL_DeferredCheck (void)
{
	int i;
	for (i = 0; i < gl_num_deferred; i++)
		GL_DeferredCheckOne (&gl_deferred[i]);
	gl_num_deferred = 0;
}

static void GL_CreateShadersAll (void);

static const char *gl_shaders_why = "start-up"; // QVR: why the next GL_CreateShaders runs (VR_GLShadersBegin: the start-up's sets counted)

static void GL_CreateShadersBegin (void)
{
	VR_GLShadersBegin (gl_shaders_why); // QVR: a set's begin and end in gl_startup.log (a start stopped mid-compile is no failed start)
	gl_shaders_why = "start-up";
	gl_progcache_on = VR_ProgCacheOn () != 0; // QVR: the program cache (vr_program_cache; vr/vr_diskcache.cpp)
	if (gl_progcache_on)
		VR_ProgCacheBegin (gl_vendor, gl_renderer, gl_version);
}

static void GL_CreateShadersEnd (void)
{
	if (gl_progcache_on)
		VR_ProgCacheEnd (); // (the pack written when a program was made)
	VR_GLShadersEnd (gl_world_lite, gl_num_programs); // QVR
}

void GL_CreateShaders (void)
{
	GL_CreateShadersBegin ();
	gl_shaders_deferred = gl_parallel_compile_able; // QVR: the driver's threads (GL_DeferredCheck at the end)
	GL_CreateShadersAll ();
	GL_DeferredCheck ();
	gl_shaders_deferred = false;
	GL_CreateShadersEnd ();
}

/*
=============
GL_ShadersAsyncOn -- QVR

Whether the start-up's set, and the settings' remakes (GL_WorldLiteCheck), are queued (GL_CreateShadersAsync) instead
of made in one call: under the OpenGL ES profile (the Quest; -glescompat), unless "-shadersasync 0"; "-shadersasync"
(1) on the PC's own GLSL too; else the environment's QVR_SHADERS_ASYNC (0 or 1) when set.
=============
*/
qboolean GL_ShadersAsyncOn (void)
{
	int i = COM_CheckParm ("-shadersasync");
	const char *env = getenv ("QVR_SHADERS_ASYNC"); // (tests: gles_render_test's PC run as its ES one)
	if (i)
		return i + 1 < com_argc && com_argv[i + 1][0] != '-' ? atoi (com_argv[i + 1]) != 0 : true;
	if (env && *env)
		return atoi (env) != 0;
	return VR_GLESForm (VR_GLES_FORM_GLSL) != 0;
}

/*
=============
GL_CreateShadersAsync -- QVR

The set queued (above: GL_ShadersPump makes it, frame by frame); with `vrprograms`, Quake VR's own passes' programs too
(VR_PrecompileProgramsBegin: the driver's threads compile them meanwhile, polled at each pump).
=============
*/
void GL_CreateShadersAsync (qboolean vrprograms)
{
	GL_CreateShadersBegin ();
	gl_num_queue = gl_queue_submitted = gl_queue_done = gl_queue_compiling = 0;
	gl_shaders_queueing = true;
	GL_CreateShadersAll ();
	gl_shaders_queueing = false;
	gl_queue_active = true;
	gl_queue_t0 = gl_queue_last = gl_queue_flushed = Sys_DoubleTime ();
	gl_queue_maxwork = gl_queue_maxgap = gl_queue_maxsubmit = gl_queue_maxcheck = 0.0;
	gl_queue_frames = 0;
	gl_vr_programs_pending = vrprograms && VR_PrecompileProgramsBegin ();
}

qboolean GL_ShadersPending (void)
{
	return gl_queue_active;
}

/*
=============
GL_ShadersProgress -- QVR

The pending set's programs done and in all (Quake VR's own counted as one batch), and the last one done's name.
=============
*/
void GL_ShadersProgress (int *done, int *total, const char **last)
{
	int i;
	*done = gl_queue_done;
	*total = gl_num_queue;
	*last = "";
	for (i = gl_num_queue - 1; i >= 0; i--)
		if (gl_queue[i].state == 2)
		{
			*last = gl_queue[i].d.name;
			break;
		}
}

/*
=============
GL_ShadersCompiling -- QVR

How many of the pending set's programs handed to the driver so far were not in the program cache (compiled from their
sources): a first start's (or a changed set's) long wait, which starts the VR session early for the loading panel
(vr/vr_shaderload.cpp). 0 while every program so far came from the cache.
=============
*/
int GL_ShadersCompiling (void)
{
	int i, n = 0;
	for (i = 0; i < gl_num_queue; i++)
		n += gl_queue[i].state >= 1 && !gl_queue[i].cached;
	return n;
}

static void GL_ShadersFinish (void)
{
	const double ms = (Sys_DoubleTime () - gl_queue_t0) * 1000.0;
	double t0, endms;
	int i, j, cached = 0, bytes = 0, slow[4] = {-1, -1, -1, -1};
	char slowest[400];
	gl_queue_active = false;
	for (i = 0; i < gl_num_queue; i++)
	{
		const glqueued_t *q = &gl_queue[i];
		cached += q->cached;
		bytes += q->cached ? 0 : q->bytes;
		for (j = 0; j < (int) countof (slow); j++)
			if (slow[j] < 0 || q->ms > gl_queue[slow[j]].ms)
			{
				memmove (&slow[j + 1], &slow[j], sizeof (slow[0]) * (countof (slow) - 1 - j));
				slow[j] = i;
				break;
			}
	}
	slowest[0] = 0;
	for (j = 0; j < (int) countof (slow) && slow[j] >= 0; j++)
		q_strlcat (slowest, va ("%s%s %.0f ms", j ? ", " : "", gl_queue[slow[j]].d.name, gl_queue[slow[j]].ms), sizeof (slowest));
	t0 = Sys_DoubleTime ();
	GL_CreateShadersEnd ();
	endms = (Sys_DoubleTime () - t0) * 1000.0;
	VR_StepNote ("GL shaders queued: %d programs (%d from the program cache, %d compiled: %d KB of sources) in %.0f ms over %d frames, %s; "
		"the longest frame's work %.1f ms (a submit %.1f ms, a check %.1f ms; the end %.1f ms), longest between frames %.1f ms; slowest: %s",
		gl_num_queue, cached, gl_num_queue - cached, bytes / 1024, ms, gl_queue_frames, gl_parallel_compile_able ? "the driver's threads" : "one at a time",
		gl_queue_maxwork, gl_queue_maxsubmit, gl_queue_maxcheck, endms, gl_queue_maxgap, slowest);
	Con_SafePrintf ("GL shaders queued: %d programs (%d cached, %d compiled, %d KB) in %.0f ms over %d frames (%s); longest frame work %.1f ms "
		"(submit %.1f, check %.1f, end %.1f), gap %.1f ms\n", gl_num_queue, cached, gl_num_queue - cached, bytes / 1024, ms, gl_queue_frames,
		gl_parallel_compile_able ? "parallel" : "one at a time", gl_queue_maxwork, gl_queue_maxsubmit, gl_queue_maxcheck, endms, gl_queue_maxgap);
	gl_queue_maxwork = q_max (gl_queue_maxwork, endms); // (this frame's: GL_ShadersPump adds the rest after)
	Con_DPrintf ("GL shaders queued: slowest %s\n", slowest);
	VR_ShadersLoaded (gl_num_queue, gl_queue_frames, ms, gl_queue_maxwork, gl_queue_maxgap);
}

/*
=============
GL_ShadersPump -- QVR

Once a frame while the queued set is pending: the programs the driver finished checked (statuses, the program cache),
more handed to it for up to `budget` seconds (one at least); the set finished when all are done (and Quake VR's own:
VR_PrecompileProgramsPoll). Returns whether it is still pending.
=============
*/
qboolean GL_ShadersPump (double budget)
{
	const double t0 = Sys_DoubleTime ();
	int i, submitted = 0;
	if (!gl_queue_active)
		return false;
	if (gl_queue_frames++) // (not the first: the rest of the start-up, Host_Init, before it)
		gl_queue_maxgap = q_max (gl_queue_maxgap, (t0 - gl_queue_last) * 1000.0);
	for (i = 0; i < gl_queue_submitted && gl_queue_compiling > 0; i++)
	{
		glqueued_t *q = &gl_queue[i];
		GLint done = GL_FALSE;
		if (q->state != 1)
			continue;
		GL_GetProgramivFunc (q->d.program, GL_COMPLETION_STATUS_KHR, &done);
		if (done)
		{
			const double c0 = Sys_DoubleTime ();
			GL_DeferredCheckOne (&q->d);
			gl_queue_maxcheck = q_max (gl_queue_maxcheck, (Sys_DoubleTime () - c0) * 1000.0);
			gl_queue_compiling--;
			GL_QueuedDone (q);
		}
	}
	while (gl_queue_submitted < gl_num_queue && gl_queue_compiling < 64 && (!submitted || Sys_DoubleTime () - t0 < budget))
	{
		const double s0 = Sys_DoubleTime ();
		GL_QueuedSubmit (&gl_queue[gl_queue_submitted++]);
		gl_queue_maxsubmit = q_max (gl_queue_maxsubmit, (Sys_DoubleTime () - s0) * 1000.0);
		submitted++;
	}
	if (gl_progcache_on && t0 - gl_queue_flushed > 10.0) // the binaries made so far kept (a start stopped mid-set keeps them)
	{
		gl_queue_flushed = t0;
		VR_ProgCacheFlush ();
	}
	if (gl_vr_programs_pending)
		gl_vr_programs_pending = VR_PrecompileProgramsPoll () != 0;
	if (gl_queue_done == gl_num_queue && !gl_vr_programs_pending)
		GL_ShadersFinish ();
	gl_queue_last = Sys_DoubleTime ();
	gl_queue_maxwork = q_max (gl_queue_maxwork, (gl_queue_last - t0) * 1000.0);
	return gl_queue_active;
}

/*
=============
GL_WorldLiteDefines -- QVR

The world programs' extra defines for their lite form under the ES profile (gl_world_lite: VR_GLESWorldLite's bits).
=============
*/
static const char *const gl_world_lite_names[8] = // QVR: VR_GLESWorldLite's bits, in order (vr_api.h)
{
	"; QVR_LITE_NOPARALLAX 1", "; QVR_LITE_NOCARVE 1", "; QVR_LITE_NOBUMP 1", "; QVR_LITE_NOAO 1",
	"; QVR_LITE_NOCAUSTICS 1", "; QVR_LITE_NORETROLIGHT 1", "; QVR_LITE_NORETRO 1", "; QVR_LITE_NODETAIL 1",
};
static char gl_world_lite_defines[512]; // the defines of gl_world_lite's bits (GL_WorldLiteDefines)

/*
=============
GL_LightsLiteDefines -- QVR

The lights' loops' lite form (C4l; QUEST_PLAN.md 6s: VR_GLESWorldLite's bits 19-23, vr_gl_lights_lite) appended to
`out`: no map lights' shadows (the world's only: the models' loop skips those lights), no DarkPlaces falloff, the shadow
filter's kernel as a constant, no portal lights (the world's only), one filter's code for spot and point lights.
=============
*/
static void GL_LightsLiteDefines (char *out, size_t size, qboolean world)
{
	if (world && (gl_world_lite & (1 << 19)))
		q_strlcat (out, "; QVR_LITE_NOMAPSHADOW 1", size);
	if (gl_world_lite & (1 << 20))
		q_strlcat (out, "; QVR_LITE_NODLFALLOFF 1", size);
	if (gl_world_lite & (1 << 21))
		q_strlcat (out, va ("; QVR_LITE_SHADOWFILT %d", (gl_world_lite >> 22) & 3), size);
	if (world && (gl_world_lite & (1 << 24)))
		q_strlcat (out, "; QVR_LITE_NOPORTALLIGHT 1", size);
	if (gl_world_lite & (1 << 25))
		q_strlcat (out, "; QVR_LITE_ONEFILTER 1", size);
	if (gl_world_lite & (1 << 26)) // C4p: plain lights alone (vr_gl_lights_plain); C4q: the models' loop too
		q_strlcat (out, "; QVR_LITE_PLAINLIGHT 1", size);
	if (world && (gl_world_lite & (1 << 27))) // C4v: the rig's small lights not on the world (vr_rig_lights_world 0)
		q_strlcat (out, "; QVR_LITE_RIGNOWORLD 1", size);
}

static const char *GL_WorldLiteDefines (void)
{
	int i;
	gl_world_lite_defines[0] = 0;
	for (i = 0; i < (int)countof (gl_world_lite_names); i++)
		if (gl_world_lite & (1 << i))
			q_strlcat (gl_world_lite_defines, gl_world_lite_names[i], sizeof (gl_world_lite_defines));
	GL_LightsLiteDefines (gl_world_lite_defines, sizeof (gl_world_lite_defines), true);
	return gl_world_lite_defines;
}

static char gl_alias_lite_defines[320]; // the models' share of them (GL_AliasLiteDefines)

/*
=============
GL_AliasLiteDefines -- QVR

The models' programs' lite form (C4j; QUEST_PLAN.md 6q): of the world's switches, the two whose code the models' shader
has and whose settings turn it off for every model: parallax mapping (bit 1: vr_parallax or vr_normalmaps 0, as
VR_ParallaxDepth gives every model 0) and retro lighting (bit 32: vr_retrolight 0, its frame data all 0); bit 256:
vr_gl_alias_lite on. C4x (QUEST_PLAN.md 6zd): bit 8, the dynamic occlusion (vr_ao_dynamic and vr_ao_brush 0: no
occluder, vr_ao.cpp build), out of the models' programs too (QVR_ALITE_NOAO), as out of the world's: the same picture
(DynamicAO gives 1 without occluders); its gain on the Quest 3 under the launch-to-launch noise (~0.1 ms).
=============
*/
static const char *GL_AliasLiteDefines (void)
{
	gl_alias_lite_defines[0] = 0;
	if (!(gl_world_lite & 256)) // vr_gl_alias_lite 0
		return gl_alias_lite_defines;
	if (gl_world_lite & 1)
		q_strlcat (gl_alias_lite_defines, gl_world_lite_names[0], sizeof (gl_alias_lite_defines));
	if (gl_world_lite & 8)
		q_strlcat (gl_alias_lite_defines, "; QVR_ALITE_NOAO 1", sizeof (gl_alias_lite_defines));
	if (gl_world_lite & 32)
		q_strlcat (gl_alias_lite_defines, gl_world_lite_names[5], sizeof (gl_alias_lite_defines));
	GL_LightsLiteDefines (gl_alias_lite_defines, sizeof (gl_alias_lite_defines), false);
	return gl_alias_lite_defines;
}

static const char *const gl_alias_plain_names[10] = // QVR: VR_AliasPlainNeeds' bits, in order (vr_api_render.h); the last two probes only
{
	"; QVR_APLAIN_NOSKIN 1", "; QVR_APLAIN_NOGLOW 1", "; QVR_APLAIN_NOWOUNDS 1", "; QVR_APLAIN_NODENTS 1", "; QVR_APLAIN_NOSTUMP 1",
	"; QVR_APLAIN_NOBUMP 1", "; QVR_APLAIN_NOMORPH 1", "; QVR_APLAIN_NORIM 1", "; QVR_APLAIN_NOAO 1", "; QVR_APLAIN_NODLIGHTS 1",
};
static char gl_alias_plain_defines[320]; // GL_AliasPlainDefines

/*
=============
GL_AliasPlainDefines -- QVR

The models' plain form (glprogs.alias_plain, C4k; QUEST_PLAN.md 6r): the features it compiles out (gl_world_lite's bits
from 9 up: VR_GLESWorldLite, vr_gl_alias_plain_mask), but those of `keep` (the gore form's: VR_ALIAS_GORE_NEEDS).
=============
*/
static const char *GL_AliasPlainDefines (int keep)
{
	int i, mask = (gl_world_lite >> 9) & ~keep;
	gl_alias_plain_defines[0] = 0;
	for (i = 0; i < (int)countof (gl_alias_plain_names); i++)
		if (mask & (1 << i))
			q_strlcat (gl_alias_plain_defines, gl_alias_plain_names[i], sizeof (gl_alias_plain_defines));
	if (gl_world_lite & (1 << 28)) // C4y: lit at their vertices (vr_gl_alias_vlight; vr_glsl_alias.h QVR_ALIAS_VS_VLIGHT)
		q_strlcat (gl_alias_plain_defines, "; QVR_APLAIN_VLIGHT 1", sizeof (gl_alias_plain_defines));
	return gl_alias_plain_defines;
}

/*
=============
GL_WorldLiteCheck -- QVR

Once a frame, before anything draws: the shaders made again when the world programs' lite form no longer fits the
settings (a feature it compiled out turned on, or one turned off: QUEST_PLAN.md 6n). The program cache gives back the
ones made before. Never on the PC's own GLSL (VR_GLESWorldLite 0 there).
=============
*/
void GL_WorldLiteCheck (void)
{
	int lite;
	qboolean defines;
	if (!host_initialized || host_framecount == 0 || cbuf_executing || !gl_num_programs) // the start-up's (first frame's) or a config's commands not all run: their settings all in first (a print mid-exec draws: one remake after them, not one a print)
		return;
	lite = VR_GLESWorldLite ();
	defines = VR_GLESDefineChanged (); // QVR: vr_gles_define's probes (C4m)
	if (lite == gl_world_lite && !defines)
		return;
	Con_DPrintf ("world programs' lite form %d -> %d%s: shaders made again\n", gl_world_lite, lite, defines ? ", vr_gles_define changed" : "");
	gl_shaders_why = defines ? "vr_gles_define changed" : "the settings changed the lite form";
	GL_DeleteShaders ();
	if (GL_ShadersAsyncOn ()) // QVR: queued: the loading panel's frames until done (SCR_UpdateScreen draws nothing meanwhile)
		GL_CreateShadersAsync (false);
	else
		GL_CreateShaders ();
}

// QVR: the unlit world's programs and the cull's split by surface are made (the ES profile's; the PC's with
// vr_gl_pc_forms 4: the set's lite form has VR_GL_PCFORM_UNLIT_BIT). r_world.c's clear goes by it too.
qboolean GL_WorldUnlitForms (void)
{
	return VR_GLESForm (VR_GLES_FORM_GLSL) || (gl_world_lite & VR_GL_PCFORM_UNLIT_BIT) != 0;
}

static void GL_CreateShadersAll (void)
{
	int palettize, dither, mode, alphatest, warp, oit, form;

	gl_world_lite = VR_GLESWorldLiteFirst (); // QVR: the start's first set: the lite form the last session made last (its settings come with the config after it)

	glprogs.gui = GL_CreateProgram (gui_vertex_shader, gui_fragment_shader, "gui");
	glprogs.viewblend = GL_CreateProgram (viewblend_vertex_shader, viewblend_fragment_shader, "viewblend");
	for (warp = 0; warp < 2; warp++)
		glprogs.warpscale[warp] = GL_CreateProgram (warpscale_vertex_shader, warpscale_fragment_shader, "view warp/scale|WARP %d", warp);
	for (palettize = 0; palettize < 3; palettize++)
		glprogs.postprocess[palettize] = GL_CreateProgram (postprocess_vertex_shader, postprocess_fragment_shader, "postprocess|PALETTIZE %d", palettize);

	for (mode = 0; mode < 2; mode++)
		glprogs.oit_resolve[mode] = GL_CreateProgram (oit_resove_vertex_shader, oit_resove_fragment_shader, "oit resolve|MSAA %d", mode);

	gl_shaders_lazy = VR_GLESForm (VR_GLES_FORM_GLSL); // QVR: the ES profile's start set (GL_ShadersLazyStart)
	for (oit = 0; oit < 2; oit++)
		for (dither = 0; dither < 3; dither++)
			for (mode = 0; mode < 3; mode++)
				if (GL_ShadersLazyStart (oit, dither, 0))
					GL_WorldProgram (oit, dither, mode);
	for (dither = 0; dither < 3; dither++) // QVR: the world without the lights' loop, for views no light reaches its surfaces in (C4t, QUEST_PLAN.md 6z: Adreno pays ~4 ms for the loop compiled in)
		for (mode = 0; mode < 2; mode++)
			glprogs.world_unlit[dither][mode] = GL_WorldUnlitForms ()
				? GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world|OIT 0; DITHER %d; MODE %d; QVR_NOLIGHTS 1%s%s", dither, mode,
					mode == WORLDSHADER_SOLID ? "; NO_DECAL_RETRO 1" : "", GL_WorldLiteDefines ())
				: 0;
	for (dither = 0; dither < 3; dither++) // QVR: the opaque world writing its parallax hits' depth (vr_parallax_depth_write)
		glprogs.world_pdo[dither] = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world pdo|OIT 0; DITHER %d; MODE %d; PDO 1; NO_DECAL_RETRO 1", dither, WORLDSHADER_SOLID);

	for (dither = 0; dither < 2; dither++)
	{
		for (oit = 0; oit < 2; oit++)
		{
			if (GL_ShadersLazyStart (oit, dither, 0))
			{
				GL_WaterProgram (oit, dither);
				GL_ParticlesProgram (oit, dither);
			}
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
					if (GL_ShadersLazyStart (oit, 0, mode))
						GL_AliasProgram (oit, mode, alphatest, poseverttype);
	for (form = 0; form < ALIASFORM_COUNT; form++) // QVR: the opaque models' forms (GL_AliasFormProgram; the ES profile's)
		for (mode = 0; mode < 3; mode++)
			for (poseverttype = 0; poseverttype < 3; poseverttype++)
				if (GL_AliasFormAvailable (form) && GL_ShadersLazyStart (0, 0, mode) && (VR_GLESForm (VR_GLES_FORM_GLSL) || (form != ALIASFORM_OPAQUE && form != ALIASFORM_BODY))) // (the PC's: plain, gore, skin; the others when asked for)
					GL_AliasFormProgram (form, mode, poseverttype);
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR; MODE 0 (standard) in these: an undefined name in #if is an error in GLSL (strict drivers)
		glprogs.alias_depth[poseverttype] = GL_CreateProgram (alias_vertex_shader, NULL, "alias depth|MODE 0; POSEVERTTYPE %d", poseverttype);
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR: into all their faces at once (vr/vr_lighting.cpp)
		glprogs.alias_depth_layered[poseverttype] = gl_viewport_layer_able ?
			GL_CreateProgram (alias_vertex_shader, NULL, "alias depth layered|MODE 0; POSEVERTTYPE %d; LAYERED %d", poseverttype, gl_viewport_layer_able) : 0;
	for (poseverttype = 0; poseverttype < 3; poseverttype++) // QVR: wounds painted on models (vr/vr_wounds.cpp)
		glprogs.woundpaint[poseverttype] = GL_CreateProgram (alias_vertex_shader, wound_paint_fragment_shader, "wound paint|MODE 0; POSEVERTTYPE %d; WOUNDPAINT 1", poseverttype);
	glprogs.woundpaintbox = GL_CreateProgram (wound_paint_box_vertex_shader, wound_paint_fragment_shader, "wound paint box"); // QVR: a held brush model's (vr/vr_wounds.cpp)

	glprogs.debug3d = GL_CreateProgram (debug3d_vertex_shader, debug3d_fragment_shader, "debug3d");

	glprogs.clear_indirect = GL_WorldUnlitForms () // QVR: the ES profile's clears the world's lit commands too (C4v; the PC's with vr_gl_pc_forms 4)
		? GL_CreateComputeProgram (clear_indirect_compute_shader, "clear indirect draw params|QVR_LITSPLIT 1")
		: GL_CreateComputeProgram (clear_indirect_compute_shader, "clear indirect draw params");
	glprogs.gather_indirect = GL_CreateComputeProgram (gather_indirect_compute_shader, "indirect draw gather");
	glprogs.cull_mark = GL_WorldUnlitForms () // QVR: the ES profile's: the world's lit surfaces to their own commands (C4v; the PC's with vr_gl_pc_forms 4)
		? GL_CreateComputeProgram (cull_mark_compute_shader, "cull/mark|QVR_LITSPLIT 1")
		: GL_CreateComputeProgram (cull_mark_compute_shader, "cull/mark");
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
			*program = GL_CreateProgram (world_vertex_shader, world_fragment_shader, "world decal retro|OIT %d; DITHER %d; MODE %d%s", oit, dither, WORLDSHADER_SOLID, GL_WorldLiteDefines ());
	}
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
	GLuint *program;
	if (!GL_AliasFormAvailable (form))
		return 0;
	switch (form)
	{
	case ALIASFORM_OPAQUE:
		program = &glprogs.alias_opaque[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias opaque|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s", mode, poseverttype, GL_AliasLiteDefines ());
		return *program;
	case ALIASFORM_PLAIN:
		program = &glprogs.alias_plain[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias plain|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefines (0));
		return *program;
	case ALIASFORM_GORE:
		program = &glprogs.alias_gore[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias gore|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefines (VR_ALIAS_GORE_NEEDS));
		return *program;
	case ALIASFORM_SKIN:
		program = &glprogs.alias_skin[mode][poseverttype];
		if (!*program)
			*program = GL_CreateProgram (alias_vertex_shader, alias_fragment_shader, "alias skin|OIT 0; MODE %d; ALPHATEST 0; POSEVERTTYPE %d; QVR_ALIAS_OPAQUE 1%s%s", mode, poseverttype, GL_AliasLiteDefines (), GL_AliasPlainDefines (VR_ALIAS_SKIN_NEEDS));
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
	gl_progsources_dropped = 0; // QVR
	gl_multiview_pending = 0; // QVR
	gl_arrays_merge_tried = false; // QVR
	gl_num_queue = gl_queue_submitted = gl_queue_done = gl_queue_compiling = 0; // QVR: a queued set's (its programs among gl_programs)
	gl_queue_active = false; // QVR

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
	gl_shaders_why = "vr_shader_reload";
	GL_DeleteShaders ();
	GL_CreateShaders ();
	Con_Printf ("shaders compiled again (QVR_SHADER_AB %d)\n", gl_shader_ab);
}

/*
=============
GL_ReloadShadersQueued_f

QVR: "vr_shader_reload_queued" (Debug > Startup): the set made again as a start under OpenGL ES makes it (queued, the
loading panel until done: QUEST_PLAN.md 6zi), on any profile; its timings in the "GL shaders queued" line.
=============
*/
void GL_ReloadShadersQueued_f (void)
{
	if (gl_queue_active)
		return;
	gl_shaders_why = "vr_shader_reload_queued";
	GL_DeleteShaders ();
	GL_CreateShadersAsync (false);
}
