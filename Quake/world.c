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
// world.c -- world query functions

#include "quakedef.h"
#include "vr/vr_api.h"

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/


typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
	qboolean	hitgibs;		// QVR: MOVE_HITGIBS, or a missile
	float		hitmodel;		// QVR: MOVE_HITMODEL: its class's tolerance (-1: not precise, the boxes)
	int			hittype;		// QVR: (its type, with the class)
	const float	*portalturned;	// QVR: with portalplane: the box is turned (SV_MovePortalTurned)
	qboolean	bspbox;			// QVR: the player's narrower box against BSP models (vr_hull_width; vr/vr_hull.cpp)
	vec3_t		bspmins, bspmaxs;
	qboolean	entbox;			// QVR: ... and against other entities' boxes (vr_hull_ent_width)
	vec3_t		entmins, entmaxs;
	const float *portalplane; // clipped portion of a player straddling a gate
	qboolean	bodymove;		// QVR: a body's move (a box, not a shot): a player it meets shows it the narrower box
	qboolean	hitpass;		// QVR: MOVE_HITPASS: the pass entity itself is met (a shot's path come out of a teleporter)
	qboolean	hitowner;		// QVR: ... and its owner: that, or a missile carried through a teleporter (VR_PortalHitsOwner)
} moveclip_t;


int SV_HullPointContents (hull_t *hull, int num, vec3_t p);

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull;
static	mclipnode_t	box_clipnodes[6]; //johnfitz -- was dclipnode_t
static	mplane_t	box_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull (void)
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i=0 ; i<6 ; i++)
	{
		box_clipnodes[i].planenum = i;

		side = i&1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;

		box_planes[i].type = i>>1;
		box_planes[i].normal[i>>1] = 1;
	}

}


/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t	*SV_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}



