// vr_crates.cpp -- wooden crates lying about the maps (their places), and what monsters can't see through; see
// vr_crates.hpp and docs/vr-port/ROUND21.md, "Wooden crates". The models are Misc/quakevr/make_crates.py's; QC
// vr_crates.qc spawns them, breaks them and drops what they hold.

#include "vr_crates.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_held.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <string.h>

namespace qvr::crates
{
namespace
{

using progs::fields;

// The crates' models and their half sizes (units, unscaled: make_crates.py's CRATES; the explosive boxes' size).
struct CrateModel
{
    const char* name;
    glm::vec3 half;
};
constexpr CrateModel models[] = {
    {"progs/vr_crate1.mdl", {16.f, 16.f, 16.f}}, // small (the small explosive box's size)
    {"progs/vr_crate2.mdl", {20.f, 20.f, 24.f}}, // large
};
constexpr int numModels = static_cast<int>(za::getArraySize(models));
constexpr int numSkins = 3; // pine, brown, weathered

// Random numbers: a seed from the map's name, the same sequence at every load (vr_debris.cpp's).
struct Rng
{
    uint64_t s;
    uint64_t next()
    {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniform() { return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24); }
    float range(float a, float b) { return a + (b - a) * uniform(); }
};

[[nodiscard]] uint64_t hashString(const char* s)
{
    uint64_t h = 1469598103934665603ull;
    for(; *s; s++)
    {
        h = (h ^ static_cast<unsigned char>(*s)) * 1099511628211ull;
    }
    return h;
}

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] trace_t traceLine(const glm::vec3& a, const glm::vec3& b)
{
    vec3_t start{a.x, a.y, a.z}, end{b.x, b.y, b.z};
    return SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, qcvm->edicts);
}

// The player's box (Quake's hull 1) from `a` to `b`, the level only.
[[nodiscard]] trace_t tracePlayer(const glm::vec3& a, const glm::vec3& b)
{
    vec3_t start{a.x, a.y, a.z}, end{b.x, b.y, b.z};
    vec3_t mins{-16.f, -16.f, -24.f}, maxs{16.f, 16.f, 32.f};
    return SV_Move(start, mins, maxs, end, MOVE_NOMONSTERS, qcvm->edicts);
}

[[nodiscard]] int contents(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return SV_PointContents(v);
}

[[nodiscard]] const qmodel_t* serverModel(const char* name)
{
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            return sv.models[i];
        }
    }
    return nullptr;
}

// A crate's size as drawn: its model's Size (Held Object Offsets).
[[nodiscard]] float sizeOf(int model)
{
    const qmodel_t* m = serverModel(models[model].name);
    return m ? props::drawnSize(m) : 1.f;
}

// A crate turned: its model's axes in the world (columns), and its half extents along the world's forward (its yaw),
// left and up. `face`: which of its six faces is up; its yaw about the vertical.
struct Orient
{
    glm::mat3 axes{1.f};
    float yaw{0.f};
    float hf{0.f}, hl{0.f}, hu{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f}, left{0.f, 1.f, 0.f};

    // Its half extent along the horizontal unit vector `d`.
    [[nodiscard]] float along(const glm::vec2& d) const
    {
        return qza::abs(glm::dot(d, glm::vec2{fwd})) * hf + qza::abs(glm::dot(d, glm::vec2{left})) * hl;
    }
};

[[nodiscard]] Orient orient(int model, int face, int forwardPick, float yawDeg)
{
    const glm::vec3 half = models[model].half * sizeOf(model);
    const int ua = face / 2;
    const float us = (face & 1) ? -1.f : 1.f;
    const int fa = (ua + 1 + (forwardPick & 1)) % 3;
    const float fs = (forwardPick & 2) ? -1.f : 1.f;
    glm::vec3 mUp{0.f}, mFwd{0.f};
    mUp[ua] = us;
    mFwd[fa] = fs;
    const glm::vec3 mLeft = glm::cross(mUp, mFwd);
    Orient o;
    o.yaw = yawDeg;
    const float y = glm::radians(yawDeg);
    o.fwd = {za::cos(y), za::sin(y), 0.f};
    const glm::vec3 up{0.f, 0.f, 1.f};
    o.left = glm::cross(up, o.fwd);
    o.axes = glm::mat3{o.fwd, o.left, up} * glm::transpose(glm::mat3{mFwd, mLeft, mUp});
    o.hf = half[fa];
    o.hl = half[3 - ua - fa];
    o.hu = half[ua];
    return o;
}

struct Placement
{
    int model;
    int skin;
    Orient o;
    glm::vec3 centre;  // the box's middle (the model's origin)
    float floorZ;      // what it stands on (the floor, or the crate under it's top)
    float clearance;   // open floor in front of it (units; a stacked one: its base's)
    int below;         // the crate it stands on (-1: the floor)
    bool corner;
    glm::vec2 out;     // the way out of the wall it stands by
    int ringOpen;      // of the 12 places round it the player fits (Planner::arcs; a stacked one: its base's)
};

za::Vector<Placement> placements;

// A crowbar lying on a crate's top (vr_crate_crowbar): the crate (the top one of a stack), its yaw, where on the top
// (-1 .. 1 of the room left along the top's forward and left: put fits it there by its drawn box).
struct CrowbarOn
{
    int crate;
    float yaw;
    float u, v;
};
za::Vector<CrowbarOn> crowbars;

