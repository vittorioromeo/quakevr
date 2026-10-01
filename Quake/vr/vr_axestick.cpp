// vr_axestick.cpp -- thrown axes stick: see vr_axestick.hpp and docs/vr-port/ROUND21.md, "Thrown axes stick" and
// "Thrown axes: the blade decides".
//
// The test, once a step for each thrown axe in flight (before Box3D moves it, so the blade meets the surface before the
// body bounces off it): points all over the axe as drawn (its model's vertices, its blades' edges and outlines and faces:
// samplesOf), each knowing what of the axe it is (a blade, how far behind its edge; the handle, the head's middle), are
// swept from where they are to where the axe's motion (its velocity and spin) takes them this step, against the level,
// brush entities and monsters (SV_Move, monsters at their drawn models) and the props (Box3D's ray). Whichever goes in
// first decides: the blade's cutting part (its edge, its corners, and the blade a share of its width behind them,
// vr_axestick_leniency) sticks; anything else bounces, and never sticks after. Then a few sanity checks only: what it
// struck must be something an axe sticks in, the point's speed into it (relative to it) at least vr_axestick_speed,
// not the blade's side first (its way out of the blade's plane within vr_axestick_angle), not glancing (within
// vr_axestick_incidence of straight in). It sticks as it was turned when it struck, pushed the way the point went (in the
// blade's plane) until it is its depth under the surface (vr_axestick_depth at twice the least speed, half at it, never
// more than 60% of the blade), and only if its handle stays out of the wall.
//
// Where it is kept: the axe's pose in the frame of what it is in (its origin and angles: a door's, a prop's as drawn,
// a monster's yaw; the world's; or the monster model's triangle it went into, as drawn now: vr_hitmodel.cpp's
// anchorFrame), in QC fields, so that a saved game keeps it. Each server frame puts it back there.

#include "vr_axestick.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

#include <stdarg.h>
#include <string.h>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// A blade's edge, in its model's own space (an alias model's vertices as scale * v + scale_origin): its ends, and the
// way the blade faces (out of the edge, square to it, in the blade's plane).
struct Edge
{
    glm::vec3 a, b, out;
};

struct Blade
{
    const char* model;
    za::Array<Edge, 2> edges;
    glm::vec3 head;   // the head's middle on the handle's axis (the blade's width: from it to the edge)
    glm::vec3 handle; // the handle's far end
};

// Quake VR's axe (quakevr/progs/v_axe.mdl, frame 0): the handle from (0.2, 0, -3.55) along (4.2, 0, 11.55) (as
// Misc/quakevr/make_swords.py measures it), the head from 25 to 37 units up it, double-bitted: an edge 12 units out on
// each side (vertices 10 to 20, and 16 to 19), the blade thin across y.
constexpr glm::vec3 axeOut{0.9398f, 0.f, -0.3417f};
const Blade blades[] = {
    {"progs/v_axe.mdl",
        {{{{20.13f, -0.12f, 15.81f}, {24.18f, -0.10f, 27.30f}, axeOut}, {{-3.00f, -0.10f, 24.50f}, {1.27f, -0.07f, 35.52f}, -axeOut}}},
        {10.79f, 0.f, 25.58f}, {0.2f, 0.f, -3.55f}},
};

// The kinds of thing it is in (.vr_stick_kind).
enum Kind : int
{
    Out = -1,    // it came out: it never sticks again
    None = 0,
    Level = 1,   // the world or a brush entity (a door, a lift): its origin and angles
    Prop = 2,    // a Box3D prop: its origin and angles as drawn
    Model = 3,   // a monster's model: the triangle it went into
    MonsterBox = 4, // a monster without a model to go by (vr_hit_precise off): its origin and yaw
};

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

void store(const glm::vec3& v, float* out)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}

[[nodiscard]] const Blade* bladeOf(edict_t* ent)
{
    const qmodel_t* model = modelOf(ent);
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    for(const Blade& b : blades)
    {
        if(!strcmp(model->name, b.model))
        {
            return &b;
        }
    }
    return nullptr;
}

[[nodiscard]] bool isProp(edict_t* ent)
{
    const int rigid = fields().vr_rigid;
    const int movetype = static_cast<int>(ent->v.movetype);
    return rigid >= 0 && fieldFloat(ent, rigid) != 0.f && (movetype == MOVETYPE_TOSS || movetype == MOVETYPE_BOUNCE);
}

[[nodiscard]] glm::mat3 rotation(const glm::vec3& spin, float t)
{
    const float angle = glm::length(spin) * t;
    if(angle < 1e-6f)
    {
        return glm::mat3{1.f};
    }
    return glm::mat3_cast(glm::angleAxis(angle, spin / glm::length(spin)));
}

[[nodiscard]] float degrees(const glm::vec3& a, const glm::vec3& b)
{
    return glm::degrees(za::acos(za::clamp(glm::dot(a, b), -1.f, 1.f)));
}

