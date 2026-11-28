// QVR: the extension builtins Quake VR adds (bitshift, strncasecmp, surface queries, particle effects...).
// Moved as it was out of pr_cmds.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
===============================================================================

QVR: AD support: the DP/FTE builtins Arcane Dimensions' QuakeC imports by name (`= #0`, QC/ad/ad_builtins.qc) that
the engine had none of (PR_WarnUnboundBuiltins named them; each returned 0). AD reaches them only on its extension
path (ad_world.qc: VR_AD_Extensions, the bridge's FALSE today, and checkextension denies every extension AD asks
for), so in Quake VR they change nothing yet; they follow DP's/FTE's semantics for any progs that calls them.
`addstat` is DP's name for clientstat (#232, bound by name only: the table's numbers are unique).

===============================================================================
*/

// float(float number, float quantity) bitshift = #218 (EXT_BITSHIFT, FTE's semantics): `number` truncated to an
// int, shifted left by `quantity` (right, arithmetic, when negative). A shift of 32 or more gives 0 (-1 when a
// negative number shifts right) instead of C's undefined behaviour.
static void PF_bitshift (void)
{
	const int number = (int)G_FLOAT(OFS_PARM0);
	const int quantity = (int)G_FLOAT(OFS_PARM1);
	int result;
	if (quantity >= 32)
		result = 0;
	else if (quantity >= 0)
		result = (int)((unsigned int)number << quantity);
	else if (quantity > -32)
		result = number >> -quantity;
	else
		result = number < 0 ? -1 : 0;
	G_FLOAT(OFS_RETURN) = result;
}

// float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs) strncasecmp = #230
// (FTE_STRINGS): compares at most `len` characters ignoring ASCII case, from the optional offsets (clamped to the
// strings); -1, 0 or 1. AD: its sky test (ad_subs_soc.qc ad_check_skycontent) matches "SKY" texture names.
static void PF_strncasecmp (void)
{
	const char *s1 = G_STRING(OFS_PARM0);
	const char *s2 = G_STRING(OFS_PARM1);
	const int len = (int)G_FLOAT(OFS_PARM2);
	int r;
	if (qcvm->argc > 3)
	{
		const int ofs = (int)G_FLOAT(OFS_PARM3);
		const int n = (int)strlen(s1);
		s1 += ofs < 0 ? 0 : ofs > n ? n : ofs;
	}
	if (qcvm->argc > 4)
	{
		const int ofs = (int)G_FLOAT(OFS_PARM4);
		const int n = (int)strlen(s2);
		s2 += ofs < 0 ? 0 : ofs > n ? n : ofs;
	}
	r = len > 0 ? q_strncasecmp(s1, s2, (size_t)len) : 0;
	G_FLOAT(OFS_RETURN) = r < 0 ? -1 : r > 0 ? 1 : 0;
}

// The brush model an entity's surface queries look at (DP's getmodel): NULL when it has none.
static qmodel_t *PF_SurfaceModel (edict_t *ed)
{
	const int index = (int)ed->v.modelindex;
	qmodel_t *mod;
	if (ed->free || index <= 0 || index >= MAX_MODELS)
		return NULL;
	mod = sv.models[index];
	return (mod && mod->type == mod_brush && mod->surfaces) ? mod : NULL;
}

// A brush model surface's vertex `i` (its edge loop, model->surfedges).
static const float *PF_SurfaceVertex (const qmodel_t *mod, const msurface_t *surf, int i)
{
	const int lindex = mod->surfedges[surf->firstedge + i];
	const medge_t *edge = &mod->edges[lindex >= 0 ? lindex : -lindex];
	return mod->vertexes[lindex >= 0 ? edge->v[0] : edge->v[1]].position;
}