/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity (edict_t *ent, vec3_t mins, vec3_t maxs, vec3_t offset)
{
	qmodel_t	*model;
	vec3_t		size;
	vec3_t		hullmins, hullmaxs;
	hull_t		*hull;

// decide which clipping hull to use, based on the size
	if (ent->v.solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		if (ent->v.movetype != MOVETYPE_PUSH)
			Host_Error ("SOLID_BSP without MOVETYPE_PUSH (%s at %f %f %f)",
				    PR_GetString(ent->v.classname), ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);

		model = sv.models[ (int)ent->v.modelindex ];

		if (!model || model->type != mod_brush)
			Host_Error ("SOLID_BSP with a non bsp model (%s at %f %f %f)",
				    PR_GetString(ent->v.classname), ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);

		VectorSubtract (maxs, mins, size);
		if (size[0] < 3)
			hull = &model->hulls[0];
		else if (size[0] <= 32)
			hull = &model->hulls[1];
		else
			hull = &model->hulls[2];

// calculate an offset value to center the origin
		VectorSubtract (hull->clip_mins, mins, offset);
		VectorAdd (offset, ent->v.origin, offset);
	}
	else
	{	// create a temp hull from bounding box sizes

		VectorSubtract (ent->v.mins, maxs, hullmins);
		VectorSubtract (ent->v.maxs, mins, hullmaxs);
		hull = SV_HullForBox (hullmins, hullmaxs);

		VectorCopy (ent->v.origin, offset);
	}


	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

// Note: changing this can affect droptofloor
// QVR: deeper (was 4: 16 leaves for a whole map), and loose: a node's children overlap by AREA_LOOSE units round its
// split, so an edict less than 2 * AREA_LOOSE across goes on down to a leaf rather than staying at the node whose split
// it crosses. With hundreds of props in a pile (Box3D's), every trace and touch near the pile walked all of them (the
// pile's node, and the straddlers of every split above it); now only the few cells round the box. The edicts each finds
// are the same (the order in which they are found differs).
#define	AREA_DEPTH	8
#define	AREA_NODES	(2<<AREA_DEPTH)
#define	AREA_LOOSE	32

static	areanode_t	sv_areanodes[AREA_NODES];
static	int			sv_numareanodes;

// QVR: each node's linked edicts (both lists) in an array as well, in no order (the last one moved into a removed one's
// place: edict_t's areanode, areaslot): SV_AreaEdictsUnordered reads their boxes without chasing the links one by one
// (each link a load waiting on the last). The same edicts as the lists, so the same ones found; in another order.
// Each slot also keeps the edict's abs box as it was linked (boxes): SV_AreaRadiusCandidates reads those, not the edicts.
typedef struct
{
	edict_t	**ents;
	float	(*boxes)[6];	// [space] absmin, absmax when linked
	int		count, space;
} areaarray_t;
static	areaarray_t	sv_areaarrays[AREA_NODES];

static void SV_AreaArrayAdd (edict_t *ent, int node)
{
	areaarray_t *a = &sv_areaarrays[node];
	if (a->count == a->space)
	{
		a->space = a->space ? a->space * 2 : 64;
		a->ents = (edict_t **) VR_HeapRealloc (a->ents, a->space * sizeof (edict_t *));
		a->boxes = (float (*)[6]) VR_HeapRealloc (a->boxes, a->space * sizeof (a->boxes[0]));
		if (!a->ents || !a->boxes)
			Sys_Error ("SV_AreaArrayAdd: out of memory");
	}
	ent->areanode = node;
	ent->areaslot = a->count;
	VectorCopy (ent->v.absmin, a->boxes[a->count]);
	VectorCopy (ent->v.absmax, a->boxes[a->count] + 3);
	a->ents[a->count++] = ent;
}

static void SV_AreaArrayRemove (edict_t *ent)
{
	areaarray_t *a;
	edict_t *last;
	if (ent->areanode < 0 || ent->areanode >= AREA_NODES)
		return;
	a = &sv_areaarrays[ent->areanode];
	if (ent->areaslot < 0 || ent->areaslot >= a->count || a->ents[ent->areaslot] != ent)
		return; // (not in it: linked before the world was cleared)
	last = a->ents[--a->count];
	a->ents[ent->areaslot] = last;
	memcpy (a->boxes[ent->areaslot], a->boxes[a->count], sizeof (a->boxes[0]));
	last->areaslot = ent->areaslot;
}

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);

	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);
	VectorCopy (mins, mins2);
	VectorCopy (maxs, maxs1);
	VectorCopy (maxs, maxs2);

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	SV_InitBoxHull ();

	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	for (int i = 0; i < AREA_NODES; i++) // QVR: (their memory kept)
		sv_areaarrays[i].count = 0;
	VR_FindRadius_Reset (); // QVR: nothing linked: findradius tests every edict until it is linked again
	SV_CreateAreaNode (0, sv.worldmodel->mins, sv.worldmodel->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (edict_t *ent)
{
	if (!ent->area.prev)
		return;		// not linked in anywhere
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
	SV_AreaArrayRemove (ent); // QVR
	VR_FindRadius_Dirty (ent); // QVR: out of the grid (findradius tests it itself until it is linked again)
}


/*
====================
SV_AreaTriggerEdicts

Spike -- just builds a list of entities within the area, rather than walking
them and risking the list getting corrupt.
====================
*/
static void
SV_AreaTriggerEdicts ( edict_t *ent, areanode_t *node, edict_t **list, int *listcount, const int listspace )
{
	link_t		*l, *next;
	edict_t		*touch;

// touch linked edicts
	for (l = node->trigger_edicts.next ; l != &node->trigger_edicts ; l = next)
	{
		next = l->next;
		touch = EDICT_FROM_AREA(l);
		if (touch == ent)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
		|| ent->v.absmin[1] > touch->v.absmax[1]
		|| ent->v.absmin[2] > touch->v.absmax[2]
		|| ent->v.absmax[0] < touch->v.absmin[0]
		|| ent->v.absmax[1] < touch->v.absmin[1]
		|| ent->v.absmax[2] < touch->v.absmin[2] )
			continue;

		if (*listcount == listspace)
			return; // should never happen

		list[*listcount] = touch;
		(*listcount)++;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if ( ent->v.absmax[node->axis] > node->dist - AREA_LOOSE ) // QVR: loose (AREA_LOOSE)
		SV_AreaTriggerEdicts ( ent, node->children[0], list, listcount, listspace );
	if ( ent->v.absmin[node->axis] < node->dist + AREA_LOOSE )
		SV_AreaTriggerEdicts ( ent, node->children[1], list, listcount, listspace );
}

/*
====================
SV_AreaEdicts

QVR: appends to `list` the linked edicts, triggers and solids alike, whose boxes touch mins..maxs (edges included),
for VR_TouchLinks (vr/vr_physics.cpp): a walk down the area nodes instead of a scan of every edict.
====================
*/
static void SV_AreaEdictsR (areanode_t *node, const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	link_t		*l, *start;
	edict_t		*touch;
	int			i;

	for (i = 0; i < 2; i++)
	{
		start = i ? &node->solid_edicts : &node->trigger_edicts;
		for (l = start->next ; l != start ; l = l->next)
		{
			touch = EDICT_FROM_AREA(l);
			if (mins[0] > touch->v.absmax[0] || mins[1] > touch->v.absmax[1] || mins[2] > touch->v.absmax[2]
			|| maxs[0] < touch->v.absmin[0] || maxs[1] < touch->v.absmin[1] || maxs[2] < touch->v.absmin[2])
				continue;
			if (*listcount == listspace)
				return;
			list[(*listcount)++] = touch;
		}
	}

	if (node->axis == -1)
		return;
	if (maxs[node->axis] >= node->dist - AREA_LOOSE) // QVR: loose (AREA_LOOSE)
		SV_AreaEdictsR (node->children[0], mins, maxs, list, listcount, listspace);
	if (mins[node->axis] <= node->dist + AREA_LOOSE)
		SV_AreaEdictsR (node->children[1], mins, maxs, list, listcount, listspace);
}

void SV_AreaEdicts (const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	SV_AreaEdictsR (sv_areanodes, mins, maxs, list, listcount, listspace);
}

/*
====================
SV_AreaEdictsUnordered

QVR: SV_AreaEdicts' edicts in no particular order (the nodes' arrays: sv_areaarrays), for VR_TouchLinks, which puts
them in edict order.
====================
*/
static void SV_AreaEdictsUnorderedR (areanode_t *node, const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	const areaarray_t *a = &sv_areaarrays[node - sv_areanodes];
	edict_t		*touch;
	int			i;

	for (i = 0; i < a->count; i++)
	{
		touch = a->ents[i];
		if (mins[0] > touch->v.absmax[0] || mins[1] > touch->v.absmax[1] || mins[2] > touch->v.absmax[2]
		|| maxs[0] < touch->v.absmin[0] || maxs[1] < touch->v.absmin[1] || maxs[2] < touch->v.absmin[2])
			continue;
		if (*listcount == listspace)
			return;
		list[(*listcount)++] = touch;
	}

	if (node->axis == -1)
		return;
	if (maxs[node->axis] >= node->dist - AREA_LOOSE)
		SV_AreaEdictsUnorderedR (node->children[0], mins, maxs, list, listcount, listspace);
	if (mins[node->axis] <= node->dist + AREA_LOOSE)
		SV_AreaEdictsUnorderedR (node->children[1], mins, maxs, list, listcount, listspace);
}

void SV_AreaEdictsUnordered (const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	SV_AreaEdictsUnorderedR (sv_areanodes, mins, maxs, list, listcount, listspace);
}

/*
====================
SV_AreaRadiusCandidates

QVR: the linked edicts whose boxes as they were linked touch mins..maxs, in no order, for findradius
(vr/vr_findradius.cpp): the same walk as SV_AreaEdictsUnordered, but the boxes are read from the nodes' arrays, not
from the edicts (each edict is kilobytes away from the next: a page each). An edict changed since it was linked is
findradius's to test itself.
====================
*/
static void SV_AreaRadiusCandidatesR (areanode_t *node, const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	const areaarray_t *a = &sv_areaarrays[node - sv_areanodes];
	int			i;

	for (i = 0; i < a->count; i++)
	{
		const float *b = a->boxes[i];
		if (mins[0] > b[3] || mins[1] > b[4] || mins[2] > b[5] || maxs[0] < b[0] || maxs[1] < b[1] || maxs[2] < b[2])
			continue;
		if (*listcount == listspace)
			return;
		list[(*listcount)++] = a->ents[i];
	}

	if (node->axis == -1)
		return;
	if (maxs[node->axis] >= node->dist - AREA_LOOSE)
		SV_AreaRadiusCandidatesR (node->children[0], mins, maxs, list, listcount, listspace);
	if (mins[node->axis] <= node->dist + AREA_LOOSE)
		SV_AreaRadiusCandidatesR (node->children[1], mins, maxs, list, listcount, listspace);
}

void SV_AreaRadiusCandidates (const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace)
{
	SV_AreaRadiusCandidatesR (sv_areanodes, mins, maxs, list, listcount, listspace);
}

/*
====================
SV_TouchLinks

ericw -- copy the touching edicts to an array so we can avoid
iteating the trigger_edicts linked list while calling PR_ExecuteProgram
which could potentially corrupt the list while it's being iterated.
Based on code from Spike.
====================
*/
void SV_TouchLinks (edict_t *ent)
{
	edict_t		**list;
	edict_t		*touch;
	int		old_self, old_other;
	int		i, listcount;
	int		mark;

	if (VR_TouchLinks (ent)) // QVR: hands and touchable non-triggers
		return;

	mark = Temp_Mark (); // QVR: the temporary stack, not the hunk (zone.h)
	list = (edict_t **) Temp_Alloc (qcvm->num_edicts*sizeof(edict_t *));

	listcount = 0;
	SV_AreaTriggerEdicts (ent, sv_areanodes, list, &listcount, qcvm->num_edicts);

	for (i = 0; i < listcount; i++)
	{
		touch = list[i];
	// re-validate in case of PR_ExecuteProgram having side effects that make
	// edicts later in the list no longer touch
		if (ent->free)
			break; // QVR: an earlier trigger's touch removed it: no more triggers fire on it (after vkQuake c664a5b3)
		if (touch == ent || touch->free)
			continue;
		if (!touch->v.touch || touch->v.solid != SOLID_TRIGGER)
			continue;
		if (ent->v.absmin[0] > touch->v.absmax[0]
		|| ent->v.absmin[1] > touch->v.absmax[1]
		|| ent->v.absmin[2] > touch->v.absmax[2]
		|| ent->v.absmax[0] < touch->v.absmin[0]
		|| ent->v.absmax[1] < touch->v.absmin[1]
		|| ent->v.absmax[2] < touch->v.absmin[2] )
			continue;
		if (VR_HullTouchTop (ent) < touch->v.absmin[2]) // QVR: lying down, the lying box's top (vr_prone)
			continue;
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;

		pr_global_struct->self = EDICT_TO_PROG(touch);
		pr_global_struct->other = EDICT_TO_PROG(ent);
		pr_global_struct->time = qcvm->time;
		PR_ExecuteProgram (touch->v.touch);

		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}

// free the edicts array
	Temp_FreeToMark (mark);
}


/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs (edict_t *ent, mnode_t *node)
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

	if (ent->num_leafs == MAX_ENT_LEAFS)
		return;

// add an efrag if the node is a leaf

	if ( node->contents < 0)
	{
		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}

// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(ent->v.absmin, ent->v.absmax, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs (ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs (ent, node->children[1]);
}