// The frame of what an axe is in (`kind`, `anchor` for a model's triangle): where its origin is and its axes, now.
bool frameOf(edict_t* host, int kind, const glm::vec3& anchor, glm::vec3& point, glm::mat3& axes)
{
    if(host == qcvm->edicts)
    {
        point = glm::vec3{0.f};
        axes = glm::mat3{1.f};
        return true;
    }
    if(host->free)
    {
        return false;
    }
    point = vec(host->v.origin);
    switch(kind)
    {
        case Level: axes = held::axesFromAngles(host->v.angles, true); return true;
        case Prop:
        {
            const qmodel_t* model = modelOf(host);
            axes = held::axesFromAngles(host->v.angles, model && model->type == mod_brush);
            return true;
        }
        case Model:
            return hitmodel::anchorFrame(host, static_cast<int>(anchor.x), anchor.y, anchor.z, point, axes);
        case MonsterBox:
        {
            const float yaw[3]{0.f, host->v.angles[1], 0.f};
            axes = held::axesFromAngles(yaw, false);
            return true;
        }
        default: return false;
    }
}

// The blade's edge `e` of `ent` as drawn, in its axes from its origin: its ends, middle and facing.
struct DrawnEdge
{
    glm::vec3 a, b, mid, out;
    float width; // from the head's middle to the edge, along the facing
};

[[nodiscard]] DrawnEdge drawnEdge(edict_t* ent, const Blade& blade, int e)
{
    const Edge& edge = blade.edges[static_cast<za::SizeT>(e)];
    DrawnEdge d;
    d.a = held::drawnModelPoint(ent, edge.a);
    d.b = held::drawnModelPoint(ent, edge.b);
    d.mid = (d.a + d.b) * 0.5f;
    const glm::vec3 head = held::drawnModelPoint(ent, blade.head);
    const glm::vec3 out = held::drawnModelPoint(ent, blade.head + edge.out) - head;
    d.out = glm::length(out) > 1e-6f ? glm::normalize(out) : glm::vec3{1.f, 0.f, 0.f};
    d.width = za::max(0.f, glm::dot(d.mid - head, d.out));
    return d;
}

// The points of the axe the test sweeps, in its model's own space (an alias model's first pose: scale * v +
// scale_origin), and what of the axe each is: each blade's edge at sevenths of its length (both its corners among them),
// every vertex (one within 1.5 units of a point already taken left out: the handle's rings, the head's), and the sides
// of the triangles that reach into a blade every 3 units (the blade's outline, its top and bottom edges in from the
// corners, and its faces). About 75 for the axe.
struct Sample
{
    glm::vec3 p{0.f};
    int bit{-1};      // the blade (its edge in Blade::edges) it is on; -1: the handle, the head's middle
    float back{1.f};  // how far behind that edge, a share of the blade's width (0: on the edge)
    float along{0.f}; // where along the edge (0 and 1: its corners)
};

// The samples of the model they were made from (models' slots: a game dir change, a model reload).
struct StickCache
{
    za::Vector<Sample> samples;
    const qmodel_t* model{nullptr};
    auto members() { return qvr::mem::list(samples, model); }
};
mem::Cache<StickCache> cache{"axe stick", mem::GameDirChange | mem::ModelReload};

// The test's buffers (Box3D's step: the main thread).
struct StickScratch
{
    za::Vector<glm::vec3> drawn; // the samples as drawn, in the axe's axes from its origin (beforeStep)
    auto members() { return qvr::mem::list(drawn); }
};
mem::Scratch<StickScratch> scratch{"axe stick"};

constexpr int edgePoints = 7;         // along each edge
constexpr float vertexSpacing = 1.5f; // units (the model's own)
constexpr float sideSpacing = 3.f;
constexpr za::SizeT maxSamples = 256;

// Which blade the point `p` (the model's own space) is on, how far behind its edge and where along it; none (-1) for
// the handle, the head's middle (more than 90% of the way back from the edge to the handle's axis), past a corner.
[[nodiscard]] Sample classify(const Blade& blade, const glm::vec3& p)
{
    Sample s;
    s.p = p;
    for(int e = 0; e < 2; e++)
    {
        const Edge& edge = blade.edges[static_cast<za::SizeT>(e)];
        const float width = za::max(1e-3f, glm::dot(edge.a - blade.head, edge.out));
        const glm::vec3 run = edge.b - edge.a;
        const float along = glm::dot(p - edge.a, run) / za::max(1e-6f, glm::dot(run, run));
        const float back = glm::dot(edge.a - p, edge.out) / width;
        if(along >= -0.15f && along <= 1.15f && back >= -0.1f && back <= 0.9f && (s.bit < 0 || back < s.back))
        {
            s.bit = e;
            s.back = za::max(0.f, back);
            s.along = along;
        }
    }
    return s;
}

