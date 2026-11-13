/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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
// zone.c

#include "quakedef.h"
#include "vr/vr_tracy.h"

#define	ZONEID		0x1d4a11
#define	ZONEFREED	0x1d4a10	// QVR: a freed block's id (Z_Free's double-free check)


// QVR: vr_zone_threadcheck 1: the hunk, the cache and the zone used from any thread but the main one crash at once, with
// that thread's stack (qvr_crash.txt). None of them is thread-safe: even Cache_Check (Mod_Extradata) relinks the cache's
// LRU list, and a pool worker's (ragdoll rigs at a map load) corrupted it, and through it the map's data on the hunk
// (ROUND21.md, "Map load crash: the cache's LRU list from the pool").
cvar_t vr_zone_threadcheck = {"vr_zone_threadcheck", "0", CVAR_NONE};
static SDL_threadID zone_main_thread;
static FUNC_NOINLINE void Zone_WrongThread (void)
{
	*(volatile int *)16 = 0; // (the crash handler prints this thread's stack: the caller is the culprit)
}
static inline void Zone_CheckThread (void)
{
	if (vr_zone_threadcheck.value && zone_main_thread && SDL_ThreadID () != zone_main_thread)
		Zone_WrongThread ();
}

void Memory_InitCvars (void)
{
	Cvar_RegisterVariable (&vr_zone_threadcheck);
}


/*
==============================================================================

						ZONE MEMORY ALLOCATION

QVR: the zone is the engine's heap now (ROUND25.md, "nozone"): Z_Malloc, Z_Free and Z_Strdup are thin wrappers over
VR_HeapCalloc / VR_HeapFree (calloc and free, counted by vr_alloc_sites). The old zone was a first-fit allocator in a
fixed block at the bottom of the hunk (32 MiB, -zone): it walked its free list from a rover, a full zone was a
Sys_Error, and its Z_Realloc once corrupted a save (ROUND25.md, "Save crash: Z_Realloc").

Every block keeps a small header: its size (the bytes in use for vr_limits and Tracy) and ZONEID, checked on Z_Free (a
pointer from elsewhere, a double free). Debug builds add a tail word after the caller's bytes, checked on Z_Free (an
overrun). Tracy builds link the live blocks (Memory_TracySnapshot, Memory_TracyValidate walk them).
Z_Free (NULL) does nothing. Like the hunk and the cache, it is for the main thread (vr_zone_threadcheck): the counts
and Tracy's ledger are not locked.
==============================================================================
*/

typedef struct zblock_s
{
#ifdef QVR_TRACY
	struct zblock_s	*prev, *next;	// the live blocks (Tracy's snapshot and validation)
#endif
	size_t	size;	// the whole block: this header, the caller's bytes (and the Debug tail)
	int		id;		// ZONEID while allocated, ZONEFREED once freed
	int		pad;	// (16-byte multiple: the caller's bytes keep calloc's alignment)
} zblock_t;

#ifndef NDEBUG
#define ZONE_TAIL	sizeof (int)	// Debug: ZONEID after the caller's bytes
#else
#define ZONE_TAIL	0
#endif

static size_t	zone_used, zone_peak; // QVR: bytes in use (headers included) and the most ever (vr_limits)
static size_t	zone_count; // blocks in use
#ifdef QVR_TRACY
static zblock_t	zone_live = {&zone_live, &zone_live, 0, 0, 0}; // the live blocks' list head
#endif

/*
========================
Z_Usage -- QVR
========================
*/
void Z_Usage (size_t *used, size_t *peak, size_t *count)
{
	*used = zone_used;
	*peak = zone_peak;
	*count = zone_count;
}