/*
===============
SV_BoxInPVS
===============
*/
qboolean SV_BoxInPVS (vec3_t mins, vec3_t maxs, byte *pvs, mnode_t *node)
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if (node->contents == CONTENTS_SOLID)
		return false;

	if (node->contents < 0)
	{
		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;
		return pvs[leafnum >> 3] & (1 << (leafnum & 7));
	}

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(mins, maxs, splitplane);

	if (sides & 1 && SV_BoxInPVS (mins, maxs, pvs, node->children[0]))
		return true;

	if (sides & 2 && SV_BoxInPVS (mins, maxs, pvs, node->children[1]))
		return true;

	return false;
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict (edict_t *ent, qboolean touch_triggers)
{
	areanode_t	*node;

	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position

	if (ent == qcvm->edicts)
		return;		// don't add the world

	if (ent->free)
		return;

// set the abs box
	VectorAdd (ent->v.origin, ent->v.mins, ent->v.absmin);
	VectorAdd (ent->v.origin, ent->v.maxs, ent->v.absmax);

//
// to make items easier to pick up and allow them to be grabbed off
// of shelves, the abs sizes are expanded
//
	if ((int)ent->v.flags & FL_ITEM)
	{
		ent->v.absmin[0] -= 15;
		ent->v.absmin[1] -= 15;
		ent->v.absmax[0] += 15;
		ent->v.absmax[1] += 15;
	}
	else if (!VR_ExpandAbsBox (ent)) // QVR: easy hand-touch entities
	{	// because movement is clipped an epsilon away from an actual edge,
		// we must fully check even when bounding boxes don't quite touch
		ent->v.absmin[0] -= 1;
		ent->v.absmin[1] -= 1;
		ent->v.absmin[2] -= 1;
		ent->v.absmax[0] += 1;
		ent->v.absmax[1] += 1;
		ent->v.absmax[2] += 1;
	}

	VR_FindRadius_Linked (ent); // QVR: in the grid as it is now (or SOLID_NOT: not in it, and findradius skips it)
	SV_PushGridLinked (ent); // QVR: in the pushers' candidate grid too (this tick's, if built: sv_phys.c)

// link to PVS leafs
	ent->num_leafs = 0;
	if (ent->v.modelindex)
		SV_FindTouchedLeafs (ent, sv.worldmodel->nodes);

	if (ent->v.solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1)
			break;
		// QVR: loose (AREA_LOOSE): a child takes what is on its side of the split or overhangs it by less than
		// AREA_LOOSE (either, the side of its middle)
		qboolean fits0 = ent->v.absmin[node->axis] > node->dist - AREA_LOOSE;
		qboolean fits1 = ent->v.absmax[node->axis] < node->dist + AREA_LOOSE;
		if (fits0 && fits1)
			node = node->children[ent->v.absmin[node->axis] + ent->v.absmax[node->axis] > 2 * node->dist ? 0 : 1];
		else if (fits0)
			node = node->children[0];
		else if (fits1)
			node = node->children[1];
		else
			break;		// crosses the node
	}

// link it in

	if (ent->v.solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);
	SV_AreaArrayAdd (ent, (int) (node - sv_areanodes)); // QVR

// if touch_triggers, touch all entities at this node and decend for more
	if (touch_triggers)
		SV_TouchLinks ( ent );
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	float		d;
	mclipnode_t	*node; //johnfitz -- was dclipnode_t
	mplane_t	*plane;

	while (num >= 0)
	{
		if (num < hull->firstclipnode || num > hull->lastclipnode)
			Sys_Error ("SV_HullPointContents: bad node number");

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DoublePrecisionDotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}


/*
==================
SV_PointContents

==================
*/
int SV_PointContents (vec3_t p)
{
	int		cont;

	cont = SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
	if (cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN)
		cont = CONTENTS_WATER;
	return VR_LiquidContents (sv.worldmodel, p, cont);
}

int SV_TruePointContents (vec3_t p)
{
	return SV_HullPointContents (&sv.worldmodel->hulls[0], 0, p);
}

//===========================================================================

/*
============
SV_TestEntityPosition

This could be a lot more efficient...
============
*/
edict_t	*SV_TestEntityPosition (edict_t *ent)
{
	trace_t	trace;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, 0, ent);

	if (trace.startsolid)
		return trace.ent ? trace.ent : qcvm->edicts;

	return NULL;
}


/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

/*
==================
QVR: FTE's hull trace (after vkQuake world.c's Q1BSP_RecursiveHullTrace, default on since vkQuake 13b4b73a)

Quake's trace below splits a move at a point DIST_EPSILON short of each plane it crosses, so the halves overlap or
leave a gap, and asks the far child's contents at that point with a separate walk down the tree; an impact then backs
off in tenths of the move while the point is still in solid ("backup past 0"). FTE's recurses at the plane itself,
tests the far side with the same recursion (one precision for both), and puts the impact DIST_EPSILON off the hit
plane from the move's own start and end: no gaps between the halves (thin brushes, sharp corners: fewer moves that
slip through or stick), no second walk. Thread-safe like Quake's (vr_ledges.cpp traces on the job pool): its state is
the caller's. sv_fte_hulltrace 0: Quake's (to compare).
==================
*/
cvar_t sv_fte_hulltrace = {"sv_fte_hulltrace", "0", CVAR_NONE}; // QVR: 1 = FTE's hull trace, 0 = Quake's (default 0: the tests pass with 1, but it was 3-14% slower in SV_Physics in play: ROUND25.md, "vkbench")

enum { rht_solid, rht_empty, rht_impact };