[[nodiscard]] const za::Vector<Sample>& samplesOf(const qmodel_t* model, const Blade& blade)
{
    za::Vector<Sample>& out = cache.samples;
    if(cache.model == model && !out.empty())
    {
        return out;
    }
    out.clear();
    cache.model = model;
    const auto add = [&](const glm::vec3& p, float spacing) {
        if(out.size() >= maxSamples)
        {
            return;
        }
        for(const Sample& s : out)
        {
            if(glm::distance(s.p, p) < spacing)
            {
                return;
            }
        }
        out.pushBack(classify(blade, p));
    };
    for(const Edge& e : blade.edges)
    {
        for(int i = 0; i < edgePoints; i++)
        {
            add(glm::mix(e.a, e.b, static_cast<float>(i) / static_cast<float>(edgePoints - 1)), 0.5f);
        }
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc || hdr->numframes <= 0)
    {
        return out; // (its edges alone)
    }
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + hdr->frames[0].firstpose * hdr->numverts;
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const auto vertex = [&](int i) {
        const trivertx_t& v = verts[mesh[indexes[i]].vertindex];
        return glm::vec3{hdr->scale[0] * v.v[0] + hdr->scale_origin[0], hdr->scale[1] * v.v[1] + hdr->scale_origin[1],
            hdr->scale[2] * v.v[2] + hdr->scale_origin[2]};
    };
    for(int i = 0; i < hdr->numindexes; i++)
    {
        add(vertex(i), vertexSpacing);
    }
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const glm::vec3 a = vertex(i + k), b = vertex(i + (k + 1) % 3);
            if(classify(blade, a).bit < 0 && classify(blade, b).bit < 0)
            {
                continue;
            }
            const int n = static_cast<int>(glm::distance(a, b) / sideSpacing);
            for(int j = 1; j <= n; j++)
            {
                add(glm::mix(a, b, static_cast<float>(j) / static_cast<float>(n + 1)), vertexSpacing);
            }
        }
    }
    return out;
}

// What the sample `s` of the axe is (the debug's lines).
[[nodiscard]] const char* partName(edict_t* ent, const Blade& blade, const Sample& s)
{
    if(s.bit >= 0)
    {
        return s.back > 0.1f ? "the blade behind its edge" : (s.along < 0.1f || s.along > 0.9f) ? "a corner" : "an edge";
    }
    const glm::vec3 head = held::drawnModelPoint(ent, blade.head);
    const glm::vec3 handle = held::drawnModelPoint(ent, blade.handle);
    const glm::vec3 axis = glm::normalize(head - handle);
    const float along = glm::dot(held::drawnModelPoint(ent, s.p) - handle, axis);
    return along > glm::length(head - handle) - 3.f ? "the head" : along < 2.f ? "the handle's end" : "the handle";
}

struct Strike
{
    float fraction{1e9f}; // of the step (negative: behind where the point was, sweepBack; more than 1: past it)
    float len{0.f};       // the point's move this step (units)
    edict_t* host{nullptr};
    int kind{None};
    int sample{0};          // which (samplesOf)
    glm::vec3 local{0.f};   // the point that struck (the axe's axes, from its origin)
    glm::vec3 point{0.f};   // where on the surface
    glm::vec3 normal{0.f};  // the surface's, out of it
    glm::vec3 anchor{0.f};  // Model: the triangle and where on it
    const char* why{nullptr}; // (not something an axe sticks in)
};

constexpr float sweepBack = 3.f;  // units each point's sweep starts behind it
constexpr float lookAhead = 16.f; // units past this step's move a monster is looked for
// vr_axestick_leniency's, at 1 (times it): units past this step's move the axe is looked for (the level, props); units
// the blade's cutting part may come after what goes in first (its corner a hair behind the head's top); the share of
// the blade's width behind its edge that cuts (and bandBase at 0: the edge and its corners); degrees more glancing; degrees
// the axe may be turned about its edge to keep its handle out of the wall.
constexpr float aheadUnits = 4.f;
constexpr float tieUnits = 1.f;
constexpr float bandBase = 0.1f;
constexpr float bandLeniency = 0.35f;
constexpr float incidenceLeniency = 10.f;
constexpr float handleLeniency = 25.f;