/*
========================
Z_Free
========================
*/
void Z_Free (void *ptr)
{
	zblock_t	*block;

	Zone_CheckThread ();
	if (!ptr)
		return;

	block = (zblock_t *) ptr - 1;
	if (block->id == ZONEFREED)
		Sys_Error ("Z_Free: freed a freed pointer");
	if (block->id != ZONEID)
		Sys_Error ("Z_Free: freed a pointer without ZONEID");
#ifndef NDEBUG
	{
		int tail;
		memcpy (&tail, (byte *) block + block->size - ZONE_TAIL, sizeof (tail));
		if (tail != ZONEID)
			Sys_Error ("Z_Free: a block's tail was overwritten (an overrun of %i bytes)", (int) (block->size - sizeof (zblock_t) - ZONE_TAIL));
	}
#endif

	VR_TracyFree (ptr);
#ifdef QVR_TRACY
	block->prev->next = block->next;
	block->next->prev = block->prev;
#endif
	zone_used -= block->size;
	zone_count--;
	block->id = ZONEFREED;
	VR_HeapFree (block);
}


/*
========================
Z_Malloc
========================
*/
void *Z_Malloc (int size)
{
	zblock_t	*block;
	size_t		total;

	Zone_CheckThread ();
	if (size < 0)
		Sys_Error ("Z_Malloc: bad size %i", size);

	total = sizeof (zblock_t) + (size_t) size + ZONE_TAIL;
	block = (zblock_t *) VR_HeapCalloc (1, total);
	if (!block)
		Sys_Error ("Z_Malloc: failed on allocation of %i bytes", size);
	block->size = total;
	block->id = ZONEID;
#ifndef NDEBUG
	{
		const int tail = ZONEID;
		memcpy ((byte *) block + total - ZONE_TAIL, &tail, sizeof (tail));
	}
#endif
#ifdef QVR_TRACY
	block->next = zone_live.next;
	block->prev = &zone_live;
	block->next->prev = block;
	zone_live.next = block;
#endif

	zone_used += total;
	zone_count++;
	if (zone_used > zone_peak)
		zone_peak = zone_used;

	VR_TracyAlloc (block + 1, total, 0, 0);
	return block + 1;
}

char *Z_Strdup (const char *s)
{
	size_t sz = strlen(s) + 1;
	char *ptr = (char *) Z_Malloc (sz);
	memcpy (ptr, s, sz);
	return ptr;
}


//============================================================================

/*
==============================================================================

						HUNK AND CACHE MEMORY

QVR: the hunk and the cache are the heap now (ROUND25.md, "nohunk"; vkQuake's route: its 85d526c replaced the zone, the
hunk and the cache with plain allocation over mimalloc). There is no big block any more (-heapsize, -minmemory: gone),
no segments, and the cache no longer lives in the hunk's free space (it never moves or evicts).

The hunk keeps its lifetimes: every Hunk_Alloc is a heap block of its own, pushed on a stack; Hunk_LowMark is the
bytes on the stack (headers included), and Hunk_FreeToLowMark (mark) frees every block pushed since that mark, newest
first. So the level's data still goes at Host_ClearMemory, a loader's temporary buffers at its own free, as before.
The blocks are no longer one contiguous range: the loaders that built a model on the hunk and copied the range to the
cache (alias models, the VR rigs) hand their blocks to the cache instead (Hunk_MoveToCache): the offsets between them
(aliashdr_t's intptr_t fields) stay valid, as nothing moves.

The cache is owned blocks: Cache_Alloc allocates, Cache_Free frees, Cache_Check is the pointer (no LRU, no purge);
Cache_Flush (a game change) frees them all.

Every block has a header (its size, its name, a sentinel, the links) checked on free and by Hunk_Check; Debug builds
add a tail word after the caller's (16-byte rounded) bytes, checked on free and by Hunk_Check (an overrun).
==============================================================================
*/

#define	HUNK_SENTINEL	0x1df001ed
#define	CACHE_SENTINEL	0x1dca11ed
#define	FREED_SENTINEL	0x1dfee1ed	// a freed block (a double free)

#define MEMNAME_LEN		32
typedef struct memblock_s
{
	struct memblock_s	*hprev;		// hunk: the block under it; a cache entry's owned blocks: the next one
	struct memblock_s	*cprev, *cnext;	// cache: the entries' list
	struct memblock_s	*owned;		// cache: the hunk blocks it took over (Hunk_MoveToCache), chained by hprev
	cache_user_t		*user;		// cache: its owner
	int					size;		// this block: the header, the caller's bytes rounded to 16 (and the Debug tail)
	int					mark;		// hunk: Hunk_LowMark before it
	int					total;		// cache: its bytes with the owned blocks'
	int					sentinel;
	char				name[MEMNAME_LEN];
} memblock_t;

