// vr_findradius.cpp -- QuakeC's findradius(): the edicts whose middles (origin + (mins + maxs) / 2) lie within a radius,
// not SOLID_NOT, chained in edict order (the last edict first). Quake walks every edict for it; an edict is ~15 KB of
// fields on a big AD map, so each is a page of its own, and 2259 of them cost ~10 us a call, ~230 calls a frame in
// AD's fights (armed humanoids: three each a frame). Here, on a map with many edicts, the area grid finds them instead:
// docs/vr-port/ROUND24.md, "findradius through the area grid".
//
// Exact, by construction: the grid (world.c's nodes, each keeping its edicts' boxes as they were linked) holds every
// edict that is not SOLID_NOT and has been linked since its origin, size and solid last changed, in a box holding its
// middle. Every other edict is "dirty", listed, and tested by itself at each search:
// - QuakeC's stores of .origin, .mins, .maxs (any component) and .solid: the edict index's field watch
//   (vr_edictindex.cpp: OP_ADDRESS, then the STOREP or STOREP_V after the write) marks the edict (markDirty);
// - the engine's: an edict unlinked (SV_UnlinkEdict), freed, taken, cleared or parsed (VR_EdictIndex_Touch), a .solid
//   set back in C (sv_phys.c's pusher, vr_propthink.cpp): VR_FindRadius_Dirty;
// - SV_LinkEdict makes it clean again (VR_FindRadius_Linked), unless its middle is outside its box (NaN, mins > maxs);
// - SV_ClearWorld (a map, a load): every edict dirty until it is linked.
// A search: the dirty list (dropping the clean, the free and the SOLID_NOT: an edict made solid again is marked), then
// the grid's candidates in a box round the sphere that are not dirty; each tested exactly as the walk tests it, then
// sorted into edict order: the walk's chain, field for field. vr_findradius_check 1 runs both and counts differences.
// Small maps walk (vr_findradius_grid_min edicts): there the walk is as quick.

#include "vr_findradius.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"

#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include "vr/vr_ea.h"
#include "vr/vr_ea_table.inc"
#include "vr/vr_ea_impl.inc"
#include "vr/vr_ea_fields.inc"

using namespace qvr;

namespace
{

constexpr unsigned char stateDirty = 1;  // tested by itself (not in the grid as it is now)
constexpr unsigned char stateListed = 2; // in Tracking::listed
constexpr float walkRadius = 4096.f;     // a radius past this walks (the grid would hand back most of a map)
constexpr int checkPrintMax = 12;
constexpr float solidNot = static_cast<float>(SOLID_NOT); // (.solid compared as a float, as Quake's C does)        // differences printed in full until the next vr_findradius_stats

// Which server edicts are dirty (the main thread). Kept across map changes: SV_ClearWorld starts it afresh; released
// (by the memory reports' tools), it starts afresh too (every edict dirty: still exact).
struct Tracking
{
    za::Vector<unsigned char> state; // [max_edicts] stateDirty | stateListed
    za::Vector<int> listed;          // the edicts with stateListed (a dirty one is always listed), in no order
    auto members() { return mem::list(state, listed); }
};
mem::Cache<Tracking> tracking{"findradius", mem::Never};

// One search's lists (the main thread).
struct Lists
{
    za::Vector<edict_t*> candidates; // the grid's
    za::Vector<edict_t*> found;      // the edicts the search chains
    za::Vector<edict_t*> walked;     // vr_findradius_check: the walk's, to compare
    auto members() { return mem::list(candidates, found, walked); }
};
mem::Scratch<Lists> lists{"findradius"};

// Since the last vr_findradius_stats (the main thread).
struct Stats
{
    long long calls{0}, gridCalls{0}, searches{0}, frames{0}, listed{0}, candidates{0}, hits{0}, compared{0}, differing{0};
    unsigned long long cycles{0}, cycles0{0};
    double seconds0{0.}, lastTime{-1.};
    int printed{0};
};
Stats stats;

[[nodiscard]] edict_t* edictAt(int e)
{
    return reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + static_cast<size_t>(e) * sv.qcvm.edict_size);
}

// The server edict's number, or -1 (another VM's, or none).
[[nodiscard]] int serverEdictNumber(const edict_t* ed)
{
    if(!sv.qcvm.edicts || !ed)
    {
        return -1;
    }
    const byte* p = reinterpret_cast<const byte*>(ed);
    const byte* base = reinterpret_cast<const byte*>(sv.qcvm.edicts);
    if(p < base || p >= base + static_cast<size_t>(sv.qcvm.max_edicts) * sv.qcvm.edict_size)
    {
        return -1;
    }
    return static_cast<int>((p - base) / sv.qcvm.edict_size);
}

