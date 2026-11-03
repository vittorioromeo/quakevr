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
// Embedded blades (ROUND23.md, "Embedded blades"): weapons.txt's `embed` says how a weapon sticks: by its edge (the test
// above, the edges' numbers its own keys) or by its point (a sword: its tip, from the Muzzle anchor; what goes in first
// must be within vr_embed_sword_tip of it, going along the blade's line within vr_embed_sword_angle and within
// vr_embed_sword_incidence of straight in; vr_embed_sword_depth_* say how deep). A sword in the level sags under its
// weight on a spring about where it went in (stepSag), stopping at what is below. Bodies lying down (touchable, not solid:
// SV_Move's MOVE_NORMAL passes them) are looked for too (findBodies). An unbroken plate stops either (QC's
// VR_AxeStick_Plate). vr_embed 0: the two axes alone, as before.
//
// Where it is kept: the axe's pose in the frame of what it is in (its origin and angles: a door's, a prop's as drawn,
// a monster's yaw; the world's; or the monster model's triangle it went into, as drawn now: vr_hitmodel.cpp's
// anchorFrame), in QC fields, so that a saved game keeps it. Each server frame puts it back there.

#include "vr_modelmetadata.hpp"
#include "vr_axestick.hpp"

#include "vr_box3d.hpp"
#include "vr_lines.hpp"
#include "vr_text3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"

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

// How a weapon sticks (weapons.txt's `embed`; QC AXESTICK_POINT, AXESTICK_EDGE).
constexpr int howNone = 0;
constexpr int howPoint = 1;
constexpr int howEdge = 2;

// A weapon's blade, in its model's own space (shapeOf: from its weapons.txt keys).
struct Blade
{
    int how{howNone};
    za::Array<Edge, 2> edges{};
    glm::vec3 head{0.f};   // an edge's: the head's middle on the handle's axis (the blade's width: from it to the edge)
    glm::vec3 handle{0.f}; // the handle's far end
    glm::vec3 tip{0.f};    // a point's: its tip (the Muzzle anchor and offset),
    glm::vec3 hand{0.f};   // where the hand holds it (the line to its tip: the blade's axis),
    glm::vec3 from{0.f};   // and where the blade proper starts on that line (BladeFrom)
};

// The kinds of thing it is in (.vr_stick_kind).
enum Kind : int
{
    Out = -1,    // it came out: it never sticks again
    None = 0,
    Level = 1,   // the world or a brush entity (a door, a lift): its origin and angles
    Prop = 2,    // a Box3D prop: its origin and angles as drawn
    Model = 3,   // a monster's model: the triangle it went into
    MonsterBox = 4, // a monster without a model to go by (no Quake .mdl): its origin and yaw
};

using geom::vec;

using geom::store;

[[nodiscard]] qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
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
            // (vr_embed: it stays in a body dying, not solid for now.)
            return hitmodel::anchorFrame(host, static_cast<int>(anchor.x), anchor.y, anchor.z, point, axes, vr_embed.value != 0.f);
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

// Each model's blade and its samples, as they were made (models' slots: a game dir change, a model reload).
struct Shape
{
    const qmodel_t* model{nullptr};
    Blade blade;
    za::SizeT firstSample{0}, sampleCount{0};
    bool sampled{false};
};
struct StickCache
{
    za::Vector<Sample> samples;
    za::Vector<Shape> shapes;
    auto members() { return qvr::mem::list(samples, shapes); }
};
mem::Cache<StickCache> cache{"axe stick", mem::GameDirChange | mem::ModelReload};

