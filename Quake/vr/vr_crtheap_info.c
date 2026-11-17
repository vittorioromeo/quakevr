// The C heap's statistics for the console (vr_crtheap.h): apart from vr_crtheap.c, which can include no C runtime
// header (mimalloc-stats.h includes <string.h>).

#include "vr_crtheap.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(QVR_MIMALLOC)

#include "mimalloc.h"
#include "mimalloc-stats.h"
#include "mimalloc/internal.h" // (vr_heap live: an empty page's owner)

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

// A map load's hold (VR_CrtHeapLoadHold): the delay it raised and the one to put back at its end.
static int loadHeld;
static long loadHeldDelay;

void VR_CrtHeapSetPurgeDelay(long milliseconds)
{
    if(VR_CrtHeapIsMimalloc())
    {
        if(loadHeld)
        {
            loadHeldDelay = milliseconds; // (put back at the load's end)
            return;
        }
        mi_option_set(mi_option_purge_delay, milliseconds);
    }
}

void VR_CrtHeapLoadHold(int on, long milliseconds)
{
    if(!VR_CrtHeapIsMimalloc())
    {
        return;
    }
    if(on && !loadHeld)
    {
        const long delay = mi_option_get(mi_option_purge_delay);
        if(milliseconds <= 0 || delay < 0 || delay >= milliseconds)
        {
            return; // (off, or the heap already keeps freed memory as long)
        }
        loadHeld = 1;
        loadHeldDelay = delay;
        mi_option_set(mi_option_purge_delay, milliseconds);
    }
    else if(!on && loadHeld)
    {
        loadHeld = 0;
        mi_collect(true); // what the load freed and kept, purged (first: at a delay of 0 mimalloc purges nothing kept)
        mi_option_set(mi_option_purge_delay, loadHeldDelay);
    }
}

void VR_CrtHeapCollect(void)
{
    if(VR_CrtHeapIsMimalloc())
    {
        mi_collect(true);
    }
}

// VR_CrtHeapThreadTidy: each thread's last collect (ms of TIME_UTC), and the period (vr_heap_thread_tidy).
static long qvrTidyMs = 1000;
static _Thread_local long long qvrTidyLast;
static unsigned long long qvrTidies[2], qvrTidyUs[2]; // (vr_heap live: how many so far, their time; [1] idle)

// After a job: mimalloc's normal collect (the pages emptied go back; quick, on the job's thread). Idle: forced (its full
// pages visited too, and the arenas' purges: ~0.2 ms, nothing waits on an idle worker).
static void qvrTidyTimed(int idle)
{
    struct timespec t0, t1;
    timespec_get(&t0, TIME_UTC);
    mi_collect(idle != 0);
    timespec_get(&t1, TIME_UTC);
    const long long us = ((long long)t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
    __atomic_fetch_add(&qvrTidies[idle], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&qvrTidyUs[idle], (unsigned long long)(us > 0 ? us : 0), __ATOMIC_RELAXED);
}

void VR_CrtHeapSetThreadTidy(long milliseconds)
{
    __atomic_store_n(&qvrTidyMs, milliseconds, __ATOMIC_RELAXED);
}

void VR_CrtHeapThreadTidy(void)
{
    const long period = __atomic_load_n(&qvrTidyMs, __ATOMIC_RELAXED);
    if(period <= 0 || !VR_CrtHeapIsMimalloc())
    {
        return;
    }
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    const long long now = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if(now - qvrTidyLast < period)
    {
        return;
    }
    qvrTidyLast = now;
    qvrTidyTimed(0);
}

void VR_CrtHeapIdleTidy(void)
{
    if(__atomic_load_n(&qvrTidyMs, __ATOMIC_RELAXED) <= 0 || !VR_CrtHeapIsMimalloc())
    {
        return;
    }
    qvrTidyTimed(1);
}

unsigned long long VR_CrtHeapThreadTidyMicroseconds(int idle)
{
    return __atomic_load_n(&qvrTidyUs[idle != 0], __ATOMIC_RELAXED);
}

long VR_CrtHeapThreadTidyPeriod(void)
{
    return __atomic_load_n(&qvrTidyMs, __ATOMIC_RELAXED);
}

unsigned long long VR_CrtHeapThreadTidies(int idle)
{
    return __atomic_load_n(&qvrTidies[idle != 0], __ATOMIC_RELAXED);
}

// VR_CrtHeapLiveBySize: one row per block size (the pages' counts, not a walk of their blocks).
typedef struct
{
    VR_CrtHeapSizeRow_t* rows;
    int maxRows, used;
} QvrLiveVisit;

static bool qvrLivePage(const mi_heap_t* heap, const mi_heap_area_t* area, void* block, size_t blockSize, void* arg)
{
    (void)heap;
    (void)block;
    QvrLiveVisit* v = (QvrLiveVisit*)arg;
    int i = 0;
    while(i < v->used && v->rows[i].blockSize != blockSize)
    {
        ++i;
    }
    if(i == v->used)
    {
        if(v->used == v->maxRows)
        {
            i = v->maxRows - 1; // (full: the rest go to the last row)
        }
        else
        {
            memset(&v->rows[i], 0, sizeof(v->rows[i]));
            v->rows[i].blockSize = blockSize;
            ++v->used;
        }
    }
    VR_CrtHeapSizeRow_t* r = &v->rows[i];
    r->pages++;
    r->blocks += area->used;
    r->liveBytes += (unsigned long long)area->used * area->full_block_size;
    r->committedBytes += area->committed;
    r->reservedBytes += area->reserved;
    if(area->used == 0)
    {
        r->emptyPages++;
        r->emptyBytes += area->reserved;
        const mi_page_t* page = (const mi_page_t*)area->reserved1;
        if(mi_page_is_abandoned(page))
        {
            r->emptyAbandonedBytes += area->reserved;
        }
        else if(mi_page_thread_id(page) != _mi_thread_id())
        {
            r->emptyOtherBytes += area->reserved;
        }
    }
    return true;
}

static bool qvrLiveHeap(mi_heap_t* heap, void* arg)
{
    mi_heap_visit_blocks(heap, false, &qvrLivePage, arg);
    return true;
}

int VR_CrtHeapLiveBySize(VR_CrtHeapSizeRow_t* rows, int maxRows)
{
    if(!VR_CrtHeapIsMimalloc() || maxRows <= 0)
    {
        return 0;
    }
    QvrLiveVisit v = {rows, maxRows, 0};
    mi_subproc_visit_heaps(mi_subproc_main(), &qvrLiveHeap, &v);
    return v.used;
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

void VR_CrtHeapLoadHold(int on, long milliseconds)
{
    (void)on;
    (void)milliseconds;
}

void VR_CrtHeapCollect(void)
{
}

void VR_CrtHeapSetThreadTidy(long milliseconds)
{
    (void)milliseconds;
}

void VR_CrtHeapThreadTidy(void)
{
}

unsigned long long VR_CrtHeapThreadTidies(int idle)
{
    (void)idle;
    return 0;
}

void VR_CrtHeapIdleTidy(void)
{
}

unsigned long long VR_CrtHeapThreadTidyMicroseconds(int idle)
{
    (void)idle;
    return 0;
}

long VR_CrtHeapThreadTidyPeriod(void)
{
    return 0;
}

int VR_CrtHeapLiveBySize(VR_CrtHeapSizeRow_t* rows, int maxRows)
{
    (void)rows;
    (void)maxRows;
    return 0;
}

#endif