// Every edict dirty (and listed): nothing is known to be in the grid as it is.
void resetAll(Tracking& t)
{
    const int n = sv.qcvm.max_edicts;
    t.state.assign(static_cast<za::SizeT>(n), static_cast<unsigned char>(stateDirty | stateListed));
    t.state[0] = 0; // (the world: never chained)
    t.listed.clear();
    t.listed.reserve(static_cast<za::SizeT>(n));
    for(int e = 1; e < n; e++)
    {
        t.listed.pushBack(e);
    }
}

// The tracking, sized for the server's edicts; nullptr when there are none.
[[nodiscard]] Tracking* ready()
{
    if(!sv.qcvm.edicts || sv.qcvm.max_edicts <= 0)
    {
        return nullptr;
    }
    Tracking& t = tracking;
    if(static_cast<int>(t.state.size()) != sv.qcvm.max_edicts)
    {
        resetAll(t);
    }
    return &t;
}

void dirty(Tracking& t, int e)
{
    unsigned char& s = t.state[static_cast<za::SizeT>(e)];
    if(!(s & stateListed))
    {
        t.listed.pushBack(e);
    }
    s = stateDirty | stateListed;
}

// The walk's test of one edict (Quake's PF_findradius, as it was: the same expressions, so the same roundings).
[[nodiscard]] inline bool inRadius(const edict_t* ent, const float* org, float rad)
{
    float d, lensq;
    if(ent->free)
    {
        return false;
    }
    if(ent->v.solid == solidNot)
    {
        return false;
    }
    d = org[0] - (ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5);
    lensq = d * d;
    if(lensq > rad)
    {
        return false;
    }
    d = org[1] - (ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5);
    lensq += d * d;
    if(lensq > rad)
    {
        return false;
    }
    d = org[2] - (ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5);
    lensq += d * d;
    return !(lensq > rad);
}

constexpr int walkAhead = 8; // edicts prefetched ahead of the walk