#define MEMHDR			((int) ((sizeof (memblock_t) + 15) & ~15))	// the caller's bytes keep the heap's 16-byte alignment
#define MEMDATA(b)		((void *) ((byte *) (b) + MEMHDR))
#define MEMBLOCK(p)		((memblock_t *) ((byte *) (p) - MEMHDR))

#ifndef NDEBUG
#define MEM_TAIL		((int) sizeof (int))	// Debug: the sentinel after the caller's bytes
#else
#define MEM_TAIL		0
#endif

static memblock_t	*hunk_top;			// the newest hunk block
static int			hunk_low_used;		// the hunk's bytes (Hunk_LowMark)
static int			hunk_peak;			// the most ever (vr_limits)
static int			hunk_count;			// hunk blocks
static int			hunk_keep_mark;		// QVR: Hunk_KeepLoaded: frees stop here
static memblock_t	cache_head;			// the cache entries' list head
static size_t		cache_used, cache_peak;	// the cache's bytes (headers included) and the most ever (vr_limits)
static int			cache_count;		// cache entries

/*
===================
Mem_CheckBlock -- a block's sentinel and (Debug) tail
===================
*/
static void Mem_CheckBlock (const memblock_t *b, int sentinel, const char *who)
{
	if (b->sentinel == FREED_SENTINEL)
		Sys_Error ("%s: a freed block", who);
	if (b->sentinel != sentinel)
		Sys_Error ("%s: trashed sentinel", who);
	if (b->size < MEMHDR + MEM_TAIL)
		Sys_Error ("%s: bad size", who);
#ifndef NDEBUG
	{
		int tail;
		memcpy (&tail, (const byte *) b + b->size - MEM_TAIL, sizeof (tail));
		if (tail != sentinel)
			Sys_Error ("%s: %s's tail was overwritten (an overrun of its %i bytes)", who, b->name[0] ? b->name : "a block",
				b->size - MEMHDR - MEM_TAIL);
	}
#endif
}

/*
===================
Mem_NewBlock -- a block of `size` caller's bytes (zeroed if `clear`), its header filled
===================
*/
static memblock_t *Mem_NewBlock (int size, const char *name, int sentinel, qboolean clear, const char *who)
{
	memblock_t	*b;
	size_t		total = (size_t) MEMHDR + (((size_t) size + 15) & ~(size_t) 15) + MEM_TAIL;

	if (total > 0x7fffffff)
		Sys_Error ("%s: %i bytes are too many", who, size);
	b = (memblock_t *) (clear ? VR_HeapCalloc (1, total) : VR_HeapMalloc (total));
	if (!b)
		Sys_Error ("%s: failed on %i bytes", who, size);
	if (!clear)
		memset (b, 0, sizeof (*b));
	b->size = (int) total;
	b->sentinel = sentinel;
	if (name)
		q_strlcpy (b->name, name, MEMNAME_LEN);
#ifndef NDEBUG
	memcpy ((byte *) b + total - MEM_TAIL, &sentinel, sizeof (sentinel));
#endif
	return b;
}

static void Mem_FreeBlock (memblock_t *b, int sentinel, const char *who)
{
	Mem_CheckBlock (b, sentinel, who);
	b->sentinel = FREED_SENTINEL;
	VR_HeapFree (b);
}

/*
==============
Hunk_Check

Run consistency and sentinel trashing checks (QVR: every hunk block's and cache block's)
==============
*/
void Hunk_Check (void)
{
	const memblock_t *b, *o;

	for (b = hunk_top; b; b = b->hprev)
		Mem_CheckBlock (b, HUNK_SENTINEL, "Hunk_Check");
	for (b = cache_head.cnext; b && b != &cache_head; b = b->cnext)
	{
		Mem_CheckBlock (b, CACHE_SENTINEL, "Hunk_Check (cache)");
		for (o = b->owned; o; o = o->hprev)
			Mem_CheckBlock (o, HUNK_SENTINEL, "Hunk_Check (a cache entry's hunk block)");
	}
}

