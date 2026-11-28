// QVR: the program registry, binary cache, keys and the driver's limits.
// Moved as it was out of gl_shaders.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

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
/*
=============
GL_VertexOnlyEmptyFrag -- QVR

A vertex-only program (the depth passes: the world's pre-pass, the models' and shadow casters' depth, the sky
stencil) is linked with an empty fragment shader instead: on OpenGL ES (which links no graphics program without one:
the Quest's Adreno refuses it) and where VR_GLSAFE_VERTEX_ONLY is off (-glsafe; Intel's, vr_gl_workarounds 1: its
Windows driver, an HD Graphics 630's, failed "world depth" with "Out of resource error" while every program with a
fragment shader linked; without a consumer the world vertex shader's ~21 outputs, to location 29, stay live, with
one none of them are). The same depths (the vertex shader's gl_Position is invariant); the colour is masked there.
=============
*/
static qboolean GL_VertexOnlyEmptyFrag (void)
{
	return VR_GLESForm (VR_GLES_FORM_GLSL) || VR_GLSafeOff (VR_GLSAFE_VERTEX_ONLY);
}

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
GL_DescribeLimits -- QVR

The driver's limits a program can run out of (a link's "Out of resource error" names none): storage blocks per stage
and in all, the interface between the stages, uniforms, attributes, clip distances, images, atomic counters and
texture units. GL_Init's "GL limits:" line, the crash report's (VR_SetCrashGlLimits) and a failed link's message.
=============
*/
#define GL_LIMIT_COUNT 18
static const GLenum gl_limit_names[GL_LIMIT_COUNT] =
{
	GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS,
	GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
	GL_MAX_VERTEX_OUTPUT_COMPONENTS, GL_MAX_FRAGMENT_INPUT_COMPONENTS, GL_MAX_VARYING_VECTORS,
	GL_MAX_VERTEX_UNIFORM_COMPONENTS, GL_MAX_VERTEX_UNIFORM_BLOCKS, GL_MAX_FRAGMENT_UNIFORM_BLOCKS, GL_MAX_UNIFORM_BLOCK_SIZE,
	GL_MAX_VERTEX_ATTRIBS, GL_MAX_VERTEX_IMAGE_UNIFORMS, GL_MAX_VERTEX_ATOMIC_COUNTERS,
	GL_MAX_TEXTURE_IMAGE_UNITS, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, GL_MAX_FRAGMENT_IMAGE_UNIFORMS,
};

void GL_DescribeLimits (char *buf, size_t size)
{
	GLint v[GL_LIMIT_COUNT + 1];
	int i;
	for (i = 0; i < GL_LIMIT_COUNT; i++)
	{
		v[i] = 0;
		glGetIntegerv (gl_limit_names[i], &v[i]);
	}
	v[GL_LIMIT_COUNT] = 0;
#ifndef __ANDROID__
	glGetIntegerv (GL_MAX_CLIP_DISTANCES, &v[GL_LIMIT_COUNT]); // (ES: only with EXT_clip_cull_distance)
#endif
	q_snprintf (buf, size,
		"storage blocks vertex %d, fragment %d, compute %d, combined %d, bindings %d; "
		"vertex output components %d, fragment input components %d, varying vectors %d; "
		"vertex uniform components %d, uniform blocks vertex %d, fragment %d, block size %d; "
		"vertex attributes %d; vertex images %d, vertex atomic counters %d; "
		"texture units fragment %d, combined %d; fragment images %d; clip distances %d",
		v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15], v[16], v[17],
		v[GL_LIMIT_COUNT]);
}

/*
=============
GL_StorageBlockLimits -- QVR

The driver's limits (GL_DescribeLimits), for a failed link's message: a link that fails on them (OpenGL ES may offer
few or no storage blocks to the vertex and fragment stages; Intel's Windows driver says "Out of resource error") says
which they are, not only the driver's log.
=============
*/
static const char *GL_StorageBlockLimits (void)
{
	char limits[640];
	GL_DescribeLimits (limits, sizeof (limits));
	return va ("the driver's limits: %s", limits);
}
