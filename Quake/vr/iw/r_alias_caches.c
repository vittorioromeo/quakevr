// QVR: the alias models' depth, eye and frame caches, frame lerp finish and pose history.
// Moved as it was out of r_alias.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

// QVR: a batch of posed models (R_AliasPosedJoin: the world's, vr_alias_batch_posed): each instance's entity, and the
// batch's bones, each instance's after the last's (its poses offset to them: R_FlushAliasInstances)
static const entity_t *aliasinstents[MAX_ALIAS_INSTANCES];
// QVR (C5f, QUEST_PLAN.md 6zq): your body under vr_gl_alias_body 2 (R_AliasLightFinishBody): its dynamic lights added flat
// to its light, Quake's way, for its form without them; the light as the per-pixel forms take it, for a batch drawn by
// another form (R_FlushAliasInstances)
static qboolean aliasinstflat[MAX_ALIAS_INSTANCES];
static vec3_t aliasinstperpixel[MAX_ALIAS_INSTANCES];
static qboolean aliaslightflat; // R_AliasLightFinish: add the dynamic lights whatever vr_dlight_models says
static qboolean aliasbodyflat; // the entity being added: its light has them added flat ...
static vec3_t aliasbodyperpixel; // ... and this is its light without them
static bonepose_t *aliasbatchbones;
static int aliasbatchbonescap;

// QVR: the layered shadow casters' (R_DrawAliasModelsDepthLayered): each batched instance's faces, and the shader's
// FaceBuffer (gl_shaders.h): the faces' view-projections, then a draw per instance and face (its index | face << 16)
static unsigned char aliasinstfaces[MAX_ALIAS_INSTANCES];

static struct {
	float		viewproj[6][16];
	uint32_t	draws[MAX_ALIAS_INSTANCES * 6];
} aliasfacebuf;

// QVR: the shadow maps' casters set up once a pass over the lights (R_AliasDepthCacheBegin/End, vr_lighting.cpp's
// VR_RenderShadowMaps): a caster in several lights had its lerp, matrices and zero blend worked out again for each.
// Within a pass nothing they read changes (the entity, the time, the hands), and the first set-up's lerp updates leave
// the entity as the next one finds it: the same numbers. Keyed by the entity (and its model and frame, against a copy
// in the same place); a full neighbourhood just sets it up again. Not the depth draws outside a pass, not cl.viewent
// (its gun offsets follow the camera's axes).
#define ALIASDEPTH_CACHE_SIZE	4096	// a power of two
#define ALIASDEPTH_CACHE_PROBES	16
// vr_shadow_setup_share (R_AliasDepthShare) 1 or 2 also keeps each caster's whole instance (its pose, matrix, zero
// blend, depth-only data, motion slots: the same for every light) and its swing rows (r_lerpmodels 3), copied into the
// other lights' batches; and the first eye's scene takes the pass's lerp and matrices for the same entities instead of
// setting them up again (R_AliasTransformShared: R_AliasTransform is the same for a light and an eye; 2 sets them up
// anyway and compares). 0: only the lerp and the matrix, as before.
typedef struct {
	const entity_t	*ent;
	const qmodel_t	*model;
	int			frame;
	int			gen;
	int			totalverts;
	int			zeroblend;
	lerpdata_t	lerpdata;
	float		model_matrix[16];
	float		bounds_matrix[16];
	int			instok;		// inst and the swing rows kept (vr_shadow_setup_share)
	const float	*boxbones;	// a posed caster's box (R_AliasPosedBox) and the bone matrices it is of (NULL: none)
	vec3_t		boxmins, boxmaxs;
	int			smoothfirst;	// its swing rows: where they are kept (VR_AnimSmoothKeep), how many, where they were in the batch
	int			smoothrows;
	int			smoothat;
	aliasinstance_t	inst;
} aliasdepthentry_t;
static aliasdepthentry_t	aliasdepthcache[ALIASDEPTH_CACHE_SIZE];
static int					aliasdepthgen; // the pass's (0: none)
static int					aliasdepthgens; // the last pass's
static int					aliasdepthhits, aliasdepthmisses; // the last passes' (R_AliasDepthCacheStats)
static int					aliasdepthshare; // vr_shadow_setup_share: 0 off, 1 on, 2 the first eye's checked
static int					aliasdepthkeep; // this pass keeps the casters' swing rows (none of the eyes' kept ones to clear)
static int					aliasdepthlast; // the frame's last pass's gen, for the first eye (R_AliasTransformShared); 0 none
static int					aliaseyemode; // (the eyes' cache's, below)
enum { ADS_TAKEN, ADS_CHECKED, ADS_BAD, ADS_NUMSTATS };
static int					aliasdepthsharestats[ADS_NUMSTATS];