/*
==============
Hunk_Print

If "all" is specified, every single allocation is printed.
Otherwise, allocations with the same name will be totaled up before printing.
==============
*/
void Hunk_Print (qboolean all)
{
	const memblock_t	**blocks, *b;
	int					i, n, count, sum;

	Hunk_Check ();
	blocks = (const memblock_t **) VR_HeapMalloc ((size_t) q_max (hunk_count, 1) * sizeof (*blocks));
	if (!blocks)
		return;
	n = 0;
	for (b = hunk_top; b && n < hunk_count; b = b->hprev)
		blocks[n++] = b; // newest first: printed oldest first

	Con_SafePrintf ("\n");
	if (all)
		Con_SafePrintf ("      mark :       size : name\n");
	else
		Con_SafePrintf ("allocs :       size : name\n");
	Con_SafePrintf ("---------------------------------\n");

	count = sum = 0;
	for (i = n - 1; i >= 0; i--)
	{
		const char *name = blocks[i]->name[0] ? blocks[i]->name : "unknown";
		count++;
		sum += blocks[i]->size;
		if (all)
			Con_SafePrintf ("%10i : %10i : %s\n", blocks[i]->mark, blocks[i]->size, name);
		if (!i || strncmp (name, blocks[i - 1]->name[0] ? blocks[i - 1]->name : "unknown", MEMNAME_LEN - 1) != 0)
		{
			if (!all)
				Con_SafePrintf ("%6i : %10i : %s\n", count, sum, name);
			count = sum = 0;
		}
	}
	VR_HeapFree ((void *) blocks);

	Con_SafePrintf ("---------------------------------\n");
	Con_SafePrintf ("%6i : %10i : USED (the heap: no maximum)\n", n, hunk_low_used);
	Con_SafePrintf ("%6i : %10i : CACHE (%s)\n", cache_count, (int) q_min (cache_used, (size_t) 0x7fffffff), "cache_print: each");
}

/*
===================
Hunk_Print_f -- johnfitz -- console command to call hunk_print
===================
*/
void Hunk_Print_f (void)
{
	Hunk_Print (Cmd_Argc () > 1 && !q_strcasecmp (Cmd_Argv (1), "all"));
}