typedef struct
{
	vec3_t			start, end;
	mclipnode_t		*clipnodes;
	mplane_t		*planes;
} rhtctx_t;

static int SV_FteHullTrace (const rhtctx_t *ctx, int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2, trace_t *trace)
{
	mclipnode_t	*node;
	mplane_t	*plane;
	float		t1, t2, midf;
	vec3_t		mid;
	int			side, rht, i;

reenter:
	if (num < 0)
	{	// a leaf
		if (num == CONTENTS_SOLID)
		{
			if (trace->allsolid)
				trace->startsolid = true;
			return rht_solid;
		}
		trace->allsolid = false;
		if (num == CONTENTS_EMPTY)
			trace->inopen = true;
		else
			trace->inwater = true;
		return rht_empty;
	}

	node = ctx->clipnodes + num;
	plane = ctx->planes + node->planenum;
	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DoublePrecisionDotProduct (plane->normal, p1) - plane->dist;
		t2 = DoublePrecisionDotProduct (plane->normal, p2) - plane->dist;
	}

	// wholly on one side: on down that side
	if (t1 >= 0 && t2 >= 0)
	{
		num = node->children[0];
		goto reenter;
	}
	if (t1 < 0 && t2 < 0)
	{
		num = node->children[1];
		goto reenter;
	}

	// the crossing, from the whole move's ends (the same precision at every depth)
	if (plane->type < 3)
	{
		t1 = ctx->start[plane->type] - plane->dist;
		t2 = ctx->end[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct (plane->normal, ctx->start) - plane->dist;
		t2 = DotProduct (plane->normal, ctx->end) - plane->dist;
	}
	side = t1 < 0;
	midf = t1 / (t1 - t2);
	if (midf < p1f)
		midf = p1f;
	if (midf > p2f)
		midf = p2f;
	for (i = 0; i < 3; i++)
		mid[i] = ctx->start[i] + (ctx->end[i] - ctx->start[i]) * midf;

	rht = SV_FteHullTrace (ctx, node->children[side], p1f, midf, p1, mid, trace);
	if (rht != rht_empty && !trace->allsolid)
		return rht;
	rht = SV_FteHullTrace (ctx, node->children[side ^ 1], midf, p2f, mid, p2, trace);
	if (rht != rht_solid)
		return rht;

	// the far side is solid: the impact, on the plane facing the start
	if (side)
	{
		trace->plane.dist = -plane->dist;
		VectorSubtract (vec3_origin, plane->normal, trace->plane.normal);
	}
	else
	{
		trace->plane.dist = plane->dist;
		VectorCopy (plane->normal, trace->plane.normal);
	}
	t1 = DoublePrecisionDotProduct (trace->plane.normal, ctx->start) - trace->plane.dist;
	t2 = DoublePrecisionDotProduct (trace->plane.normal, ctx->end) - trace->plane.dist;
	midf = (t1 - DIST_EPSILON) / (t1 - t2);
	if (midf < 0.f)
		midf = 0.f;
	if (midf > 1.f)
		midf = 1.f;
	trace->fraction = midf;
	for (i = 0; i < 3; i++)
		trace->endpos[i] = ctx->start[i] + (ctx->end[i] - ctx->start[i]) * midf;
	return rht_impact;
}

static qboolean SV_QuakeHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);

// a whole move (fractions 0 to 1) by FTE's trace
static qboolean SV_FteHullCheck (hull_t *hull, int num, vec3_t p1, vec3_t p2, trace_t *trace)
{
	rhtctx_t ctx;
	if (p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2])
	{	// a point crosses no plane
		int c = SV_HullPointContents (hull, num, p1);
		if (c == CONTENTS_SOLID)
			trace->startsolid = true;
		else
		{
			trace->allsolid = false;
			if (c == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		}
		return true;
	}
	VectorCopy (p1, ctx.start);
	VectorCopy (p2, ctx.end);
	ctx.clipnodes = hull->clipnodes;
	ctx.planes = hull->planes;
	return SV_FteHullTrace (&ctx, num, 0.f, 1.f, p1, p2, trace) != rht_impact;
}

/*
==================
SV_RecursiveHullCheck

QVR: FTE's trace (above) for a whole move with sv_fte_hulltrace 1, else Quake's
==================
*/
qboolean SV_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace)
{
	if (sv_fte_hulltrace.value && p1f == 0.f && p2f == 1.f)
		return SV_FteHullCheck (hull, num, p1, p2, trace);
	return SV_QuakeHullCheck (hull, num, p1f, p2f, p1, p2, trace);
}

/*
==================
SV_QuakeHullCheck (QVR: was SV_RecursiveHullCheck)

==================
*/
static qboolean SV_QuakeHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace)
{
	mclipnode_t	*node; //johnfitz -- was dclipnode_t
	mplane_t	*plane;
	float		t1, t2;
	float		frac;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

// check for empty
	if (num < 0)
	{
		if (num != CONTENTS_SOLID)
		{
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		}
		else
			trace->startsolid = true;
		return true;		// empty
	}

	if (num < hull->firstclipnode || num > hull->lastclipnode)
		Sys_Error ("SV_RecursiveHullCheck: bad node number");

//
// find the point distances
//
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DoublePrecisionDotProduct (plane->normal, p1) - plane->dist;
		t2 = DoublePrecisionDotProduct (plane->normal, p2) - plane->dist;
	}

#if 1
	if (t1 >= 0 && t2 >= 0)
		return SV_QuakeHullCheck (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if (t1 < 0 && t2 < 0)
		return SV_QuakeHullCheck (hull, node->children[1], p1f, p2f, p1, p2, trace);
#else
	if ( (t1 >= DIST_EPSILON && t2 >= DIST_EPSILON) || (t2 > t1 && t1 >= 0) )
		return SV_QuakeHullCheck (hull, node->children[0], p1f, p2f, p1, p2, trace);
	if ( (t1 <= -DIST_EPSILON && t2 <= -DIST_EPSILON) || (t2 < t1 && t1 <= 0) )
		return SV_QuakeHullCheck (hull, node->children[1], p1f, p2f, p1, p2, trace);
#endif

// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < 0)
		frac = (t1 + DIST_EPSILON)/(t1-t2);
	else
		frac = (t1 - DIST_EPSILON)/(t1-t2);
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f)*frac;
	for (i=0 ; i<3 ; i++)
		mid[i] = p1[i] + frac*(p2[i] - p1[i]);

	side = (t1 < 0);

// move up to the node
	if (!SV_QuakeHullCheck (hull, node->children[side], p1f, midf, p1, mid, trace) )
		return false;

#ifdef PARANOID
	if (SV_HullPointContents (sv_hullmodel, mid, node->children[side])
	== CONTENTS_SOLID)
	{
		Con_Printf ("mid PointInHullSolid\n");
		return false;
	}
#endif

	if (SV_HullPointContents (hull, node->children[side^1], mid)
	!= CONTENTS_SOLID)
