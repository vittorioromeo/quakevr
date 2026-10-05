#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Engine-owned CRT calls. Compatible with ordinary CRT pointers; no ownership header.
   Counts are per thread. Third-party DLL allocations are outside this wrapper. */
void* VR_HeapMalloc(size_t size);
void* VR_HeapCalloc(size_t count, size_t size);
void* VR_HeapRealloc(void* pointer, size_t size);
void VR_HeapFree(void* pointer);
/* Aligned allocations must be paired with the aligned free (Box3D callbacks). */
void* VR_HeapAlignedAlloc(size_t size, size_t alignment);
void VR_HeapAlignedFree(void* pointer);
#ifdef __cplusplus
}
#endif
