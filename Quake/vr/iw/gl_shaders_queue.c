// QVR: the deferred link checks and the queued program compiles.
// Moved as it was out of gl_shaders.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

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