// Quake's walk: every edict, in order.
void walk(const float* org, float rad, za::Vector<edict_t*>& out)
{
    out.clear();
    edict_t* ent = NEXT_EDICT(qcvm->edicts);
    EA_OPEN(59) for(int i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
    {
        // The edicts are kilobytes apart, beyond the hardware prefetchers' reach (a page): the lines this walk reads
        // asked for walkAhead edicts ahead (ROUND24.md, "Tracy session 11-07").
        if(i + walkAhead < qcvm->num_edicts)
        {
            const byte* ahead = reinterpret_cast<const byte*>(ent) + walkAhead * qcvm->edict_size;
            __builtin_prefetch(ahead);
            __builtin_prefetch(ahead + offsetof(edict_t, v.solid));
            __builtin_prefetch(ahead + offsetof(edict_t, v.origin));
            __builtin_prefetch(ahead + offsetof(edict_t, v.mins));
            __builtin_prefetch(ahead + offsetof(edict_t, v.maxs) + sizeof(ent->v.maxs) - 1);
        }
        if(inRadius(ent, org, rad))
        {
            out.pushBack(ent);
        }
    } EA_CLOSE(59)
}

// The grid's search: the dirty edicts, then the grid's clean ones near the sphere; sorted into edict order.
void gridSearch(Tracking& t, const float* org, float rad, za::Vector<edict_t*>& out)
{
    out.clear();
    stats.searches++;
    const int numEdicts = sv.qcvm.num_edicts;
    za::Vector<int>& listed = t.listed;
    za::SizeT kept = 0;
    for(za::SizeT i = 0; i < listed.size(); i++)
    {
        const int e = listed[i];
        unsigned char& s = t.state[static_cast<za::SizeT>(e)];
        if(!(s & stateDirty))
        {
            s = 0; // (linked since)
            continue;
        }
        if(e >= numEdicts)
        {
            s = 0; // (not in use: taken again through ED_Alloc, which marks it)
            continue;
        }
        const edict_t* ent = edictAt(e);
        if(ent->free || ent->v.solid == solidNot)
        {
            s = 0; // (skipped as it is; made solid again, or taken again, it is marked)
            continue;
        }
        listed[kept++] = e;
        if(inRadius(ent, org, rad))
        {
            out.pushBack(edictAt(e));
        }
    }
    stats.listed += static_cast<long long>(kept);
    listed.resize(kept);

    // A box round the sphere, with room for the roundings: a middle within it passes the box (its linked box holds it).
    const float r = sqrtf(rad);
    float lo[3], hi[3];
    for(int k = 0; k < 3; k++)
    {
        const float margin = 1.f + 1e-5f * (fabsf(org[k]) + r);
        lo[k] = org[k] - r - margin;
        hi[k] = org[k] + r + margin;
    }
    za::Vector<edict_t*>& candidates = lists.candidates;
    candidates.resize(static_cast<za::SizeT>(numEdicts));
    int count = 0;
    SV_AreaRadiusCandidates(lo, hi, candidates.data(), &count, numEdicts);
    stats.candidates += count;
    const byte* base = reinterpret_cast<const byte*>(sv.qcvm.edicts);
    for(int i = 0; i < count; i++)
    {
        edict_t* ent = candidates[static_cast<za::SizeT>(i)];
        const int e = static_cast<int>((reinterpret_cast<const byte*>(ent) - base) / sv.qcvm.edict_size);
        if(t.state[static_cast<za::SizeT>(e)] & stateDirty)
        {
            continue; // (tested above)
        }
        if(inRadius(ent, org, rad))
        {
            out.pushBack(ent);
        }
    }
    if(out.size() > 1)
    {
        za::quickSort(out.data(), out.data() + out.size()); // edict order (the edicts are one array)
    }
}

[[nodiscard]] bool finite3(const float* v)
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

// vr_findradius_check: the grid's list against the walk's.
void compare(const za::Vector<edict_t*>& walked, const za::Vector<edict_t*>& grid, const float* org, float radius)
{
    stats.compared++;
    if(walked.size() == grid.size() && (walked.empty() || !memcmp(walked.data(), grid.data(), walked.size() * sizeof(edict_t*))))
    {
        return;
    }
    stats.differing++;
    if(stats.printed >= checkPrintMax)
    {
        return;
    }
    stats.printed++;
    const char* caller = qcvm->xfunction ? PR_GetString(qcvm->xfunction->s_name) : "?";
    Con_Printf("findradius check: %s at %.0f %.0f %.0f r %g: walk %d, grid %d\n", caller, org[0], org[1], org[2], radius,
        static_cast<int>(walked.size()), static_cast<int>(grid.size()));
    // The edicts in one list and not the other (both are in edict order).
    za::SizeT a = 0, b = 0;
    int shown = 0;
    while((a < walked.size() || b < grid.size()) && shown < 4)
    {
        edict_t* ent;
        const char* which;
        if(b == grid.size() || (a < walked.size() && walked[a] < grid[b]))
        {
            ent = walked[a++];
            which = "walk only";
        }
        else if(a == walked.size() || grid[b] < walked[a])
        {
            ent = grid[b++];
            which = "grid only";
        }
        else
        {
            a++;
            b++;
            continue;
        }
        shown++;
        const int e = serverEdictNumber(ent);
        const Tracking& t = tracking;
        Con_Printf("findradius check: %s #%d %s solid %g move %g linked %d state %d\n", which, e,
            PR_GetString(ent->v.classname), ent->v.solid, ent->v.movetype, ent->area.prev != nullptr,
            e >= 0 && e < static_cast<int>(t.state.size()) ? t.state[static_cast<za::SizeT>(e)] : -1);
        Con_Printf("findradius check: origin %.1f %.1f %.1f abs %.1f %.1f %.1f .. %.1f %.1f %.1f\n", ent->v.origin[0],
            ent->v.origin[1], ent->v.origin[2], ent->v.absmin[0], ent->v.absmin[1], ent->v.absmin[2], ent->v.absmax[0],
            ent->v.absmax[1], ent->v.absmax[2]);
    }
}

// vr_findradius_stats: since the last time (then zeroed).
void stats_f()
{
    const double seconds = Sys_DoubleTime() - stats.seconds0;
    const unsigned long long cycles = __rdtsc() - stats.cycles0;
    const double nsPerCycle = stats.cycles0 && cycles ? seconds * 1e9 / static_cast<double>(cycles) : 0.;
    const double frames = stats.frames > 0 ? static_cast<double>(stats.frames) : 1.;
    const double calls = stats.calls > 0 ? static_cast<double>(stats.calls) : 1.;
    const double grids = stats.searches > 0 ? static_cast<double>(stats.searches) : 1.;
    // (short lines: the console wraps at its width)
    Con_Printf("findradius: %lld frames, %.1f calls/frame, %lld by grid, %.3f ms/frame, %.2f us/call\n", stats.frames,
        static_cast<double>(stats.calls) / frames, stats.gridCalls, static_cast<double>(stats.cycles) * nsPerCycle / 1e6 / frames,
        static_cast<double>(stats.cycles) * nsPerCycle / 1e3 / calls);
    Con_Printf("findradius grid: %.1f dirty, %.1f candidates a search; %.1f found a call; %d edicts\n",
        static_cast<double>(stats.listed) / grids, static_cast<double>(stats.candidates) / grids,
        static_cast<double>(stats.hits) / calls, sv.qcvm.edicts ? sv.qcvm.num_edicts : 0);
    Con_Printf("findradius check: %lld compared, %lld differ\n", stats.compared, stats.differing);
    stats = Stats{};
    stats.cycles0 = __rdtsc();
    stats.seconds0 = Sys_DoubleTime();
}

} // namespace

