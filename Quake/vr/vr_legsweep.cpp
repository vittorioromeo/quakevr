// vr_legsweep.cpp -- the leg sweep: see vr_legsweep.hpp and docs/vr-port/ROUND22.md, "Leg sweep".
//
// What a hand holds is measured once per model from its drawn shape:
// - a weapon: its model's vertices (the zero pose, as the view draws it: the weapon scaling included) along the line from
//   its grip (the entity's origin, where a hand holds it) to its melee tip (the muzzle anchor: the QC melee's line). Its
//   length is the vertices' extent along that line; its cross-section the median of its 8 slices' along it (each slice's
//   larger extent across the line): a gun's barrel, not its grip and trigger guard, which make every gun's thickest about
//   20 cm and hide the lightning gun's fat body; its slenderness the length over that. A weapon with a blade (an axe's
//   head, a sword's blade past the guard, the chainsaw's bar: the QC melee layout's edge, vr_melee.qc VR_Melee_PartAt) is
//   sharp from that share of the grip-to-tip way on: a contact there is a cut (the melee's), never a sweep.
// - a prop in a hand (mainhand_held / offhand_held): its drawn box in its own axes (held::modelBox: its model's bounds,
//   its networked scale), its length the largest extent, its cross-section the next. Nothing on a prop is sharp.
// Staff-like: at least vr_legsweep_min_length long and vr_legsweep_min_slender slender (a crate, a rock, a brick, the
// lightning gun and the super shotgun fall short of one or the other: vr_legsweep_table prints every weapon's and some
// props').
//
// Each server frame the thing's line is placed where it is now and sampled at sweepPoints points, end to end: a
// weapon's from the hand to its muzzle as the server has them (QC VR_Melee_Line's: handpos, muzzlepos; carried off its
// handle, its drawn handle and tip in the hand: VR_Melee_FreeLine's), stretched over its drawn extent; a prop's along its
// box's long axis through its middle, at its entity. Each point's move since the last frame is swept against each live
// humanoid's lower legs (vr_foegrab_lift_classes: its drawn model below legShare of its height, vr_hit_precise; else its
// box there). Of the points that met them, the lowest decides: not the blade, level enough (its rise or fall at most
// levelSlope of its level speed), and at least vr_legsweep_speed level (more for a heavy one: times the cube root of its
// mass over vr_legsweep_heavy). Then foegrab::sweepDown knocks it down with its feet swept along and its body toppling
// over the other way (forward or backward of its own facing, or to a side: whichever way the sweep goes past it).

#include "vr_legsweep.hpp"

#include "vr_anchor.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_foegrab.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mem.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_render.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <string.h>

using namespace qvr;