// go past the node
		return SV_QuakeHullCheck (hull, node->children[side^1], midf, p2f, mid, p2, trace);

	if (trace->allsolid)
		return false;		// never got out of the solid area

//==================
// the other side of the node is solid, this is the impact point
//==================
	if (!side)
	{
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	}
	else
	{
		VectorSubtract (vec3_origin, plane->normal, trace->plane.normal);
		trace->plane.dist = -plane->dist;
	}

	while (SV_HullPointContents (hull, hull->firstclipnode, mid)
	== CONTENTS_SOLID)
	{ // shouldn't really happen, but does occasionally
		frac -= 0.1;
		if (frac < 0)
		{
			trace->fraction = midf;
			VectorCopy (mid, trace->endpos);
			if (VR_OnMainThread ()) // QVR: the hull's traces also run on the game's thread pool (vr/vr_ledges.cpp)
				Con_DPrintf ("backup past 0\n");
			return false;
		}
		midf = p1f + (p2f - p1f)*frac;
		for (i=0 ; i<3 ; i++)
			mid[i] = p1[i] + frac*(p2[i] - p1[i]);
	}

	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);

	return false;
}

/*
==================
QVR: sv_hulltrace_compare [traces] [seed]: the same seeded random moves through the map's point and player hulls
(starts out of solid, up to 512 units long) traced by Quake's and by FTE's: how many differ (a hit against a miss,
the end more than 1/8 unit apart, all-solid), how many of each end inside solid (a move that would stick), and each
one's time. Debug > Profiling and Memory, "Compare Hull Traces".
==================
*/
static float SV_HullCompareRand (unsigned *s)
{
	*s = *s * 1664525u + 1013904223u;
	return (float)((*s >> 8) & 0xffffff) / (float)0x1000000;
}

static void SV_HullTraceCompare_f (void)
{
	int			n = Cmd_Argc () >= 2 ? Q_atoi (Cmd_Argv (1)) : 100000;
	unsigned	seed = Cmd_Argc () >= 3 ? (unsigned)Q_atoi (Cmd_Argv (2)) : 7u;
	int			h, k, i;
	qmodel_t	*w = sv.active ? sv.worldmodel : NULL;

	if (!w || n <= 0)
	{
		Con_Printf ("sv_hulltrace_compare: no map\n");
		return;
	}
	for (h = 0; h < 2; h++)
	{
		hull_t	*hull = &w->hulls[h];
		int		made = 0, hitdiff = 0, enddiff = 0, soliddiff = 0, stuck[2] = {0, 0};
		double	time[2] = {0, 0};
		vec3_t	*from = (vec3_t *)malloc ((size_t)n * sizeof (vec3_t)), *to = (vec3_t *)malloc ((size_t)n * sizeof (vec3_t));
		trace_t	*res = (trace_t *)malloc ((size_t)n * 2 * sizeof (trace_t));
		unsigned s = seed;
		int		tries = 0;

		if (!from || !to || !res)
			Sys_Error ("sv_hulltrace_compare: out of memory");
		while (made < n && tries++ < n * 50)
		{
			vec3_t p;
			for (i = 0; i < 3; i++)
				p[i] = w->mins[i] + (w->maxs[i] - w->mins[i]) * SV_HullCompareRand (&s);
			if (SV_HullPointContents (hull, hull->firstclipnode, p) == CONTENTS_SOLID)
				continue;
			VectorCopy (p, from[made]);
			for (i = 0; i < 3; i++)
				to[made][i] = p[i] + (SV_HullCompareRand (&s) * 2.f - 1.f) * 512.f;
			made++;
		}
		for (k = 0; k < 2; k++)
		{
			double t0 = Sys_DoubleTime ();
			for (i = 0; i < made; i++)
			{
				trace_t *t = &res[i * 2 + k];
				memset (t, 0, sizeof (*t));
				t->fraction = 1;
				t->allsolid = true;
				VectorCopy (to[i], t->endpos);
				if (k == 0)
					SV_QuakeHullCheck (hull, hull->firstclipnode, 0, 1, from[i], to[i], t);
				else
					SV_FteHullCheck (hull, hull->firstclipnode, from[i], to[i], t);
			}
			time[k] = Sys_DoubleTime () - t0;
		}
		for (i = 0; i < made; i++)
		{
			trace_t *a = &res[i * 2], *b = &res[i * 2 + 1];
			vec3_t d;
			if ((a->fraction < 1) != (b->fraction < 1))
				hitdiff++;
			VectorSubtract (a->endpos, b->endpos, d);
			if (DotProduct (d, d) > 0.125f * 0.125f)
				enddiff++;
			if (a->allsolid != b->allsolid || a->startsolid != b->startsolid)
				soliddiff++;
			for (k = 0; k < 2; k++)
				if (!res[i * 2 + k].allsolid && SV_HullPointContents (hull, hull->firstclipnode, res[i * 2 + k].endpos) == CONTENTS_SOLID)
					stuck[k]++;
		}
		Con_Printf ("hulltrace: hull %d, %d traces: %d hit/miss differ, %d ends > 1/8 apart, %d solid flags differ; "
			"ends in solid: quake %d, fte %d; time: quake %.1f ms, fte %.1f ms\n",
			h, made, hitdiff, enddiff, soliddiff, stuck[0], stuck[1], time[0] * 1000.0, time[1] * 1000.0);
		free (from);
		free (to);
		free (res);
	}
}

void SV_HullTraceInit (void)
{
	Cvar_RegisterVariable (&sv_fte_hulltrace);
	Cmd_AddCommand ("sv_hulltrace_compare", SV_HullTraceCompare_f);
}


/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t		trace;
	vec3_t		offset;
	vec3_t		start_l, end_l;
	hull_t		*hull;

// fill in a default trace
	memset (&trace, 0, sizeof(trace_t));
	trace.fraction = 1;
	trace.allsolid = true;
	VectorCopy (end, trace.endpos);

// get the clipping hull
	hull = SV_HullForEntity (ent, mins, maxs, offset);

	VectorSubtract (start, offset, start_l);
	VectorSubtract (end, offset, end_l);

// trace a line through the apropriate clipping hull
	++vr_profcounts.hullchecks; // QVR: profile
	SV_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);

// fix trace up by the offset
	if (trace.fraction != 1)
		VectorAdd (trace.endpos, offset, trace.endpos);

// did we clip the move?
	if (trace.fraction < 1 || trace.startsolid  )
		trace.ent = ent;

	return trace;
}

/*
==================
SV_ClipMoveToEntityQVR

QVR: SV_ClipMoveToEntity, but a BSP model met by the move's narrower box (vr_hull_width; vr/vr_hull.cpp) is
traced against the brushes rebuilt from its hull 0.
==================
*/
static trace_t SV_ClipMoveToEntityQVR (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, const moveclip_t *clip)
{
	trace_t		trace;

	if (clip->portalplane && VR_HullClipPortal (ent, start, clip->bspmins, clip->bspmaxs,
		end, clip->portalplane, clip->portalturned, &trace))
		return trace;
	if (clip->bspbox && ent->v.solid == SOLID_BSP)
	{
		memset (&trace, 0, sizeof(trace_t));
		if (VR_HullClipBSP (ent, start, clip->bspmins, clip->bspmaxs, end, &trace))
		{
			if (trace.fraction < 1 || trace.startsolid)
				trace.ent = ent;
			return trace;
		}
	}
	return SV_ClipMoveToEntity (ent, start, mins, maxs, end);
}