/*
=================
R_AliasDepthCacheBegin / R_AliasDepthCacheEnd -- QVR

A pass over the shadow maps' lights (on: 0 sets each up every time, vr_shadow_layered_check's comparison).
=================
*/
void R_AliasDepthCacheBegin (qboolean on)
{
	if (++aliasdepthgens <= 0)
	{
		memset (aliasdepthcache, 0, sizeof (aliasdepthcache));
		aliasdepthgens = 1;
	}
	aliasdepthgen = on ? aliasdepthgens : 0;
	aliasdepthlast = 0;
	// the swing rows are kept in the store the eyes' cache keeps its own in (cleared as the first eye begins): kept here
	// only outside the eyes' modes (then cleared here: none kept), or in the first eye's (whose clearing came first)
	aliasdepthkeep = aliasdepthgen && aliasdepthshare && (aliaseyemode == 0 || aliaseyemode == 1);
	if (aliasdepthkeep && aliaseyemode == 0)
		VR_AnimSmoothKeepClear ();
}

void R_AliasDepthCacheEnd (void)
{
	aliasdepthlast = aliasdepthshare ? aliasdepthgen : 0;
	aliasdepthgen = 0;
	aliasdepthkeep = 0;
}

void R_AliasDepthShare (int mode)
{
	aliasdepthshare = mode < 0 ? 0 : mode > 2 ? 2 : mode;
}

void R_AliasDepthShareStats (int *taken, int *checked, int *bad)
{
	*taken = aliasdepthsharestats[ADS_TAKEN];
	*checked = aliasdepthsharestats[ADS_CHECKED];
	*bad = aliasdepthsharestats[ADS_BAD];
	memset (aliasdepthsharestats, 0, sizeof (aliasdepthsharestats));
}

void R_AliasDepthCacheStats (int *hits, int *misses)
{
	*hits = aliasdepthhits;
	*misses = aliasdepthmisses;
	aliasdepthhits = aliasdepthmisses = 0;
}

// e's entry this pass (*hit: already set up) or the slot to keep it in; NULL none (no pass, or no room)
static aliasdepthentry_t *R_AliasDepthSlot (const entity_t *e, qboolean *hit)
{
	uint64_t h;
	int i;
	*hit = false;
	if (!aliasdepthgen || e == &cl.viewent)
		return NULL;
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 12;
	for (i = 0; i < ALIASDEPTH_CACHE_PROBES; i++)
	{
		aliasdepthentry_t *s = &aliasdepthcache[(h + i) & (ALIASDEPTH_CACHE_SIZE - 1)];
		if (s->gen != aliasdepthgen)
		{
			s->gen = 0; // claimed below once set up
			return s;
		}
		if (s->ent == e)
		{
			if (s->model != e->model || s->frame != e->frame)
				return NULL;
			*hit = true;
			return s;
		}
	}
	return NULL;
}

