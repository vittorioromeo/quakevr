// The C heap's statistics for the console (vr_crtheap.h): apart from vr_crtheap.c, which can include no C runtime
// header (mimalloc-stats.h includes <string.h>).

#include "vr_crtheap.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(QVR_MIMALLOC)

#include "mimalloc.h"
#include "mimalloc-stats.h"

static char qvrHeapName[64];

// mimalloc's messages (its warnings and errors; also on stderr, as without these hooks) and errors (an invalid or double
// free, corrupted heap data: EFAULT; out of memory: ENOMEM), counted for `vr_heap`, the first 2 KB of the messages kept.
static char qvrHeapMessages[2048];
static unsigned qvrHeapMessagesUsed;
static unsigned long long qvrHeapMessageCount, qvrHeapErrorCount;
static int qvrHeapLastError;

static void qvrHeapOutput(const char* message, void* arg)
{
    (void)arg;
    fputs(message, stderr);
    if(!message[strspn(message, " \t\r\n")]) // a bare line break (two at start-up): nothing to count
    {
        return;
    }
    __atomic_fetch_add(&qvrHeapMessageCount, 1, __ATOMIC_RELAXED);
    const unsigned length = (unsigned)strlen(message);
    const unsigned at = __atomic_fetch_add(&qvrHeapMessagesUsed, length, __ATOMIC_RELAXED);
    if(at + 1 < sizeof(qvrHeapMessages)) // (what fits: the last byte stays the terminating zero)
    {
        const unsigned room = (unsigned)sizeof(qvrHeapMessages) - 1 - at;
        memcpy(qvrHeapMessages + at, message, length < room ? length : room);
    }
}

static void qvrHeapError(int error, void* arg)
{
    (void)arg;
    __atomic_fetch_add(&qvrHeapErrorCount, 1, __ATOMIC_RELAXED);
    qvrHeapLastError = error;
#if !defined(NDEBUG)
    if(error == EFAULT) // as mimalloc's own in Debug: a corrupted heap ends the game (the crash report)
    {
        abort();
    }
#endif
}

void VR_CrtHeapHooks(void)
{
    mi_register_output(qvrHeapOutput, NULL);
    mi_register_error(qvrHeapError, NULL);
}

void VR_CrtHeapMessages(unsigned long long* messages, unsigned long long* errors, int* lastError, const char** text)
{
    *messages = __atomic_load_n(&qvrHeapMessageCount, __ATOMIC_RELAXED);
    *errors = __atomic_load_n(&qvrHeapErrorCount, __ATOMIC_RELAXED);
    *lastError = qvrHeapLastError;
    *text = qvrHeapMessages;
}

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

void VR_CrtHeapSetPurgeDelay(long milliseconds)
{
    if(VR_CrtHeapIsMimalloc())
    {
        mi_option_set(mi_option_purge_delay, milliseconds);
    }
}

void VR_CrtHeapCollect(void)
{
    if(VR_CrtHeapIsMimalloc())
    {
        mi_collect(true);
    }
}

#else // !QVR_MIMALLOC

const char* VR_CrtHeapName(void)
{
    return "the C runtime's (a build without mimalloc)";
}

void VR_CrtHeapHooks(void)
{
}

void VR_CrtHeapMessages(unsigned long long* messages, unsigned long long* errors, int* lastError, const char** text)
{
    *messages = *errors = 0;
    *lastError = 0;
    *text = "";
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

void VR_CrtHeapSetPurgeDelay(long milliseconds)
{
    (void)milliseconds;
}

void VR_CrtHeapCollect(void)
{
}

#endif
