// The process's C heap: mimalloc in place of the C runtime's (vr_crtheap.c; external/mimalloc/README.md). The console's
// `vr_heap` (vr_main.cpp) reports it.
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// mimalloc serves malloc/free and new/delete (0: the C runtime's heap).
int VR_CrtHeapIsMimalloc(void);
// Why the C runtime's heap serves them: "-nomimalloc", "QVR_MIMALLOC=0", "a build without mimalloc" ("" with mimalloc).
const char* VR_CrtHeapOffReason(void);
// The heap, for the console: "mimalloc 3.5.4" or "the C runtime's (-nomimalloc)".
const char* VR_CrtHeapName(void);
// The pointer is a mimalloc block (0 for the C runtime's heap, other DLLs' heaps, the stack, null).
int VR_CrtHeapOwns(const void* pointer);
// Frees, reallocations and size queries of blocks mimalloc did not hand out, sent to the C runtime's heap: blocks the C
// runtime's own functions allocated (_getcwd(NULL), _fullpath(NULL), ...). Counted, each a cross-heap call to look at.
unsigned long long VR_CrtHeapForeignCalls(void);

// The process's memory as mimalloc sees it, MB (zeros without mimalloc).
typedef struct
{
    double rssMb, peakRssMb, commitMb, peakCommitMb; // the process's (working set; commit charge)
    double reservedMb, committedMb, peakCommittedMb; // mimalloc's own arenas
    long long threads, arenas;
} VR_CrtHeapStats_t;
VR_CrtHeapStats_t VR_CrtHeapStats(void);
// mimalloc's own statistics table (mi_stats_print_out) and its options, piece by piece (each piece may hold several
// lines or part of one).
void VR_CrtHeapPrintStats(void (*out)(const char* text, void* arg), void* arg);
// mimalloc's messages and errors (counted; the first 2 KB of the messages kept; also on stderr): its error handler and
// output, installed at the first allocation (vr_crtheap.c). In Debug, a corrupted heap (EFAULT) aborts, as mimalloc's own.
void VR_CrtHeapHooks(void);
void VR_CrtHeapMessages(unsigned long long* messages, unsigned long long* errors, int* lastError, const char** text);
// How long mimalloc keeps freed memory before it goes back to the OS (ms; 0 at once, -1 never): vr_heap_purge_delay.
void VR_CrtHeapSetPurgeDelay(long milliseconds);
// A map load (VR_TimeLoadBegin on, its first frame drawn or its failure off): while it runs, freed memory is kept for
// `milliseconds` (the purge delay raised, when shorter) and so used again by the load's own allocations instead of being
// given back to the OS and asked for again (vrstart's hull build: 37% of its CPU time in VirtualAlloc/VirtualFree, its
// 32 threads queueing on the kernel); at the end it is all given back at once (mi_collect(true)) and the delay put back.
// 0: off (vr_heap_load_hold).
void VR_CrtHeapLoadHold(int on, long milliseconds);
// Returns the memory mimalloc holds unused to the OS (mi_collect(true), on this thread's heap and the abandoned ones).
void VR_CrtHeapCollect(void);
// A job worker's own heap tidied (mi_collect(true) on it): VR_CrtHeapThreadTidy after each job, at most once a period;
// VR_CrtHeapIdleTidy once the worker has had no job for a period (Zancle's idle hook; vr_jobs.cpp). mimalloc's pages
// belong to the thread that made them, and only that thread gives back the ones that frees on other threads emptied
// (the main thread freeing what a job made): an idle worker kept them all, committed -- after a map load, ~140 MB of
// empty pages in single player; on a 16-player server the heap grew in steps as fights' jobs ran (B22).
// vr_heap_thread_tidy: the period, ms (0: never).
void VR_CrtHeapThreadTidy(void);
void VR_CrtHeapIdleTidy(void);
void VR_CrtHeapSetThreadTidy(long milliseconds);
long VR_CrtHeapThreadTidyPeriod(void);
unsigned long long VR_CrtHeapThreadTidies(int idle); // (how many so far, all threads; idle: the idle ones, else after a job)
unsigned long long VR_CrtHeapThreadTidyMicroseconds(int idle); // (their time)
// The live blocks by block size (vr_heap live): each of mimalloc's pages counted at its size (its blocks in use and
// committed bytes -- the blocks made so far -- and its whole size, reserved; blocks other threads freed but its owner
// has not taken back yet count as live; the pages with no block in use, held). Fills up to `maxRows` rows (the overflow
// adds into the last one) and returns how many; 0 without mimalloc. Diagnostics only: it reads the pages while other
// threads may be changing them.
typedef struct
{
    unsigned long long blockSize, pages, blocks, liveBytes, committedBytes, reservedBytes, emptyPages, emptyBytes;
    unsigned long long emptyAbandonedBytes, emptyOtherBytes; // (of emptyBytes: abandoned; another thread's, not the caller's)
} VR_CrtHeapSizeRow_t;
int VR_CrtHeapLiveBySize(VR_CrtHeapSizeRow_t* rows, int maxRows);
// The live-block trace (vr_heap_sites, vr_allocsites.cpp): while set, `fn` is called on every thread with each block
// malloc, calloc, realloc and _aligned_malloc/_aligned_realloc return (its requested size) and with each block freed
// or moved by realloc (size 0) -- the process's every C and C++ allocation through these overrides (Windows with
// mimalloc; elsewhere a no-op). null: off.
typedef void (*VR_CrtHeapTraceFn)(void* block, size_t size);
void VR_CrtHeapSetTrace(VR_CrtHeapTraceFn fn);

#ifdef __cplusplus
}
#endif