// QVR: the second eye's set-up of each alias entity (vr_alias_eye_cache; R_AliasEyeCacheBegin, from the eyes' loop in
// vr_stereo.cpp): what of it does not depend on the eye, made in the first eye's scene and kept for the second's, which
// set it all up again from the same entity at the same time. Kept: the lerp and the transform (R_SetupAliasFrame,
// R_SetupEntityTransform, VR_AliasPreTransform/PostTransform), the light at its origin (R_LightPoint's, before the
// dynamic lights: those are culled by each eye's frustum), its vertex count and zero blend, its Quake VR instance data
// (VR_AliasInstance) and its smooth animation's (R_AliasSmoothInstance: its swing rows kept and added to the second
// eye's batch where it lands). Done per eye still: the portals (which side of a gate), the cull, the alpha, the dynamic
// lights and the light's minimums and clamp, the near-eye flags, the batching, and the view entities' instance data (a
// bare forearm takes the body's ambient light as last drawn: the first eye draws the hands before the body, the second
// after; the hands, weapons and body are few). Never: cl.viewent (its gun offsets
// follow the eye's axes), the views through teleporters, the shadow maps, r_showtris/r_showbboxes, the spectator's view,
// the drawn moves' log. Mode 2 (the check): the second eye sets everything up anew and compares (vr_alias_eye_cache_stats).
#define ALIASEYE_CACHE_SIZE		2048	// a power of two
#define ALIASEYE_CACHE_PROBES	16
typedef struct {
	const entity_t	*ent;
	const qmodel_t	*model;
	int			frame;
	int			skinnum;
	int			gen;
	int			lit;		// 0: the transform only (culled in the first eye); 1: all of it
	int			vrok;		// its instance data too (0: a view entity's, made per eye)
	lerpdata_t	lerpdata;
	float		model_matrix[16];
	float		bounds_matrix[16];
	vec3_t		light;		// R_LightPoint's
	int			totalverts;
	int			zeroblend;
	int			smoothfirst;	// its swing rows: where they are kept (VR_AnimSmoothKeep), how many, where they were in the batch
	int			smoothrows;
	int			smoothat;
	vraliasinstance_t	vr;
} aliaseyeentry_t;
static aliaseyeentry_t	aliaseyecache[ALIASEYE_CACHE_SIZE];
static int				aliaseyegen;	// the frame's (0: none)
static int				aliaseyegens;	// the last frame's
// aliaseyemode (declared above, with the shadow casters' cache): 0 off, 1 the first eye (kept), 2 the second (reused), 3
// the second (checked)

// vr_alias_eye_cache_stats: what the second eyes found since the last call; the check's mismatches by part
enum { AEYE_HIT, AEYE_TRANSFORM_ONLY, AEYE_MISS, AEYE_CHECKED, AEYE_BAD_LERP, AEYE_BAD_MATRIX, AEYE_BAD_LIGHT,
	AEYE_BAD_COUNTS, AEYE_BAD_VR, AEYE_BAD_SMOOTH, AEYE_NUMSTATS };
static int				aliaseyestats[AEYE_NUMSTATS];

void R_AliasEyeCacheBegin (int mode)
{
	if (mode == 1)
	{
		if (++aliaseyegens <= 0)
		{
			memset (aliaseyecache, 0, sizeof (aliaseyecache));
			aliaseyegens = 1;
		}
		aliaseyegen = aliaseyegens;
		VR_AnimSmoothKeepClear ();
	}
	aliaseyemode = aliaseyegen ? mode : 0; // (0 between the eyes: the frame's kept)
}

void R_AliasEyeCacheEnd (void)
{
	aliaseyemode = 0;
	aliaseyegen = 0;
	aliasdepthlast = 0; // QVR: (the frame's shadow casters' set-ups: R_AliasTransformShared)
}

void R_AliasEyeCacheStats (int *out, int count)
{
	int i;
	for (i = 0; i < count && i < AEYE_NUMSTATS; i++)
		out[i] = aliaseyestats[i];
	memset (aliaseyestats, 0, sizeof (aliaseyestats));
}

// e's entry this frame (*found: the first eye's), the slot to keep it in (the first eye), or NULL (not kept)
static aliaseyeentry_t *R_AliasEyeSlot (entity_t *e, qboolean *found)
{
	uint64_t h;
	int i;
	*found = false;
	if (!aliaseyemode || e == &cl.viewent || VR_PortalDrawing () || VR_DebugDrawnOn ())
		return NULL;
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 11;
	for (i = 0; i < ALIASEYE_CACHE_PROBES; i++)
	{
		aliaseyeentry_t *s = &aliaseyecache[(h + i) & (ALIASEYE_CACHE_SIZE - 1)];
		if (s->gen != aliaseyegen)
		{
			if (aliaseyemode != 1)
				break;
			s->gen = 0; // claimed below once set up
			return s;
		}
		if (s->ent == e)
		{
			if (s->model != e->model || s->frame != e->frame || s->skinnum != e->skinnum)
				return NULL;
			*found = true;
			return s;
		}
	}
	if (aliaseyemode != 1)
		aliaseyestats[AEYE_MISS]++;
	return NULL;
}

