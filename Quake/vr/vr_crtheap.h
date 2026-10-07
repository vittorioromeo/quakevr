// The process's C heap: mimalloc in place of the C runtime's (vr_crtheap.c; external/mimalloc/README.md). The console's
// `vr_heap` (vr_main.cpp) reports it.
#pragma once

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

#ifdef __cplusplus
}
#endif