/*
===================
Hunk_AllocInternal
===================
*/
static void *Hunk_AllocInternal (int size, const char *name, qboolean clear)
{
	memblock_t	*b;

	Zone_CheckThread ();
	if (size == 0)
		return NULL;
	if (size < 0)
		Sys_Error ("Hunk_Alloc: bad size: %i", size);

	b = Mem_NewBlock (size, name, HUNK_SENTINEL, clear, "Hunk_Alloc");
	if (hunk_low_used > 0x7fffffff - b->size)
		Sys_Error ("Hunk_Alloc: more than 2 GiB on the hunk");
	b->mark = hunk_low_used;
	b->hprev = hunk_top;
	hunk_top = b;
	hunk_low_used += b->size;
	hunk_count++;
	if (hunk_low_used > hunk_peak)
		hunk_peak = hunk_low_used;

	VR_TracyAlloc (MEMDATA (b), b->size, 1, hunk_low_used);
	return MEMDATA (b);
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName (int size, const char *name)
{
	return Hunk_AllocInternal (size, name, true);
}

/*
===================
Hunk_AllocNameNoFill
===================
*/
void *Hunk_AllocNameNoFill (int size, const char *name)
{
	return Hunk_AllocInternal (size, name, false);
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc (int size)
{
	return Hunk_AllocName (size, NULL);
}

/*
===================
Hunk_AllocNoFill
===================
*/
void *Hunk_AllocNoFill (int size)
{
	return Hunk_AllocNameNoFill (size, NULL);
}

int	Hunk_LowMark (void)
{
	return hunk_low_used;
}

/*
===================
Hunk_Usage, Cache_Usage -- QVR (vr_limits)
===================
*/
void Hunk_Usage (int *used, int *peak, int *blocks)
{
	*used = hunk_low_used;
	*peak = hunk_peak;
	*blocks = hunk_count;
}

void Cache_Usage (size_t *used, size_t *peak, int *entries)
{
	*used = cache_used;
	*peak = cache_peak;
	*entries = cache_count;
}

void Hunk_FreeToLowMark (int mark)
{
	Zone_CheckThread ();

	if (mark < 0 || mark > hunk_low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);

	// QVR: never under a model kept on the hunk (Hunk_KeepLoaded): one loaded inside this temporary allocation (a late
	// precache from QuakeC run in a touch or a push, which hold their lists here) stays, and the list under it until the
	// map's end (Hunk_ClearKeep)
	if (mark < hunk_keep_mark && hunk_keep_mark <= hunk_low_used)
	{
		Con_DPrintf ("Hunk_FreeToLowMark: %i bytes held under a model loaded inside them, until the map's end\n",
			hunk_keep_mark - mark);
		mark = hunk_keep_mark;
	}

	VR_TracyHunkReset (mark);
	while (hunk_top && hunk_top->mark >= mark)
	{
		memblock_t *b = hunk_top;
		hunk_top = b->hprev;
		hunk_count--;
		Mem_FreeBlock (b, HUNK_SENTINEL, "Hunk_FreeToLowMark");
	}
	hunk_low_used = hunk_top ? hunk_top->mark + hunk_top->size : 0;
	if (hunk_low_used != mark)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i (inside a block: %i)", mark, hunk_low_used);
}

/*
===================
Hunk_KeepLoaded -- QVR

A model whose data stays on the hunk (a sprite, a brush model) was just loaded: nothing frees under it until
Hunk_ClearKeep. Quake loads them only with the map, with no temporary allocation open; Quake VR also loads them during
play (a late precache: VR_LatePrecacheModel), maybe from QuakeC called while one is open (SV_TouchLinks, VR_TouchLinks,
SV_PushMove keep their lists on the hunk), whose free would otherwise free the model's memory.
===================
*/
void Hunk_KeepLoaded (void)
{
	hunk_keep_mark = hunk_low_used;
}

/*
===================
Hunk_ClearKeep -- QVR: the memory is cleared (Host_ClearMemory), the models with it
===================
*/
void Hunk_ClearKeep (void)
{
	hunk_keep_mark = 0;
}

char *Hunk_Strdup (const char *s, const char *name)
{
	size_t sz = strlen(s) + 1;
	char *ptr = (char *) Hunk_AllocNameNoFill (sz, name);
	memcpy (ptr, s, sz);
	return ptr;
}

/*
===============================================================================

CACHE MEMORY

===============================================================================
*/

static void Cache_Link (memblock_t *b)
{
	if (!cache_head.cnext)
		cache_head.cnext = cache_head.cprev = &cache_head;
	b->cnext = cache_head.cnext;
	b->cprev = &cache_head;
	cache_head.cnext->cprev = b;
	cache_head.cnext = b;
	cache_used += (size_t) b->total;
	cache_count++;
	if (cache_used > cache_peak)
		cache_peak = cache_used;
}

/*
===================
Hunk_MoveToCache -- QVR (ROUND25.md, "nohunk")

What the loaders did as "Cache_Alloc (c, Hunk_LowMark () - start); memcpy (c->data, data, ...); Hunk_FreeToLowMark
(start)", without the copy: every hunk block pushed since `start` goes to the cache entry, `data` (one of those blocks,
the model's header) its data. The blocks stay where they are, so the offsets between them stay valid.
===================
*/
void *Hunk_MoveToCache (int start, cache_user_t *c, void *data, const char *name)
{
	memblock_t	*owner = NULL, *owned = NULL, *b, *next;
	int			total = 0;

	Zone_CheckThread ();
	if (c->data)
		Sys_Error ("Hunk_MoveToCache: already allocated");
	if (start < 0 || start > hunk_low_used)
		Sys_Error ("Hunk_MoveToCache: bad mark %i", start);
	for (b = hunk_top; b && b->mark >= start; b = b->hprev)
		if (MEMDATA (b) == data)
			owner = b;
	if (!owner)
		Sys_Error ("Hunk_MoveToCache: %s's data is not a hunk block above the mark", name);
	if (start < hunk_keep_mark && hunk_keep_mark <= hunk_low_used)
	{
		Con_DPrintf ("Hunk_MoveToCache: %s took a model kept on the hunk with it\n", name);
		hunk_keep_mark = start;
	}

	for (b = hunk_top; b && b->mark >= start; b = next)
	{
		next = b->hprev;
		Mem_CheckBlock (b, HUNK_SENTINEL, "Hunk_MoveToCache");
		VR_TracyFree (MEMDATA (b));
		total += b->size;
		hunk_count--;
		if (b != owner)
		{
			b->hprev = owned;
			owned = b;
		}
	}
	hunk_top = b;
	hunk_low_used = start;

	// the owner becomes a cache block (its tail rewritten with the cache's sentinel)
	owner->hprev = NULL;
	owner->owned = owned;
	owner->user = c;
	owner->total = total;
	owner->sentinel = CACHE_SENTINEL;
#ifndef NDEBUG
	{
		const int s = CACHE_SENTINEL;
		memcpy ((byte *) owner + owner->size - MEM_TAIL, &s, sizeof (s));
	}
#endif
	q_strlcpy (owner->name, name, MEMNAME_LEN);
	Cache_Link (owner);
	c->data = data;
	VR_TracyAlloc (data, (unsigned long long) total, 2, 0);
	return data;
}

/*
============
Cache_Flush

Throw everything out, so new data will be demand cached
============
*/
void Cache_Flush (void)
{
	S_MixJoin (); // QVR: the pool's mix (S_MixKick) done first
	while (cache_head.cnext && cache_head.cnext != &cache_head)
		Cache_Free (cache_head.cnext->user, true); // reclaim the space //johnfitz -- added second argument
}

/*
============
Cache_FlushExcept -- QVR: Cache_Flush, but the entries `keep` accepts stay (a campaign switch's kept alias models)
============
*/
void Cache_FlushExcept (qboolean (*keep) (cache_user_t *c))
{
	memblock_t	*b, *next;

	S_MixJoin (); // QVR: the pool's mix (S_MixKick) done first
	for (b = cache_head.cnext; b && b != &cache_head; b = next)
	{
		next = b->cnext;
		if (!keep (b->user))
			Cache_Free (b->user, true);
	}
}

/*
============
Cache_Print

============
*/
void Cache_Print (void)
{
	const memblock_t	*b;

	for (b = cache_head.cnext; b && b != &cache_head; b = b->cnext)
		Con_Printf ("%8i : %s\n", b->total, b->name);
	Con_Printf ("%8i : %i entries\n", (int) q_min (cache_used, (size_t) 0x7fffffff), cache_count);
}

/*
============
Cache_Report

============
*/
void Cache_Report (void)
{
	Con_DPrintf ("%4.1f megabyte data cache (the heap: no maximum)\n", cache_used / (float)(1024*1024));
}

/*
============
Cache_Init

============
*/
void Cache_Init (void)
{
	cache_head.cnext = cache_head.cprev = &cache_head;

	Cmd_AddCommand ("flush", Cache_Flush);
	Cmd_AddCommand ("cache_print", Cache_Print); // QVR
}

/*
==============
Cache_Free

Frees the memory (QVR: and the hunk blocks it took over: Hunk_MoveToCache)
==============
*/
void Cache_Free (cache_user_t *c, qboolean freetextures) //johnfitz -- added second argument
{
	memblock_t	*b, *o;

	S_MixJoin (); // QVR: the pool's mix (S_MixKick) done first
	Zone_CheckThread ();

	if (!c->data)
		Sys_Error ("Cache_Free: not allocated");

	b = MEMBLOCK (c->data);
	Mem_CheckBlock (b, CACHE_SENTINEL, "Cache_Free");
	if (b->user != c)
		Sys_Error ("Cache_Free: %s is not this owner's", b->name);

	VR_TracyFree (c->data);
	b->cprev->cnext = b->cnext;
	b->cnext->cprev = b->cprev;
	cache_used -= (size_t) b->total;
	cache_count--;
	while ((o = b->owned) != NULL)
	{
		b->owned = o->hprev;
		Mem_FreeBlock (o, HUNK_SENTINEL, "Cache_Free (a hunk block it took over)");
	}
	Mem_FreeBlock (b, CACHE_SENTINEL, "Cache_Free");

	c->data = NULL;

	//johnfitz -- if a model becomes uncached, free the gltextures.  This only works
	//becuase the cache_user_t is the last component of the qmodel_t struct.  Should
	//fail harmlessly if *c is actually part of an sfx_t struct.  I FEEL DIRTY
	if (freetextures)
		TexMgr_FreeTexturesForOwner ((qmodel_t *)(c + 1) - 1);
}



/*
==============
Cache_Check
==============
*/
void *Cache_Check (cache_user_t *c)
{
	Zone_CheckThread ();
	return c->data; // QVR: no LRU to move it in (nothing is purged)
}


/*
==============
Cache_Peek -- QVR: the data, or NULL; from any thread (the sound mix on the thread pool, S_MixKick: the main thread
frees only after joining it)
==============
*/
void *Cache_Peek (const cache_user_t *c)
{
	return c->data;
}

/*
==============
Cache_Alloc
==============
*/
void *Cache_Alloc (cache_user_t *c, int size, const char *name)
{
	memblock_t	*b;

	Zone_CheckThread ();

	if (c->data)
		Sys_Error ("Cache_Alloc: already allocated");

	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

	b = Mem_NewBlock (size, name, CACHE_SENTINEL, true, "Cache_Alloc");
	b->user = c;
	b->total = b->size;
	Cache_Link (b);
	c->data = MEMDATA (b);

	VR_TracyAlloc (c->data, (unsigned long long) b->total, 2, 0);
	return c->data;
}

//============================================================================


/*
========================
Memory_Init
========================
*/
void Memory_Init (void)
{
	zone_main_thread = SDL_ThreadID ();

	Cache_Init ();
	// QVR: no big block any more: the zone, the hunk and the cache are the heap (ROUND25.md, "nozone", "nohunk")

	Cmd_AddCommand ("hunk_print", Hunk_Print_f); //johnfitz
}


#ifdef QVR_TRACY
// Called only when starting a memory session, before allocator mutation. Physical block sizes include headers.
void Memory_TracySnapshot(void)
{
    zblock_t *b;
    const memblock_t *m;
    for(b = zone_live.next; b != &zone_live; b = b->next)
        VR_TracyAlloc(b + 1, b->size, 0, 0);
    for(m = hunk_top; m; m = m->hprev)
        VR_TracyAlloc(MEMDATA(m), m->size, 1, m->mark + m->size);
    for(m = cache_head.cnext; m && m != &cache_head; m = m->cnext)
        VR_TracyAlloc(MEMDATA(m), m->total, 2, 0);
    VR_TracyPlot("Zone physical bytes", (double) zone_used);
    VR_TracyPlot("Hunk physical bytes", hunk_low_used);
}
#endif

#ifdef QVR_TRACY
void Memory_TracyPlots(void)
{
    VR_TracyPlot("Zone physical bytes", (double) zone_used);
    VR_TracyPlot("Hunk physical bytes", hunk_low_used);
    VR_TracyPlot("Cache physical bytes", (double) cache_used);
}
#endif

#ifdef QVR_TRACY
// Independent live allocator traversal reconciles the event ledger, including reset and hand-over boundaries.
void Memory_TracyValidate(void)
{
    zblock_t *b;
    const memblock_t *m;
    unsigned long long count[3] = {0}, bytes[3] = {0};
    int i;
    for(b = zone_live.next; b != &zone_live; b = b->next)
        { ++count[0]; bytes[0] += b->size; }
    for(m = hunk_top; m; m = m->hprev)
        { ++count[1]; bytes[1] += m->size; }
    for(m = cache_head.cnext; m && m != &cache_head; m = m->cnext)
        { ++count[2]; bytes[2] += m->total; }
    for(i = 0; i < 3; ++i) VR_TracyCheckPool(i, count[i], bytes[i]);
}
#endif