extern "C" edict_t* VR_FindRadius(const float* org, float rad)
{
    const float radius = rad;
    rad *= rad;
    const unsigned long long start = __rdtsc();
    if(!stats.cycles0)
    {
        stats.cycles0 = start;
        stats.seconds0 = Sys_DoubleTime();
    }
    if(qcvm == &sv.qcvm && sv.qcvm.time != stats.lastTime)
    {
        stats.lastTime = sv.qcvm.time;
        stats.frames++;
    }
    stats.calls++;

    Tracking* t = qcvm == &sv.qcvm ? ready() : nullptr;
    // The grid: the server's, a sphere it can search (a number, not most of the map); a big map (or forced).
    const bool gridUsable = t && finite3(org) && rad >= 0.f && rad <= walkRadius * walkRadius;
    const int mode = static_cast<int>(vr_findradius_grid.value);
    const bool check = gridUsable && vr_findradius_check.value != 0.f;
    const bool grid = gridUsable && (mode == 2 || (mode == 1 && qcvm->num_edicts >= static_cast<int>(vr_findradius_grid_min.value)));

    za::Vector<edict_t*>& found = lists.found;
    if(check)
    {
        walk(org, rad, lists.walked);
        gridSearch(*t, org, rad, found);
        compare(lists.walked, found, org, radius);
        found = lists.walked; // (the walk's chain: the game as it was, whatever the grid found)
    }
    else if(grid)
    {
        stats.gridCalls++;
        gridSearch(*t, org, rad, found);
    }
    else
    {
        walk(org, rad, found);
    }

    edict_t* chain = qcvm->edicts;
    for(edict_t* ent : found)
    {
        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }
    stats.hits += static_cast<long long>(found.size());
    stats.cycles += __rdtsc() - start;
    return chain;
}

extern "C" void VR_FindRadius_Dirty(edict_t* ed)
{
    const int e = serverEdictNumber(ed);
    Tracking* t = e > 0 ? ready() : nullptr;
    if(t)
    {
        dirty(*t, e);
    }
}

extern "C" void VR_FindRadius_Linked(edict_t* ed)
{
    const int e = serverEdictNumber(ed);
    Tracking* t = e > 0 ? ready() : nullptr;
    if(!t)
    {
        return;
    }
    unsigned char& s = t->state[static_cast<za::SizeT>(e)];
    if(ed->v.solid != solidNot)
    {
        // In the grid by its abs box: clean only when that box holds its middle (the walk's, in its precision).
        for(int k = 0; k < 3; k++)
        {
            const double middle = ed->v.origin[k] + (ed->v.mins[k] + ed->v.maxs[k]) * 0.5;
            if(!(middle >= ed->v.absmin[k] && middle <= ed->v.absmax[k]))
            {
                dirty(*t, e);
                return;
            }
        }
    }
    s &= static_cast<unsigned char>(~stateDirty); // (still listed: the next search drops it)
}

extern "C" void VR_FindRadius_Reset()
{
    if(sv.qcvm.edicts && sv.qcvm.max_edicts > 0)
    {
        resetAll(tracking);
    }
}

namespace qvr::findradius
{

void registerCommands()
{
    Cmd_AddCommand("vr_findradius_stats", stats_f);
    Cmd_AddCommand("ea_reset", ea_reset_f);
    Cmd_AddCommand("ea_stats", ea_stats_f);
    Cmd_AddCommand("ea_fields", ea_fields_f);
}

void markDirty(int e)
{
    Tracking* t = e > 0 ? ready() : nullptr;
    if(t && e < static_cast<int>(t->state.size()))
    {
        dirty(*t, e);
    }
}

} // namespace qvr::findradius