// QVR (questperf2): the alias draw-list cache across frames (vr_alias_frame_cache): an entity's Quake VR instance data
// (VR_AliasInstance: its light's direction and ambient cube, surface, glows, wounds, retro, ...) kept from the frame
// before when nothing it is made from changed: the same model, frame, skin, alpha, effects and colormap, the same
// transform (bit for bit), the same light styles and dynamic lights, and it was set up in the frame before. Made anew:
// your own models (view entities: hands, body, gear follow you), cl.viewent, the views through teleporters, the shadow
// maps' casters, anything glowing (the force grab's and the weapons' glows pulse) or with a morph. Mode 2 (the check):
// made anew and compared (vr_alias_frame_cache_stats).
#define ALIASFRAME_CACHE_SIZE	2048	// a power of two
#define ALIASFRAME_CACHE_PROBES	16
typedef struct {
	const entity_t	*ent;
	const qmodel_t	*model;
	int			frame;
	int			skinnum;
	int			alpha;
	int			effects;
	const byte	*colormap;
	int			seen;		// host_framecount it was last made or used in (0: free)
	unsigned	lightkey;	// aliasframelightkey when made
	float		matrix[16];
	vraliasinstance_t	vr;
} aliasframeentry_t;
static aliasframeentry_t	aliasframecache[ALIASFRAME_CACHE_SIZE]; // seat-shared: keyed by entity pointer (each seat's client has its own entities) and host frame
static unsigned			aliasframelightkey;		// the light styles' and dynamic lights' hash this frame // seat-shared: the frame's light hash
static int				aliasframelightframe = -1;	// the host frame it was taken in // seat-shared: the frame's light hash
enum { AFRAME_HIT, AFRAME_MADE, AFRAME_CHECKED, AFRAME_BAD, AFRAME_NUMSTATS };
static int				aliasframestats[AFRAME_NUMSTATS]; // seat-shared: the cache's stats

void R_AliasFrameCacheStats (int *out, int count)
{
	int i;
	for (i = 0; i < count && i < AFRAME_NUMSTATS; i++)
		out[i] = aliasframestats[i];
	memset (aliasframestats, 0, sizeof (aliasframestats));
}

static unsigned R_AliasFrameLightKey (void)
{
	if (aliasframelightframe != host_framecount)
	{
		unsigned h = 2166136261u;
		int i;
		for (i = 0; i < r_framedata.numlights; i++) // the dynamic lights (not the map lights' shadow entries): place, reach, colour
		{
			const unsigned *l = (const unsigned *) &r_lightbuffer.lights[i];
			int k;
			if (r_lightbuffer.lights[i].shadow[3] != 0.f)
				continue;
			for (k = 0; k < 8; k++) // (pos, radius, color, minlight)
				h = (h ^ l[k]) * 16777619u;
		}
		aliasframelightkey = h;
		aliasframelightframe = host_framecount;
	}
	return aliasframelightkey;
}