namespace
{

constexpr int sweepPoints = 9;      // points along the thing's line, end to end
constexpr float legShare = 0.4f;    // its legs: the lowest share of its height (below the knees and up the thighs)
constexpr float levelSlope = 0.7f;  // a sweep's point rises or falls at most this times its level speed (about 35 deg)
constexpr float maxRadius = 4.f;    // units: the points' thickness at most (half the cross-section)
constexpr float maxSpeed = 25.f;    // m/s: a point faster than this was put there (a teleport), not swung
constexpr float maxFeet = 100.f;    // units/s: its feet slide no faster (a hard sweep slides them a metre and more)
constexpr double sweptAgain = 1.0;  // s: one swept is not swept again before this (it is down anyway)
constexpr double handRest = 0.4;    // s: after a sweep, the hand's next
constexpr double traceLength = 1.0; // s: the sweep's trace (vr_legsweep_debug) follows the body this long
constexpr double traceEvery = 0.1;  // s: a line every this often
constexpr int maxSwept = 32;

// A thing's measure (its model's: see the top). A prop's line from `a` to `b` (in its own axes) runs end to end through
// the middle of its box; a weapon's from `along0` to `along1` along the line from its grip to its tip.
struct Measure
{
    char name[64]{};
    bool weapon{false};
    bool ok{false};
    float length{0.f};
    float cross{0.f};
    float edge{-1.f}; // the share of the way from a to b from which it is sharp (-1: nowhere)
    float thickest{0.f}; // (a weapon's: `cross` is its typical, the median of its slices')
    float reach{0.f}, behind{0.f}; // (a weapon's: its grip to its tip, and its near end behind its grip)
    float along0{0.f}, along1{0.f}; // (a weapon's: its ends along its grip-to-tip line, from the grip)
    float profile[8]{};
    glm::vec3 a{0.f}, b{0.f};
};

struct MeasureCache
{
    za::Vector<Measure> measures;
    auto members() { return mem::list(measures); }
};
mem::Cache<MeasureCache> measureCache{"leg sweep", mem::MapChange | mem::GameDirChange | mem::ModelReload};

struct MeasureScratch
{
    za::Vector<glm::vec3> rest;
    za::Vector<glm::vec3> now;
    auto members() { return mem::list(rest, now); }
};
mem::Scratch<MeasureScratch> measureScratch{"leg sweep"};

// A hand's thing as last placed.
struct Track
{
    bool valid{false};
    const void* what{nullptr}; // its model
    int ent{0};                // a prop's edict (0: a weapon)
    glm::vec3 pts[sweepPoints]{};
    double time{0.0};
    double restUntil{0.0};
};

struct Swept
{
    int ent{0};
    double time{0.0};
};

// After a sweep (vr_legsweep_debug): its body's feet, pelvis and head followed for traceLength.
struct Trace
{
    int ent{0};
    double start{0.0};
    double next{0.0};
    glm::vec3 along{1.f, 0.f, 0.f};  // the sweep's way (level)
    glm::vec3 facing{1.f, 0.f, 0.f}; // its own, as it stood
    glm::vec3 feet0{0.f}, pelvis0{0.f};
};

struct State
{
    Track hands[2];
    Swept swept[maxSwept];
    int sweptNext{0};
    Trace trace;
    double lastTime{-1.0};
    double lastNote[2]{};
};
State state;

[[nodiscard]] bool debugOn()
{
    return vr_legsweep_debug.value != 0.f;
}

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

// The sharp part of a weapon (the QC melee layout's edge: vr_melee.qc VR_Melee_PartAt; the hot blades' too,
// vr_hotblade.cpp bladeShare): from this share of the grip-to-tip way; -1 for none (a gun, the crowbar's bar, Mjolnir).
[[nodiscard]] float edgeShare(const qmodel_t* model)
{
    using modelmeta::Id;
    if(modelmeta::is(model, Id::VAxe) || modelmeta::is(model, Id::Mg3SuperAxe))
    {
        return 0.6f; // the head
    }
    if(modelmeta::is(model, Id::VKsword) || modelmeta::is(model, Id::VHksword))
    {
        return 0.2f; // the blade past the guard
    }
    if(modelmeta::is(model, Id::VChainsaw))
    {
        return 0.45f; // the bar past the engine block
    }
    return -1.f;
}

[[nodiscard]] Measure* cached(const qmodel_t* model)
{
    for(Measure& m : measureCache.measures)
    {
        if(!strcmp(m.name, model->name))
        {
            return &m;
        }
    }
    return nullptr;
}

[[nodiscard]] Measure& store(const qmodel_t* model, bool weapon)
{
    Measure& m = measureCache.measures.emplaceBack();
    q_strlcpy(m.name, model->name, sizeof(m.name));
    m.weapon = weapon;
    return m;
}

// A weapon model's measure (see the top), drawn at the origin unturned as a hand holds it.
[[nodiscard]] const Measure& measureWeapon(qmodel_t* model)
{
    if(Measure* m = cached(model))
    {
        return *m;
    }
    Measure& out = store(model, true);
    const int slot = weapons::slotForModel(model);
    if(model->type != mod_alias || slot < 0 || slot == weapons::fistSlot())
    {
        return out;
    }
    view::ViewEntity ve;
    ve.ent.model = model;
    ve.ent.scale = ENTSCALE_DEFAULT;
    float raw[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, raw);
    const glm::mat4 toWorld = glm::make_mat4(raw);
    const glm::vec3 tip = view::anchorPosition(ve, weapons::anchor(slot, weapons::Key::MuzzleAnchor),
        weapons::vec(slot, weapons::Key::MuzzleOffsetX, weapons::Key::MuzzleOffsetY, weapons::Key::MuzzleOffsetZ));
    const float reach = glm::length(tip);
    if(reach < 1.f || !anchor::posedVertices(ve.ent, 1.f, measureScratch.rest, measureScratch.now) ||
        measureScratch.rest.empty())
    {
        return out;
    }
    const glm::vec3 axis = tip / reach;
    const glm::vec3 side0 = za::fabs(axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 u = glm::normalize(side0 - axis * glm::dot(side0, axis));
    const glm::vec3 w = glm::cross(axis, u);
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const glm::vec3& v : measureScratch.rest)
    {
        const glm::vec3 p{toWorld * glm::vec4{v, 1.f}};
        const glm::vec3 q{glm::dot(p, axis), glm::dot(p, u), glm::dot(p, w)};
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    out.length = hi.x - lo.x;
    out.cross = za::max(hi.y - lo.y, hi.z - lo.z);
    out.reach = reach;
    out.behind = -lo.x;
    out.along0 = lo.x;
    out.along1 = hi.x;
    {
        constexpr int bins = 8;
        glm::vec2 blo[bins], bhi[bins];
        for(int k = 0; k < bins; k++)
        {
            blo[k] = glm::vec2{1e9f};
            bhi[k] = glm::vec2{-1e9f};
        }
        for(const glm::vec3& v : measureScratch.rest)
        {
            const glm::vec3 p{toWorld * glm::vec4{v, 1.f}};
            const int k = za::clamp(static_cast<int>((glm::dot(p, axis) - lo.x) / out.length * bins), 0, bins - 1);
            const glm::vec2 q{glm::dot(p, u), glm::dot(p, w)};
            blo[k] = glm::min(blo[k], q);
            bhi[k] = glm::max(bhi[k], q);
        }
        // Its typical cross-section: the median of its slices' (those with vertices in them; sorted).
        float sorted[bins];
        int n = 0;
        for(int k = 0; k < bins; k++)
        {
            out.profile[k] = bhi[k].x >= blo[k].x ? za::max(bhi[k].x - blo[k].x, bhi[k].y - blo[k].y) : 0.f;
            if(out.profile[k] > 0.f)
            {
                int j = n++;
                for(; j > 0 && sorted[j - 1] > out.profile[k]; j--)
                {
                    sorted[j] = sorted[j - 1];
                }
                sorted[j] = out.profile[k];
            }
        }
        out.thickest = out.cross;
        if(n > 0)
        {
            out.cross = n % 2 ? sorted[n / 2] : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);
        }
    }
    const float edge = edgeShare(model);
    out.edge = edge < 0.f || out.length < 1e-3f ? -1.f : za::clamp((edge * reach - lo.x) / out.length, 0.f, 1.f);
    out.ok = out.length > 1e-3f && out.cross > 1e-3f;
    return out;
}

// A prop model's measure (see the top): its drawn box, scaled by `scale` (the networked scale, an offset from 1) about
// `scaleOrigin`, `offset` on its raw vertices (held::modelBox). Not cached when scaled (a prop's own).
[[nodiscard]] Measure measurePropBox(const qmodel_t* model, const glm::vec3& scale, const glm::vec3& scaleOrigin,
    const glm::vec3& offset)
{
    Measure out;
    q_strlcpy(out.name, model->name, sizeof(out.name));
    glm::vec3 lo, hi;
    held::modelBox(model, scale, scaleOrigin, offset, lo, hi);
    const glm::vec3 size = hi - lo;
    const int axis = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
    out.length = size[axis];
    out.cross = za::max(size[(axis + 1) % 3], size[(axis + 2) % 3]);
    const glm::vec3 middle = (lo + hi) * 0.5f;
    out.a = middle;
    out.b = middle;
    out.a[axis] = lo[axis];
    out.b[axis] = hi[axis];
    out.ok = out.length > 1e-3f && out.cross > 1e-3f;
    return out;
}

[[nodiscard]] const Measure& measureProp(const qmodel_t* model)
{
    if(Measure* m = cached(model))
    {
        return *m;
    }
    Measure& out = store(model, false);
    out = measurePropBox(model, glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f});
    return out;
}

