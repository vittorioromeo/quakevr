// vr_mem.cpp -- the registered scratch buffers and caches (vr_mem.hpp): released on their events, counted for
// vr_memstats and vr_limits.

#include "vr_mem.hpp"

#include "vr_engine.hpp"

#include <algorithm>

namespace qvr::mem
{

namespace
{

// The list's head: constant-initialised (zero before any static initialiser runs), so a set registered from another
// file's initialiser finds it ready.
constinit Registration* head = nullptr;

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
        std::size_t bytes;
    };
    Row rows[64];
    int n = 0;
    for(Registration* r = Registration::first(); r && n < 64; r = r->next())
    {
        rows[n++] = {r, r->bytes()};
    }
    std::sort(rows, rows + n, [](const Row& a, const Row& b) { return a.bytes > b.bytes; });
    for(int i = 0; i < n && i < count && rows[i].bytes > 0; i++)
    {
        Con_Printf("    %-12s %-7s %7.1f KiB\n", rows[i].set->system(), rows[i].set->kind() == Kind::Scratch ? "scratch" : "cache",
            static_cast<double>(rows[i].bytes) / 1024.0);
    }
}

} // namespace qvr::mem