// The squared distance from p to the surface's polygon (convex, on its plane): to the projection when it lies
// inside every edge, else to the nearest edge segment.
static float PF_SurfaceDistance2 (const qmodel_t *mod, const msurface_t *surf, const vec3_t p)
{
	const float *n = surf->plane->normal;
	const float planedist = DotProduct(p, n) - surf->plane->dist;
	vec3_t q, e, d, c;
	float best = 1e30f;
	qboolean inside = true;
	int i, sign = 0;
	VectorMA(p, -planedist, n, q);
	for (i = 0; i < surf->numedges; i++)
	{
		const float *a = PF_SurfaceVertex(mod, surf, i);
		const float *b = PF_SurfaceVertex(mod, surf, (i + 1) % surf->numedges);
		float t, len2, dist2, side;
		VectorSubtract(b, a, e);
		VectorSubtract(q, a, d);
		CrossProduct(e, d, c);
		side = DotProduct(c, n);
		if (side > 0.001f || side < -0.001f)
		{
			const int s = side > 0 ? 1 : -1;
			if (sign && s != sign)
				inside = false;
			sign = s;
		}
		VectorSubtract(p, a, d); // nearest point of the edge segment to p
		len2 = DotProduct(e, e);
		t = len2 > 0 ? DotProduct(d, e) / len2 : 0;
		t = t < 0 ? 0 : t > 1 ? 1 : t;
		VectorMA(a, t, e, c);
		VectorSubtract(p, c, d);
		dist2 = DotProduct(d, d);
		if (dist2 < best)
			best = dist2;
	}
	return inside ? planedist * planedist : best;
}

// float(entity e, vector p) getsurfacenearpoint = #438 (DP_QC_GETSURFACE, DP's semantics): the index of the
// surface of e's brush model nearest to p (p made relative to e's origin; no rotation, as DP), -1 when e has no
// brush model. AD: its sky test (ad_check_skycontent) on world.
static void PF_getsurfacenearpoint (void)
{
	edict_t *ed = G_EDICT(OFS_PARM0);
	const float *point = G_VECTOR(OFS_PARM1);
	qmodel_t *mod = PF_SurfaceModel(ed);
	vec3_t p, clipped;
	float bestdist = 1e30f;
	int i, j, best = -1;
	G_FLOAT(OFS_RETURN) = -1;
	if (!mod)
		return;
	VectorSubtract(point, ed->v.origin, p);
	for (i = 0; i < mod->nummodelsurfaces; i++)
	{
		const msurface_t *surf = &mod->surfaces[mod->firstmodelsurface + i];
		float dist;
		for (j = 0; j < 3; j++) // the surface's box first: skip it when even that is farther
			clipped[j] = (p[j] < surf->mins[j] ? surf->mins[j] : p[j] > surf->maxs[j] ? surf->maxs[j] : p[j]) - p[j];
		if (DotProduct(clipped, clipped) >= bestdist)
			continue;
		dist = PF_SurfaceDistance2(mod, surf, p);
		if (dist < bestdist)
		{
			bestdist = dist;
			best = i;
		}
	}
	G_FLOAT(OFS_RETURN) = best;
}

// string(entity e, float s) getsurfacetexture = #437 (DP_QC_GETSURFACE): the texture name of surface s of e's brush
// model (as getsurfacenearpoint numbers them), "" when there is none.
static void PF_getsurfacetexture (void)
{
	qmodel_t *mod = PF_SurfaceModel(G_EDICT(OFS_PARM0));
	const int s = (int)G_FLOAT(OFS_PARM1);
	const msurface_t *surf;
	char *out;
	G_INT(OFS_RETURN) = 0;
	if (!mod || s < 0 || s >= mod->nummodelsurfaces)
		return;
	surf = &mod->surfaces[mod->firstmodelsurface + s];
	if (!surf->texinfo || surf->texinfo->texnum < 0 || surf->texinfo->texnum >= mod->numtextures
		|| !mod->textures[surf->texinfo->texnum])
		return;
	out = PR_GetTempString();
	q_strlcpy(out, mod->textures[surf->texinfo->texnum]->name, STRINGTEMP_LENGTH);
	G_INT(OFS_RETURN) = PR_SetEngineString(out);
}

// The named effects particleeffectnum knows: DP's built-in names for the classic temp entities (its fallback when
// effectinfo.txt has no such effect), drawn as those temp entities' particles without their sounds (DP's
// pointparticles plays none). Quake VR has no effectinfo.txt: any other name (AD's TE_PLASMA, TR_BLOOD, DPP_ITS...)
// is effect 0, which pointparticles ignores. An impact's count is its classic particle count per pointparticles
// count; count 255 is the client's particle explosion (R_ParseParticleEffect).
typedef struct
{
	const char *name;
	int tempentity;		// a svc_temp_entity type, or -1: a svc_particle burst
	int color, count;
} pf_particleeffect_t;
static const pf_particleeffect_t pf_particleeffects[] =
{
	{"TE_EXPLOSION",	-1,				0,		255},
	{"TE_GUNSHOT",		-1,				0,		20},
	{"TE_SPIKE",		-1,				0,		10},
	{"TE_SUPERSPIKE",	-1,				0,		20},
	{"TE_WIZSPIKE",		-1,				20,		30},
	{"TE_KNIGHTSPIKE",	-1,				226,	20},
	{"TE_LAVASPLASH",	TE_LAVASPLASH,	0,		0},
	{"TE_TELEPORT",		TE_TELEPORT,	0,		0},
};