[[nodiscard]] float slenderness(const Measure& m)
{
    return m.cross > 1e-3f ? m.length / m.cross : 0.f;
}

[[nodiscard]] bool staffLike(const Measure& m)
{
    return m.ok && m.length * 100.f / units::metresToUnits() >= vr_legsweep_min_length.value &&
           slenderness(m) >= vr_legsweep_min_slender.value;
}

// What `hand` (HAND_OFF, HAND_MAIN) of the local player holds, placed where it is drawn now: its points (end to end) and
// its measure. False: nothing measured (an empty hand, the fist).
struct Held
{
    const void* what{nullptr};
    int ent{0};
    Measure measure;
    glm::vec3 pts[sweepPoints]{};
};

[[nodiscard]] bool heldThing(edict_t* player, int hand, Held& out)
{
    const auto& f = progs::fields();
    const int ofs = hand == HAND_MAIN ? f.mainhand_held : f.offhand_held;
    const int num = ofs >= 0 ? progs::fieldInt(player, ofs) : 0;
    if(num > 0)
    {
        edict_t* ent = PROG_TO_EDICT(num);
        const int index = static_cast<int>(ent->v.modelindex);
        if(ent->free || index <= 0 || index >= MAX_MODELS || !sv.models[index])
        {
            return false;
        }
        const qmodel_t* model = sv.models[index];
        const glm::vec3 scale = progs::fieldVec(ent, f.model_scale);
        out.measure = scale == glm::vec3{0.f}
                          ? measureProp(model)
                          : measurePropBox(model, scale, progs::fieldVec(ent, f.model_scale_origin),
                                progs::fieldVec(ent, f.model_offset));
        if(!out.measure.ok)
        {
            return false;
        }
        out.what = model;
        out.ent = NUM_FOR_EDICT(ent);
        const glm::mat3 axes = held::axesFromAngles(ent->v.angles, model->type == mod_brush);
        const glm::vec3 origin = vec(ent->v.origin);
        for(int i = 0; i < sweepPoints; i++)
        {
            const glm::vec3 p = glm::mix(out.measure.a, out.measure.b, static_cast<float>(i) / (sweepPoints - 1));
            out.pts[i] = origin + axes * p;
        }
        return true;
    }
    // A weapon: its line as the QC melee has it (vr_melee.qc VR_Melee_Line, VR_Melee_FreeLine): from the hand (its grip)
    // to its muzzle (the melee's tip) as the server has them this frame; carried off its handle (by a hotspot, anywhere
    // on it), its handle and tip as drawn in the hand, placed on the hand of this frame.
    const char* name = hand == HAND_MAIN ? PR_GetString(player->v.weaponmodel)
                                         : (f.weaponmodel2 >= 0 ? PR_GetString(progs::fieldInt(player, f.weaponmodel2)) : "");
    qmodel_t* model = name && *name ? Mod_ForName(name, false) : nullptr;
    if(!model)
    {
        return false;
    }
    out.measure = measureWeapon(model);
    if(!out.measure.ok)
    {
        return false;
    }
    const glm::vec3 handPos = progs::fieldVec(player, hand == HAND_MAIN ? f.handpos : f.offhandpos);
    glm::vec3 grip = handPos;
    glm::vec3 tip = progs::fieldVec(player, hand == HAND_MAIN ? f.muzzlepos : f.offmuzzlepos);
    glm::vec3 lg, lt;
    if(twohand::carrying(hand) && view::carriedWeaponInHand(hand, lg, lt))
    {
        const glm::vec3 rot = progs::fieldVec(player, hand == HAND_MAIN ? f.handrot : f.offhandrot);
        const glm::mat3 axes = held::axesFromAngles(&rot[0], true); // (forward, left, up; the drawn frame's right: -left)
        grip = handPos + axes * glm::vec3{lg.x, -lg.y, lg.z};
        tip = handPos + axes * glm::vec3{lt.x, -lt.y, lt.z};
    }
    const float reach = glm::distance(grip, tip);
    if(reach < 2.f || reach > 160.f)
    {
        return false; // (no muzzle known)
    }
    out.what = model;
    out.ent = 0;
    // Its drawn extent along the line, at the drawn size of this hand's (the muzzle's distance from the grip).
    const glm::vec3 dir = (tip - grip) / reach;
    const float k = reach / out.measure.reach;
    for(int i = 0; i < sweepPoints; i++)
    {
        const float s = glm::mix(out.measure.along0, out.measure.along1, static_cast<float>(i) / (sweepPoints - 1));
        out.pts[i] = grip + dir * (s * k);
    }
    return true;
}