// Rolls the crowbars on the crates' tops (after the plan): each top (a crate with none on it) at vr_crate_crowbar's
// chance, at most vr_crate_crowbar_max; a random way and place. Its own random numbers (from the map's name and
// vr_crates_seed): the crates' layout is the same whatever the chance.
void planCrowbars(uint64_t seed)
{
    crowbars.clear();
    const float chance = vr_crate_crowbar.value;
    const int most = static_cast<int>(za::max(vr_crate_crowbar_max.value, 0.f));
    if(chance <= 0.f || most <= 0)
    {
        return;
    }
    Rng rng{seed ^ 0x6A09E667F3BCC909ull};
    za::Vector<bool> covered(placements.size(), false);
    for(const Placement& p : placements)
    {
        if(p.below >= 0)
        {
            covered[static_cast<size_t>(p.below)] = true;
        }
    }
    for(size_t i = 0; i < placements.size() && static_cast<int>(crowbars.size()) < most; i++)
    {
        if(covered[i] || rng.uniform() >= chance)
        {
            continue;
        }
        const float yaw = rng.range(0.f, 360.f), u = rng.range(-1.f, 1.f), v = rng.range(-1.f, 1.f);
        crowbars.pushBack({static_cast<int>(i), yaw, u, v});
    }
}

// Rejection reasons, counted for vr_debug_crates.
enum Reason
{
    RNoFloor,
    RUneven,
    RSolid,
    RLow,
    RLiquid,
    RBacked,
    RPassage,
    RSplits,
    REntity,
    RSpacing,
    RCount
};
constexpr const char* reasonNames[RCount] = {"no floor", "uneven or an edge", "in the level", "low ceiling", "liquid",
    "not against a wall", "would narrow a passage", "would part a way", "by an entity", "too near another crate"};

// A place at a wall's foot: a point of its bottom edge, the way out of the wall, along it.
struct Spot
{
    glm::vec3 at;
    glm::vec2 out, along;
};

// The fixed sight blockers (not Box3D's: an explosive box fixed in place), found once a server frame (sightBlocked).
struct SightCache
{
    double time{-1.0};
    za::Vector<int> fixed;
    auto members() { return qvr::mem::list(time, fixed); }
};
mem::Cache<SightCache> sightCache{"crates", mem::MapChange};

struct Planner
{
    Rng rng{0};
    za::Vector<debris::Obstacle> obstacles;
    int rejected[RCount]{};

    // The footprint's corners (inset) and its sides' middles, at height z.
    static void footprint(const Orient& o, const glm::vec3& c, float inset, glm::vec3 out[8])
    {
        const float f = o.hf - inset, l = o.hl - inset;
        const glm::vec3 F = o.fwd * f, L = o.left * l;
        out[0] = c + F + L;
        out[1] = c + F - L;
        out[2] = c - F - L;
        out[3] = c - F + L;
        out[4] = c + F;
        out[5] = c - L;
        out[6] = c - F;
        out[7] = c + L;
    }

    // Whether crate `o` fits with its middle over `c` (x, y) standing at `floorZ` (a floor's: found under it and
    // checked flat, `onFloor`; or the crate under it's top). Nothing in its volume, no liquid, `headroom` open above.
    [[nodiscard]] bool fits(const Orient& o, glm::vec3 c, bool onFloor, float& floorZ, float headroom, int& reason)
    {
        if(onFloor)
        {
            const trace_t down = traceLine({c.x, c.y, floorZ + 6.f}, {c.x, c.y, floorZ - 6.f});
            if(down.fraction >= 1.f || down.startsolid || down.ent != qcvm->edicts || down.plane.normal[2] < 0.95f)
            {
                reason = RNoFloor;
                return false;
            }
            floorZ = down.endpos[2];
            glm::vec3 pts[8];
            footprint(o, {c.x, c.y, floorZ}, 1.f, pts);
            for(const glm::vec3& p : pts)
            {
                const trace_t t = traceLine(p + glm::vec3{0.f, 0.f, 4.f}, p - glm::vec3{0.f, 0.f, 4.f});
                if(t.fraction >= 1.f || t.startsolid || t.ent != qcvm->edicts || qza::abs(t.endpos[2] - floorZ) > 1.5f ||
                    t.plane.normal[2] < 0.95f)
                {
                    reason = RUneven;
                    return false;
                }
            }
        }
        const float height = o.hu * 2.f;
        for(const float h : {2.f, height - 2.f})
        {
            const int inside = contents({c.x, c.y, floorZ + h});
            if(inside != CONTENTS_EMPTY)
            {
                if(vr_debug_crates.value >= 3)
                {
                    Con_Printf("crates: (%.0f %.0f %.0f) contents %d at %.0f up\n", c.x, c.y, floorZ, inside, h);
                }
                reason = inside == CONTENTS_SOLID ? RSolid : RLiquid;
                return false;
            }
        }
        // Nothing of the level in it: lines from its middle to its corners and sides, and round its edges, low, halfway
        // and high.
        for(const float h : {1.5f, height * 0.5f, height - 1.5f})
        {
            const glm::vec3 mid{c.x, c.y, floorZ + h};
            glm::vec3 pts[8];
            footprint(o, mid, 0.5f, pts);
            for(int k = 0; k < 8; k++)
            {
                if(traceLine(mid, pts[k]).fraction < 1.f || (k < 4 && traceLine(pts[k], pts[(k + 1) % 4]).fraction < 1.f))
                {
                    reason = RSolid;
                    return false;
                }
            }
        }
        // Open above: its middle and corners.
        glm::vec3 pts[8];
        footprint(o, {c.x, c.y, floorZ + 1.f}, 1.f, pts);
        for(int k = -1; k < 4; k++)
        {
            const glm::vec3 p = k < 0 ? glm::vec3{c.x, c.y, floorZ + 1.f} : pts[k];
            if(traceLine(p, p + glm::vec3{0.f, 0.f, height + headroom - 1.f}).fraction < 1.f)
            {
                reason = RLow;
                return false;
            }
        }
        // Clear of the entities (their boxes grown by their margins).
        const float rx = qza::abs(o.fwd.x) * o.hf + qza::abs(o.left.x) * o.hl;
        const float ry = qza::abs(o.fwd.y) * o.hf + qza::abs(o.left.y) * o.hl;
        for(const debris::Obstacle& ob : obstacles)
        {
            if(c.x + rx > ob.lo.x && c.x - rx < ob.hi.x && c.y + ry > ob.lo.y && c.y - ry < ob.hi.y &&
                floorZ + height > ob.lo.z && floorZ < ob.hi.z)
            {
                if(vr_debug_crates.value >= 3)
                {
                    Con_Printf("crates: (%.0f %.0f %.0f) by %s\n", c.x, c.y, floorZ, ob.why);
                }
                reason = REntity;
                return false;
            }
        }
        return true;
    }