void report(edict_t* ent, const char* fmt, ...)
{
    if(!vr_debug_axestick.value)
    {
        return;
    }
    char text[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    Con_Printf("axestick: %d %s\n", NUM_FOR_EDICT(ent), text);
}

// What the axe's sample `sample` (`local`, going from `from` to `to` this step) strikes first. `from` is `back` units
// behind the point, `to` `step` units past it (this step's move) and more (the look ahead). `monsters`: only a monster
// counts (the look ahead).
[[nodiscard]] Strike sweep(edict_t* ent, int sample, const glm::vec3& local, const glm::vec3& from, const glm::vec3& to,
    float back, float step, bool monsters)
{
    Strike best;
    // (Fractions of the step: 0 where the point is, negative behind it, more than 1 past this step.)
    const float total = glm::length(to - from);
    const auto stepShare = [=](float fraction) { return (fraction * total - back) / za::max(1e-3f, step); };
    vec3_t start, end, zero{0.f, 0.f, 0.f};
    store(from, start);
    store(to, end);
    const int precise = MOVE_HITMODEL | (static_cast<int>(hitmodel::Class::Thrown) << MOVE_HITMODEL_CLASS_SHIFT);
    const trace_t tr = SV_Move(start, zero, zero, end, MOVE_NORMAL | precise, ent);
    if(vr_debug_axestick.value >= 3 && tr.fraction < 1.f)
    {
        report(ent, "  sample %d %.1f %.1f %.1f -> %.1f %.1f %.1f: %.2f (%.2f of the step)%s %s", sample, from.x, from.y,
            from.z, to.x, to.y, to.z, tr.fraction, stepShare(tr.fraction), tr.startsolid ? " (starts in)" : "",
            tr.ent ? PR_GetString(tr.ent->v.classname) : "");
    }
    if(!tr.startsolid && !tr.allsolid && tr.fraction < 1.f && tr.ent && !isProp(tr.ent) &&
        (!monsters || (static_cast<int>(tr.ent->v.flags) & FL_MONSTER)))
    {
        Strike& s = best;
        s.fraction = stepShare(tr.fraction);
        s.host = tr.ent;
        s.sample = sample;
        s.local = local;
        s.point = vec(tr.endpos);
        s.normal = vec(tr.plane.normal);
        edict_t* h = tr.ent;
        const int solid = static_cast<int>(h->v.solid);
        if(h == qcvm->edicts || solid == SOLID_BSP)
        {
            s.kind = Level;
            vec3_t beyond;
            store(s.point - s.normal * 2.f, beyond);
            if(SV_PointContents(beyond) == CONTENTS_SKY)
            {
                s.why = "the sky";
            }
        }
        else if(static_cast<int>(h->v.flags) & FL_MONSTER)
        {
            s.kind = MonsterBox;
            hitmodel::Hit hit;
            if(hitmodel::target(h) &&
                hitmodel::segment(h, from, to, hitmodel::tolerance(hitmodel::Class::Thrown), 1.f, hitmodel::Class::Thrown, hit) &&
                hit.tri >= 0)
            {
                s.kind = Model;
                s.point = hit.surface; // (the model itself, not its grown surface)
                s.normal = hit.normal;
                s.anchor = glm::vec3{static_cast<float>(hit.tri), hit.u, hit.v};
            }
        }
        else
        {
            s.why = (static_cast<int>(h->v.flags) & FL_CLIENT) ? "a player" : "not something axes stick in";
        }
    }

    box3d::PropHit ph;
    if(!monsters && box3d::castProps(from, to, NUM_FOR_EDICT(ent), ph) && stepShare(ph.fraction) < best.fraction)
    {
        best = Strike{};
        best.fraction = stepShare(ph.fraction);
        best.host = EDICT_NUM(ph.num);
        best.kind = Prop;
        best.sample = sample;
        best.local = local;
        best.point = ph.point;
        best.normal = ph.normal;
    }
    return best;
}

// Whether anything (the level, a door, a monster's box, a prop) is within `size` units of the axe's centre `com` along
// its move this step and `reach` units further: the sweeps' broad phase (most steps of a flight meet nothing).
[[nodiscard]] bool anythingNear(edict_t* ent, const glm::vec3& com, const glm::vec3& move, float size, float reach)
{
    const float len = glm::length(move);
    const glm::vec3 to = len > 1e-3f ? com + move * ((len + reach) / len) : com;
    vec3_t start, end, mins, maxs;
    store(com, start);
    store(to, end);
    store(glm::vec3{-size}, mins);
    store(glm::vec3{size}, maxs);
    const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NORMAL, ent);
    if(tr.startsolid || tr.allsolid || tr.fraction < 1.f)
    {
        return true;
    }
    const int self = NUM_FOR_EDICT(ent);
    box3d::RopeHit hit;
    return box3d::ropeOverlaps(com, size, self, 0) || box3d::ropeCast(com, to, size, self, 0, hit);
}

// Which part of the axe the world point `p` is on (its debug: what Box3D's body touched).
[[nodiscard]] const char* partOf(edict_t* ent, const Blade& blade, const glm::vec3& p)
{
    const glm::vec3 local = glm::transpose(held::axesFromAngles(ent->v.angles, false)) * (p - vec(ent->v.origin));
    const za::Vector<Sample>& samples = samplesOf(modelOf(ent), blade);
    const Sample* nearest = nullptr;
    float best = 1e9f;
    for(const Sample& s : samples)
    {
        const float d = glm::distance(held::drawnModelPoint(ent, s.p), local);
        if(d < best)
        {
            best = d;
            nearest = &s;
        }
    }
    return nearest ? partName(ent, blade, *nearest) : "?";
}

