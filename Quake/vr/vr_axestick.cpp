// vr_axestick.cpp -- thrown axes stick: see vr_axestick.hpp and docs/vr-port/ROUND21.md, "Thrown axes stick".
//
// The test, once a step for each thrown axe in flight (before Box3D moves it, so the blade meets the surface before the
// body bounces off it): points along each blade's edge (its ends and middle) are swept from where they are to where
// the axe's motion (its velocity and spin) takes them this step, against the level, brush entities and monsters
// (SV_Move, monsters at their drawn models) and the props (Box3D's ray). The first one struck decides: what it struck
// must be something an axe sticks in; the edge's speed into it (relative to it) at least vr_axestick_speed; the blade
// facing within vr_axestick_angle of the way the edge goes (a flat throw, the blade's side first, or a handle-first
// one fails it, or never strikes with the edge at all), and within vr_axestick_incidence of straight into the surface.
// It then sticks as it was turned when it struck, pushed along the way the blade faces until the edge is its depth
// under the surface (vr_axestick_depth at twice the least speed, half at it, never more than 60% of the blade), and
// only if its handle stays out of the wall.
//
// Where it is kept: the axe's pose in the frame of what it is in (its origin and angles: a door's, a prop's as drawn,
// a monster's yaw; the world's; or the monster model's triangle it went into, as drawn now: vr_hitmodel.cpp's
// anchorFrame), in QC fields, so that a saved game keeps it. Each server frame puts it back there.

#include "vr_axestick.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstring>

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
    std::array<Edge, 2> edges;
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
    return glm::degrees(std::acos(std::clamp(glm::dot(a, b), -1.f, 1.f)));
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
    const Edge& edge = blade.edges[static_cast<std::size_t>(e)];
    DrawnEdge d;
    d.a = held::drawnModelPoint(ent, edge.a);
    d.b = held::drawnModelPoint(ent, edge.b);
    d.mid = (d.a + d.b) * 0.5f;
    const glm::vec3 head = held::drawnModelPoint(ent, blade.head);
    const glm::vec3 out = held::drawnModelPoint(ent, blade.head + edge.out) - head;
    d.out = glm::length(out) > 1e-6f ? glm::normalize(out) : glm::vec3{1.f, 0.f, 0.f};
    d.width = std::max(0.f, glm::dot(d.mid - head, d.out));
    return d;
}

struct Strike
{
    float fraction{1e9f}; // of the step (negative: behind where the point was, sweepBack; more than 1: past it)
    edict_t* host{nullptr};
    int kind{None};
    int edge{0};
    glm::vec3 local{0.f};   // the point of the edge that struck (the axe's axes, from its origin)
    glm::vec3 point{0.f};   // where on the surface
    glm::vec3 normal{0.f};  // the surface's, out of it
    glm::vec3 anchor{0.f};  // Model: the triangle and where on it
    const char* why{nullptr}; // (not something an axe sticks in)
};

constexpr float sweepBack = 3.f;  // units each point's sweep starts behind it
constexpr float lookAhead = 16.f; // units past this step's move a monster is looked for

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