[[nodiscard]] bool sweptLately(int ent, double now)
{
    for(const Swept& s : state.swept)
    {
        if(s.ent == ent && now - s.time < sweptAgain && now >= s.time)
        {
            return true;
        }
    }
    return false;
}

// Where segment p0..p1 enters the box lo..hi (0..1), -1 if it doesn't.
[[nodiscard]] float segmentBox(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& lo, const glm::vec3& hi)
{
    float t0 = 0.f, t1 = 1.f;
    const glm::vec3 d = p1 - p0;
    for(int k = 0; k < 3; k++)
    {
        if(za::fabs(d[k]) < 1e-6f)
        {
            if(p0[k] < lo[k] || p0[k] > hi[k])
            {
                return -1.f;
            }
            continue;
        }
        float a = (lo[k] - p0[k]) / d[k];
        float b = (hi[k] - p0[k]) / d[k];
        if(a > b)
        {
            const float s = a;
            a = b;
            b = s;
        }
        t0 = za::max(t0, a);
        t1 = za::min(t1, b);
        if(t0 > t1)
        {
            return -1.f;
        }
    }
    return t0;
}

// Where point `p0`..`p1` (radius `r`) first meets `m`'s lower legs (0..1), -1 if it doesn't.
[[nodiscard]] float legsMet(edict_t* m, const glm::vec3& p0, const glm::vec3& p1, float r)
{
    const glm::vec3 lo = vec(m->v.absmin), hi = vec(m->v.absmax);
    const float top = lo.z + legShare * (hi.z - lo.z);
    const float boxT = segmentBox(p0, p1, glm::vec3{lo.x - r, lo.y - r, lo.z - r}, glm::vec3{hi.x + r, hi.y + r, top});
    if(boxT < 0.f)
    {
        return -1.f;
    }
    if(hitmodel::enabled() && hitmodel::target(m))
    {
        hitmodel::Hit hit;
        // (Already in them where it starts: not a sweep into them, a thing resting or moved through them.)
        if(!hitmodel::segment(m, p0, p1, r, 1.f, hitmodel::Class::Melee, hit) || hit.startInside || hit.point.z > top + r)
        {
            return -1.f;
        }
        return hit.t;
    }
    return boxT;
}