    // Whether the crate's back is against the wall along all of its width (not overhanging an opening beside it).
    [[nodiscard]] static bool backed(const Orient& o, const glm::vec3& c, float floorZ, const Spot& s, float gap)
    {
        const float back = o.along(s.out), side = o.along(s.along) - 2.f;
        for(const float h : {6.f, o.hu * 2.f - 4.f})
        {
            for(const float k : {-1.f, 1.f})
            {
                const glm::vec3 p = glm::vec3{c.x, c.y, floorZ + h} + glm::vec3{s.out * -back + s.along * (k * side), 0.f};
                if(traceLine(p + glm::vec3{s.out, 0.f}, p - glm::vec3{s.out * (gap + 6.f), 0.f}).fraction >= 1.f)
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Whether it would split a way round it: the places the player's box fits on a ring round it (12, just clear of it),
    // and whether he can go from each to the next (the level only). One unbroken arc of them (all round it, or along the
    // room's side of a crate by a wall or in a corner): he can walk round it. Two or more: it stands where a way passed
    // (between two openings, in a doorway's side) and parts it. `free` gets how many places are open.
    [[nodiscard]] static int arcs(const Orient& o, const glm::vec3& c, float floorZ, int& free)
    {
        constexpr int n = 12;
        const float r = za::sqrt(o.hf * o.hf + o.hl * o.hl) + 18.f;
        glm::vec3 pts[n];
        bool open[n];
        free = 0;
        for(int k = 0; k < n; k++)
        {
            const float a = static_cast<float>(k) * (6.2831853f / n);
            pts[k] = glm::vec3{c.x + za::cos(a) * r, c.y + za::sin(a) * r, floorZ + 25.f};
            const trace_t t = tracePlayer(pts[k], pts[k]);
            open[k] = !t.startsolid && !t.allsolid;
            free += open[k];
        }
        if(free == n)
        {
            bool all = true;
            for(int k = 0; k < n && all; k++)
            {
                all = tracePlayer(pts[k], pts[(k + 1) % n]).fraction >= 1.f;
            }
            if(all)
            {
                return 1;
            }
        }
        // Arcs: runs of open places each reachable from the one before.
        int count = 0;
        for(int k = 0; k < n; k++)
        {
            const int prev = (k + n - 1) % n;
            const bool linked = open[prev] && open[k] && tracePlayer(pts[prev], pts[k]).fraction >= 1.f;
            count += open[k] && !linked;
        }
        return count;
    }

    // The open floor in front of it: the player's box from its front, out from the wall (the least of its middle and
    // its two ends): how far across the room one could pass it. 0: no room for the player there.
    [[nodiscard]] static float clearance(const Orient& o, const glm::vec3& c, float floorZ, const Spot& s)
    {
        const float front = o.along(s.out), side = za::max(o.along(s.along) - 16.f, 0.f);
        float least = 1e9f;
        for(const float k : {0.f, -1.f, 1.f})
        {
            const glm::vec3 p = glm::vec3{c.x, c.y, floorZ + 25.f} + glm::vec3{s.out * (front + 17.f) + s.along * (k * side), 0.f};
            const trace_t t = tracePlayer(p, p + glm::vec3{s.out * 256.f, 0.f});
            least = za::min(least, t.startsolid || t.allsolid ? 0.f : 17.f + t.fraction * 256.f + 16.f);
        }
        return least;
    }
};

// The crates in the map now (the server's entities), each with its model, skin, place, health and whether it rests;
// the last plan's clearance for the placed ones.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    int n = 0, pieces = 0;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const char* cls = e->free ? "" : PR_GetString(e->v.classname);
        if(!strcmp(cls, "vr_crate_piece"))
        {
            pieces++;
            continue;
        }
        if(strcmp(cls, "vr_crate") != 0)
        {
            continue;
        }
        n++;
        Con_Printf("crates: %d %s skin %d at (%.1f %.1f %.1f) angles (%.0f %.0f %.0f) health %.0f%s%s\n", i,
            PR_GetString(e->v.model) + 6, static_cast<int>(e->v.skin), e->v.origin[0], e->v.origin[1], e->v.origin[2],
            e->v.angles[0], e->v.angles[1], e->v.angles[2], e->v.health,
            (static_cast<int>(e->v.flags) & FL_ONGROUND) ? ", resting" : "", box3d::isBox3DProp(i) ? ", a body" : "");
        // What would keep shots and blows off it: not damageable, not solid, or owned (a trace from its owner passes
        // through it: a crate thrown and never given back; NOTES.md e1m2_2026-10-01_02-52-58).
        if(e->v.takedamage == 0.f || static_cast<int>(e->v.solid) != SOLID_BBOX || e->v.owner != 0)
        {
            Con_Printf("crates: %d takedamage %.0f solid %.0f owner %d\n", i, e->v.takedamage, e->v.solid,
                NUM_FOR_EDICT(PROG_TO_EDICT(e->v.owner)));
        }
    }
    PR_PopQCVM(oldVm);
    Con_Printf("crates: %d crates, %d pieces\n", n, pieces);
}

// "vr_crates_goto [i | crowbar [k]]": you in front of crate i of the last plan, or the next one (a test aid: setpos, 110
// units out from it, looking at it from a little above; Debug > Tests: Go to the Next Crate); or over the next crate with
// a crowbar on it, or the k-th (Go to a Crowbar on a Crate).
int gotoNext = 0;    // vr_crates_goto with no number: the next crate each time
int gotoCrowbar = 0; // vr_crates_goto crowbar: the next crate with a crowbar on it each time

void goto_f()
{
    const int count = static_cast<int>(placements.size());
    int i = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : (count ? gotoNext++ % count : 0);
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "crowbar"))
    {
        if(crowbars.empty())
        {
            Con_Printf("vr_crates_goto crowbar: no crowbar on a crate in this map's plan\n");
            return;
        }
        // Above it and 64 units out (setpos: noclip, floating), looking down at it.
        const int k = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : gotoCrowbar++;
        const CrowbarOn& c = crowbars[static_cast<size_t>(za::max(k, 0)) % crowbars.size()];
        const Placement& p = placements[static_cast<size_t>(c.crate)];
        const glm::vec2 out = p.out;
        const glm::vec2 at = glm::vec2{p.centre} + out * 64.f;
        const float yaw = glm::degrees(za::atan2(-out.y, -out.x));
        Con_Printf("vr_crates_goto crowbar: crate %d, looking at the crowbar at yaw %.0f, pitch 30\n", c.crate, yaw);
        Cbuf_InsertText(va("setpos %.1f %.1f %.1f 30 %.1f 0\n", at.x, at.y, p.centre.z + p.o.hu + 15.f, yaw));
        return;
    }
    if(i < 0 || i >= static_cast<int>(placements.size()))
    {
        Con_Printf("vr_crates_goto <0..%d>: crate i of this map's plan\n", static_cast<int>(placements.size()) - 1);
        return;
    }
    const Placement& p = placements[static_cast<size_t>(i)];
    const glm::vec2 at = glm::vec2{p.centre} + p.out * 110.f;
    const float yaw = glm::degrees(za::atan2(-p.out.y, -p.out.x));
    Cbuf_InsertText(va("setpos %.1f %.1f %.1f 12 %.1f 0\n", at.x, at.y, p.floorZ + 30.f, yaw));
}

} // namespace

