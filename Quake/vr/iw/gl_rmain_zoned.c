// QVR: the model fragment counts and the zoned brush and alias passes.
// Moved as it was out of gl_rmain.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

static entity_t *r_stereo_alias[MAX_VISEDICTS]; // QVR: the alias models a single-pass stereo pass draws
static entity_t *r_first_alias[MAX_VISEDICTS], *r_rest_alias[MAX_VISEDICTS]; // QVR: those drawn before the world and after (R_AliasDrawsFirst)
static entity_t *r_zoned_world[MAX_VISEDICTS], *r_zoned_player[MAX_VISEDICTS]; // QVR: R_DrawAliasModelsZoned's parts
static const char *const r_zoned_player_parts[VR_DRAW_GROUPS] = { // QVR: the player's models' GPU sub-zones (VR_DRAW_*)
	"player: hands", "player: body", "player: held", "player: holstered", "player: worn", "player: lying gear",
	"player: mirror image", "player: other"};

/*
=============
R_ModelFragments_f -- QVR

vr_model_fragments [passes] (a debug tool: Debug > Profiling and Memory): over the next passes (16) of the opaque alias
models, each part's (the world's models: monsters, gibs, corpses, props; the player's own by VR_DRAW_* group) GPU time,
vertex shader runs, triangles in, fragment shader runs and samples kept (the depth pre-passes' included), a pass's
average, in the console. Each part's counts are read at once (a stall: for counts and shares, not frame times).
=============
*/
#define QVR_GL_VERTEX_SHADER_INVOCATIONS	0x82F0 // GL 4.6 / ARB_pipeline_statistics_query
#define QVR_GL_FRAGMENT_SHADER_INVOCATIONS	0x82F4
#define QVR_GL_CLIPPING_INPUT_PRIMITIVES	0x82F6
enum { MF_WORLD = VR_DRAW_GROUPS, MF_BRUSH, MF_PARTS, MF_QUERIES = 6, MF_STATS = 6 };
typedef struct
{
	int		left, passes;
	GLuint	queries[MF_QUERIES];		// timestamps (begin, end), vertices, triangles, fragments, samples passed
	double	sum[MF_PARTS][MF_STATS];	// ms, vertices, triangles, fragments, samples, entities
} modelfragments_t;
static modelfragments_t modelfragments;

void R_ModelFragments_f (void)
{
	if (VR_GLESCompat ())
	{
		Con_Printf ("vr_model_fragments: not under -glescompat (OpenGL ES has no pipeline statistics or sample counts)\n");
		return;
	}
	if (modelfragments.queries[0])
		GL_DeleteQueriesFunc (MF_QUERIES, modelfragments.queries);
	memset (&modelfragments, 0, sizeof (modelfragments));
	GL_GenQueriesFunc (MF_QUERIES, modelfragments.queries);
	modelfragments.left = Cmd_Argc () > 1 ? q_max (1, atoi (Cmd_Argv (1))) : 16;
	Con_Printf ("vr_model_fragments: counting the next %d opaque model passes\n", modelfragments.left);
}

static void R_ModelFragmentsBegin (void)
{
	GLuint *q = modelfragments.queries;
	GL_QueryCounterFunc (q[0], GL_TIMESTAMP);
	GL_BeginQueryFunc (QVR_GL_VERTEX_SHADER_INVOCATIONS, q[2]);
	GL_BeginQueryFunc (QVR_GL_CLIPPING_INPUT_PRIMITIVES, q[3]);
	GL_BeginQueryFunc (QVR_GL_FRAGMENT_SHADER_INVOCATIONS, q[4]);
	GL_BeginQueryFunc (GL_SAMPLES_PASSED, q[5]);
}

static void R_ModelFragmentsEnd (int part, int count)
{
	GLuint64 v[MF_QUERIES] = {0};
	int i;
	GLuint *q = modelfragments.queries;
	GL_EndQueryFunc (QVR_GL_VERTEX_SHADER_INVOCATIONS);
	GL_EndQueryFunc (QVR_GL_CLIPPING_INPUT_PRIMITIVES);
	GL_EndQueryFunc (QVR_GL_FRAGMENT_SHADER_INVOCATIONS);
	GL_EndQueryFunc (GL_SAMPLES_PASSED);
	GL_QueryCounterFunc (q[1], GL_TIMESTAMP);
	for (i = 0; i < MF_QUERIES; i++)
		GL_GetQueryObjectui64vFunc (q[i], GL_QUERY_RESULT, &v[i]);
	modelfragments.sum[part][0] += (double)(v[1] - v[0]) * 1e-6;
	for (i = 2; i < MF_QUERIES; i++)
		modelfragments.sum[part][i - 1] += (double)v[i];
	modelfragments.sum[part][5] += count;
}

