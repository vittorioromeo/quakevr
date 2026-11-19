// vr_mem.cpp -- the registered scratch buffers and caches (vr_mem.hpp): released on their events, counted for
// vr_memstats and vr_limits; and the registered resets, run in their order at their events.

#include "vr_mem.hpp"

#include "vr_engine.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"


namespace qvr::mem
{

namespace
{

// The list's head: constant-initialised (zero before any static initialiser runs), so a set registered from another
// file's initialiser finds it ready.
constinit Registration* head = nullptr;

// The resets' list, in their order (event, phase, system): constant-initialised as the sets' head.
constinit Reset* resetHead = nullptr;

// vr_resets' watch on resetAll (main thread): each reset printed as it runs, and kept until vr_resets takes them.
struct ResetWatch
{
    bool trace{false};
    bool record{false};
    za::Vector<ResetCall> calls;
};
ResetWatch resetWatch;

// Whether `a`'s place in the list is before `b`'s.
[[nodiscard]] bool before(const Reset& a, const Reset& b)
{
    if(a.event() != b.event())
    {
        return a.event() < b.event();
    }
    if(a.phase() != b.phase())
    {
        return a.phase() < b.phase();
    }
    return ZA_STRCMP(a.system(), b.system()) < 0;
}

} // namespace

Registration::Registration(const char* system, Kind kind, unsigned events) noexcept
    : system_(system), kind_(kind), events_(events), next_(head)
{
    head = this;
}

Registration::~Registration()
{
    for(Registration** r = &head; *r; r = &(*r)->next_)
    {
        if(*r == this)
        {
            *r = next_;
            break;
        }
    }
}

Registration* Registration::first()
{
    return head;
}

void on(Event event)
{
    for(Registration* r = Registration::first(); r; r = r->next())
    {
        if(r->events() & event)
        {
            r->release();
        }
    }
}

Totals totals()
{
    Totals t;
    for(Registration* r = Registration::first(); r; r = r->next())
    {
        if(r->kind() == Kind::Scratch)
        {
            t.scratchBytes += r->bytes();
            t.scratchSets++;
        }
        else
        {
            t.cacheBytes += r->bytes();
            t.cacheSets++;
        }
    }
    return t;
}

void printLargest(int count)
{
    struct Row
    {
        Registration* set;
        za::SizeT bytes;
    };
    Row rows[64];
    int n = 0;
    for(Registration* r = Registration::first(); r; r = r->next()) // every set (about 100): keep the 64 largest
    {
        const Row row{r, r->bytes()};
        if(n < 64)
        {
            rows[n++] = row;
            continue;
        }
        int smallest = 0;
        for(int i = 1; i < n; i++)
        {
            smallest = rows[i].bytes < rows[smallest].bytes ? i : smallest;
        }
        if(row.bytes > rows[smallest].bytes)
        {
            rows[smallest] = row;
        }
    }
    za::quickSort(rows, rows + n, [](const Row& a, const Row& b) { return a.bytes > b.bytes; });
    for(int i = 0; i < n && i < count && rows[i].bytes > 0; i++)
    {
        Con_Printf("    %-12s %-7s %7.1f KiB\n", rows[i].set->system(), rows[i].set->kind() == Kind::Scratch ? "scratch" : "cache",
            static_cast<double>(rows[i].bytes) / 1024.0);
    }
}

const char* name(ResetEvent event)
{
    switch(event)
    {
        case ResetEvent::ServerWorld: return "server";
        case ResetEvent::ClientState: return "client";
        case ResetEvent::Test: return "test";
        case ResetEvent::Count: break;
    }
    return "?";
}

const char* name(ResetPhase phase)
{
    switch(phase)
    {
        case ResetPhase::World: return "world";
        case ResetPhase::State: return "state";
        case ResetPhase::Settings: return "settings";
    }
    return "?";
}

Reset::Reset(const char* system, ResetEvent event, ResetPhase phase, void (*forget)()) noexcept
    : system_(system), event_(event), phase_(phase), forget_(forget), next_(nullptr)
{
    // After every reset whose place is not after this one's (one with the same event, phase and name: vr_resets test
    // fails on it, its place being the initialisers' order).
    Reset** at = &resetHead;
    while(*at && !before(*this, **at))
    {
        at = &(*at)->next_;
    }
    next_ = *at;
    *at = this;
}

Reset::~Reset()
{
    for(Reset** r = &resetHead; *r; r = &(*r)->next_)
    {
        if(*r == this)
        {
            *r = next_;
            break;
        }
    }
}

const Reset* Reset::first()
{
    return resetHead;
}

void resetAll(ResetEvent event)
{
    ResetWatch& w = resetWatch;
    if(w.trace)
    {
        Con_Printf("reset %s begin\n", name(event));
    }
    if(w.record)
    {
        w.calls.pushBack(ResetCall{event, nullptr});
    }
    for(const Reset* r = Reset::first(); r; r = r->next())
    {
        if(r->event() != event)
        {
            continue;
        }
        if(w.trace)
        {
            Con_Printf("reset %s %s (%s)\n", name(event), r->system(), name(r->phase()));
        }
        if(w.record)
        {
            w.calls.pushBack(ResetCall{event, r});
        }
        r->run();
    }
}

int resetCount(ResetEvent event)
{
    int n = 0;
    for(const Reset* r = Reset::first(); r; r = r->next())
    {
        n += r->event() == event ? 1 : 0;
    }
    return n;
}

void traceResets(bool on)
{
    resetWatch.trace = on;
}

void recordResets(bool on)
{
    resetWatch.record = on;
}

za::Vector<ResetCall> takeRecordedResets()
{
    za::Vector<ResetCall> taken = ZA_MOVE(resetWatch.calls);
    resetWatch.calls.clear();
    return taken;
}

} // namespace qvr::mem