void callStuck(func_t fn, edict_t* ent, edict_t* host, int kind, float speed)
{
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = EDICT_TO_PROG(host);
    pr_global_struct->time = qcvm->time;
    G_FLOAT(OFS_PARM0) = static_cast<float>(kind);
    G_FLOAT(OFS_PARM1) = speed;
    PR_ExecuteProgram(fn);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
}

// The axe in what it is in: its pose there (the frame's), the blade's edge and facing in the world.
void place(edict_t* ent, const Blade& blade, const glm::vec3& origin, const glm::mat3& axes)
{
    const FieldOffsets& f = fields();
    const DrawnEdge e = drawnEdge(ent, blade, za::clamp(static_cast<int>(fieldFloat(ent, f.vr_stick_edge)), 0, 1));
    glm::vec3 o = origin;
    glm::mat3 r = axes;
    const float wiggle = fieldFloatOr(ent, f.vr_stick_wiggle, 0.f);
    if(wiggle != 0.f)
    {
        // Rocked about the edge's middle, across the blade (the head is thin along its model's y).
        const glm::vec3 pivot = o + r * e.mid;
        const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(glm::radians(wiggle), glm::normalize(r[1])));
        r = turn * r;
        o = pivot + turn * (o - pivot);
    }
    vec3_t angles;
    held::anglesFromAxes(r, angles, false);
    const bool moved = glm::distance(o, vec(ent->v.origin)) > 1e-3f || glm::distance(vec(angles), vec(ent->v.angles)) > 1e-3f;
    store(o, ent->v.origin);
    store(vec(angles), ent->v.angles);
    setFieldVec(ent, f.vr_stick_at, o + r * e.mid);
    setFieldVec(ent, f.vr_stick_dir, r * e.out);
    if(vr_debug_axestick.value >= 3)
    {
        const glm::vec3 d = fieldVec(ent, f.vr_stick_dir);
        report(ent, "placed at %.1f %.1f %.1f, facing %.2f %.2f %.2f (field %d)", o.x, o.y, o.z, d.x, d.y, d.z, f.vr_stick_dir);
    }
    if(moved)
    {
        SV_LinkEdict(ent, false);
    }
}

} // namespace

namespace qvr::axestick
{

bool beforeStep(edict_t* ent, const glm::vec3& com, const glm::vec3& vel, const glm::vec3& spin, float dt)
{
    const FieldOffsets& f = fields();
    if(!vr_axestick.value || f.vr_stick_kind < 0 || f.vr_stick_ent < 0 || fieldFloat(ent, f.vr_stick_kind) != 0.f || dt <= 0.f)
    {
        return false;
    }
    // Thrown by a player (not a weapon dropped or knocked off a rack: those fall as before).
    const int owner = NUM_FOR_EDICT(PROG_TO_EDICT(ent->v.owner));
    if(owner < 1 || owner > svs.maxclients)
    {
        return false;
    }
    const Blade* blade = bladeOf(ent);
    if(!blade)
    {
        return false;
    }

    // Only its first contact may stick it: Box3D's body pushed against something last step (the handle first, the
    // head's side), or its flight changed more than gravity does (a touch QC made): it never sticks after. (Not its
    // spin: a throw spun off its steadiest axis turns its spin about in the air, Box3D's gyroscopic step and Spin
    // Alignment, by more than a light hit would; that once ended real throws' chances in mid-air.)
    const glm::vec3 lastVel = fieldVec(ent, f.vr_stick_vel), lastSpin = fieldVec(ent, f.vr_stick_spin);
    setFieldVec(ent, f.vr_stick_vel, vel);
    setFieldVec(ent, f.vr_stick_spin, spin);
    glm::vec3 touch;
    const bool touched = box3d::contactPoint(NUM_FOR_EDICT(ent), touch);
    const glm::vec3 dv = vel - lastVel;
    const float fall = sv_gravity.value * dt * 1.5f;
    const bool changed = f.vr_stick_vel >= 0 && lastVel != glm::vec3{0.f} &&
                         (glm::length(glm::vec2{dv.x, dv.y}) > 25.f || dv.z > 25.f || dv.z < -fall - 25.f);
    if(touched || changed)
    {
        setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(Out));
        report(ent, "hit something with %s (%.0f u/s, spin %.1f rad/s changed): never sticks now",
            touched ? partOf(ent, *blade, touch) : "?", glm::length(dv), glm::length(spin - lastSpin));
        return false;
    }

