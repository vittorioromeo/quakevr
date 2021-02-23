/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

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

#pragma once

/*
 memory allocation


H_??? The hunk manages the entire memory block given to quake.  It must be
contiguous.  Memory can be allocated from either the low or high end in a
stack fashion.  The only way memory is released is by resetting one of the
pointers.

Hunk allocations should be given a name, so the Hunk_Print () function
can display usage.

Hunk allocations are guaranteed to be 16 byte aligned.

The video buffers are allocated high to avoid leaving a hole underneath
server allocations when changing to a higher video mode.


Z_??? Zone memory functions used for small, dynamic allocations like text
strings from command input.  There is only about 48K for it, allocated at
the very bottom of the hunk.

Cache_??? Cache memory is for objects that can be dynamically loaded and
can usefully stay persistant between levels.  The size of the cache
fluctuates from level to level.

To allocate a cachable object


Temp_??? Temp memory is used for file loading and surface caching.  The size
of the cache memory is adjusted so that there is a minimum of 512k remaining
for temp memory.


------ Top of Memory -------

high hunk allocations

<--- high hunk reset point held by vid

video buffer

z buffer

surface cache

<--- high hunk used

cachable memory

<--- low hunk used

client and server low hunk allocations

<-- low hunk reset point held by host

startup hunk allocations

Zone block

----- Bottom of Memory -----



*/

void Memory_Init(void* buf, int size);

void Z_Free(void* ptr);
void* Z_Malloc(int size); // returns 0 filled memory
void* Z_Realloc(void* ptr, int size);
char* Z_Strdup(const char* s);

[[nodiscard]] void* Hunk_Alloc(
    const int size) noexcept; // returns 0 filled memory
[[nodiscard]] void* Hunk_AllocName(const int size, const char* name) noexcept;
[[nodiscard]] void* Hunk_HighAllocName(int size, const char* name) noexcept;
[[nodiscard]] char* Hunk_Strdup(const char* s, const char* name) noexcept;

template <typename T>
[[nodiscard]] T* Hunk_Alloc(const int count) noexcept
{
    return (T*)Hunk_Alloc(count * sizeof(T));
}

template <typename T>
[[nodiscard]] T* Hunk_AllocName(const int count, const char* name) noexcept
{
    return (T*)Hunk_AllocName(count * sizeof(T), name);
}

template <typename T>
[[nodiscard]] T* Hunk_AllocNameAndConstruct(
    const int count, const char* name) noexcept
{
    auto ptr = Hunk_AllocName(count, name);

    for(int i = 0; i < count; ++i)
    {
        new((char*)ptr + count) T{};
    }

    return ptr;
}

[[nodiscard]] int Hunk_LowMark() noexcept;
void Hunk_FreeToLowMark(const int mark) noexcept;

[[nodiscard]] int Hunk_HighMark() noexcept;
void Hunk_FreeToHighMark(const int mark) noexcept;

void* Hunk_TempAlloc(int size);

void Hunk_Check();

typedef struct cache_user_s
{
    void* data;
} cache_user_t;

void Cache_Flush();

void* Cache_Check(cache_user_t* c);
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns nullptr

void Cache_Free(cache_user_t* c,
    bool freetextures); // johnfitz -- added second argument

void* Cache_Alloc(cache_user_t* c, int size, const char* name);
// Returns nullptr if all purgable data was tossed and there still
// wasn't enough room.

void Cache_Report();
