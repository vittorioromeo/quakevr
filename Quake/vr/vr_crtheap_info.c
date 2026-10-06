// The C heap's statistics for the console (vr_crtheap.h): apart from vr_crtheap.c, which can include no C runtime
// header (mimalloc-stats.h includes <string.h>).

#include "vr_crtheap.h"

#include <stdio.h>

#if defined(QVR_MIMALLOC)

#include "mimalloc.h"
#include "mimalloc-stats.h"

static char qvrHeapName[64];

const char* VR_CrtHeapName(void)
{
    if(VR_CrtHeapIsMimalloc())
    {
        const int v = mi_version();
        snprintf(qvrHeapName, sizeof(qvrHeapName), "mimalloc %d.%d.%d", v / 10000, v / 100 % 100, v % 100);
    }
    else
    {
        snprintf(qvrHeapName, sizeof(qvrHeapName), "the C runtime's (%s)", VR_CrtHeapOffReason());
    }
    return qvrHeapName;
}

VR_CrtHeapStats_t VR_CrtHeapStats(void)
{
    VR_CrtHeapStats_t s;
    memset(&s, 0, sizeof(s));
    const double mb = 1.0 / (1024.0 * 1024.0);
    size_t elapsed, user, sys, rss, peakRss, commit, peakCommit, faults;
    mi_process_info(&elapsed, &user, &sys, &rss, &peakRss, &commit, &peakCommit, &faults);
    s.rssMb = rss * mb;
    s.peakRssMb = peakRss * mb;
    s.commitMb = commit * mb;
    s.peakCommitMb = peakCommit * mb;
    if(!VR_CrtHeapIsMimalloc())
    {
        return s;
    }
    // Every thread's counts (merged into the process's when a thread ends, otherwise only this thread's own).
    mi_stats_t st;
    mi_stats_init(&st);
    if(mi_stats_get(&st))
    {
        s.reservedMb = st.reserved.current * mb;
        s.committedMb = st.committed.current * mb;
        s.peakCommittedMb = st.committed.peak * mb;
        s.threads = st.threads.current;
        s.arenas = st.arena_count.total;
    }
    return s;
}

void VR_CrtHeapPrintStats(void (*out)(const char* text, void* arg), void* arg)
{
    if(!VR_CrtHeapIsMimalloc())
    {
        out("mimalloc is off: no statistics\n", arg);
        return;
    }
    mi_stats_print_out(out, arg);
    mi_options_print_out(out, arg);
}

void VR_CrtHeapCollect(void)
{
    if(VR_CrtHeapIsMimalloc())
    {
        mi_collect(true);
    }
}

#else // !QVR_MIMALLOC

#include <string.h>

const char* VR_CrtHeapName(void)
{
    return "the C runtime's (a build without mimalloc)";
}

VR_CrtHeapStats_t VR_CrtHeapStats(void)
{
    VR_CrtHeapStats_t s;
    memset(&s, 0, sizeof(s));
    return s;
}

void VR_CrtHeapPrintStats(void (*out)(const char* text, void* arg), void* arg)
{
    out("a build without mimalloc: no statistics\n", arg);
}

void VR_CrtHeapCollect(void)
{
}

#endif