/*
==================
SV_ClipMoveToBoxEntityQVR

QVR: SV_ClipMoveToEntity for an entity's box (not SOLID_BSP), with the players' narrower boxes (vr_hull_ent_width;
vr/vr_hull.cpp): the moving player's own box when it narrows against this entity, and the entity's when it is a
player that a body moves into (or its hit box, vr_hull_hit_width, when a shot or missile does).
==================
*/
static trace_t SV_ClipMoveToBoxEntityQVR (edict_t *touch, vec3_t mins, vec3_t maxs, const moveclip_t *clip)
{
	trace_t		trace;
	vec3_t		tmins, tmaxs, hullmins, hullmaxs;
	const float	*m = mins, *M = maxs;
	hull_t		*hull;
	vec3_t		start_l, end_l;
	qboolean	narrowtouch;
	qboolean	pair; // a player's and a monster's bodies (vr_body_monster_inset; VR_BodyContactHull)

	pair = clip->bodymove && VR_BodyContactHull (clip->passedict, touch, mins, maxs, hullmins, hullmaxs);
	if (!pair)
	{
		if (clip->entbox)
		{
			m = clip->entmins;
			M = clip->entmaxs;
		}
		narrowtouch = clip->bodymove ? VR_HullTouchBox (touch, clip->passedict, tmins, tmaxs)
			: VR_HullHitBox (touch, tmins, tmaxs, clip->hitmodel >= 0 || clip->hitgibs || clip->type == MOVE_MISSILE); // a shot or missile: the player's hit box (vr_hull_hit_width, vr_hull_hit_head)
		if (m == mins && !narrowtouch)
			return SV_ClipMoveToEntity (touch, clip->start, mins, maxs, clip->end);

		// SV_HullForEntity's box, with the narrower boxes
		VectorSubtract (narrowtouch ? tmins : touch->v.mins, M, hullmins);
		VectorSubtract (narrowtouch ? tmaxs : touch->v.maxs, m, hullmaxs);
	}
	hull = SV_HullForBox (hullmins, hullmaxs);

	memset (&trace, 0, sizeof(trace_t));
	trace.fraction = 1;
	trace.allsolid = true;
	VectorCopy (clip->end, trace.endpos);
	VectorSubtract (clip->start, touch->v.origin, start_l);
	VectorSubtract (clip->end, touch->v.origin, end_l);
	++vr_profcounts.hullchecks;
	SV_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);
	// a player and a monster already in each other: a move that goes no deeper passes (they never stick)
	if (pair && trace.startsolid && VR_BodyContactLetsOut (clip->passedict, touch, hullmins, hullmaxs, start_l, end_l))
	{
		memset (&trace, 0, sizeof(trace_t));
		trace.fraction = 1;
		VectorCopy (clip->end, trace.endpos);
		return trace;
	}
	if (trace.fraction != 1)
		VectorAdd (trace.endpos, touch->v.origin, trace.endpos);
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = touch;
	return trace;
}

