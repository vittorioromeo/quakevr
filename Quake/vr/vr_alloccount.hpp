#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

// Per-thread cumulative heap traffic. The profiler's legacy "allocations" remains C++ new only.


namespace qvr::alloccount
{

enum class Kind { New, Delete, Malloc, Calloc, Realloc, Free };
struct Stats
{
    za::U64 calls[6]{};
    za::U64 requestedBytes{}; // request traffic, not live memory; realloc counts the full request
};
[[nodiscard]] Stats statsThisThread();
[[nodiscard]] const char* kindName(Kind kind);
[[nodiscard]] za::U64 thisThread();

// vr_alloc_sites: the calling thread's heap events recorded by call stack from traceBegin (the last trace cleared) to
// traceEnd.
void traceBegin();
void traceEnd();
// Close one traced frame; optionally retain its per-stack counts as the busiest frame.
void traceFrameEnd(bool retainPeak);

// A place that allocates: the first frame of the stack outside the allocators (Zancle's containers, the standard
// library, the scratch sets), its caller (that of its commonest stack), and how many.
struct Site
{
    za::String where; // "function (file:line)"
    za::String via;
    za::U64 count{0};
    za::U64 viaCount{0};
    Kind kind{Kind::New};
    za::U64 bytes{0};
};
// The last trace's sites, most first (Windows; elsewhere none), its heap events, and those whose stack didn't fit.
[[nodiscard]] za::Vector<Site> traceSites(za::U64& total, za::U64& dropped, bool peakFrame = false);
// vr_heap_sites: every thread's blocks of minSize..maxSize bytes from liveBegin (false: not on this platform) to
// liveEnd, kept by stack until freed; traceSites then gives each site's blocks still live (count) and their bytes.
// liveEnd: the blocks traced, those still live, those not placed (a table full).
[[nodiscard]] bool liveBegin(za::U64 minSize, za::U64 maxSize);
void liveEnd(za::U64& allocs, za::U64& live, za::U64& dropped);
[[nodiscard]] bool liveTracing();

} // namespace qvr::alloccount

// vr_allocsites.cpp: the vr_alloc_sites command, and the end of each host frame (a trace's countdown).
namespace qvr::allocsites
{

void registerCommands();
void frameEnd();

} // namespace qvr::allocsites