void reset()
{
    placements.clear();
    crowbars.clear();
    gotoNext = 0;
    gotoCrowbar = 0;
}

namespace
{
bool commandsRegistered = false; // (vr_crates_list, vr_crates_goto: registered on the first call)
} // namespace

int plan()
{
    if(!commandsRegistered) // (no init hook of its own)
    {
        commandsRegistered = true;
        Cmd_AddCommand("vr_crates_list", list_f);
        Cmd_AddCommand("vr_crates_goto", goto_f);
    }
    placements.clear();
    crowbars.clear();
    const float worldspawn = sv.worldmodel ? debris::worldspawnValue("_vr_crates") : 0.f;
    if(vr_crates.value == 0.f || !sv.active || !sv.worldmodel || svs.maxclients != 1 ||
        debris::inList(vr_crates_exclude.string, sv.name) || worldspawn <= 0.f)
    {
        if(vr_debug_crates.value && sv.active)
        {
            const char* why = vr_crates.value == 0.f ? "vr_crates 0" : svs.maxclients != 1 ? "multiplayer" :
                              debris::inList(vr_crates_exclude.string, sv.name) ? "in vr_crates_exclude" :
                                                                                     "its worldspawn's _vr_crates is 0";
            Con_Printf("crates: %s: none (%s)\n", sv.name, why);
        }
        return 0;
    }
    const double t0 = Sys_DoubleTime();
    Planner pl;
    const uint64_t seed = hashString(sv.name) ^ (static_cast<uint64_t>(static_cast<int64_t>(vr_crates_seed.value)) * 0xD1B54A32D192ED03ull);
    pl.rng.s = seed;
    const float margin = za::max(vr_crates_margin.value, 0.f);
    debris::gatherObstacles(pl.obstacles, za::max(margin - 24.f, 0.f));
    // The player's start: well clear (where the map begins, and the way out of it).
    for(debris::Obstacle& ob : pl.obstacles)
    {
        if(!strncmp(ob.why, "info_player", 11))
        {
            ob.lo -= glm::vec3{64.f, 64.f, 0.f};
            ob.hi += glm::vec3{64.f, 64.f, 0.f};
        }
    }

    // The spots: along each wall's bottom edge, every 24 units, where a floor of the world meets it.
    const qmodel_t* map = sv.worldmodel;
    za::Vector<Spot> spots;
    int noFloor = 0;
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.numedges < 3 || !surf.texinfo || (surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB)))
        {
            continue;
        }
        glm::vec3 n = vec(surf.plane->normal);
        if(surf.flags & SURF_PLANEBACK)
        {
            n = -n;
        }
        const int texnum = surf.texinfo->texnum;
        const texture_t* tex = texnum >= 0 && texnum < map->numtextures ? map->textures[texnum] : nullptr;
        if(qza::abs(n.z) > 0.3f || glm::length(glm::vec2{n}) < 0.9f || debris::materialOf(tex ? tex->name : "") == debris::Material::None)
        {
            continue;
        }
        float lo = 1e9f;
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
            lo = za::min(lo, map->vertexes[v].position[2]);
        }
        const glm::vec2 out = glm::normalize(glm::vec2{n});
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e0 = map->surfedges[surf.firstedge + k];
            const int e1 = map->surfedges[surf.firstedge + (k + 1) % surf.numedges];
            const int va = static_cast<int>(e0 >= 0 ? map->edges[e0].v[0] : map->edges[-e0].v[1]);
            const int vb = static_cast<int>(e1 >= 0 ? map->edges[e1].v[0] : map->edges[-e1].v[1]);
            const glm::vec3 a = vec(map->vertexes[va].position), b = vec(map->vertexes[vb].position);
            if(qza::abs(a.z - lo) > 0.5f || qza::abs(b.z - lo) > 0.5f || glm::length(b - a) < 32.f)
            {
                continue;
            }
            const glm::vec3 edge = b - a;
            const float len = glm::length(edge);
            const glm::vec2 along = glm::normalize(glm::vec2{edge});
            for(float t = 12.f; t < len - 4.f; t += 24.f)
            {
                const glm::vec3 at = a + edge * (t / len);
                const glm::vec3 probe = at + glm::vec3{out * 2.f, 0.f};
                const trace_t down = traceLine(probe + glm::vec3{0.f, 0.f, 4.f}, probe - glm::vec3{0.f, 0.f, 4.f});
                if(down.fraction >= 1.f || down.startsolid || down.ent != qcvm->edicts || qza::abs(down.endpos[2] - at.z) > 1.5f)
                {
                    noFloor++;
                    continue;
                }
                spots.pushBack({vec(down.endpos), out, along});
            }
        }
    }
    // In a random order (the same each load), so that the limits leave no part of the map favoured.
    for(size_t i = spots.size(); i > 1; i--)
    {
        za::genericSwap(spots[i - 1], spots[pl.rng.next() % i]);
    }

    const int freeEdicts = qcvm->max_edicts - qcvm->num_edicts - static_cast<int>(za::max(vr_debris_edicts_left.value, 0.f));
    const int most = za::min(static_cast<int>(za::max(vr_crates_max.value, 0.f)), za::max(freeEdicts, 0));
    const float chance = za::max(vr_crates_chance.value, 0.f) * worldspawn;
    const float cornerMult = za::max(vr_crates_corner.value, 0.f);
    const float spacing = za::max(vr_crates_spacing.value, 0.f);
    const float needClear = za::max(vr_crates_clearance.value, 0.f);
    const float largeShare = za::clamp(vr_crates_large.value, 0.f, 1.f);
    const float stackChance = za::clamp(vr_crates_stack.value, 0.f, 1.f);
    int rolled = 0, stacks = 0;

    for(const Spot& s : spots)
    {
        if(static_cast<int>(placements.size()) >= most)
        {
            break;
        }
        // A corner: a wall beside the spot, along its own wall (either way), and how far.
        const glm::vec3 side = s.at + glm::vec3{s.out * 8.f, 6.f};
        const trace_t plus = traceLine(side, side + glm::vec3{s.along * 36.f, 0.f});
        const trace_t minus = traceLine(side, side - glm::vec3{s.along * 36.f, 0.f});
        const bool corner = plus.fraction < 1.f || minus.fraction < 1.f;
        if(pl.rng.uniform() >= chance * (corner ? cornerMult : 1.f))
        {
            continue;
        }
        rolled++;
        const float wallYaw = glm::degrees(za::atan2(s.along.y, s.along.x));
        bool placed = false;
        int reason = RCount;
        for(int attempt = 0; attempt < 4 && !placed; attempt++)
        {
            const int model = pl.rng.uniform() < largeShare ? 1 : 0;
            const Orient o = orient(model, static_cast<int>(pl.rng.next() % 6), static_cast<int>(pl.rng.next() & 3),
                wallYaw + pl.rng.range(-45.f, 45.f));
            const float gap = pl.rng.range(0.5f, 5.f);
            const float extOut = o.along(s.out), extAlong = o.along(s.along);
            float slide = pl.rng.range(-6.f, 6.f);
            if(corner)
            {
                // Into the corner, a little off it.
                const bool towardsPlus = plus.fraction < 1.f && (minus.fraction >= 1.f || plus.fraction <= minus.fraction);
                const float d = (towardsPlus ? plus.fraction : minus.fraction) * 36.f;
                slide = (towardsPlus ? 1.f : -1.f) * (d - extAlong - pl.rng.range(0.5f, 4.f));
            }
            glm::vec3 c = s.at + glm::vec3{s.out * (extOut + gap) + s.along * slide, 0.f};
            float floorZ = s.at.z;
            const bool stack = pl.rng.uniform() < stackChance && static_cast<int>(placements.size()) + 2 <= most;
            if(!pl.fits(o, c, true, floorZ, stack ? 8.f + 2.f * 24.f : 8.f, reason))
            {
                continue;
            }
            if(!Planner::backed(o, {c.x, c.y, floorZ}, floorZ, s, gap))
            {
                reason = RBacked;
                continue;
            }
            const float clear = Planner::clearance(o, c, floorZ, s);
            int ringFree = 0;
            const int ways = Planner::arcs(o, {c.x, c.y, floorZ}, floorZ, ringFree);
            if(ways > 1)
            {
                reason = RSplits;
                if(vr_debug_crates.value >= 3)
                {
                    Con_Printf("crates: (%.0f %.0f %.0f) would part %d ways round it (%d of 12 places open)\n", c.x, c.y, floorZ, ways,
                        ringFree);
                }
                continue;
            }
            if(clear < needClear)
            {
                reason = RPassage;
                if(vr_debug_crates.value >= 3)
                {
                    Con_Printf("crates: (%.0f %.0f %.0f) would leave %.0f units in front\n", c.x, c.y, floorZ, clear);
                }
                continue;
            }
            bool tooNear = false;
            for(const Placement& p : placements)
            {
                if(glm::length(glm::vec3{p.centre.x, p.centre.y, p.floorZ} - glm::vec3{c.x, c.y, floorZ}) < spacing)
                {
                    tooNear = true;
                    break;
                }
            }
            if(tooNear)
            {
                reason = RSpacing;
                break; // (another spot)
            }
            Placement p{model, static_cast<int>(pl.rng.next() % numSkins), o, glm::vec3{c.x, c.y, floorZ + o.hu}, floorZ, clear,
                -1, corner, s.out, ringFree};
            placements.pushBack(p);
            placed = true;

            // Another on it: no bigger than it, turned a little, off its middle a little (its weight well over the
            // lower one's top), room above for both.
            if(stack)
            {
                const int topModel = model == 1 && pl.rng.uniform() < 0.4f ? 1 : 0;
                const Orient t = orient(topModel, static_cast<int>(pl.rng.next() % 6), static_cast<int>(pl.rng.next() & 3),
                    o.yaw + pl.rng.range(-25.f, 25.f));
                const float room = za::max(za::min(o.hf, o.hl) * 0.3f, 0.f);
                const glm::vec3 off = o.fwd * pl.rng.range(-room, room) + o.left * pl.rng.range(-room, room);
                float topZ = floorZ + o.hu * 2.f + 0.05f;
                const glm::vec3 tc = glm::vec3{c.x, c.y, 0.f} + off;
                int topReason = RCount;
                if(pl.fits(t, tc, false, topZ, 8.f, topReason))
                {
                    placements.pushBack({topModel, pl.rng.uniform() < 0.7f ? p.skin : static_cast<int>(pl.rng.next() % numSkins), t,
                        glm::vec3{tc.x, tc.y, topZ + t.hu}, topZ, clear, static_cast<int>(placements.size()) - 1, corner, s.out, ringFree});
                    stacks++;
                }
            }
        }
        if(!placed && reason < RCount)
        {
            pl.rejected[reason]++;
        }
    }

    planCrowbars(seed);

    if(vr_debug_crates.value || developer.value)
    {
        uint64_t layout = 1469598103934665603ull;
        int large = 0, corners = 0;
        float leastClear = 1e9f;
        for(const Placement& p : placements)
        {
            large += p.model == 1;
            corners += p.below < 0 && p.corner;
            leastClear = za::min(leastClear, p.clearance);
            const int q[5] = {p.model, p.skin, static_cast<int>(za::lround(p.centre.x * 8.f)), static_cast<int>(za::lround(p.centre.y * 8.f)),
                static_cast<int>(za::lround(p.o.yaw * 10.f))};
            for(const int v : q)
            {
                layout = (layout ^ static_cast<uint32_t>(v)) * 1099511628211ull;
            }
        }
        Con_Printf("crates: %s: %d crates (%d large, %d stacked on another; %d in corners; %d crowbars on them) from %d spots, "
                   "%d rolled; limit %d; least clearance %.0f units; %.1f ms; layout %08x\n",
            sv.name, static_cast<int>(placements.size()), large, stacks, corners, static_cast<int>(crowbars.size()),
            static_cast<int>(spots.size()), rolled, most,
            placements.empty() ? 0.f : leastClear, (Sys_DoubleTime() - t0) * 1000.0, static_cast<unsigned>(layout ^ (layout >> 32)));
        if(vr_debug_crates.value >= 1)
        {
            za::String why;
            for(int r = 0; r < RCount; r++)
            {
                if(pl.rejected[r])
                {
                    why += va("%s%d %s", why.empty() ? "" : ", ", pl.rejected[r], reasonNames[r]);
                }
            }
            Con_Printf("crates: rolled spots rejected: %s (%d wall points had no floor)\n", why.empty() ? "none" : why.cStr(), noFloor);
            for(const CrowbarOn& c : crowbars)
            {
                Con_Printf("crates: a crowbar on crate %d (yaw %.0f, place %.2f %.2f)\n", c.crate, c.yaw, c.u, c.v);
            }
        }
        if(vr_debug_crates.value >= 2)
        {
            for(size_t i = 0; i < placements.size(); i++)
            {
                const Placement& p = placements[i];
                // The nearest entity's box (before its margin) from the crate's footprint.
                Con_Printf("crates: %d %s skin %d at (%.1f %.1f %.1f) yaw %.0f%s%s; clearance %.0f units, %d of 12 places round "
                           "it open (one way round)\n", static_cast<int>(i), models[p.model].name + 6, p.skin, p.centre.x, p.centre.y,
                    p.centre.z, p.o.yaw, p.below >= 0 ? va(", on crate %d", p.below) : "", p.corner && p.below < 0 ? ", in a corner" : "",
                    p.clearance, p.ringOpen);
            }
        }
    }
    return static_cast<int>(placements.size());
}