// The test's buffers (Box3D's step: the main thread).
struct StickScratch
{
    za::Vector<glm::vec3> drawn; // the samples as drawn, in the axe's axes from its origin (beforeStep)
    za::Vector<int> bodies;      // the lying bodies near it (findBodies)
    auto members() { return qvr::mem::list(drawn, bodies); }
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
    if(blade.how == howPoint)
    {
        // How far behind its tip, a share of the way from the hand to it (the front half: vr_embed_sword_tip takes less).
        const glm::vec3 line = blade.tip - blade.hand;
        const float back = glm::dot(blade.tip - p, line) / za::max(1e-6f, glm::dot(line, line));
        if(back >= -0.1f && back <= 0.5f)
        {
            s.bit = 0;
            s.back = za::max(0.f, back);
            s.along = 0.5f;
        }
        return s;
    }
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

// A point's blade, from the weapon's Muzzle anchor and offset (view::weaponLocalGeometry: as drawn from its origin, where a
// hand holds it; turned back into its model's own space, as the drawn transform is a scale and a move).
[[nodiscard]] Blade pointBlade(edict_t* ent, qmodel_t* model, int slot)
{
    Blade b;
    glm::vec3 muzzle{0.f}, butt{0.f}, second{0.f};
    int secondType = 0;
    if(!view::weaponLocalGeometry(model, muzzle, butt, second, secondType) || glm::length(muzzle) < 1.f)
    {
        return b;
    }
    const glm::vec3 zero = held::drawnModelPoint(ent, glm::vec3{0.f});
    const glm::vec3 scale = held::drawnModelPoint(ent, glm::vec3{1.f}) - zero;
    if(za::abs(scale.x) < 1e-4f || za::abs(scale.y) < 1e-4f || za::abs(scale.z) < 1e-4f)
    {
        return b;
    }
    const auto own = [&](const glm::vec3& drawn) { return (drawn - zero) / scale; };
    constexpr float guessedFrom = 0.25f; // (bl_from -1: the kind's own, QC VR_Melee_Layout's; about the guard's)
    const float from = weapons::value(slot, weapons::Key::BladeFrom);
    b.how = howPoint;
    b.tip = own(muzzle);
    b.hand = own(glm::vec3{0.f});
    b.handle = own(butt);
    b.from = glm::mix(b.hand, b.tip, from >= 0.f ? za::min(from, 0.9f) : guessedFrom);
    return b;
}

// The blade of the model `ent` is drawn with, as its weapons.txt keys say.
[[nodiscard]] Blade bladeFromKeys(edict_t* ent, qmodel_t* model)
{
    using weapons::Key;
    Blade b;
    const int slot = weapons::slotForModel(model);
    if(slot < 0)
    {
        return b;
    }
    const int how = static_cast<int>(weapons::value(slot, Key::Embed));
    if(how == howPoint)
    {
        return pointBlade(ent, model, slot);
    }
    if(how == howEdge)
    {
        b.how = howEdge;
        b.edges[0] = {weapons::vec(slot, Key::EmbedE1AX, Key::EmbedE1AY, Key::EmbedE1AZ),
            weapons::vec(slot, Key::EmbedE1BX, Key::EmbedE1BY, Key::EmbedE1BZ),
            weapons::vec(slot, Key::EmbedE1OX, Key::EmbedE1OY, Key::EmbedE1OZ)};
        b.edges[1] = {weapons::vec(slot, Key::EmbedE2AX, Key::EmbedE2AY, Key::EmbedE2AZ),
            weapons::vec(slot, Key::EmbedE2BX, Key::EmbedE2BY, Key::EmbedE2BZ),
            weapons::vec(slot, Key::EmbedE2OX, Key::EmbedE2OY, Key::EmbedE2OZ)};
        b.head = weapons::vec(slot, Key::EmbedHeadX, Key::EmbedHeadY, Key::EmbedHeadZ);
        b.handle = weapons::vec(slot, Key::EmbedButtX, Key::EmbedButtY, Key::EmbedButtZ);
    }
    return b;
}

[[nodiscard]] Shape& shapeOf(edict_t* ent, qmodel_t* model)
{
    for(Shape& s : cache.shapes)
    {
        if(s.model == model)
        {
            return s;
        }
    }
    Shape made;
    made.model = model;
    made.blade = bladeFromKeys(ent, model);
    cache.shapes.pushBack(made);
    return cache.shapes.back();
}

// How `ent` sticks (null: it doesn't). vr_embed 0: the axe and the Shadow Axe alone, as before it.
[[nodiscard]] const Blade* bladeOf(edict_t* ent)
{
    qmodel_t* model = modelOf(ent);
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    if(!vr_embed.value)
    {
        const auto& info = modelmeta::get(model);
        if(!info.is(modelmeta::Id::VAxe) && !info.is(modelmeta::Id::AdVrAxe))
        {
            return nullptr;
        }
    }
    const Blade& blade = shapeOf(ent, model).blade;
    return blade.how != howNone ? &blade : nullptr;
}

// A view of a model's samples.
struct SampleRange
{
    const Sample* first{nullptr};
    za::SizeT count{0};
    [[nodiscard]] za::SizeT size() const { return count; }
    [[nodiscard]] const Sample& operator[](za::SizeT i) const { return first[i]; }
    [[nodiscard]] const Sample* begin() const { return first; }
    [[nodiscard]] const Sample* end() const { return first + count; }
};

[[nodiscard]] SampleRange samplesOf(edict_t* ent, const Blade& blade)
{
    qmodel_t* model = modelOf(ent);
    Shape& shape = shapeOf(ent, model);
    za::Vector<Sample>& out = cache.samples;
    if(shape.sampled)
    {
        return {out.data() + shape.firstSample, shape.sampleCount};
    }
    shape.sampled = true;
    shape.firstSample = out.size();
    const za::SizeT firstOfThis = out.size();
    const auto done = [&] {
        shape.sampleCount = out.size() - firstOfThis;
        return SampleRange{out.data() + firstOfThis, shape.sampleCount};
    };
    const auto add = [&](const glm::vec3& p, float spacing) {
        if(out.size() - firstOfThis >= maxSamples)
        {
            return;
        }
        for(za::SizeT i = firstOfThis; i < out.size(); i++)
        {
            if(glm::distance(out[i].p, p) < spacing)
            {
                return;
            }
        }
        out.pushBack(classify(blade, p));
    };
    if(blade.how == howPoint)
    {
        add(blade.tip, 0.5f);
    }
    else
    {
        for(const Edge& e : blade.edges)
        {
            for(int i = 0; i < edgePoints; i++)
            {
                add(glm::mix(e.a, e.b, static_cast<float>(i) / static_cast<float>(edgePoints - 1)), 0.5f);
            }
        }
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc || hdr->numframes <= 0)
    {
        return done(); // (its edges alone)
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
    return done();
}

// What the sample `s` of the axe is (the debug's lines).
[[nodiscard]] const char* partName(edict_t* ent, const Blade& blade, const Sample& s)
{
    if(blade.how == howPoint)
    {
        if(s.bit >= 0)
        {
            return s.back * 100.f <= vr_embed_sword_tip.value ? "the tip" : "the blade";
        }
        const glm::vec3 line = blade.tip - blade.hand;
        return glm::dot(s.p - blade.from, line) >= 0.f ? "the blade" : "the hilt";
    }
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
constexpr float stillGlance = 20.f; // degrees less glancing an axe that hardly turns may be (beforeStep)
constexpr float handleLeniency = 25.f;

// Embedded Blades' panel (vr_debug_embed): the last reasons.
constexpr int sayMost = 7;
constexpr double sayKeep = 8.0; // s of real time: read in slow motion
struct SayLine
{
    double when{-1.0};
    char text[150]{};
};
SayLine sayLines[sayMost];
int sayNext = 0;

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

// report(), and on the panel. (Forward: the namespace's.)
void reason(edict_t* ent, const char* fmt, ...)
{
    if(!vr_debug_axestick.value && !vr_debug_embed.value)
    {
        return;
    }
    char text[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    report(ent, "%s", text);
    if(vr_debug_embed.value)
    {
        SayLine& l = sayLines[sayNext];
        q_strlcpy(l.text, text, sizeof(l.text));
        l.when = realtime;
        sayNext = (sayNext + 1) % sayMost;
    }
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

    // A body lying down (touchable, not solid: SV_Move passed it).
    for(const int num : scratch.bodies)
    {
        edict_t* h = EDICT_NUM(num);
        hitmodel::Hit hit;
        if(hitmodel::segment(h, from, to, hitmodel::tolerance(hitmodel::Class::Thrown), 1.f, hitmodel::Class::Thrown, hit) &&
            hit.tri >= 0 && stepShare(hit.t) < best.fraction)
        {
            best = Strike{};
            best.fraction = stepShare(hit.t);
            best.host = h;
            best.kind = Model;
            best.sample = sample;
            best.local = local;
            best.point = hit.surface;
            best.normal = hit.normal;
            best.anchor = glm::vec3{static_cast<float>(hit.tri), hit.u, hit.v};
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
        // An explosive box is a metal drum (physsound::materialOf's metal): the blade rings off it, unless
        // vr_axestick_metal.
        const qmodel_t* model = modelOf(best.host);
        if(!vr_axestick_metal.value && model && model->type == mod_brush &&
            (modelmeta::has(model, modelmeta::Trait::ExplosiveBox) || modelmeta::has(model, modelmeta::Trait::ExplosiveCrate)))
        {
            best.why = "metal (vr_axestick_metal 0)";
        }
    }
    return best;
}

// The bodies lying down within `reach` units of the blade's centre `com` and that its sweeps look at (scratch.bodies):
// a corpse, a knocked down monster: touchable, a move passes them.
void findBodies(edict_t* ent, const glm::vec3& com, float reach)
{
    za::Vector<int>& out = scratch.bodies;
    out.clear();
    if(!vr_embed.value)
    {
        return;
    }
    constexpr float limbs = 48.f; // (units a limb lies out of its box)
    progs::forEachEdict(svs.maxclients + 1, [&](edict_t* e) {
        if(e == ent || static_cast<int>(e->v.solid) != SOLID_NOT_BUT_TOUCHABLE || !(static_cast<int>(e->v.flags) & FL_MONSTER))
        {
            return;
        }
        const glm::vec3 lo = vec(e->v.absmin), hi = vec(e->v.absmax);
        if(glm::distance(com, (lo + hi) * 0.5f) <= reach + glm::distance(lo, hi) * 0.5f + limbs && hitmodel::target(e))
        {
            out.pushBack(NUM_FOR_EDICT(e));
        }
    });
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
    const SampleRange samples = samplesOf(ent, blade);
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

// The pose `o`, `r` given: the entity's, and for QC the place it went in at (`at`) and the way into what it is in (`dir`).
void setPose(edict_t* ent, const glm::vec3& o, const glm::mat3& r, const glm::vec3& at, const glm::vec3& dir)
{
    const FieldOffsets& f = fields();
    vec3_t angles;
    held::anglesFromAxes(r, angles, false);
    const bool moved = glm::distance(o, vec(ent->v.origin)) > 1e-3f || glm::distance(vec(angles), vec(ent->v.angles)) > 1e-3f;
    store(o, ent->v.origin);
    store(vec(angles), ent->v.angles);
    setFieldVec(ent, f.vr_stick_at, at);
    setFieldVec(ent, f.vr_stick_dir, dir);
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

// A sword's way along its line (the model's own turned by its axes: from the hand to the tip), as drawn, and where it went
// in (from its origin): `.vr_stick_depth` back from its tip.
struct PointLine
{
    glm::vec3 tip{0.f}, entry{0.f}, butt{0.f}, along{1.f, 0.f, 0.f};
};
[[nodiscard]] PointLine pointLine(edict_t* ent, const Blade& blade)
{
    PointLine l;
    l.tip = held::drawnModelPoint(ent, blade.tip);
    l.butt = held::drawnModelPoint(ent, blade.handle);
    const glm::vec3 line = l.tip - held::drawnModelPoint(ent, blade.hand);
    l.along = glm::length(line) > 1e-3f ? glm::normalize(line) : l.along;
    // (Slid: its tip further in, or back, where it went in the same place.)
    l.entry = l.tip - l.along * (fieldFloatOr(ent, fields().vr_stick_depth, 0.f) + fieldFloatOr(ent, fields().vr_stick_slide, 0.f));
    return l;
}

// The turn a sag of `degrees` makes of a blade going `in` (its tip's way): about where it went in, its hilt towards the
// floor. None for a blade upright (nothing to pull it over).
[[nodiscard]] glm::mat3 sagTurn(const glm::vec3& in, float deg)
{
    const glm::vec3 side = glm::cross(-in, glm::vec3{0.f, 0.f, -1.f});
    const float lever = glm::length(side);
    return lever > 0.05f ? glm::mat3_cast(glm::angleAxis(glm::radians(deg), side / lever)) : glm::mat3{1.f};
}

// A sword in the level: its sag's spring this frame (`origin`, `axes`: as it stuck, in the world). Gravity pulls it as a
// rod hung where it went in (1.5 g / length, by how level it lies), the spring vr_embed_sag_stiffness and
// vr_embed_sag_damping hold it back, at most vr_embed_sag_most; what is under its hilt stops it.
void stepSag(edict_t* ent, const Blade& blade, const glm::vec3& origin, const glm::mat3& axes, float dt)
{
    const FieldOffsets& f = fields();
    const float most = za::max(0.f, vr_embed_sag_most.value);
    float sag = fieldFloatOr(ent, f.vr_stick_sag, 0.f);
    float rate = fieldFloatOr(ent, f.vr_stick_sagv, 0.f);
    if(most <= 0.f)
    {
        setFieldFloat(ent, f.vr_stick_sag, 0.f);
        setFieldFloat(ent, f.vr_stick_sagv, 0.f);
        return;
    }
    if(dt <= 0.f)
    {
        return;
    }
    dt = za::min(dt, 0.05f);
    const PointLine l = pointLine(ent, blade);
    const glm::vec3 pivot = origin + axes * l.entry;
    const glm::vec3 butt = origin + axes * l.butt;
    const glm::vec3 in = axes * l.along;
    const float length = za::max(8.f, glm::distance(pivot, butt));
    const float lever = glm::length(glm::cross(in, glm::vec3{0.f, 0.f, 1.f}));
    const float pull = glm::degrees(1.5f * sv_gravity.value / length * lever);
    rate += (pull - za::max(1.f, vr_embed_sag_stiffness.value) * sag - za::max(0.f, vr_embed_sag_damping.value) * rate) * dt;
    float next = sag + rate * dt;
    if(next < 0.f || next > most)
    {
        next = za::clamp(next, 0.f, most);
        rate = 0.f;
    }
    if(za::abs(next - sag) > 0.02f)
    {
        // From just out of the wall to its hilt, sagged that far: anything there holds it where it was (it bounces a little).
        const glm::vec3 buttNext = pivot + sagTurn(in, next) * (butt - pivot);
        vec3_t a, b, zero{0.f, 0.f, 0.f};
        store(pivot + glm::normalize(buttNext - pivot) * 2.f, a);
        store(buttNext, b);
        const trace_t tr = SV_Move(a, zero, zero, b, MOVE_NOMONSTERS, ent);
        if(!tr.startsolid && tr.fraction < 1.f)
        {
            next = sag;
            rate *= -0.3f;
        }
    }
    setFieldFloat(ent, f.vr_stick_sag, next);
    setFieldFloat(ent, f.vr_stick_sagv, rate);
}

// The blade in what it is in: its pose there (the frame's), the blade's edge (or where its point went in) and facing in the
// world.
void place(edict_t* ent, const Blade& blade, const glm::vec3& origin, const glm::mat3& axes)
{
    const FieldOffsets& f = fields();
    glm::vec3 o = origin;
    glm::mat3 r = axes;
    const float wiggle = fieldFloatOr(ent, f.vr_stick_wiggle, 0.f);
    const float slide = fieldFloatOr(ent, f.vr_stick_slide, 0.f);
    if(blade.how == howPoint)
    {
        const PointLine l = pointLine(ent, blade);
        o += r * l.along * slide; // (a hand's hold: along its line, QC's)
        const glm::vec3 pivot = o + r * l.entry;
        const auto turnAbout = [&](const glm::mat3& turn) {
            r = turn * r;
            o = pivot + turn * (o - pivot);
        };
        const float sag = fieldFloatOr(ent, f.vr_stick_sag, 0.f);
        if(sag != 0.f)
        {
            turnAbout(sagTurn(r * l.along, sag));
        }
        if(wiggle != 0.f)
        {
            turnAbout(glm::mat3_cast(glm::angleAxis(glm::radians(wiggle), glm::normalize(r[1]))));
        }
        setPose(ent, o, r, pivot, r * l.along);
        return;
    }
    const DrawnEdge e = drawnEdge(ent, blade, za::clamp(static_cast<int>(fieldFloat(ent, f.vr_stick_edge)), 0, 1));
    const glm::vec3 wound = o + r * e.mid; // (a hand's hold slides it along its facing, from where it went in)
    o += r * e.out * slide;
    if(wiggle != 0.f)
    {
        // Rocked about the edge's middle, across the blade (the head is thin along its model's y).
        const glm::vec3 pivot = o + r * e.mid;
        const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(glm::radians(wiggle), glm::normalize(r[1])));
        r = turn * r;
        o = pivot + turn * (o - pivot);
    }
    setPose(ent, o, r, wiggle != 0.f ? o + r * e.mid : wound, r * e.out);
}

// Where a blade went in, in its own axes from its origin (not slid: as it stuck; a sword's where its depth says, an edge's middle).
[[nodiscard]] glm::vec3 woundLocal(edict_t* ent, const Blade& blade)
{
    if(blade.how == howPoint)
    {
        const PointLine l = pointLine(ent, blade);
        return l.tip - l.along * fieldFloatOr(ent, fields().vr_stick_depth, 0.f);
    }
    return drawnEdge(ent, blade, za::clamp(static_cast<int>(fieldFloat(ent, fields().vr_stick_edge)), 0, 1)).mid;
}

// A blade in a body's model: where it went in stays on its triangle, its turn follows the triangle's eased (from how it lies now, its
// angles): fast in the torso, the pelvis and the head (vr_embed_torso_follow), slower in an arm or a leg (.vr_stick_loose,
// vr_embed_limb_follow; their animations are fast). 0: rigid. `origin`, `axes`: rigid on it, then as it is.
void easeOnBody(edict_t* ent, const Blade& blade, glm::vec3& origin, glm::mat3& axes, float dt)
{
    const float follow = fieldFloatOr(ent, fields().vr_stick_loose, 0.f) != 0.f ? vr_embed_limb_follow.value : vr_embed_torso_follow.value;
    if(follow <= 0.f || dt <= 0.f)
    {
        return;
    }
    const glm::vec3 local = woundLocal(ent, blade);
    const glm::vec3 wound = origin + axes * local;
    const glm::quat from = glm::quat_cast(held::axesFromAngles(ent->v.angles, false));
    const glm::quat to = glm::quat_cast(axes);
    axes = glm::mat3_cast(glm::slerp(from, to, 1.f - za::exp(-za::min(dt, 0.05f) * follow)));
    origin = wound - axes * local;
}

// Whether an unbroken plate of the body `host` stops the blade at `at` going along `dir` (QC's, with its spark). It never
// sticks after.
[[nodiscard]] bool plateStops(edict_t* ent, edict_t* host, const glm::vec3& at, const glm::vec3& dir)
{
    const func_t fn = bindings().AxeStick_Plate;
    if(!fn || !vr_embed.value)
    {
        return false;
    }
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = EDICT_TO_PROG(host);
    pr_global_struct->time = qcvm->time;
    store(at, G_VECTOR(OFS_PARM0));
    store(dir, G_VECTOR(OFS_PARM1));
    PR_ExecuteProgram(fn);
    const bool stops = G_FLOAT(OFS_RETURN) != 0.f;
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    if(stops)
    {
        setFieldFloat(ent, fields().vr_stick_kind, static_cast<float>(Out));
    }
    return stops;
}

// Stuck: `ent`'s pose in what it is in (`best`'s), for the frames to come. False: no frame there.
[[nodiscard]] bool keepPose(edict_t* ent, const Blade& blade, const Strike& best, int bit, const glm::vec3& stuckOrigin,
    const glm::mat3& stuckAxes, float depth, const glm::vec3& pointVel)
{
    const FieldOffsets& f = fields();
    glm::vec3 framePoint;
    glm::mat3 frameAxes;
    if(!frameOf(best.host, best.kind, best.anchor, framePoint, frameAxes))
    {
        return false;
    }
    const glm::mat3 inv = glm::transpose(frameAxes); // (orthonormal)
    vec3_t localAngles;
    held::anglesFromAxes(inv * stuckAxes, localAngles, true);
    setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(best.kind));
    fieldInt(ent, f.vr_stick_ent) = EDICT_TO_PROG(best.host);
    setFieldVec(ent, f.vr_stick_ofs, inv * (stuckOrigin - framePoint));
    setFieldVec(ent, f.vr_stick_ang, vec(localAngles));
    setFieldVec(ent, f.vr_stick_anchor, best.anchor);
    setFieldFloat(ent, f.vr_stick_edge, static_cast<float>(bit));
    setFieldFloat(ent, f.vr_stick_wiggle, 0.f);
    setFieldFloat(ent, f.vr_stick_embed, static_cast<float>(blade.how));
    setFieldFloat(ent, f.vr_stick_depth, depth);
    setFieldFloat(ent, f.vr_stick_sag, 0.f);
    setFieldFloat(ent, f.vr_stick_sagv, 0.f);
    store(pointVel, ent->v.velocity); // (the throw's, for QC's blow)
    place(ent, blade, stuckOrigin, stuckAxes);

    // A prop takes the blow.
    if(best.kind == Prop)
    {
        box3d::push(best.host, best.point, pointVel, box3d::propMass(ent));
    }
    return true;
}

// A sword's point going in (its checks and its depth; what went in first: `best`): true if it stuck.
[[nodiscard]] bool stickByPoint(edict_t* ent, const Blade& blade, const Strike& best, const glm::vec3& com, const glm::vec3& vel,
    const glm::vec3& spin, float dt, float minSpeed, func_t stuckFn)
{
    const float m2u = units::metresToUnits();
    edict_t* host = best.host;
    const float stepAt = za::clamp(best.fraction, 0.f, 1.f); // (of the step; a monster further on: its end)
    const glm::mat3 turnAt = rotation(spin, dt * stepAt);
    const glm::mat3 axesAt = turnAt * held::axesFromAngles(ent->v.angles, false);
    const glm::vec3 originAt = com + vel * dt * stepAt + turnAt * (vec(ent->v.origin) - com);
    const glm::vec3 pointAt = originAt + axesAt * best.local;
    const glm::vec3 hostVel = host == qcvm->edicts ? glm::vec3{0.f} : vec(host->v.velocity);
    const glm::vec3 pointVel = vel + glm::cross(spin, pointAt - com);
    const glm::vec3 rel = pointVel - hostVel;
    const float speed = glm::length(rel);
    const glm::vec3 tip = held::drawnModelPoint(ent, blade.tip);
    const glm::vec3 line = tip - held::drawnModelPoint(ent, blade.hand);
    const glm::vec3 along = axesAt * glm::normalize(line);
    const glm::vec3 relDir = speed > 1e-3f ? rel / speed : along;
    const float sideOn = degrees(relDir, along);
    const float incidence = degrees(relDir, -best.normal);
    const char* hostName = host == qcvm->edicts ? "the level" : PR_GetString(host->v.classname);
    const bool inBody = best.kind == Model || best.kind == MonsterBox;

    const char* why = best.why;
    if(!why && speed < minSpeed)
    {
        why = "too slow";
    }
    else if(!why && glm::dot(rel, best.normal) >= 0.f)
    {
        why = "not going into it";
    }
    else if(!why && sideOn > vr_embed_sword_angle.value)
    {
        why = "side on";
    }
    else if(!why && incidence > vr_embed_sword_incidence.value)
    {
        why = "too glancing";
    }

    // How deep, by its speed: a share of the blade, less in the level and props; along its line from where it struck.
    const float bladeLength = glm::distance(tip, held::drawnModelPoint(ent, blade.from));
    const float least = za::max(0.f, vr_embed_sword_depth_min.value), full = za::max(least, vr_embed_sword_depth_max.value);
    const float share = za::clamp((speed - minSpeed) / za::max(1.f, vr_embed_sword_depth_speed.value * m2u - minSpeed), 0.f, 1.f);
    const float depth = glm::mix(least, full, share) * 0.01f * bladeLength * (inBody ? 1.f : za::max(0.f, vr_embed_sword_wall.value));
    const glm::vec3 stuckOrigin = originAt + (best.point - pointAt) + along * depth;
    const float tipDepth = glm::dot(stuckOrigin + axesAt * tip - best.point, along);

    // Its hilt out of the wall (the level's, a door's, a prop's).
    if(!why && !inBody)
    {
        vec3_t a, b, zero{0.f, 0.f, 0.f};
        const glm::vec3 entry = best.point - along * 2.f;
        store(entry, a);
        store(glm::mix(entry, stuckOrigin + axesAt * held::drawnModelPoint(ent, blade.handle), 0.9f), b);
        const trace_t tr = SV_Move(a, zero, zero, b, MOVE_NOMONSTERS, ent);
        if(tr.startsolid || tr.fraction < 1.f)
        {
            why = "its hilt would be in the wall";
        }
    }
    if(!why && inBody && plateStops(ent, host, best.point, relDir))
    {
        why = "an unbroken plate";
    }
    if(why)
    {
        reason(ent, "the point struck %s at %.0f u/s (%.1f m/s), %.0f deg off its line, %.0f deg off straight in, %.2f of the step: "
                    "bounces (%s)",
            hostName, speed, speed / m2u, sideOn, incidence, best.fraction, why);
        return false;
    }
    if(!keepPose(ent, blade, best, 0, stuckOrigin, axesAt, tipDepth, pointVel))
    {
        reason(ent, "no frame on %s: bounces", hostName);
        return false;
    }
    setFieldFloat(ent, fields().vr_stick_bladelen, bladeLength);
    reason(ent, "the point stuck in %s (%d, kind %d) at %.0f u/s (%.1f m/s), %.0f deg off its line, %.0f deg off straight in, "
                "%.2f of the step, %.1f units deep (%.0f%% of the blade %.1f long)",
        hostName, NUM_FOR_EDICT(host), best.kind, speed, speed / m2u, sideOn, incidence, best.fraction, tipDepth,
        depth / za::max(bladeLength, 1e-3f) * 100.f, bladeLength);
    callStuck(stuckFn, ent, host, best.kind, speed);
    return true;
}

} // namespace

namespace qvr::axestick
{

bool beforeStep(edict_t* ent, const glm::vec3& com, const glm::vec3& vel, const glm::vec3& spin, float dt)
{
    const FieldOffsets& f = fields();
    if((!vr_axestick.value && !vr_embed.value) || f.vr_stick_kind < 0 || f.vr_stick_ent < 0 ||
        fieldFloat(ent, f.vr_stick_kind) != 0.f || dt <= 0.f)
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
    if(!blade || (blade->how == howEdge && !vr_axestick.value))
    {
        return false;
    }
    const bool byPoint = blade->how == howPoint;

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
    const bool changed = lastVel != glm::vec3{0.f} &&
                         (glm::length(glm::vec2{dv.x, dv.y}) > 25.f || dv.z > 25.f || dv.z < -fall - 25.f);
    if(touched || changed)
    {
        setFieldFloat(ent, f.vr_stick_kind, static_cast<float>(Out));
        reason(ent, "hit something with %s (%.0f u/s, spin %.1f rad/s changed): never sticks now",
            touched ? partOf(ent, *blade, touch) : "?", glm::length(dv), glm::length(spin - lastSpin));
        return false;
    }

    const float m2u = units::metresToUnits();
    const float minSpeed = za::max(0.1f, byPoint ? vr_embed_sword_speed.value : vr_axestick_speed.value) * m2u;
    // (A blade 40 units from its centre at most; a sword's point its length.)
    const float bladeReach = byPoint ? glm::distance(blade->tip, blade->handle) : 40.f;
    if(glm::length(vel) + glm::length(spin) * bladeReach < minSpeed)
    {
        return false;
    }
    const func_t stuckFn = bindings().AxeStick_Stuck;
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
    const float band = byPoint ? za::max(0.f, vr_embed_sword_tip.value) * 0.01f : bandBase + bandLeniency * leniency;
    const float tie = tieUnits * leniency;

    // The axe's points as drawn now, and how far they reach from its centre.
    const SampleRange samples = samplesOf(ent, *blade);
    za::Vector<glm::vec3>& drawn = scratch.drawn;
    drawn.clear();
    float radius = 0.f;
    for(const Sample& s : samples)
    {
        drawn.pushBack(held::drawnModelPoint(ent, s.p));
        radius = za::max(radius, glm::distance(origin + axes * drawn.back(), com));
    }
    const float nearSize = radius * (1.f + glm::length(spin) * dt) + sweepBack + 1.f;
    findBodies(ent, com, nearSize + glm::length(move) + za::max(lookAhead, ahead));
    if(scratch.bodies.empty() && !anythingNear(ent, com, move, nearSize, za::max(lookAhead, ahead)))
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
            reason(ent, "%s%s struck %s first, %.2f of the step (the blade %s): bounces (%s first)%s", partName(ent, *blade, s),
                s.bit >= 0 ? va(" (blade %d, %.0f%% behind its edge)", s.bit, s.back * 100.f) : "", firstHost,
                first.fraction, cut.host ? va("%.1f units after", (cut.fraction - first.fraction) * first.len) : "not near",
                partName(ent, *blade, s), now ? ", never sticks now" : "");
        }
        return false;
    }

    // As it strikes: its turn, where it is, the point's velocity (relative to what it strikes), the blade's facing and
    // the way across it.
    const Strike& best = cut;
    if(byPoint)
    {
        return stickByPoint(ent, *blade, best, com, vel, spin, dt, minSpeed, stuckFn);
    }
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
    const float sideOn = glm::degrees(za::asin(za::clamp(za::abs(glm::dot(relDir, across)), 0.f, 1.f)));
    const float incidence = degrees(relDir, -best.normal);
    const float offFacing = degrees(facing, relDir); // (the debug's: the old test's angle)
    const float spinIn = glm::dot(glm::cross(spin, pointAt - com), -best.normal); // (u/s the spin drives the point in)
    const float incidenceMax = vr_axestick_incidence.value + incidenceLeniency * leniency;
    // A blow more glancing than vr_axestick_incidence - stillGlance (45 degrees) skids unless the axe turns (its spin a
    // tenth of the point's speed): the spin drives the point in, a still blade slides along (a handle-first throw
    // falling bit down onto the floor).
    const float spinShare = glm::length(glm::cross(spin, pointAt - com)) / za::max(glm::length(edgeVel), 1e-3f);
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
    else if(!why && incidence > vr_axestick_incidence.value - stillGlance && spinShare < 0.1f)
    {
        why = "glancing, not turning";
    }

    // How deep: the depth at twice the least speed, half as deep at it, never more than 60% of the blade; pushed the
    // way the point goes in the blade's plane (a corner along the way it came, the edge straight in).
    const glm::vec3 inPlane = relDir - across * glm::dot(relDir, across);
    const glm::vec3 push = glm::length(inPlane) > 1e-3f ? glm::normalize(inPlane) : facing;
    const float depthMax = za::max(0.f, vr_embed.value ? vr_embed_axe_depth.value : vr_axestick_depth.value) * 0.01f * m2u;
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
    if(!why && (best.kind == Model || best.kind == MonsterBox) && plateStops(ent, host, best.point, relDir))
    {
        why = "an unbroken plate";
    }
    if(why)
    {
        reason(ent, "blade %d (%s) struck %s at %.0f u/s (%.1f m/s), %.0f deg side on, %.0f deg off straight in (%.0f off its "
                    "facing), %.2f of the step: bounces (%s)",
            part.bit, partLabel, hostName, speed, speed / m2u, sideOn, incidence, offFacing, best.fraction, why);
        return false;
    }

    // Stuck: its pose in what it is in.
    const int kind = best.kind;
    if(!keepPose(ent, *blade, best, part.bit, stuckOrigin, stuckAxes, 0.f, edgeVel))
    {
        reason(ent, "no frame on %s: bounces", hostName);
        return false;
    }
    reason(ent, "blade %d stuck in %s (%d, kind %d) by %s (%.0f%% behind its edge) at %.0f u/s (%.1f m/s), %.0f deg side on, "
                "%.0f deg off straight in (%.0f off its facing), spin in %.0f u/s, %.2f of the step, %.2f units deep (%.1f cm; the "
                "blade %.2f wide)%s",
        part.bit, hostName, NUM_FOR_EDICT(host), kind, partLabel, part.back * 100.f, speed, speed / m2u, sideOn, incidence,
        offFacing, spinIn, best.fraction, depth, depth / m2u * 100.f, edge.width,
        tilted > 0.f ? va(", turned %.0f deg to keep its handle out", tilted) : "");

    callStuck(stuckFn, ent, host, kind, speed);
    return true;
}

bool stickHeld(edict_t* ent, edict_t* host, const glm::vec3& at, const glm::vec3& vel, int hand)
{
    const FieldOffsets& f = fields();
    const func_t stuckFn = bindings().AxeStick_Stuck;
    const Blade* blade = bladeOf(ent);
    if(!stuckFn || !blade || blade->how == howNone || !vr_embed.value || host == qcvm->edicts || host->free)
    {
        return false;
    }
    // Where the hand's weapon is drawn (the local player's; the hand's own place, QC's, is off it by its settings' angles).
    glm::vec3 drawnAt, drawnAngles;
    if(hand >= 0 && view::drawnWeaponPlace(hand, modelOf(ent), drawnAt, drawnAngles))
    {
        store(drawnAt, ent->v.origin);
        store(drawnAngles, ent->v.angles);
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
    const float speed = glm::length(vel);
    const glm::vec3 way = speed > 1e-3f ? vel / speed : axes[0];
    const char* hostName = PR_GetString(host->v.classname);

    Strike best;
    best.host = host;
    hitmodel::Hit hit;
    float away = 0.f;
    constexpr float nearEnough = 32.f; // units: its model no longer there (the blow cut it off): no
    if(hitmodel::nearestAny(host, at, hit, away) && hit.tri >= 0 && away <= nearEnough)
    {
        best.kind = Model;
        best.point = hit.surface;
        best.normal = hit.normal;
        best.anchor = glm::vec3{static_cast<float>(hit.tri), hit.u, hit.v};
    }
    else if((static_cast<int>(host->v.flags) & FL_MONSTER) && !hitmodel::target(host))
    {
        best.kind = MonsterBox;
        best.point = at;
        best.normal = -way;
    }
    else
    {
        reason(ent, "a hand's blow at %s, %.1f units from its model: not stuck", hostName, away);
        return false;
    }

    const float m2u = units::metresToUnits();
    glm::vec3 stuckOrigin = origin;
    int bit = 0;
    float depth = 0.f, goal = 0.f, bladeLength = 0.f;
    if(blade->how == howPoint)
    {
        const glm::vec3 tip = held::drawnModelPoint(ent, blade->tip);
        const glm::vec3 along = axes * glm::normalize(tip - held::drawnModelPoint(ent, blade->hand));
        bladeLength = glm::distance(tip, held::drawnModelPoint(ent, blade->from));
        // How deep it goes on to, by the blow's speed; as it is struck it is in as far as it is, the hand's own motion
        // and QC's ease take it on (a hand must not be flung there).
        const float least = vr_embed_stab_speed.value * m2u;
        const float lo = za::max(0.f, vr_embed_sword_depth_min.value), hi = za::max(lo, vr_embed_sword_depth_max.value);
        const float share = za::clamp((speed - least) / za::max(1.f, vr_embed_sword_depth_speed.value * m2u - least), 0.f, 1.f);
        goal = glm::mix(lo, hi, share) * 0.01f * bladeLength;
        const float now = glm::dot(origin + axes * tip - best.point, along);
        depth = za::clamp(now, za::min(1.f, goal), za::max(goal, 1.f));
        stuckOrigin = origin + along * (depth - now);
    }
    else
    {
        // The edge leading the blow: edge first, not its side nor along the handle.
        float bestLead = -2.f;
        for(int e = 0; e < 2; e++)
        {
            const float lead = glm::dot(axes * drawnEdge(ent, *blade, e).out, way);
            if(lead > bestLead)
            {
                bestLead = lead;
                bit = e;
            }
        }
        const DrawnEdge e = drawnEdge(ent, *blade, bit);
        const glm::vec3 out = axes * e.out;
        const glm::vec3 run = axes * (e.b - e.a);
        const glm::vec3 across = glm::length(glm::cross(run, out)) > 1e-4f ? glm::normalize(glm::cross(run, out)) : glm::vec3{0.f, 0.f, 1.f};
        const float sideOn = glm::degrees(za::asin(za::clamp(za::abs(glm::dot(way, across)), 0.f, 1.f)));
        const glm::vec3 handleWay = axes * (held::drawnModelPoint(ent, blade->head) - held::drawnModelPoint(ent, blade->handle));
        const float alongHandle = glm::length(handleWay) > 1e-3f ? za::abs(glm::dot(way, glm::normalize(handleWay))) : 0.f;
        constexpr float leadLeast = 0.25f; // the way the blow goes on the edge's facing: less, along its edge
        constexpr float handleMost = 0.8f; // the way the blow goes along the handle: more, driven head first, a stab
        if(sideOn > vr_embed_swing_angle.value || bestLead < leadLeast || alongHandle > handleMost)
        {
            reason(ent, "swing at %s NOT STUCK: side on %.0f of %.0f deg, edge first %.2f (least %.2f), along the handle %.2f (most %.2f)", hostName, sideOn,
                vr_embed_swing_angle.value, bestLead, leadLeast, alongHandle, handleMost);
            return false;
        }
        const float most = za::max(0.f, vr_embed_axe_depth.value) * 0.01f * m2u;
        const float wanted = za::min(most * za::clamp(speed / (2.f * vr_embed_swing_speed.value * m2u), 0.5f, 1.f), 0.6f * e.width);
        const float now = glm::dot(origin + axes * e.mid - best.point, out);
        constexpr float shiftMost = 8.f; // units a blow's edge is moved to its depth (a hand must not be flung there)
        stuckOrigin = origin + out * za::clamp(wanted - now, -shiftMost, shiftMost);
    }
    if((best.kind == Model || best.kind == MonsterBox) && plateStops(ent, host, best.point, way))
    {
        reason(ent, "a hand's blow at %s: an unbroken plate", hostName);
        return false;
    }
    setFieldFloat(ent, f.vr_stick_slide, 0.f);
    if(!keepPose(ent, *blade, best, bit, stuckOrigin, axes, depth, vel))
    {
        reason(ent, "a hand's blow at %s: no frame there", hostName);
        return false;
    }
    setFieldFloat(ent, f.vr_stick_goal, goal);
    setFieldFloat(ent, f.vr_stick_bladelen, bladeLength);
    reason(ent, "STUCK by the hand in %s (%d, kind %d) at %.1f m/s, %.1f units deep, going to %.1f (the blade %.1f long)", hostName,
        NUM_FOR_EDICT(host), best.kind, speed / m2u, depth, goal, bladeLength);
    SV_LinkEdict(ent, false);
    callStuck(stuckFn, ent, host, best.kind, speed);
    return true;
}

void calcStats(edict_t* player, int* statsi)
{
    using namespace protocol;
    const FieldOffsets& f = fields();
    const int handle = static_cast<int>(fieldFloatOr(player, f.vr_embed_handle, 0.f));
    for(int h = 0; h < 2; h++)
    {
        const int ofs = h ? f.vr_embed_main : f.vr_embed_off;
        int stat = 0;
        if(ofs >= 0)
        {
            const edict_t* w = PROG_TO_EDICT(fieldInt(player, ofs));
            if(w != qcvm->edicts && !w->free)
            {
                stat = NUM_FOR_EDICT(const_cast<edict_t*>(w)) * 2 + ((handle & (1 << h)) ? 1 : 0);
            }
        }
        statsi[h ? STAT_QVR_EMBEDMAIN : STAT_QVR_EMBEDOFF] = stat;
    }
}

void serverFrame()
{
    const FieldOffsets& f = fields();
    if(!sv.active || f.vr_stick_kind < 0 || f.vr_stick_ent < 0 || f.vr_stick_ofs < 0 || f.vr_stick_ang < 0)
    {
        return;
    }
    QVR_PROFILE("axe stick");
    progs::forEachEdict(1, [&](edict_t* ent) {
        if(fieldFloat(ent, f.vr_stick_kind) <= 0.f || static_cast<int>(ent->v.movetype) != MOVETYPE_NONE)
        {
            return;
        }
        const Blade* blade = bladeOf(ent);
        edict_t* host = PROG_TO_EDICT(fieldInt(ent, f.vr_stick_ent));
        glm::vec3 point;
        glm::mat3 axes;
        if(!blade || !frameOf(host, static_cast<int>(fieldFloat(ent, f.vr_stick_kind)), fieldVec(ent, f.vr_stick_anchor), point, axes))
        {
            return; // (gone, or its model can't say: QC drops it)
        }
        const glm::vec3 ang = fieldVec(ent, f.vr_stick_ang);
        const float a[3]{ang.x, ang.y, ang.z};
        glm::vec3 stuckAt = point + axes * fieldVec(ent, f.vr_stick_ofs);
        glm::mat3 stuckAxes = axes * held::axesFromAngles(a, true);
        if(vr_embed.value && static_cast<int>(fieldFloat(ent, f.vr_stick_kind)) == Model)
        {
            easeOnBody(ent, *blade, stuckAt, stuckAxes, static_cast<float>(host_frametime));
        }
        if(blade->how == howPoint && static_cast<int>(fieldFloat(ent, f.vr_stick_kind)) == Level)
        {
            stepSag(ent, *blade, stuckAt, stuckAxes, static_cast<float>(host_frametime));
        }
        place(ent, *blade, stuckAt, stuckAxes);
    });
}

// `text`'s lines (split at \n and wrapped) one under another from `at`, facing `eye`; the overlay itself draws one line.
static void overlayLines(const char* text, glm::vec3 at, const glm::vec3& eye, float scale, int wrap, float step)
{
    const glm::vec3 toText = at - eye;
    const glm::vec3 turn{0.f, glm::degrees(za::atan2(toText.y, toText.x)), 0.f};
    char row[96];
    int n = 0;
    for(const char* c = text;; c++)
    {
        const bool end = !*c || *c == '\n';
        if(!end)
        {
            row[n++] = *c;
        }
        if(end || n >= za::min(wrap, static_cast<int>(sizeof(row)) - 1))
        {
            row[n] = 0;
            if(n > 0)
            {
                text3d::queueOverlay(row, at, turn, scale);
                at.z -= step;
            }
            n = 0;
        }
        if(!*c)
        {
            return;
        }
    }
}

void say(const char* fmt, ...)
{
    if(!vr_debug_embed.value)
    {
        return;
    }
    char text[512];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    SayLine& l = sayLines[sayNext];
    q_strlcpy(l.text, text, sizeof(l.text));
    l.when = realtime;
    sayNext = (sayNext + 1) % sayMost;
}

void debugDraw(const glm::vec3& eye, const glm::vec3& headAngles)
{
    const FieldOffsets& f = fields();
    if(vr_debug_embed.value == 0.f || !sv.active || f.vr_stick_kind < 0 || f.vr_stick_ent < 0)
    {
        return;
    }
    const progs::ServerVm vm; // (the view's: the edicts and the fields need the server's VM)
    const glm::vec4 white{1.f, 1.f, 1.f, 0.9f}, yellow{1.f, 0.9f, 0.1f, 1.f}, red{1.f, 0.15f, 0.1f, 1.f}, green{0.2f, 1.f, 0.3f, 1.f},
        blue{0.3f, 0.6f, 1.f, 1.f}, magenta{1.f, 0.2f, 1.f, 1.f};
    progs::forEachEdict(1, [&](edict_t* ent) {
        const int kind = static_cast<int>(fieldFloat(ent, f.vr_stick_kind));
        const Blade* blade = kind > 0 && static_cast<int>(ent->v.movetype) == MOVETYPE_NONE ? bladeOf(ent) : nullptr;
        if(!blade)
        {
            return;
        }
        edict_t* host = PROG_TO_EDICT(fieldInt(ent, f.vr_stick_ent));
        const glm::vec3 at = fieldVec(ent, f.vr_stick_at);
        const glm::vec3 dir = fieldVec(ent, f.vr_stick_dir);
        const int hands = static_cast<int>(fieldFloatOr(ent, f.vr_hold_hands, 0.f));
        const float slide = fieldFloatOr(ent, f.vr_stick_slide, 0.f);
        const float depth = blade->how == howPoint ? fieldFloatOr(ent, f.vr_stick_depth, 0.f) + slide : slide;
        const glm::vec3 from = vec(ent->v.origin);
        // The axis: outside as far back as the handle (an edge's: a hand's length), inside to the tip, which is the depth.
        const float back = blade->how == howPoint ? za::max(glm::distance(from, at), 4.f) : 12.f;
        lines::line(at - dir * back, at, 0.25f, hands ? yellow : white, hands ? yellow : white);
        const glm::vec3 tip = at + dir * za::max(depth, 0.f);
        lines::line(at, tip, 0.35f, red, red);
        lines::point(tip, 1.2f, red);   // its tip
        lines::point(at, 1.2f, yellow); // where it went in
        lines::point(from, 1.f, white); // its origin, the handle
        glm::vec3 anchor;
        glm::mat3 axes;
        if(frameOf(host, kind, fieldVec(ent, f.vr_stick_anchor), anchor, axes))
        {
            lines::point(anchor, 1.2f, magenta); // what carries it: on the body's triangle, or its box
            lines::line(anchor, at, 0.1f, magenta, magenta);
        }
        float force = 0.f;
        if(hands && f.vr_hold_player >= 0)
        {
            // The hold: the hand's motion along the axis it hasn't followed, and off it, drawn from where it went in; the spring, on the handle's hand.
            const float err = fieldFloatOr(ent, f.vr_hold_err, 0.f);
            lines::line(at, at + dir * err, 0.5f, blue, blue);
            lines::line(at, at + fieldVec(ent, f.vr_hold_off), 0.5f, blue, blue);
            const int handle = static_cast<int>(fieldFloatOr(ent, f.vr_hold_handle, 0.f));
            const int player = NUM_FOR_EDICT(PROG_TO_EDICT(fieldInt(ent, f.vr_hold_player)));
            glm::vec3 limb, target;
            if(box3d::ragdollBladeSpring(player, (handle ? handle : hands) == 2 ? 1 : 0, limb, target, force))
            {
                lines::line(limb, target, 0.4f, green, green);
                lines::point(limb, 1.5f, green);
            }
        }
        const float cmPer = 100.f / units::metresToUnits();
        char text[160];
        q_snprintf(text, sizeof(text), "%s %.1f in\nalong %.1f of %.1f cm, off %.1f of %.0f cm\nfurther %.1f of %.0f cm, %.0f N", blade->how == howPoint ? "point" : "edge", depth,
            fieldFloatOr(ent, f.vr_hold_err, 0.f) * cmPer, fieldFloatOr(ent, f.vr_hold_stick, 0.f), glm::length(fieldVec(ent, f.vr_hold_off)) * cmPer, vr_embed_hold_break.value,
            fieldFloatOr(ent, f.vr_hold_far, 0.f), fieldFloatOr(ent, f.vr_hold_farmost, 0.f), force);
        overlayLines(text, at - dir * 2.f + glm::vec3{0.f, 0.f, 6.f}, eye, 0.05f, 60, 1.6f);
    });

    // The reasons, newest last, ahead of you and a little low, in real time.
    char panel[(sizeof(SayLine::text) + 1) * sayMost + 1];
    panel[0] = 0;
    for(int i = 0; i < sayMost; i++)
    {
        const SayLine& l = sayLines[(sayNext + i) % sayMost];
        if(l.when >= 0.0 && realtime - l.when < sayKeep)
        {
            q_strlcat(panel, l.text, sizeof(panel));
            q_strlcat(panel, "\n", sizeof(panel));
        }
    }
    if(panel[0])
    {
        const float yaw = glm::radians(headAngles.y);
        const float pitch = glm::radians(za::clamp(headAngles.x, -30.f, 30.f));
        const glm::vec3 fw{za::cos(pitch) * za::cos(yaw), za::cos(pitch) * za::sin(yaw), -za::sin(pitch)};
        const glm::vec3 at = eye + fw * 44.f + glm::vec3{0.f, 0.f, -8.f};
        overlayLines(panel, at, eye, 0.07f, 60, 2.2f);
    }
}

} // namespace qvr::axestick