    const float m2u = units::metresToUnits();
    const float minSpeed = za::max(0.1f, vr_axestick_speed.value) * m2u;
    if(glm::length(vel) + glm::length(spin) * 40.f < minSpeed) // (a blade 40 units from its centre at most)
    {
        return false;
    }
    const func_t stuckFn = findFunction("VR_AxeStick_Stuck");
    if(!stuckFn)
    {
        return false; // (progs without the feature)
    }
    QVR_PROFILE("axe stick");
    if(vr_debug_axestick.value >= 2)
    {
        report(ent, "flying: %.0f u/s at %.0f %.0f %.0f, spin %.2f %.2f %.2f, angles %.1f %.1f %.1f", glm::length(vel), com.x,
            com.y, com.z, spin.x, spin.y, spin.z, ent->v.angles[0], ent->v.angles[1], ent->v.angles[2]);
    }

    const glm::vec3 origin = vec(ent->v.origin);
    const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
    const glm::mat3 turn = rotation(spin, dt);
    const glm::vec3 move = vel * dt;

    // Leniency (vr_axestick_leniency, 0..2): how much of the blade behind its edge cuts (a tenth of its width at 0: the
    // edge and its corners; 45% at 1, 80% at 2: the blade's top and bottom edges in from the corners), how much later
    // than what goes in first it may come (a unit at 1), how far past this step's move the axe is looked for (the
    // blade about to meet what the body meets first), how glancing, how far it may be turned to keep its handle out.
    const float leniency = za::clamp(vr_axestick_leniency.value, 0.f, 2.f);
    const float ahead = aheadUnits * leniency;
    const float band = bandBase + bandLeniency * leniency;
    const float tie = tieUnits * leniency;

    // The axe's points as drawn now, and how far they reach from its centre.
    const za::Vector<Sample>& samples = samplesOf(modelOf(ent), *blade);
    za::Vector<glm::vec3>& drawn = scratch.drawn;
    drawn.clear();
    float radius = 0.f;
    for(const Sample& s : samples)
    {
        drawn.pushBack(held::drawnModelPoint(ent, s.p));
        radius = za::max(radius, glm::distance(origin + axes * drawn.back(), com));
    }
    if(!anythingNear(ent, com, move, radius * (1.f + glm::length(spin) * dt) + sweepBack + 1.f, za::max(lookAhead, ahead)))
    {
        return false;
    }

    // Each point swept along its move (its velocity and spin), from a little behind it: Box3D's hull is inside the
    // drawn model by its rounding, so a drawn point may be in the surface already, a step before the body meets it.
    // What any point strikes first, and what the blade's cutting part strikes first.
    Strike first, cut;
    const auto pass = [&](bool monsters) {
        for(za::SizeT i = 0; i < samples.size(); i++)
        {
            const glm::vec3 at = origin + axes * drawn[i];
            const glm::vec3 to = com + move + turn * (at - com);
            const float len = glm::length(to - at);
            if(len < 1e-3f)
            {
                continue;
            }
            const glm::vec3 dir = (to - at) / len;
            const float past = monsters ? za::max(lookAhead, ahead) : ahead;
            Strike s = sweep(ent, static_cast<int>(i), drawn[i], at - dir * sweepBack, to + dir * past, sweepBack, len, monsters);
            s.len = len;
            if(!s.host)
            {
                continue;
            }
            if(s.fraction < first.fraction)
            {
                first = s;
            }
            if(samples[i].bit >= 0 && samples[i].back <= band && s.fraction < cut.fraction)
            {
                cut = s;
            }
        }
    };
    pass(false);
    // A monster a little further on: Box3D meets it at its body's hull (its standing pose's, round it), before the
    // axe reaches the model as drawn; the blade then goes into the model a step early (no more than lookAhead).
    if(!first.host || first.fraction > 1.f)
    {
        pass(true);
    }
    if(!first.host)
    {
        return false;
    }