void note(int hand, edict_t* m, const char* text)
{
    if(!debugOn() || qcvm->time - state.lastNote[hand] < 0.25)
    {
        return;
    }
    state.lastNote[hand] = qcvm->time;
    Con_Printf("legsweep: the %s hand's thing on %s (%d)'s legs: %s\n", hand == HAND_MAIN ? "main" : "off",
        PR_GetString(m->v.classname), NUM_FOR_EDICT(m), text);
}

void traceStep()
{
    Trace& t = state.trace;
    if(t.ent <= 0 || qcvm->time < t.next)
    {
        return;
    }
    glm::vec3 pelvis, head, feet;
    if(!box3d::ragdollStance(t.ent, pelvis, head, feet))
    {
        t.ent = 0;
        return;
    }
    t.next = qcvm->time + traceEvery;
    const glm::vec3 left = glm::cross(glm::vec3{0.f, 0.f, 1.f}, t.along);
    const glm::vec3 df = feet - t.feet0, dp = pelvis - t.pelvis0;
    const double age = qcvm->time - t.start;
    Con_Printf("legsweep trace: %.2f s feet %.1f along the sweep, %.1f left of it; pelvis %.1f along, %.1f left, %.1f up; "
               "head %.1f up\n",
        age, glm::dot(df, t.along), glm::dot(df, left), glm::dot(dp, t.along), glm::dot(dp, left), dp.z, head.z - t.feet0.z);
    if(age >= traceLength)
    {
        const glm::vec3 lying = head - feet;
        const float fwd = glm::dot(lying, t.facing);
        const float side = glm::dot(lying, glm::cross(glm::vec3{0.f, 0.f, 1.f}, t.facing));
        const float alongSweep = glm::dot(lying, t.along);
        const float acrossSweep = glm::dot(lying, left);
        // (Which way the sweep went in its own frame as it stood: its feet that way, so it should fall the other.)
        const float sf = glm::dot(t.along, t.facing);
        const float ss = glm::dot(t.along, glm::cross(glm::vec3{0.f, 0.f, 1.f}, t.facing));
        const char* sweptTo = za::fabs(sf) >= za::fabs(ss) ? (sf > 0.f ? "its front" : "its back")
                                                           : (ss > 0.f ? "its left" : "its right");
        Con_Printf("legsweep fell: its head %.1f along the sweep from its feet, %.1f across it (%s), %.1f ahead of them "
                   "by its facing, %.1f to its left: fell %s (swept towards %s); feet slid %.1f along the sweep\n",
            alongSweep, acrossSweep, alongSweep < 0.f ? "against it: toppled the other way" : "with it", fwd, side,
            za::fabs(fwd) >= za::fabs(side) ? (fwd > 0.f ? "forward" : "backward") : (side > 0.f ? "to its left" : "to its right"),
            sweptTo, glm::dot(df, t.along));
        t.ent = 0;
    }
}