/*
==================
SV_ClipMoveToCorpseQVR

QVR: a body's move against a corpse in its way (VR_CorpseBox): its box `cmins`, `cmaxs` (from its origin), met by the
move's box (`narrow`: the player's narrower one, as SV_ClipMoveToBoxEntityQVR).
==================
*/
static trace_t SV_ClipMoveToCorpseQVR (edict_t *touch, const vec3_t cmins, const vec3_t cmaxs, qboolean narrow, const moveclip_t *clip)
{
	trace_t		trace;
	vec3_t		hullmins, hullmaxs, start_l, end_l;
	const float	*m = narrow ? clip->entmins : clip->mins, *M = narrow ? clip->entmaxs : clip->maxs;
	hull_t		*hull;

	VectorSubtract (cmins, M, hullmins);
	VectorSubtract (cmaxs, m, hullmaxs);
	hull = SV_HullForBox (hullmins, hullmaxs);

	memset (&trace, 0, sizeof(trace_t));
	trace.fraction = 1;
	trace.allsolid = true;
	VectorCopy (clip->end, trace.endpos);
	VectorSubtract (clip->start, touch->v.origin, start_l);
	VectorSubtract (clip->end, touch->v.origin, end_l);
	++vr_profcounts.hullchecks;
	SV_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace);
	if (trace.fraction != 1)
		VectorAdd (trace.endpos, touch->v.origin, trace.endpos);
	if (trace.fraction < 1 || trace.startsolid)
		trace.ent = touch;
	return trace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks ( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;
	qboolean	propshape; // QVR: met as a solid prop's drawn shape (VR_PropClip)
	int			shot; // QVR: a grenade shots set off (VR_ShotTargetClip)
	qboolean	corpse, narrow; // QVR: a corpse in a body's way (VR_CorpseBox), met by the narrower box
	vec3_t		corpsemins, corpsemaxs;

// touch linked edicts
	for (l = node->solid_edicts.next ; l != &node->solid_edicts ; l = next)
	{
		next = l->next;
		propshape = false; // QVR
		corpse = false; // QVR
		touch = EDICT_FROM_AREA(l);
		if (touch->v.solid == SOLID_NOT)
			continue;
		if (VR_AD_PelletsPass (clip->passedict, touch)) // AD support: a blast's pellets never stop one another
			continue;
		// QVR: a grenade shots set off (.vr_shot_radius: VR_ShotTargetClip): a shot or a missile meets a cube round it,
		// its owner's own shots too; any other move meets it by Quake's rules.
		if (touch != clip->passedict && touch->v.takedamage && touch->v.solid != SOLID_BSP
			&& clip->boxmins[0] <= touch->v.absmax[0] && clip->boxmins[1] <= touch->v.absmax[1]
			&& clip->boxmins[2] <= touch->v.absmax[2] && clip->boxmaxs[0] >= touch->v.absmin[0]
			&& clip->boxmaxs[1] >= touch->v.absmin[1] && clip->boxmaxs[2] >= touch->v.absmin[2])
		{
			shot = VR_ShotTargetClip (clip->passedict, touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hittype, &trace);
			if (shot == 0)
				continue;
			if (shot > 0)
			{
				if (clip->trace.allsolid)
					return;
				if (trace.fraction < clip->trace.fraction)
				{
					qboolean wasStartSolid = clip->trace.startsolid;
					clip->trace = trace;
					clip->trace.startsolid = wasStartSolid;
				}
				continue;
			}
		}
		// QVR: a flying missile meets a dropped pickup lying loose (a backpack) at its drawn box as it lies, in its own move
		// (VR_ItemShotClip), not where FL_ITEM's grown box overlaps it (box3d::missileMeetsItem).
		if (touch->v.solid == SOLID_NOT_BUT_TOUCHABLE && touch != clip->passedict
			&& clip->boxmins[0] <= touch->v.absmax[0] && clip->boxmins[1] <= touch->v.absmax[1]
			&& clip->boxmins[2] <= touch->v.absmax[2] && clip->boxmaxs[0] >= touch->v.absmin[0]
			&& clip->boxmaxs[1] >= touch->v.absmin[1] && clip->boxmaxs[2] >= touch->v.absmin[2])
		{
			shot = VR_ItemShotClip (clip->passedict, touch, clip->start, clip->mins, clip->maxs, clip->end, &trace);
			if (shot == 0)
				continue;
			if (shot > 0)
			{
				if (clip->trace.allsolid)
					return;
				if (trace.fraction < clip->trace.fraction)
				{
					qboolean wasStartSolid = clip->trace.startsolid;
					clip->trace = trace;
					clip->trace.startsolid = wasStartSolid;
				}
				continue;
			}
		}
		// QVR: touchable non-solids never block, but shots and missiles stop at gibs and heads
		// (those that take damage: vr_gib_destroy).
		if (touch->v.solid == SOLID_NOT_BUT_TOUCHABLE && !(clip->hitgibs && touch->v.takedamage))
		{
			// QVR: but a corpse is in a player's or a monster's way (vr_corpse_collide_player, _monsters): its box, met by
			// the move's (the player's narrower one where it narrows, as SV_ClipMoveToBoxEntityQVR)
			if (!clip->bodymove || clip->hitmodel >= 0 || touch == clip->passedict)
				continue;
			narrow = clip->entbox;
			if (!VR_CorpseBox (clip->passedict, touch, clip->start, narrow ? clip->entmins : clip->mins,
				narrow ? clip->entmaxs : clip->maxs, corpsemins, corpsemaxs))
				continue;
			corpse = true;
		}
		if (touch == clip->passedict && !clip->hitpass)
			continue;
		// QVR: a player and the enemy his hand holds don't meet (vr_foegrab_step_in)
		if (clip->bodymove && VR_BodyContactPasses (clip->passedict, touch, clip->mins, clip->maxs))
			continue;
		// QVR: a trigger linked as a solid (its solid changed after its link: game code's mistake, as the items a
		// trigger brings were, ROUND22.md "The Matriarch of Decay") is passed through, not a fatal error; it is in
		// the right list at its next link.
		if (touch->v.solid == SOLID_TRIGGER)
		{
			Con_DWarning ("Trigger in clipping list: edict %d %s %s (passed through)\n", NUM_FOR_EDICT (touch),
				PR_GetString (touch->v.classname), PR_GetString (touch->v.model));
			continue;
		}

		if (clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP)
			continue;

		if (clip->boxmins[0] > touch->v.absmax[0]
		|| clip->boxmins[1] > touch->v.absmax[1]
		|| clip->boxmins[2] > touch->v.absmax[2]
		|| clip->boxmaxs[0] < touch->v.absmin[0]
		|| clip->boxmaxs[1] < touch->v.absmin[1]
		|| clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		if (clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0])
			continue;	// points never interact

	// might intersect, so do an exact clip
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (PROG_TO_EDICT(touch->v.owner) == clip->passedict &&
				!VR_OwnPropMeets (clip->passedict, touch)) // QVR: not a solid prop a player threw
				continue;	// don't clip against own missiles
			if (PROG_TO_EDICT(clip->passedict->v.owner) == touch && !clip->hitowner)
				continue;	// don't clip against owner (QVR: but a missile come through a teleporter meets it: VR_PortalHitsOwner)
		}

		// QVR: precise hits (vr/vr_hitmodel.cpp): a monster's box (grown by the
		// tolerance) is only the broad phase; the move meets its model as drawn, or goes on through.
		// A monster moving (knocked back) into a missile doesn't meet it with its box: the missile's
		// own move meets its model.
		if (clip->passedict && VR_HitModelMoveFlags (touch) && VR_HitModelTarget (clip->passedict))
			continue;
		if (clip->portalplane && touch->v.solid != SOLID_BSP &&
			VR_HullClipPortal (touch, clip->start, clip->bspmins, clip->bspmaxs, clip->end,
				clip->portalplane, clip->portalturned, &trace))
		{
			// The clipped portion has already been tested against this solid.
		}
		else if (corpse) // QVR
			trace = SV_ClipMoveToCorpseQVR (touch, corpsemins, corpsemaxs, narrow, clip);
		else if (clip->hitmodel >= 0 && VR_HitModelTarget (touch))
		{
			if (!VR_HitModelClip (touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hittype,
				clip->hitmodel, clip->trace.fraction, &trace))
				continue;
		}
		// QVR: missiles meet monsters with a fatter box, but not a corpse (vr_corpse_gib, touchable):
		// lying low, it would stop them well above it.
		else if (((int)touch->v.flags & FL_MONSTER) && touch->v.solid != SOLID_NOT_BUT_TOUCHABLE)
			trace = SV_ClipMoveToBoxEntityQVR (touch, clip->mins2, clip->maxs2, clip); // QVR
		// QVR: shots and missiles (and the flying grappling hook) meet a solid prop's drawn box as it is turned (a
		// tilted explosive box's empty corners let them by: vr_box3d_shot_shape).
		else if (touch->v.solid == SOLID_BBOX &&
			VR_PropShotClip (clip->passedict, touch, clip->start, clip->mins, clip->maxs, clip->end, clip->hittype, &trace))
			propshape = true;
		// QVR: a player's box meets a solid prop's drawn box as it is turned, not the box round it
		// (vr_box3d_player_shape).
		else if (touch->v.solid == SOLID_BBOX && clip->bodymove && clip->passedict &&
			VR_PropClip (clip->passedict, touch, clip->start, clip->mins, clip->maxs,
				clip->entbox ? clip->entmins : clip->mins,
				clip->entbox ? clip->entmaxs : clip->maxs,
				clip->end, &trace))
			propshape = true;
		else if (touch->v.solid != SOLID_BSP) // QVR
			trace = SV_ClipMoveToBoxEntityQVR (touch, clip->mins, clip->maxs, clip);
		else
			trace = SV_ClipMoveToEntityQVR (touch, clip->start, clip->mins, clip->maxs, clip->end, clip); // QVR
		// QVR: a player already inside a solid prop (toppled onto him) isn't held by it: he walks out. (Not with his
		// feet a little into its top: he is on it, and SV_CheckStuck puts him back on top.)
		if (trace.startsolid && clip->passedict &&
			VR_PropLetsOut (clip->passedict, touch, clip->start, clip->mins, clip->maxs, clip->end, propshape))
			continue;
		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}

// recurse down both sides
	if (node->axis == -1)
		return;

	if ( clip->boxmaxs[node->axis] > node->dist - AREA_LOOSE ) // QVR: loose (AREA_LOOSE)
		SV_ClipToLinks ( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist + AREA_LOOSE )
		SV_ClipToLinks ( node->children[1], clip );
}


