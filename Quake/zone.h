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

#ifndef __ZZONE_H
#define __ZZONE_H

/*
 Memory (QVR: every allocation is a block of the heap, mimalloc's; the API names three lifetime groups over it, plus
 a scratch stack. zone.c, ROUND25.md "nozone", "nohunk")

Z_???   Single allocations: Z_Malloc returns zeroed memory, Z_Free releases it (Z_Free (NULL) is fine). For small,
        dynamic things (command text, strings) whose owner frees them.

Hunk_???  Lifetimes by mark: each allocation is zeroed, 16 byte aligned, a block of its own, and named for hunk_print.
        Hunk_LowMark is a mark (the bytes allocated so far); Hunk_FreeToLowMark frees every block allocated since it,
        and is the only way hunk memory is released (a map's data, an upload's temporaries). A loader that built a
        model on the hunk can hand its blocks to the cache (Hunk_MoveToCache). Hunk_KeepLoaded holds a model loaded
        mid-play: no free goes below it until Hunk_ClearKeep.

Cache_???  Flushable handles for model and sound data that may stay loaded between levels: owned blocks, freed by
        Cache_Free and Cache_Flush (a game change); never moved or purged.

Temp_???  Per-frame scratch: a stack of reused chunks for short-lived lists, with marks like the hunk's.
*/

void Memory_Init (void);
void Memory_InitCvars (void); // QVR: vr_zone_threadcheck (after Cvar_Init)

#ifdef __cplusplus
extern "C" {
#endif
void Z_Free (void *ptr);			// QVR: Z_Free (NULL) does nothing
void *Z_Malloc (int size);			// returns 0 filled memory
char *Z_Strdup (const char *s);
#ifdef __cplusplus
}
#endif

void *Hunk_Alloc (int size); // returns 0 filled memory
void *Hunk_AllocName (int size, const char *name); // returns 0 filled memory
char *Hunk_Strdup (const char *s, const char *name);

// QVR: short-lived scratch for a frame's lists (SV_TouchLinks, SV_PushMove, VR_TouchLinks...: they were hunk
// temporaries, one heap block each since "nohunk"): a stack in grow-only chunks kept between uses, so no heap call once
// warm. Nests like the hunk's marks; not zeroed; main thread only; Host_Error and Host_ClearMemory empty it.
int		Temp_Mark (void);
void	*Temp_Alloc (size_t size);
void	Temp_FreeToMark (int mark);
void	Temp_Usage (int *chunks, int *bytes, int *peak);

int	Hunk_LowMark (void);
void Hunk_FreeToLowMark (int mark);
void Hunk_KeepLoaded (void); // QVR: a model just loaded on the hunk: no free goes under it (zone.c)
void Hunk_ClearKeep (void); // QVR: Host_ClearMemory's

void Hunk_Check (void);
void Hunk_Usage (int *used, int *peak, int *blocks); // QVR: the hunk's bytes (headers included), most ever, blocks (vr_limits)

typedef struct cache_user_s
{
	void	*data;
} cache_user_t;

void Cache_Flush (void);

void *Cache_Check (cache_user_t *c);
void *Cache_Peek (const cache_user_t *c); // QVR: Cache_Check without the LRU's move, from any thread (the sound mix on the pool)
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void Cache_Free (cache_user_t *c, qboolean freetextures); //johnfitz -- added second argument
void Cache_FlushExcept (qboolean (*keep) (cache_user_t *c)); // QVR: Cache_Flush but what keep accepts

void *Cache_Alloc (cache_user_t *c, int size, const char *name);
// QVR: returns zeroed memory (a failed allocation is a Sys_Error)
void *Hunk_MoveToCache (int start, cache_user_t *c, void *data, const char *name);
// QVR: every hunk block since the mark `start` goes to the cache entry, `data` (one of them) its data: what
// "Cache_Alloc (c, Hunk_LowMark () - start); memcpy (c->data, data, ...); Hunk_FreeToLowMark (start)" did, without the copy
void Cache_Usage (size_t *used, size_t *peak, int *entries); // QVR: vr_limits

void Cache_Report (void);

#endif	/* __ZZONE_H */