void startTrace(edict_t* m, const glm::vec3& along)
{
    glm::vec3 pelvis, head, feet;
    if(!box3d::ragdollStance(NUM_FOR_EDICT(m), pelvis, head, feet))
    {
        return;
    }
    const float yaw = glm::radians(m->v.angles[1]);
    state.trace = Trace{NUM_FOR_EDICT(m), qcvm->time, qcvm->time, along, glm::vec3{za::cos(yaw), za::sin(yaw), 0.f}, feet,
        pelvis};
}

// The hand's thing swept from `was` to `held` this frame (`dt` s) against the humanoids near it.
void sweepHand(edict_t* player, int hand, const Track& was, const Held& held, float dt)
{
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int i = 0; i < sweepPoints; i++)
    {
        lo = glm::min(lo, glm::min(was.pts[i], held.pts[i]));
        hi = glm::max(hi, glm::max(was.pts[i], held.pts[i]));
    }
    const float r = za::clamp(held.measure.cross * 0.5f, 1.f, maxRadius);
    lo -= glm::vec3{r};
    hi += glm::vec3{r};
    for(int e = 1; e < qcvm->num_edicts; e++)
    {
        edict_t* m = EDICT_NUM(e);
        if(m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f ||
            (static_cast<int>(m->v.solid) != SOLID_SLIDEBOX && static_cast<int>(m->v.solid) != SOLID_BBOX))
        {
            continue;
        }
        if(m->v.absmin[0] > hi.x || m->v.absmax[0] < lo.x || m->v.absmin[1] > hi.y || m->v.absmax[1] < lo.y ||
            m->v.absmin[2] > hi.z || m->v.absmin[2] + legShare * (m->v.absmax[2] - m->v.absmin[2]) < lo.z)
        {
            continue;
        }
        if(!foegrab::humanoid(m) || sweptLately(e, qcvm->time))
        {
            continue;
        }
        // Of its points that met the legs, the lowest where it met them decides (what takes the feet: a sword held
        // hilt down sweeps with its pommel under the blade's root).
        float bestZ = 1e9f;
        int best = -1;
        for(int i = 0; i < sweepPoints; i++)
        {
            const float t = legsMet(m, was.pts[i], held.pts[i], r);
            const float z = was.pts[i].z + (held.pts[i].z - was.pts[i].z) * za::max(t, 0.f);
            if(t >= 0.f && z < bestZ)
            {
                bestZ = z;
                best = i;
            }
        }
        if(best < 0)
        {
            continue;
        }
        const glm::vec3 v = (held.pts[best] - was.pts[best]) / dt;
        const glm::vec3 level{v.x, v.y, 0.f};
        const float speed = glm::length(level);
        if(glm::length(v) > maxSpeed * units::metresToUnits())
        {
            continue; // (moved there at once: a hand put somewhere, a teleport)
        }
        const float share = static_cast<float>(best) / (sweepPoints - 1);
        const Measure& me = held.measure;
        char why[192];
        q_snprintf(why, sizeof(why), "%s, %.0f long, %.1f slender, its point %.2f of the way, %.2f m/s level, %.2f up",
            me.name, me.length, slenderness(me), share, speed / units::metresToUnits(), v.z / units::metresToUnits());
        if(!staffLike(me))
        {
            note(hand, m, va("%s: not staff-like (vr_legsweep_min_length %g, _min_slender %g)", why,
                              vr_legsweep_min_length.value, vr_legsweep_min_slender.value));
            continue;
        }
        if(me.edge >= 0.f && share >= me.edge)
        {
            note(hand, m, va("%s: the blade (sharp from %.2f): a cut, not a sweep", why, me.edge));
            continue;
        }
        if(za::fabs(v.z) > levelSlope * speed)
        {
            note(hand, m, va("%s: not level", why));
            continue;
        }
        const float mass = foegrab::monsterMass(player, m);
        const float heavy = vr_legsweep_heavy.value;
        const float need = za::max(vr_legsweep_speed.value, 0.f) * units::metresToUnits() *
                           (heavy > 0.f && mass > heavy ? za::pow(mass / heavy, 1.f / 3.f) : 1.f);
        if(speed < need)
        {
            note(hand, m, va("%s: too slow (%.0f kg: %.2f m/s needed)", why, mass, need / units::metresToUnits()));
            continue;
        }
        const glm::vec3 along = level / speed;
        const float feet = za::clamp(speed * za::max(vr_legsweep_feet.value, 0.f), 0.f, maxFeet);
        const float topple = glm::radians(za::clamp(vr_legsweep_topple.value, 0.f, 2000.f));
        const float result = foegrab::sweepDown(player, m, along, topple, feet, za::max(vr_legsweep_damage.value, 0.f));
        state.swept[state.sweptNext] = Swept{e, qcvm->time};
        state.sweptNext = (state.sweptNext + 1) % maxSwept;
        if(debugOn())
        {
            state.lastNote[hand] = 0.0;
            note(hand, m, va("%s, %.0f kg: %s (sweep yaw %.0f, feet %.0f u/s)", why, mass,
                              result >= 2.f   ? "the training dummy: would be swept"
                              : result >= 1.f ? "SWEPT DOWN"
                                              : "can't be knocked down now",
                              glm::degrees(za::atan2(along.y, along.x)), feet));
        }
        if(result >= 1.f)
        {
            state.hands[hand].restUntil = qcvm->time + handRest;
            if(result < 2.f && debugOn())
            {
                startTrace(m, along);
            }
            return;
        }
    }
}