bool hasCrowbar(int i)
{
    return za::anyOf(crowbars.begin(), crowbars.end(), [i](const CrowbarOn& c) { return c.crate == i; });
}

const char* modelOf(int i)
{
    return i >= 0 && i < static_cast<int>(placements.size()) ? models[placements[static_cast<size_t>(i)].model].name : "";
}

namespace
{

// Sets `e`'s turn (`axes`), its middle at `centre`, its box round its turned shape, on the ground and still.
void rest(edict_t* e, const glm::mat3& axes, const glm::vec3& centre, const glm::vec3& half)
{
    held::anglesFromAxes(axes, e->v.angles, false);
    glm::vec3 ext{0.f};
    for(int k = 0; k < 3; k++)
    {
        ext += glm::abs(axes[k]) * half[k];
    }
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = centre[k];
        e->v.mins[k] = -ext[k];
        e->v.maxs[k] = ext[k];
        e->v.size[k] = 2.f * ext[k];
    }
    e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) | FL_ONGROUND);
    e->v.groundentity = EDICT_TO_PROG(qcvm->edicts);
    e->v.velocity[0] = e->v.velocity[1] = e->v.velocity[2] = 0.f;
    e->v.avelocity[0] = e->v.avelocity[1] = e->v.avelocity[2] = 0.f;
    SV_LinkEdict(e, false);
}