/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
#if 0
// debug to test against everything
boxmins[0] = boxmins[1] = boxmins[2] = -9999;
boxmaxs[0] = boxmaxs[1] = boxmaxs[2] = 9999;
#else
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
#endif
}

/*
==================
SV_Move
==================
*/
static trace_t SV_MoveRun (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict, const float *portalplane, const float *portalturned);

// QVR: counted for the profiler; with vr_profile_detail 2, each timed ("trace").
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict)
{
	trace_t trace;
	++vr_profcounts.traces; // QVR: profile (the rest in SV_MoveRun)
	if (VR_PortalBodyMove (passedict, start, mins, maxs, end, type, &trace))
		return trace;
	if (!vr_profile_fine)
		return SV_MoveRun (start, mins, maxs, end, type, passedict, NULL, NULL);
	VR_ProfileBegin ("trace");
	trace = SV_MoveRun (start, mins, maxs, end, type, passedict, NULL, NULL);
	VR_ProfileEnd ();
	return trace;
}

trace_t SV_MovePortalHalf (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type,
	edict_t *passedict, const float plane[4])
{
	return SV_MoveRun (start, mins, maxs, end, type, passedict, plane, NULL);
}

trace_t SV_MovePortalTurned (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type,
	edict_t *passedict, const float plane[4], const float turned[12])
{
	return SV_MoveRun (start, mins, maxs, end, type, passedict, plane, turned);
}

/*
==================
SV_ClipToLeanBoxes

QVR: a shot, a missile or a point's trace (not a body's move) against the flat-screen players' leant upper bodies
(VR_LeanHitBoxes, vr/vr_flatmove.cpp: the head and the chest out of his box, while he leans round a corner). The nearer
hit wins, and it is his: the damage is his. His own shots and missiles pass them, as they pass his box.
==================
*/
static void SV_ClipToLeanBoxes (moveclip_t *clip)
{
	int			i, k, n;
	float		boxes[2][6];
	edict_t		*touch;
	trace_t		trace;
	hull_t		*hull;
	vec3_t		hullmins, hullmaxs;

	for (i = 1; i <= svs.maxclients; i++)
	{
		touch = EDICT_NUM (i);
		if (touch == clip->passedict)
			continue;
		if (clip->passedict && PROG_TO_EDICT (clip->passedict->v.owner) == touch && !clip->hitowner)
			continue;
		if (clip->passedict && PROG_TO_EDICT (touch->v.owner) == clip->passedict)
			continue;
		n = VR_LeanHitBoxes (touch, boxes);
		for (k = 0; k < n; k++)
		{
			if (clip->boxmins[0] > boxes[k][3] || clip->boxmins[1] > boxes[k][4] || clip->boxmins[2] > boxes[k][5] ||
				clip->boxmaxs[0] < boxes[k][0] || clip->boxmaxs[1] < boxes[k][1] || clip->boxmaxs[2] < boxes[k][2])
				continue;
			VectorSubtract (boxes[k], clip->maxs, hullmins);
			VectorSubtract (boxes[k] + 3, clip->mins, hullmaxs);
			hull = SV_HullForBox (hullmins, hullmaxs);
			memset (&trace, 0, sizeof (trace_t));
			trace.fraction = 1;
			trace.allsolid = true;
			VectorCopy (clip->end, trace.endpos);
			SV_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, (float *) clip->start, (float *) clip->end, &trace);
			if (trace.startsolid || trace.fraction >= clip->trace.fraction)
				continue; // (from inside it: not a hit on it; the world or a nearer one first)
			trace.ent = touch;
			clip->trace = trace;
		}
	}
}

static trace_t SV_MoveRun (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict, const float *portalplane, const float *portalturned)
{
	moveclip_t	clip;
	int			i;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	// QVR: a flag on the type (a traceline's "nomonsters").
	clip.hitgibs = (type & MOVE_HITGIBS) != 0;
	clip.hitmodel = VR_HitModelTolerance (type); // QVR: and precise hits (MOVE_HITMODEL and its class)
	clip.hitpass = (type & MOVE_HITPASS) != 0; // QVR: through a teleporter, at itself (your shot at your own image)
	clip.hitowner = clip.hitpass || VR_PortalHitsOwner (passedict);
	type &= ~MOVE_HITPASS;
	clip.hittype = type;
	type &= ~(MOVE_HITGIBS | MOVE_HITMODEL | MOVE_HITMODEL_CLASS);
	if (type == MOVE_MISSILE)
		clip.hitgibs = true;

// clip to world
	clip.portalplane = portalplane;
	clip.portalturned = portalturned;
	if (portalplane)
	{
		VectorCopy (mins, clip.bspmins);
		VectorCopy (maxs, clip.bspmaxs);
	}
	clip.bspbox = portalplane ? true : VR_HullMoveBox (passedict, mins, maxs, clip.bspmins, clip.bspmaxs); // QVR: the player's narrower box
	clip.entbox = VR_HullEntBox (passedict, mins, maxs, clip.entmins, clip.entmaxs); // QVR: ... against entities
	// QVR: a body moving (a box; not a shot, a missile or a precise hit's trace) meets players' narrower boxes
	clip.bodymove = type != MOVE_MISSILE && !clip.hitgibs && clip.hitmodel < 0 && maxs[0] - mins[0] > 0 && maxs[2] - mins[2] > 0;
	clip.trace = SV_ClipMoveToEntityQVR ( qcvm->edicts, start, mins, maxs, end, &clip ); // QVR

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = passedict;

	if (type == MOVE_MISSILE)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.mins2[i] = -VR_MissileExtent (15); // QVR
			clip.maxs2[i] = VR_MissileExtent (15); // QVR
		}
	}
	else
	{
		VectorCopy (mins, clip.mins2);
		VectorCopy (maxs, clip.maxs2);
	}

// create the bounding box of the entire move
	SV_MoveBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );
	if (clip.hitmodel >= 0) // QVR: precise hits: the tolerance, and a model reaching out of its box (MOVE_HITMODEL_REACH)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.boxmins[i] -= clip.hitmodel + MOVE_HITMODEL_REACH;
			clip.boxmaxs[i] += clip.hitmodel + MOVE_HITMODEL_REACH;
		}
	}
	else if (clip.hitgibs || (clip.hittype & MOVE_HITMODEL)) // QVR: a shot or a missile: a grenade's cube reaching out of its point (VR_ShotTargetClip)
	{
		for (i=0 ; i<3 ; i++)
		{
			clip.boxmins[i] -= VR_SHOT_TARGET_REACH;
			clip.boxmaxs[i] += VR_SHOT_TARGET_REACH;
		}
	}

// clip to entities
	SV_ClipToLinks ( sv_areanodes, &clip );
	if (!clip.bodymove && type != MOVE_NOMONSTERS && !clip.trace.allsolid)
		SV_ClipToLeanBoxes (&clip); // QVR: a flat-screen player's leant upper body (vr/vr_flatmove.cpp)

	return clip.trace;
}