// vr_legsweep_table: every weapon model loaded and some props: their length, cross-section, slenderness, blade and
// verdict.
void table_f()
{
    const float m2u = units::metresToUnits();
    const float k = 100.f / m2u; // (units to cm)
    Con_Printf("leg sweep table (vr_legsweep_min_length %g cm, _min_slender %g; cm):\n", vr_legsweep_min_length.value,
        vr_legsweep_min_slender.value);
    Con_Printf("%-28s %6s %6s %6s %6s %6s %6s  %s\n", "model", "length", "cross", "thick", "reach", "slendr", "blade",
        "verdict");
    const auto row = [&](const Measure& me) {
        Con_Printf("%-28s %6.1f %6.1f %6s %6s %6.2f %6s  %s\n", me.name, me.length * k, me.cross * k,
            me.weapon ? va("%.1f", me.thickest * k) : "-", me.weapon ? va("%.1f", me.reach * k) : "-", slenderness(me),
            me.edge >= 0.f ? va("%.2f+", me.edge) : "-",
            !me.ok ? "not measured" : staffLike(me) ? (me.edge >= 0.f ? "STAFF (its blunt part)" : "STAFF") : "no");
    };
    for(int i = 1; i < MAX_MODELS; i++)
    {
        qmodel_t* model = cl.model_precache[i];
        if(!model)
        {
            break;
        }
        const int slot = weapons::slotForModel(model);
        if(model->type == mod_alias && slot >= 0 && slot != weapons::fistSlot() && !strncmp(model->name, "progs/v_", 8))
        {
            row(measureWeapon(model));
        }
    }
    constexpr const char* props[] = {"progs/vr_plank1.mdl", "progs/vr_plank2.mdl", "progs/vr_plank3.mdl",
        "progs/vr_plank4.mdl", "progs/vrtorch.mdl", "progs/vr_crate1.mdl", "progs/vr_crate2.mdl", "progs/vr_brick1.mdl",
        "progs/vr_brick3.mdl", "progs/vr_rock1.mdl", "progs/vr_rock3.mdl", "progs/vr_barrel.mdl", "progs/vrflashlight.mdl",
        "progs/gib1.mdl", "progs/h_guard.mdl", "maps/b_explob.bsp", "maps/b_bh10.bsp"};
    for(const char* name : props)
    {
        for(int i = 1; i < MAX_MODELS && sv.models[i]; i++)
        {
            if(!strcmp(sv.models[i]->name, name))
            {
                row(measureProp(sv.models[i]));
                break;
            }
        }
    }
}

} // namespace