// VR_AliasInstance for e, from the frame before when it can be (vr_alias_frame_cache): true when *out was filled
static qboolean R_AliasFrameInstance (entity_t *e, const float model_matrix[16], aliashdr_t *hdr, vraliasinstance_t *out)
{
	const int mode = VR_AliasFrameCacheMode ();
	aliasframeentry_t *s = NULL;
	uint64_t h;
	unsigned key;
	int i;
	if (!mode || e == &cl.viewent || VR_IsViewEntity (e) || VR_PortalDrawing () || VR_DebugDrawnOn ())
		return false;
	key = R_AliasFrameLightKey ();
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 11;
	for (i = 0; i < ALIASFRAME_CACHE_PROBES; i++)
	{
		aliasframeentry_t *c = &aliasframecache[(h + i) & (ALIASFRAME_CACHE_SIZE - 1)];
		if (c->ent == e || (!s && (c->seen == 0 || c->seen < host_framecount - 1)))
			s = c;
		if (c->ent == e)
			break;
	}
	if (!s)
		return false;
	if (s->ent == e && s->seen >= host_framecount - 1 && s->model == e->model && s->frame == e->frame
		&& s->skinnum == e->skinnum && s->alpha == e->alpha && s->effects == e->effects && s->colormap == e->colormap
		&& (s->lightkey == key || mode == 2) && !memcmp (s->matrix, model_matrix, sizeof (s->matrix)))
	{
		s->seen = host_framecount;
		if (mode == 1)
		{
			*out = s->vr;
			aliasframestats[AFRAME_HIT]++;
			return true;
		}
		VR_AliasInstance (e, model_matrix, hdr, 1, out); // the check
		aliasframestats[AFRAME_CHECKED]++;
		if (memcmp (out, &s->vr, sizeof (*out)))
		{
			const float *f = (const float *) out, *k = (const float *) &s->vr;
			int n;
			for (n = 0; n < (int) (sizeof (*out) / sizeof (float)) && !memcmp (f + n, k + n, sizeof (float)); n++)
				;
			if (aliasframestats[AFRAME_BAD]++ < 8)
				Con_Printf ("vr_alias_frame_cache check: %s's instance data differs from float %d (%g, kept %g)\n",
					e->model->name, n, f[n], k[n]);
		}
		s->vr = *out;
		return true;
	}
	VR_AliasInstance (e, model_matrix, hdr, 1, out);
	aliasframestats[AFRAME_MADE]++;
	if (out->glow[0] != 0.f || out->ambient[2][3] != 0.f) // glowing or morphing (VR_AliasMorph): made each frame
	{
		s->ent = NULL;
		s->seen = 0;
		return true;
	}
	s->ent = e;
	s->model = e->model;
	s->frame = e->frame;
	s->skinnum = e->skinnum;
	s->alpha = e->alpha;
	s->effects = e->effects;
	s->colormap = e->colormap;
	s->seen = host_framecount;
	s->lightkey = key;
	memcpy (s->matrix, model_matrix, sizeof (s->matrix));
	s->vr = *out;
	return true;
}

// The check (mode 3): a part set up anew against the first eye's
static void R_AliasEyeCompare (const void *a, const void *b, size_t size, int stat)
{
	if (memcmp (a, b, size))
		aliaseyestats[stat]++;
}

/*
=================
R_FrameLerpFinish -- QVR: when the frame lerp of an entity whose messages carry a lerpfinish ends: as the message that
changed its pose said, not as the latest says. (Each message sends the time to its next think again, rounded to 1/255 s:
the lerp's end wandered by a few milliseconds from message to message, and the pose went back and forth by as much, in slow
motion several frames' worth: a knocked-down monster's get-up jittered in bullet time.) A lerp begun by a message
without one ends as it began, lerptime after its start: a later lerpfinish (a think begun out of turn: a leaping dog
landing) stretched it, and the pose was drawn back along it (ROUND26.md, "doglerp").
=================
*/
float R_FrameLerpFinish (const entity_t *e)
{
	return e->animlerpfinish > e->lerpstart ? e->animlerpfinish : e->lerpstart + e->lerptime;
}

/*
=================
R_AnimHistoryPush -- QVR: r_lerpmodels 3 (vr/vr_animsmooth.cpp): a new lerp starts from e's current pose: the lerp
that ends here (animfrom to currentpose) becomes the history its spline curves in from, if it ran about its time up to
now; one cut short by this one (Quake jumps to its end: never drawn at that speed) or that ended a while ago (the pose
held, the model out of sight) is none: the new lerp starts from rest. Called before lerpstart moves on.
=================
*/
static void R_AnimHistoryPush (entity_t *e)
{
	float since = (float)(cl.time - e->lerpstart);
	float span = e->animlerpfinish > e->lerpstart ? e->animlerpfinish - e->lerpstart : e->lerptime;
	if (e->lerpstart > 0.f && since >= span * 0.5f && since <= span * 1.6f + 0.02f && e->animfrom != e->currentpose)
	{
		e->animpose0 = e->animfrom;
		e->animspan0 = since;
	}
	else
	{
		e->animpose0 = e->currentpose;
		e->animspan0 = 0.f;
	}
	e->animfrom = e->currentpose;
}