[[nodiscard]] int modelIndexOf(const char* name)
{
    for(int k = 0; k < numModels; k++)
    {
        if(!strcmp(models[k].name, name))
        {
            return k;
        }
    }
    return -1;
}

} // namespace

int put(edict_t* e, int i)
{
    if(i < 0 || i >= static_cast<int>(placements.size()))
    {
        return 0;
    }
    const Placement& p = placements[static_cast<size_t>(i)];
    e->v.skin = static_cast<float>(p.skin);
    e->v.frame = 0.f;
    const glm::vec3 up = p.centre + glm::vec3{0.f, 0.f, 0.02f};
    rest(e, p.o.axes, up, models[p.model].half * sizeOf(p.model));
    return p.model + 1;
}

bool putCrowbar(edict_t* e, int i)
{
    const auto it = za::findIf(crowbars.begin(), crowbars.end(), [i](const CrowbarOn& c) { return c.crate == i; });
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(it == crowbars.end() || !model)
    {
        return false;
    }
    const CrowbarOn& c = *it;
    const Placement& p = placements[static_cast<size_t>(c.crate)];
    // Its box as drawn (the weapon's scaling), in its axes from its origin.
    const progs::FieldOffsets& f = fields();
    glm::vec3 lo, hi;
    held::modelBox(model, progs::fieldVec(e, f.model_scale), progs::fieldVec(e, f.model_scale_origin), progs::fieldVec(e, f.model_offset),
        lo, hi);
    // Lying flat (rolled a quarter turn: the hook's plane level) at its yaw, or a quarter turn more if that overhangs the
    // top less: its box's reach along the top's forward and left (from its origin), and below it.
    struct Fit
    {
        float yaw, fLo, fHi, lLo, lHi, zLo, over;
    };
    Fit best{};
    for(int k = 0; k < 2; k++)
    {
        const float yaw = za::fmod(c.yaw + 90.f * static_cast<float>(k), 360.f);
        const float angles[3] = {0.f, yaw, 90.f};
        const glm::mat3 axes = held::axesFromAngles(angles, false);
        Fit fit{yaw, 1e9f, -1e9f, 1e9f, -1e9f, 1e9f, 0.f};
        for(int n = 0; n < 8; n++)
        {
            const glm::vec3 w = axes * glm::vec3{(n & 1) ? hi.x : lo.x, (n & 2) ? hi.y : lo.y, (n & 4) ? hi.z : lo.z};
            const float fw = glm::dot(w, p.o.fwd), lf = glm::dot(w, p.o.left);
            fit.fLo = za::min(fit.fLo, fw);
            fit.fHi = za::max(fit.fHi, fw);
            fit.lLo = za::min(fit.lLo, lf);
            fit.lHi = za::max(fit.lHi, lf);
            fit.zLo = za::min(fit.zLo, w.z);
        }
        fit.over = za::max(0.f, (fit.fHi - fit.fLo) * 0.5f - p.o.hf) + za::max(0.f, (fit.lHi - fit.lLo) * 0.5f - p.o.hl);
        if(k == 0 || fit.over < best.over - 0.01f)
        {
            best = fit;
        }
    }
    // Somewhere on the top it stays within (the plan's u, v of the room left; none left: in the middle).
    const float roomF = za::max(0.f, p.o.hf - (best.fHi - best.fLo) * 0.5f);
    const float roomL = za::max(0.f, p.o.hl - (best.lHi - best.lLo) * 0.5f);
    const float atF = c.u * roomF * 0.9f - (best.fLo + best.fHi) * 0.5f;
    const float atL = c.v * roomL * 0.9f - (best.lLo + best.lHi) * 0.5f;
    const float topZ = p.centre.z + p.o.hu;
    const glm::vec3 origin = glm::vec3{p.centre.x, p.centre.y, topZ + 0.25f - best.zLo} + p.o.fwd * atF + p.o.left * atL;
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = origin[k];
        e->v.velocity[k] = 0.f;
        e->v.avelocity[k] = 0.f;
    }
    e->v.angles[0] = 0.f;
    e->v.angles[1] = best.yaw;
    e->v.angles[2] = 90.f;
    SV_LinkEdict(e, false);
    if(vr_debug_crates.value)
    {
        Con_Printf("crates: a crowbar on crate %d: %.1f x %.1f units lying at yaw %.0f on its %.0f x %.0f top (z %.1f), %.1f units "
                   "past its edges; origin (%.1f %.1f %.1f)\n", c.crate, best.fHi - best.fLo, best.lHi - best.lLo, best.yaw,
            p.o.hf * 2.f, p.o.hl * 2.f, topZ, best.over, origin.x, origin.y, origin.z);
    }
    return true;
}

