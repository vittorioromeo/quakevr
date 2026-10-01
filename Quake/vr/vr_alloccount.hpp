#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

// vr_alloccount.cpp: the C++ allocations (operator new) the calling thread has made so far (the profiler's per-frame
// "allocations", read on the main thread).


namespace qvr::alloccount
{

[[nodiscard]] za::U64 thisThread();

// vr_alloc_sites: the calling thread's allocations recorded by call stack from traceBegin (the last trace cleared) to
// traceEnd.
void traceBegin();
void traceEnd();

// A place that allocates: the first frame of the stack outside the allocators (Zancle's containers, the standard
// library, the scratch sets), its caller (that of its commonest stack), and how many.
struct Site
{
    za::String where; // "function (file:line)"
    za::String via;
    za::U64 count{0};
    za::U64 viaCount{0};
};
// The last trace's sites, most first (Windows; elsewhere none), its allocations, and those whose stack didn't fit.
[[nodiscard]] za::Vector<Site> traceSites(za::U64& total, za::U64& dropped);

} // namespace qvr::alloccount

// vr_allocsites.cpp: the vr_alloc_sites command, and the end of each host frame (a trace's countdown).
namespace qvr::allocsites
{

void registerCommands();
void frameEnd();

} // namespace qvr::allocsites