void qvr::legsweep::init()
{
    Cmd_AddCommand("vr_legsweep_table", table_f);
}

void qvr::legsweep::serverFrame()
{
    if(!sv.active || svs.maxclients < 1 || cls.state != ca_connected)
    {
        return;
    }
    VR_ProfileBegin("legsweep");
    const double now = qcvm->time;
    if(now < state.lastTime)
    {
        state = State{}; // a new map or a loaded game
    }
    state.lastTime = now;
    traceStep();
    edict_t* player = EDICT_NUM(1);
    const bool on = vr_legsweep.value != 0.f && !player->free && player->v.health > 0.f;
    for(int hand = 0; hand < 2; hand++)
    {
        Track& tr = state.hands[hand];
        Held held;
        if(!on || !heldThing(player, hand, held))
        {
            tr.valid = false;
            continue;
        }
        if(vr_legsweep_debug.value >= 2.f)
        {
            Con_Printf("legsweep: %.2f the %s hand's %s from %.0f %.0f %.0f to %.0f %.0f %.0f\n", now,
                hand == HAND_MAIN ? "main" : "off", held.measure.name, held.pts[0].x, held.pts[0].y, held.pts[0].z,
                held.pts[sweepPoints - 1].x, held.pts[sweepPoints - 1].y, held.pts[sweepPoints - 1].z);
        }
        bool moved = false;
        for(int i = 0; i < sweepPoints && !moved; i++)
        {
            moved = held.pts[i] != tr.pts[i];
        }
        if(tr.valid && !moved && tr.what == held.what && tr.ent == held.ent)
        {
            continue; // (not drawn again since: its move counts at the next)
        }
        const float dt = static_cast<float>(now - tr.time);
        if(tr.valid && tr.what == held.what && tr.ent == held.ent && dt > 1e-4f && dt < 0.25f && now >= tr.restUntil)
        {
            sweepHand(player, hand, tr, held, dt);
        }
        tr.valid = true;
        tr.what = held.what;
        tr.ent = held.ent;
        tr.time = now;
        for(int i = 0; i < sweepPoints; i++)
        {
            tr.pts[i] = held.pts[i];
        }
    }
    VR_ProfileEnd();
}