static void R_ModelFragmentsPassDone (void)
{
	int part;
	double n;
	modelfragments.passes++;
	if (--modelfragments.left > 0)
		return;
	n = modelfragments.passes;
	Con_Printf ("vr_model_fragments: %d passes; a pass's ms, vertices, triangles, fragments, samples kept, models:\n", modelfragments.passes);
	for (part = 0; part < MF_PARTS; part++)
	{
		const double *s = modelfragments.sum[part];
		if (s[5] <= 0.0)
			continue;
		Con_Printf ("vr_model_fragments: %-20s %.3f ms, %.0f, %.0f, %.0f, %.0f, %.1f\n",
			part == MF_WORLD ? "world models" : part == MF_BRUSH ? "world+brush" : r_zoned_player_parts[part], s[0] / n, s[1] / n, s[2] / n, s[3] / n, s[4] / n, s[5] / n);
	}
	Con_Printf ("vr_model_fragments: done\n");
	GL_DeleteQueriesFunc (MF_QUERIES, modelfragments.queries);
	modelfragments.queries[0] = 0;
}

/*
=============
R_DrawBrushModelsZoned -- QVR

R_DrawBrushModels, in profiling builds with vr_tracy_gpu_detail split into the world and the brush entities, each its
own GPU zone (drawn as one batch otherwise: the same draws, in the same order). Split, the brush entities' depth is not
in the world's depth pre-pass, so the world shades a few more pixels than unsplit: a detail capture's cost.
=============
*/
static void R_DrawBrushModelsZoned (entity_t **ents, int count)
{
	if (!vr_tracy_gpu_zones || count < 2 || ents[0] != &cl_entities[0])
	{
		R_DrawBrushModels (ents, count);
		return;
	}
	VR_GPUZoneBegin ("world");
	R_DrawBrushModels (ents, 1);
	VR_GPUZoneEnd ();
	VR_GPUZoneBegin ("brush entities");
	R_DrawBrushModels (ents + 1, count - 1);
	VR_GPUZoneEnd ();
}

/*
=============
R_DrawAliasModelsZoned -- QVR

R_DrawAliasModels, in profiling builds with vr_tracy_gpu_detail (the opaque pass only: the translucent one keeps its
sorted order) split into the monsters and the rest of the world's models, then the player's own (the hands, the body,
the gloves, what is held and holstered: VR_IsViewEntity), each its own GPU zone. Each part keeps its order.
=============
*/
static void R_DrawAliasModelsZoned (entity_t **ents, int count, qboolean alphapass)
{
	int i, world = 0, player = 0;
	const qboolean counting = modelfragments.left > 0 && !alphapass; // (vr_model_fragments)
	if ((!vr_tracy_gpu_zones || alphapass || count < 2) && !counting)
	{
		R_DrawAliasModels (ents, count);
		return;
	}
	for (i = 0; i < count; i++) // (copied: the sorted list is the other passes' too)
	{
		if (VR_IsViewEntity (ents[i]))
			r_zoned_player[player++] = ents[i];
		else
			r_zoned_world[world++] = ents[i];
	}
	VR_GPUZoneBegin ("alias: world models");
	if (counting)
		R_ModelFragmentsBegin ();
	R_DrawAliasModels (r_zoned_world, world);
	if (counting)
		R_ModelFragmentsEnd (MF_WORLD, world);
	VR_GPUZoneEnd ();
	VR_GPUZoneBegin ("alias: player models");
	for (i = 0; i < player; ) // its parts (VR_ViewEntityGroup), each run of one a zone of its own, in the drawn order
	{
		int group = VR_ViewEntityGroup (r_zoned_player[i]), j;
		for (j = i + 1; j < player && VR_ViewEntityGroup (r_zoned_player[j]) == group; j++)
			;
		group = group >= 0 && group < VR_DRAW_GROUPS ? group : VR_DRAW_OTHER;
		VR_GPUZoneBegin (r_zoned_player_parts[group]);
		if (counting)
			R_ModelFragmentsBegin ();
		R_DrawAliasModels (r_zoned_player + i, j - i);
		if (counting)
			R_ModelFragmentsEnd (group, j - i);
		VR_GPUZoneEnd ();
		i = j;
	}
	VR_GPUZoneEnd ();
}