// What the point `local` of the axe (going from `from` to `to` this step) strikes first, if sooner than `best`.
// `from` is `back` units behind the point, `to` `step` units past it (this step's move) and more (the monsters' look ahead).
// `monsters`: only a monster struck counts (the look ahead).
void sweep(edict_t* ent, int edge, const glm::vec3& local, const glm::vec3& from, const glm::vec3& to, float back, float step,
    bool monsters, Strike& best)
{
    // (Fractions of the step: 0 where the point is, negative behind it, more than 1 past this step.)
    const float total = glm::length(to - from);
    const auto stepShare = [=](float fraction) { return (fraction * total - back) / std::max(1e-3f, step); };
    vec3_t start, end, zero{0.f, 0.f, 0.f};
    store(from, start);
    store(to, end);
    const int precise = MOVE_HITMODEL | (static_cast<int>(hitmodel::Class::Thrown) << MOVE_HITMODEL_CLASS_SHIFT);
    const trace_t tr = SV_Move(start, zero, zero, end, MOVE_NORMAL | precise, ent);
    if(vr_debug_axestick.value >= 3)
    {
        report(ent, "  edge %d %.1f %.1f %.1f -> %.1f %.1f %.1f: %.2f (%.2f of the step)%s %s", edge, from.x, from.y, from.z, to.x,
            to.y, to.z, tr.fraction, stepShare(tr.fraction), tr.startsolid ? " (starts in)" : "",
            tr.fraction < 1.f && tr.ent ? PR_GetString(tr.ent->v.classname) : "");
    }
    if(!tr.startsolid && !tr.allsolid && tr.fraction < 1.f && stepShare(tr.fraction) < best.fraction && tr.ent && !isProp(tr.ent) &&
        (!monsters || (static_cast<int>(tr.ent->v.flags) & FL_MONSTER)))
    {
        Strike s;
        s.fraction = stepShare(tr.fraction);
        s.host = tr.ent;
        s.edge = edge;
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
        best = s;
    }

    box3d::PropHit ph;
    if(!monsters && box3d::castProps(from, to, NUM_FOR_EDICT(ent), ph) && stepShare(ph.fraction) < best.fraction)
    {
        Strike s;
        s.fraction = stepShare(ph.fraction);
        s.host = EDICT_NUM(ph.num);
        s.kind = Prop;
        s.edge = edge;
        s.local = local;
        s.point = ph.point;
        s.normal = ph.normal;
        best = s;
    }
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
    const DrawnEdge e = drawnEdge(ent, blade, std::clamp(static_cast<int>(fieldFloat(ent, f.vr_stick_edge)), 0, 1));
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

    // Only its first contact may stick it: its flight changed since the last step (more than gravity does) is a hit
    // (the handle first, the head's side: Box3D bounced it), and it never sticks after.
    const glm::vec3 lastVel = fieldVec(ent, f.vr_stick_vel), lastSpin = fieldVec(ent, f.vr_stick_spin);
    setFieldVec(ent, f.vr_stick_vel, vel);
    setFieldVec(ent, f.vr_stick_spin, spin);
    if(f.vr_stick_vel >= 0 && f.vr_stick_spin >= 0 && lastVel != glm::vec3{0.f})
    {
        const glm::vec3 dv = vel - lastVel;
        const float fall = sv_gravity.value * dt * 1.5f;
        if(glm::length(glm::vec2{dv.x, dv.y}) > 25.f || dv.z > 25.f || dv.z < -fall - 25.f || glm::length(spin - lastSpin) > 2.f)
        {
            setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(Out));
            report(ent, "hit something (%.0f u/s, spin %.1f rad/s changed): never sticks now", glm::length(dv),
                glm::length(spin - lastSpin));
            return false;
        }
    }

    const float m2u = units::metresToUnits();
    const float minSpeed = std::max(0.1f, vr_axestick_speed.value) * m2u;
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

    Strike best;
    DrawnEdge edges[2];
    for(int e = 0; e < 2; e++)
    {
        edges[e] = drawnEdge(ent, *blade, e);
        for(const float t : {0.f, 0.5f, 1.f})
        {
            const glm::vec3 local = glm::mix(edges[e].a, edges[e].b, t);
            const glm::vec3 at = origin + axes * local;
            const glm::vec3 to = com + move + turn * (at - com);
            // From a little behind it: Box3D's hull is inside the drawn model by its rounding, so the drawn edge may be
            // in the surface already, a step before the body meets it.
            const float len = glm::length(to - at);
            if(len < 1e-3f)
            {
                continue;
            }
            const glm::vec3 dir = (to - at) / len;
            sweep(ent, e, local, at - dir * sweepBack, to, sweepBack, len, false, best);
        }
    }
    // A monster a little further on: Box3D meets it at its body's hull (its standing pose's, round it), before the
    // blade reaches the model as drawn; the blade then goes into the model a step early (no more than lookAhead).
    if(!best.host || best.kind == None || best.fraction > 1.f)
    {
        for(int e = 0; e < 2; e++)
        {
            for(const float t : {0.f, 0.5f, 1.f})
            {
                const glm::vec3 local = glm::mix(edges[e].a, edges[e].b, t);
                const glm::vec3 at = origin + axes * local;
                const glm::vec3 to = com + move + turn * (at - com);
                const float len = glm::length(to - at);
                if(len < 1e-3f)
                {
                    continue;
                }
                const glm::vec3 dir = (to - at) / len;
                sweep(ent, e, local, at - dir * sweepBack, to + dir * lookAhead, sweepBack, len, true, best);
            }
        }
    }
    if(!best.host)
    {
        return false;
    }

    // As it strikes: its turn, where it is, the edge's velocity (relative to what it strikes) and facing.
    edict_t* host = best.host;
    const DrawnEdge& edge = edges[best.edge];
    const float at = std::clamp(best.fraction, 0.f, 1.f); // (of the step; a monster further on: its end)
    const glm::mat3 turnAt = rotation(spin, dt * at);
    const glm::mat3 axesAt = turnAt * axes;
    const glm::vec3 originAt = com + move * at + turnAt * (origin - com);
    const glm::vec3 pointAt = originAt + axesAt * best.local;
    const glm::vec3 hostVel = host == qcvm->edicts ? glm::vec3{0.f} : vec(host->v.velocity);
    const glm::vec3 edgeVel = vel + glm::cross(spin, pointAt - com);
    const glm::vec3 rel = edgeVel - hostVel;
    const float speed = glm::length(rel);
    const glm::vec3 facing = glm::normalize(axesAt * edge.out);
    const float angle = speed > 1e-3f ? degrees(facing, rel / speed) : 180.f;
    const float incidence = degrees(facing, -best.normal);
    const char* hostName = host == qcvm->edicts ? "the level" : PR_GetString(host->v.classname);

    const char* why = best.why;
    if(!why && speed < minSpeed)
    {
        why = "too slow";
    }
    else if(!why && glm::dot(rel, best.normal) >= 0.f)
    {
        why = "not going into it";
    }
    else if(!why && angle > vr_axestick_angle.value)
    {
        why = "not blade first";
    }
    else if(!why && incidence > vr_axestick_incidence.value)
    {
        why = "too glancing";
    }

    // How deep: the depth at twice the least speed, half as deep at it, never more than 60% of the blade.
    const float depthMax = std::max(0.f, vr_axestick_depth.value) * 0.01f * m2u;
    const float depth = std::min(depthMax * std::clamp(speed / (2.f * minSpeed), 0.5f, 1.f), 0.6f * edge.width);
    const float along = depth / std::max(0.35f, glm::dot(facing, -best.normal));
    const glm::vec3 stuckOrigin = originAt + (best.point - pointAt) + facing * along;

    // Its handle out of the wall (the level's, a door's, a prop's): from the head's middle to the handle's end.
    if(!why && best.kind != Model && best.kind != MonsterBox)
    {
        const glm::vec3 head = stuckOrigin + axesAt * held::drawnModelPoint(ent, blade->head);
        // (To 90% of the handle's length: its end may touch the wall.)
        const glm::vec3 handle = glm::mix(head, stuckOrigin + axesAt * held::drawnModelPoint(ent, blade->handle), 0.9f);
        vec3_t a, b, zero{0.f, 0.f, 0.f};
        store(head, a);
        store(handle, b);
        const trace_t tr = SV_Move(a, zero, zero, b, MOVE_NOMONSTERS, ent);
        if(SV_PointContents(a) == CONTENTS_SOLID || tr.startsolid || tr.fraction < 1.f)
        {
            why = "its handle would be in the wall";
        }
    }

    if(vr_debug_axestick.value >= 2)
    {
        report(ent, "angles %.1f %.1f %.1f spin %.2f %.2f %.2f, the blade facing %.2f %.2f %.2f (its axes), %.2f wide",
            ent->v.angles[0], ent->v.angles[1], ent->v.angles[2], spin.x, spin.y, spin.z, edge.out.x, edge.out.y, edge.out.z,
            edge.width);
    }
    if(why)
    {
        report(ent, "blade %d struck %s at %.0f u/s (%.1f m/s), %.0f deg off its edge, %.0f deg to the surface: bounces (%s)",
            best.edge, hostName, speed, speed / m2u, angle, incidence, why);
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
    held::anglesFromAxes(inv * axesAt, localAngles, true);
    setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(kind));
    fieldInt(ent, f.vr_stick_ent) = EDICT_TO_PROG(host);
    setFieldVec(ent, f.vr_stick_ofs, inv * (stuckOrigin - framePoint));
    setFieldVec(ent, f.vr_stick_ang, vec(localAngles));
    setFieldVec(ent, f.vr_stick_anchor, best.anchor);
    setFieldFloat(ent, f.vr_stick_edge, static_cast<float>(best.edge));
    setFieldFloat(ent, f.vr_stick_wiggle, 0.f);
    store(edgeVel, ent->v.velocity); // (the throw's, for QC's blow)
    place(ent, *blade, stuckOrigin, axesAt);

    // A prop takes the blow.
    if(kind == Prop)
    {
        box3d::push(host, best.point, edgeVel, box3d::propMass(ent));
    }
    report(ent, "blade %d stuck in %s (%d, kind %d) at %.0f u/s (%.1f m/s), %.0f deg off its edge, %.0f deg to the surface, "
                "%.2f units deep (%.1f cm; the blade %.2f wide)",
        best.edge, hostName, NUM_FOR_EDICT(host), kind, speed, speed / m2u, angle, incidence, depth, depth / m2u * 100.f,
        edge.width);

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