    // What goes in first decides: the blade's cutting part sticks (or comes `tie` units after it), anything else (the
    // handle, its end, the head's middle, the blade too far behind its edge) bounces, and if it meets it this step,
    // that was its first contact: it never sticks after.
    const char* firstHost = first.host == qcvm->edicts ? "the level" : PR_GetString(first.host->v.classname);
    if(!cut.host || cut.fraction > first.fraction + tie / za::max(first.len, 1e-3f))
    {
        const Sample& s = samples[static_cast<za::SizeT>(first.sample)];
        const bool now = first.fraction <= 1.f;
        if(now)
        {
            setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(Out));
        }
        if(now || vr_debug_axestick.value >= 2)
        {
            report(ent, "%s%s struck %s first, %.2f of the step (the blade %s): bounces (%s first)%s", partName(ent, *blade, s),
                s.bit >= 0 ? va(" (blade %d, %.0f%% behind its edge)", s.bit, s.back * 100.f) : "", firstHost,
                first.fraction, cut.host ? va("%.1f units after", (cut.fraction - first.fraction) * first.len) : "not near",
                partName(ent, *blade, s), now ? ", never sticks now" : "");
        }
        return false;
    }

    // As it strikes: its turn, where it is, the point's velocity (relative to what it strikes), the blade's facing and
    // the way across it.
    const Strike& best = cut;
    const Sample& part = samples[static_cast<za::SizeT>(best.sample)];
    edict_t* host = best.host;
    const DrawnEdge edge = drawnEdge(ent, *blade, part.bit);
    const float at = za::clamp(best.fraction, 0.f, 1.f); // (of the step; a monster further on: its end)
    const glm::mat3 turnAt = rotation(spin, dt * at);
    const glm::mat3 axesAt = turnAt * axes;
    const glm::vec3 originAt = com + move * at + turnAt * (origin - com);
    const glm::vec3 pointAt = originAt + axesAt * best.local;
    const glm::vec3 hostVel = host == qcvm->edicts ? glm::vec3{0.f} : vec(host->v.velocity);
    const glm::vec3 edgeVel = vel + glm::cross(spin, pointAt - com);
    const glm::vec3 rel = edgeVel - hostVel;
    const float speed = glm::length(rel);
    const glm::vec3 facing = glm::normalize(axesAt * edge.out);
    const glm::vec3 across = glm::normalize(axesAt * glm::cross(edge.b - edge.a, edge.out));
    const glm::vec3 relDir = speed > 1e-3f ? rel / speed : -facing;
    // The sanity checks: fast enough, going into it, not the blade's side first (its way out of the blade's plane), not
    // glancing (its way off straight into the surface).
    const float sideOn = glm::degrees(za::asin(za::clamp(qza::abs(glm::dot(relDir, across)), 0.f, 1.f)));
    const float incidence = degrees(relDir, -best.normal);
    const float offFacing = degrees(facing, relDir); // (the debug's: the old test's angle)
    const float incidenceMax = vr_axestick_incidence.value + incidenceLeniency * leniency;
    const char* hostName = host == qcvm->edicts ? "the level" : PR_GetString(host->v.classname);
    const char* partLabel = partName(ent, *blade, part);

    const char* why = best.why;
    if(!why && speed < minSpeed)
    {
        why = "too slow";
    }
    else if(!why && glm::dot(rel, best.normal) >= 0.f)
    {
        why = "not going into it";
    }
    else if(!why && sideOn > vr_axestick_angle.value)
    {
        why = "its side first";
    }
    else if(!why && incidence > incidenceMax)
    {
        why = "too glancing";
    }

    // How deep: the depth at twice the least speed, half as deep at it, never more than 60% of the blade; pushed the
    // way the point goes in the blade's plane (a corner along the way it came, the edge straight in).
    const glm::vec3 inPlane = relDir - across * glm::dot(relDir, across);
    const glm::vec3 push = glm::length(inPlane) > 1e-3f ? glm::normalize(inPlane) : facing;
    const float depthMax = za::max(0.f, vr_axestick_depth.value) * 0.01f * m2u;
    const float depth = za::min(depthMax * za::clamp(speed / (2.f * minSpeed), 0.5f, 1.f), 0.6f * edge.width);
    const float along = depth / za::max(0.35f, glm::dot(push, -best.normal));
    glm::vec3 stuckOrigin = originAt + (best.point - pointAt) + push * along;
    glm::mat3 stuckAxes = axesAt;
    float tilted = 0.f;

    // Its handle out of the wall (the level's, a door's, a prop's): from the head's middle to the handle's end. With
    // leniency, if it would be in, the axe is turned about the point where it went in, the handle away from the wall,
    // by up to handleLeniency degrees (at 1) before it gives up.
    if(!why && best.kind != Model && best.kind != MonsterBox)
    {
        const auto handleIn = [&](const glm::vec3& o, const glm::mat3& r) {
            const glm::vec3 head = o + r * held::drawnModelPoint(ent, blade->head);
            // (To 90% of the handle's length: its end may touch the wall.)
            const glm::vec3 handle = glm::mix(head, o + r * held::drawnModelPoint(ent, blade->handle), 0.9f);
            vec3_t a, b, zero{0.f, 0.f, 0.f};
            store(head, a);
            store(handle, b);
            const trace_t tr = SV_Move(a, zero, zero, b, MOVE_NOMONSTERS, ent);
            return SV_PointContents(a) == CONTENTS_SOLID || tr.startsolid || tr.fraction < 1.f;
        };
        if(handleIn(stuckOrigin, stuckAxes))
        {
            why = "its handle would be in the wall";
            const glm::vec3 butt = axesAt * (held::drawnModelPoint(ent, blade->handle) - held::drawnModelPoint(ent, blade->head));
            const glm::vec3 pivotAxis = glm::cross(butt, best.normal);
            const float most = handleLeniency * leniency;
            if(glm::length(pivotAxis) > 1e-4f && most > 0.f)
            {
                const glm::vec3 pivot = best.point - push * along * 0.5f; // (in the wall, where it went in)
                for(float deg = 5.f; deg <= most + 1e-3f; deg += 5.f)
                {
                    const glm::mat3 t = glm::mat3_cast(glm::angleAxis(glm::radians(deg), glm::normalize(pivotAxis)));
                    const glm::vec3 o = pivot + t * (stuckOrigin - pivot);
                    if(!handleIn(o, t * axesAt))
                    {
                        stuckOrigin = o;
                        stuckAxes = t * axesAt;
                        tilted = deg;
                        why = nullptr;
                        break;
                    }
                }
            }
        }
    }

    if(vr_debug_axestick.value >= 2)
    {
        report(ent, "angles %.1f %.1f %.1f spin %.2f %.2f %.2f, %d points, blade %d %.2f wide, %s %.0f%% behind its edge (%.0f%% "
                    "cuts), first at %.2f of the step, the blade at %.2f",
            ent->v.angles[0], ent->v.angles[1], ent->v.angles[2], spin.x, spin.y, spin.z, static_cast<int>(samples.size()),
            part.bit, edge.width, partLabel, part.back * 100.f, band * 100.f, first.fraction, best.fraction);
    }
    if(why)
    {
        report(ent, "blade %d (%s) struck %s at %.0f u/s (%.1f m/s), %.0f deg side on, %.0f deg off straight in (%.0f off its "
                    "facing), %.2f of the step: bounces (%s)",
            part.bit, partLabel, hostName, speed, speed / m2u, sideOn, incidence, offFacing, best.fraction, why);
        return false;
    }

    // Stuck: its pose in what it is in.
    const int kind = best.kind;
    glm::vec3 framePoint;
    glm::mat3 frameAxes;
    if(!frameOf(host, kind, best.anchor, framePoint, frameAxes))
    {
        report(ent, "no frame on %s: bounces", hostName);
        return false;
    }
    const glm::mat3 inv = glm::transpose(frameAxes); // (orthonormal)
    vec3_t localAngles;
    held::anglesFromAxes(inv * stuckAxes, localAngles, true);
    setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(kind));
    fieldInt(ent, f.vr_stick_ent) = EDICT_TO_PROG(host);
    setFieldVec(ent, f.vr_stick_ofs, inv * (stuckOrigin - framePoint));
    setFieldVec(ent, f.vr_stick_ang, vec(localAngles));
    setFieldVec(ent, f.vr_stick_anchor, best.anchor);
    setFieldFloat(ent, f.vr_stick_edge, static_cast<float>(part.bit));
    setFieldFloat(ent, f.vr_stick_wiggle, 0.f);
    store(edgeVel, ent->v.velocity); // (the throw's, for QC's blow)
    place(ent, *blade, stuckOrigin, stuckAxes);

    // A prop takes the blow.
    if(kind == Prop)
    {
        box3d::push(host, best.point, edgeVel, box3d::propMass(ent));
    }
    report(ent, "blade %d stuck in %s (%d, kind %d) by %s (%.0f%% behind its edge) at %.0f u/s (%.1f m/s), %.0f deg side on, "
                "%.0f deg off straight in (%.0f off its facing), %.2f of the step, %.2f units deep (%.1f cm; the blade %.2f "
                "wide)%s",
        part.bit, hostName, NUM_FOR_EDICT(host), kind, partLabel, part.back * 100.f, speed, speed / m2u, sideOn, incidence,
        offFacing, best.fraction, depth, depth / m2u * 100.f, edge.width,
        tilted > 0.f ? va(", turned %.0f deg to keep its handle out", tilted) : "");

    callStuck(stuckFn, ent, host, kind, speed);
    return true;
}