// float(string effectname) particleeffectnum = #335 (DP_SV_POINTPARTICLES): the effect's number, 0 when unknown.
static void PF_particleeffectnum (void)
{
	const char *name = G_STRING(OFS_PARM0);
	int i;
	G_FLOAT(OFS_RETURN) = 0;
	for (i = 0; i < (int)Q_COUNTOF(pf_particleeffects); i++)
	{
		if (!q_strcasecmp(name, pf_particleeffects[i].name))
		{
			G_FLOAT(OFS_RETURN) = i + 1;
			return;
		}
	}
}

// Room for one more message in this frame's datagram (SV_StartParticle's margin; a coord is up to 4 bytes).
static qboolean PF_DatagramRoom (int bytes)
{
	return sv.datagram.cursize + bytes <= MAX_DATAGRAM;
}

// void(float effectnum, vector origin, optional vector dir, optional float count) pointparticles = #337
// (DP_SV_POINTPARTICLES): effect `effectnum` (particleeffectnum's) at origin, sent to every client. dir (default
// '0 0 0') is an impact's particle velocity; count (default 1, 0 or less: nothing) multiplies an impact's particles.
static void PF_pointparticles (void)
{
	const int effect = (int)G_FLOAT(OFS_PARM0);
	const float *org = G_VECTOR(OFS_PARM1);
	const float count = (qcvm->argc > 3) ? G_FLOAT(OFS_PARM3) : 1;
	vec3_t pos, dir = {0, 0, 0};
	const pf_particleeffect_t *fx;
	if (effect < 1 || effect > (int)Q_COUNTOF(pf_particleeffects) || count <= 0)
		return;
	if (qcvm->argc > 2)
		VectorCopy(G_VECTOR(OFS_PARM2), dir);
	VectorCopy(org, pos);
	fx = &pf_particleeffects[effect - 1];
	if (fx->tempentity < 0)
	{
		int n = fx->count;
		if (n != 255)
			n = CLAMP(1, (int)(n * count), 254); // (255 is the explosion)
		SV_StartParticle(pos, dir, fx->color, n);
		return;
	}
	if (!PF_DatagramRoom(2 + 3 * 4))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, fx->tempentity);
	MSG_WriteCoord(&sv.datagram, pos[0], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, pos[1], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, pos[2], sv.protocolflags);
	VR_BroadcastMessageEnd(); // (a boundary, as SV_StartParticle's: vr/vr_server.cpp)
}

// void(vector org, float color, float colorlength) te_explosion2 = #427 (DP_TE_EXPLOSION2): Quake's TE_EXPLOSION2
// (Hipnotic's colour-mapped explosion: particles of palette colours color .. color+colorlength-1, a light and the
// explosion sound). AD: the mammoth boss's explosions (ad_mon_bossmammoth.qc).
static void PF_te_explosion2 (void)
{
	const float *org = G_VECTOR(OFS_PARM0);
	if (!PF_DatagramRoom(2 + 3 * 4 + 2))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION2);
	MSG_WriteCoord(&sv.datagram, org[0], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, org[1], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, org[2], sv.protocolflags);
	MSG_WriteByte(&sv.datagram, CLAMP(0, (int)G_FLOAT(OFS_PARM1), 255));
	MSG_WriteByte(&sv.datagram, CLAMP(1, (int)G_FLOAT(OFS_PARM2), 255)); // (the client takes colorMod % colorLength)
	VR_BroadcastMessageEnd();
}

// void(vector mins, vector maxs, vector vel, float count, float color) te_particlerain = #409 /
// te_particlesnow = #410 (DP_TE_PARTICLERAIN / DP_TE_PARTICLESNOW): a documented no-op. DP sends its own temp
// entities (64, 65) that this engine's client and protocol don't have (an unknown type is a client Host_Error), and
// svc_particle can't carry rain's speed (about 120 units/s at most). AD's weather (ad_part_generate.qc) needs both
// extensions, which checkextension denies, so it never calls them.
static void PF_te_particleweather (void)
{
}