int putPlaced(edict_t* e)
{
    const int model = modelIndexOf(PR_GetString(e->v.model));
    if(model < 0)
    {
        return 0;
    }
    const Orient o = orient(model, 4, 0, e->v.angles[1]); // (upright)
    const glm::vec3 at = vec(e->v.origin);
    // On the highest floor under its footprint (its middle and corners): not sunk into a step it stands over.
    glm::vec3 pts[8];
    Planner::footprint(o, at, 1.f, pts);
    float top = -1e9f;
    for(int k = -1; k < 4; k++)
    {
        const glm::vec3 p = k < 0 ? at : pts[k];
        const trace_t down = traceLine(p + glm::vec3{0.f, 0.f, 1.f}, p - glm::vec3{0.f, 0.f, 128.f});
        if(!down.startsolid && down.fraction < 1.f)
        {
            top = za::max(top, down.endpos[2]);
        }
    }
    const float floorZ = top > -1e9f ? top : at.z;
    e->v.frame = 0.f;
    rest(e, o.axes, glm::vec3{at.x, at.y, floorZ + o.hu + 0.02f}, models[model].half * sizeOf(model));
    return model + 1;
}

int sightBlocked(const glm::vec3& start, const glm::vec3& end, int ignoreA, int ignoreB)
{
    if(const int n = box3d::sightRay(start, end, ignoreA, ignoreB))
    {
        return n;
    }
    const int field = fields().vr_blocksight;
    if(field < 0)
    {
        return 0;
    }
    // The blockers Box3D doesn't hold (an explosive box fixed in place: its box is its shape), found once a frame.
    SightCache& c = sightCache;
    if(c.time != qcvm->time)
    {
        c.time = qcvm->time;
        c.fixed.clear();
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
        {
            edict_t* e = EDICT_NUM(i);
            if(!e->free && progs::fieldFloat(e, field) > 0.f && static_cast<int>(e->v.solid) == SOLID_BBOX && !box3d::isBox3DProp(i))
            {
                c.fixed.pushBack(i);
            }
        }
    }
    const glm::vec3 d = end - start;
    for(const int i : c.fixed)
    {
        if(i == ignoreA || i == ignoreB || i >= qcvm->num_edicts)
        {
            continue;
        }
        edict_t* e = EDICT_NUM(i);
        if(e->free || progs::fieldFloat(e, field) <= 0.f || static_cast<int>(e->v.solid) != SOLID_BBOX)
        {
            continue;
        }
        // The segment against its box (slabs).
        float t0 = 0.f, t1 = 1.f;
        bool hit = true;
        for(int k = 0; k < 3 && hit; k++)
        {
            const float lo = e->v.absmin[k], hi = e->v.absmax[k];
            if(qza::abs(d[k]) < 1e-6f)
            {
                hit = start[k] >= lo && start[k] <= hi;
                continue;
            }
            float u = (lo - start[k]) / d[k], v = (hi - start[k]) / d[k];
            if(u > v)
            {
                za::genericSwap(u, v);
            }
            t0 = za::max(t0, u);
            t1 = za::min(t1, v);
            hit = t0 <= t1;
        }
        if(hit)
        {
            return i;
        }
    }
    return 0;
}

} // namespace qvr::crates