void serverFrame()
{
    const FieldOffsets& f = fields();
    if(!sv.active || f.vr_stick_kind < 0 || f.vr_stick_ent < 0 || f.vr_stick_ofs < 0 || f.vr_stick_ang < 0)
    {
        return;
    }
    QVR_PROFILE("axe stick");
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(ent->free || fieldFloat(ent, f.vr_stick_kind) <= 0.f || static_cast<int>(ent->v.movetype) != MOVETYPE_NONE)
        {
            continue;
        }
        const Blade* blade = bladeOf(ent);
        edict_t* host = PROG_TO_EDICT(fieldInt(ent, f.vr_stick_ent));
        glm::vec3 point;
        glm::mat3 axes;
        if(!blade || !frameOf(host, static_cast<int>(fieldFloat(ent, f.vr_stick_kind)), fieldVec(ent, f.vr_stick_anchor), point, axes))
        {
            continue; // (gone, or its model can't say: QC drops it)
        }
        const glm::vec3 ang = fieldVec(ent, f.vr_stick_ang);
        const float a[3]{ang.x, ang.y, ang.z};
        place(ent, *blade, point + axes * fieldVec(ent, f.vr_stick_ofs), axes * held::axesFromAngles(a, true));
    }
}

} // namespace qvr::axestick
