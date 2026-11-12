// vr_drips.cpp -- see vr_drips.hpp.

#include "vr_drips.hpp"

#include "vr_ambient.hpp"
#include "vr_bloodstream.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_gore.hpp"
#include "vr_goresound.hpp"
#include "vr_main.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_random.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"

#include <cstdio>

namespace qvr::drips
{
namespace
{

// What a wet thing is: a model the client has (cl_entities), or one of your own (each frame), or a test's probe.
enum class What : za::U8
{
    Thing,
    Body,
    Hand,
    Held,
    Holstered,
    Probe
};

struct Wet
{
    const entity_t* ent{nullptr};
    int num{0}; // its cl_entities number (> 0); 0 your own or a probe
    What what{What::Thing};
    int hand{-1};          // a hand's or a held weapon's
    float wet{0.f};        // 1 soaked .. 0 dry
    float waterline{0.f};  // how high its liquid came up it, over its box's bottom (it carries it out with it)
    Liquid liquid{Liquid::Water};
    double lastIn{-1e9};   // cl.time it was last in its liquid
    double seen{-1e9};     // cl.time it was last looked at
    float pending{0.f};    // drops owed
    glm::vec3 lo{0.f}, hi{0.f}; // its box as last looked at
    glm::vec3 lastPos{0.f}, vel{0.f};
    bool havePos{false};
    int drops{0};          // dropped since it was soaked (the test, vr_drips_info)
};

constexpr za::SizeT maxWet = 96;
za::Vector<Wet> wets;

// A wounded monster (or its corpse, a while): where it was hit, in its own turning frame (its yaw) from its origin, and
// bound to its model as drawn (bloodstream::anchorPoint: the vertex nearest, animated, armed or its ragdoll's) once it
// first drips there.
struct Bleeder
{
    int num{0};
    float start{0.f}; // its health before its first hit seen (the local server's), 0 unknown
    double firstHit{-1e9}; // cl.time of the first hit seen (the same frame's hits add to `start`)
    float burst{0.f}; // the hits' extra drops, fading
    float pending{0.f};
    double lastHit{-1e9};
    double lastSeen{-1e9}; // cl.time it was last in the server's message (out of sight a while: forgotten)
    double died{-1.0};     // cl.time its death was seen (a corpse drips on for vr_drips_blood_ragdoll), -1 alive
    glm::vec3 rel[4]{};
    int vertex[4]{-2, -2, -2, -2}; // its model's vertex there as drawn (-2 not bound yet, -1 none)
    int poseKind[4]{-1, -1, -1, -1};
    glm::vec3 lastAt[4]{}; // where it last dripped from (bound afresh from there when the body is drawn another way)
    int count{0}, next{0};
    int drops{0};
};

constexpr za::SizeT maxBleeders = 32;
za::Vector<Bleeder> bleeders;

struct Counters
{
    int emitted[3]{}; // drops that fell, by Liquid
    int landed[3]{};  // water and slime ones landed (landed); blood: the gore's landing (noteBloodLanding)
    int capped{0};    // frames the budget ran out
};

Counters counters;

za::FastNonCryptoRng rng{static_cast<za::U64>(za::Clock::nowNanoseconds())};
za::Vector<za::U32> drawnStamp; // by cl_entities number: the frame it was last in cl_visedicts
za::U32 frameStamp = 1;
double lastTime = -1.0;
double nextScan = 0.0;
int lastFrame = -1;
const qmodel_t* lastWorld = nullptr;
int lastWorldGen = -1; // (worldGeneration: the same map loaded again keeps cl.worldmodel, not cl_entities)
za::SizeT cursor = 0; // the round robin's start (wets, then bleeders)
glm::vec3 eyes{0.f};

// The test's overrides (vr_drips_test): the rates (0: the tunables'), its probes.
float testRate = 0.f;
float testBloodRate = 0.f;
float testCorpseTime = 0.f; // (vr_drips_blood_ragdoll's, a tunable: a constant in a release build)
entity_t probes[21]{};

constexpr double inGrace = 0.3;    // s after the last look in the liquid it counts as still in it (none drip)
constexpr double scanEvery = 0.1;  // s between the looks at the client's models
constexpr float farAway = 1536.f;  // units: nothing drips this far from the eyes (too pad to see)

[[nodiscard]] float dryTime()
{
    return za::clamp(vr_wounds_wet_time.value, 2.f, 600.f);
}

[[nodiscard]] float mToU()
{
    return units::metresToUnits();
}

// The liquid at `p` (a func_water's too): water (a current too) or slime; lava as -1; 0 none.
[[nodiscard]] int liquidAt(const glm::vec3& p)
{
    const int c = worldtrace::liquidKind(worldtrace::liquidContents(p, true));
    return c == CONTENTS_WATER ? 1 : c == CONTENTS_SLIME ? 2 : c == CONTENTS_LAVA ? -1 : 0;
}

[[nodiscard]] glm::vec3 tunableColor(const Tunable& t, const glm::vec3& def)
{
    glm::vec3 c = def;
    if(std::sscanf(t.string, "%f %f %f", &c.x, &c.y, &c.z) != 3)
    {
        return def;
    }
    return glm::clamp(c, glm::vec3{0.f}, glm::vec3{1.f});
}

lightcache_t dropLight{}; // dropColor's light lookup

// A water or slime drop's colour at `p`: the tunable's, lit as the place is and a little brighter (a particle takes no
// light of its own), its alpha vr_drips_alpha.
[[nodiscard]] glm::vec4 dropColor(Liquid l, const glm::vec3& p)
{
    const glm::vec3 base = l == Liquid::Slime ? tunableColor(vr_drips_slime_color, {0.32f, 0.55f, 0.1f})
                                              : tunableColor(vr_drips_color, {0.62f, 0.74f, 0.88f});
    const float light = cl.worldmodel ? static_cast<float>(ambient::lightPoint(p, dropLight)) : 110.f;
    const float k = za::clamp(za::max(light, 40.f) / 100.f, 0.5f, 1.3f) * 1.15f;
    return {glm::min(base * k, glm::vec3{1.f}), za::clamp(vr_drips_alpha.value, 0.f, 1.f)};
}

[[nodiscard]] bool own(What w)
{
    return w != What::Thing && w != What::Probe;
}

// Close under the eyes (a hand raised to the face): no drop there, to fall across the view.
[[nodiscard]] bool nearEyes(const glm::vec3& p)
{
    const glm::vec2 flat{p.x - eyes.x, p.y - eyes.y};
    return p.z > eyes.z - 0.35f * mToU() && glm::length(flat) < 0.5f * mToU();
}

Wet* find(const entity_t* e)
{
    for(Wet& w : wets)
    {
        if(w.ent == e)
        {
            return &w;
        }
    }
    return nullptr;
}

Wet* add(const entity_t* e, int num, What what, int hand)
{
    if(Wet* w = find(e))
    {
        return w;
    }
    if(wets.size() >= maxWet)
    {
        return nullptr;
    }
    Wet w;
    w.ent = e;
    w.num = num;
    w.what = what;
    w.hand = hand;
    wets.pushBack(w);
    return &wets.back();
}

// A model's box in the world (an alias model's frames' bounds about its origin; at most 24 units out to each side).
[[nodiscard]] bool boxOf(const entity_t& e, glm::vec3& lo, glm::vec3& hi)
{
    if(!e.model || e.model->type != mod_alias)
    {
        return false;
    }
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    lo = o + glm::max(glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]}, glm::vec3{-24.f, -24.f, -1e4f});
    hi = o + glm::min(glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]}, glm::vec3{24.f, 24.f, 1e4f});
    return hi.z > lo.z;
}

// In a liquid at its box's bottom: soaked, the waterline found (the top's, or the surface between, to a unit).
// `kind` the bottom's liquidAt (1 water, 2 slime, -1 lava).
void soak(Wet& w, int kind, const glm::vec3& lo, const glm::vec3& hi)
{
    if(kind < 0) // lava: nothing wet (it burns)
    {
        w.wet = 0.f;
        w.lastIn = -1e9;
        return;
    }
    const glm::vec3 mid{0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), 0.f};
    float in = lo.z + 1.f, out = za::max(hi.z - 1.f, in);
    float surface = out;
    if(liquidAt({mid.x, mid.y, out}) == 0)
    {
        for(int i = 0; i < 7 && out - in > 1.f; i++)
        {
            const float m = 0.5f * (in + out);
            (liquidAt({mid.x, mid.y, m}) != 0 ? in : out) = m;
        }
        surface = in;
    }
    const bool fresh = cl.time - w.lastIn > inGrace || w.wet < 0.999f;
    w.waterline = fresh ? surface - lo.z : za::max(w.waterline, surface - lo.z);
    if(fresh && w.wet < 0.999f && vr_drips_debug.value)
    {
        Con_Printf("drips: %s%d in %s up to %.0f\n", own(w.what) ? "your own " : "entity ", w.num, kind == 2 ? "slime" : "water",
            static_cast<double>(w.waterline));
    }
    w.wet = 1.f;
    w.liquid = kind == 2 ? Liquid::Slime : Liquid::Water;
    w.lastIn = cl.time;
    w.drops = 0;
}

// Looks at one thing: its box, its speed, in its liquid or not.
void look(Wet& w, const glm::vec3& lo, const glm::vec3& hi, float dt)
{
    const glm::vec3 c = 0.5f * (lo + hi);
    if(w.havePos && dt > 0.f)
    {
        const glm::vec3 moved = c - w.lastPos;
        const glm::vec3 v = glm::length(moved) > 64.f ? glm::vec3{0.f} : moved / dt; // (a teleport)
        w.vel = glm::mix(w.vel, v, 0.5f);
    }
    w.lastPos = c;
    w.havePos = true;
    w.lo = lo;
    w.hi = hi;
    w.seen = cl.time;
    const int kind = liquidAt({c.x, c.y, lo.z + 1.f});
    if(kind != 0)
    {
        soak(w, kind, lo, hi);
    }
}

// The client's models, ten times a second: the ones in a liquid soaked; the gone forgotten.
void scan(float dt)
{
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t& e = cl_entities[i];
        if(i == cl.viewentity || !e.model || e.model->type != mod_alias || e.msgtime != cl.mtime[0])
        {
            continue;
        }
        glm::vec3 lo, hi;
        if(!boxOf(e, lo, hi))
        {
            continue;
        }
        Wet* w = find(&e);
        if(!w)
        {
            const glm::vec3 bottom{0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), lo.z + 1.f};
            if(liquidAt(bottom) <= 0)
            {
                continue; // dry and out of any liquid: nothing to look after
            }
            w = add(&e, i, What::Thing, -1);
            if(!w)
            {
                continue;
            }
        }
        look(*w, lo, hi, dt);
    }
    za::vectorEraseIf(wets, [](const Wet& w) {
        if(w.what != What::Thing)
        {
            return false;
        }
        return w.num <= 0 || w.num >= cl.num_entities || !cl_entities[w.num].model || cl_entities[w.num].msgtime != cl.mtime[0];
    });
}

// Your own (each frame): the body (the player's box from the feet to the eyes), the hands, the held and holstered weapons.
void lookOwn(float dt)
{
    entity_t* mine[3]{};
    view::woundTargets(mine);
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 feet{pl.origin[0], pl.origin[1], pl.origin[2] - 24.f};
    const float pad = 2.f;
    const auto lookAt = [&](const entity_t* e, What what, int hand, const glm::vec3& lo, const glm::vec3& hi) {
        if(!e)
        {
            return;
        }
        Wet* w = find(e);
        if(!w)
        {
            if(liquidAt({0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), lo.z + 1.f}) <= 0)
            {
                return;
            }
            w = add(e, 0, what, hand);
            if(!w)
            {
                return;
            }
        }
        look(*w, lo, hi, dt);
    };
    if(mine[0])
    {
        lookAt(mine[0], What::Body, -1, feet - glm::vec3{10.f, 10.f, 0.f}, glm::vec3{feet.x + 10.f, feet.y + 10.f, eyes.z});
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(const entity_t* h = mine[1 + hand])
        {
            const glm::vec3 o{h->origin[0], h->origin[1], h->origin[2]};
            lookAt(h, What::Hand, hand, o - glm::vec3{pad}, o + glm::vec3{pad});
        }
        const view::ViewEntity* held = view::heldWeapon(hand);
        glm::vec3 handle, tip;
        if(held && held->visible && held->ent.model && view::heldWeaponLine(hand, handle, tip))
        {
            lookAt(&held->ent, What::Held, hand, glm::min(handle, tip) - glm::vec3{pad}, glm::max(handle, tip) + glm::vec3{pad});
        }
    }
    for(int stat = 0; stat < protocol::numHolsters; stat++)
    {
        if(!cl.stats[protocol::STAT_QVR_HOLSTERWEAPONUID0 + stat])
        {
            continue;
        }
        if(const entity_t* e = view::holsteredWeapon(stat))
        {
            const glm::vec3 o{e->origin[0], e->origin[1], e->origin[2]};
            lookAt(e, What::Holstered, -1, o - glm::vec3{pad * 2.f}, o + glm::vec3{pad * 2.f});
        }
    }
    // Your own not seen this frame (a weapon put away, the body hidden): forgotten.
    za::vectorEraseIf(wets, [](const Wet& w) { return own(w.what) && w.seen < cl.time - 0.25; });
}

// Where a drop falls from `w` (under its waterline): false if nowhere now.
[[nodiscard]] bool dropPoint(const Wet& w, glm::vec3& at)
{
    const float under = za::min(w.lo.z + w.waterline, w.hi.z);
    switch(w.what)
    {
        case What::Thing:
        {
            float lowZ = 0.f;
            if(wounds::dripPoint(*w.ent, w.num, under, at, lowZ))
            {
                return true;
            }
            break; // (its box below)
        }
        case What::Held:
        {
            glm::vec3 handle, tip;
            if(!view::heldWeaponLine(w.hand, handle, tip))
            {
                return false;
            }
            // Along the barrel, the most towards the muzzle, from its underside.
            at = glm::mix(handle, tip, vrand::range(rng, 0.45f, 1.f)) - glm::vec3{0.f, 0.f, 1.2f};
            return at.z < under + 2.f && !nearEyes(at);
        }
        case What::Hand:
        case What::Holstered:
            at = 0.5f * (w.lo + w.hi) + glm::vec3{vrand::range(rng, -1.5f, 1.5f), vrand::range(rng, -1.5f, 1.5f), -1.5f};
            return !nearEyes(at);
        case What::Body:
        {
            // The legs and the hem: round the body, under the waterline, well under the eyes.
            const float top = za::min(under, eyes.z - 0.45f * mToU());
            const float bottom = w.lo.z + 10.f;
            if(top <= bottom)
            {
                return false;
            }
            const float a = vrand::range(rng, 0.f, 6.2831853f);
            const float r = vrand::range(rng, 5.f, 9.f);
            at = {0.5f * (w.lo.x + w.hi.x) + za::cos(a) * r, 0.5f * (w.lo.y + w.hi.y) + za::sin(a) * r, vrand::range(rng, bottom, top)};
            return !nearEyes(at);
        }
        case What::Probe: break;
    }
    // A box: under the waterline, inside its sides, above its lowest sixth (a body's feet on the floor drip nothing seen).
    const float h = w.hi.z - w.lo.z;
    const float top = za::min(under, w.hi.z) - 1.f;
    const float bottom = w.lo.z + h / 6.f;
    if(top <= bottom)
    {
        return false;
    }
    const glm::vec3 c = 0.5f * (w.lo + w.hi);
    const glm::vec3 half = 0.4f * (w.hi - w.lo);
    at = {c.x + vrand::range(rng, -half.x, half.x), c.y + vrand::range(rng, -half.y, half.y), vrand::range(rng, bottom, top)};
    return true;
}

void dropWater(Wet& w)
{
    glm::vec3 at;
    if(!dropPoint(w, at))
    {
        return;
    }
    const float size = za::clamp(vr_drips_size.value, 0.05f, 3.f) * (own(w.what) ? 0.8f : 1.f) * vrand::range(rng, 0.8f, 1.2f);
    if(gore::dropFrom(static_cast<int>(w.liquid), at, size, dropColor(w.liquid, at)))
    {
        counters.emitted[static_cast<int>(w.liquid)]++;
        w.drops++;
        if(vr_drips_debug.value >= 2)
        {
            Con_Printf("drips: a drop off %d at %.0f %.0f %.0f (wet %.2f)\n", w.num, static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z), static_cast<double>(w.wet));
        }
    }
}

// A swung wet weapon or hand throws a drop off it, flying on with it.
void fling(Wet& w, float dt)
{
    const float speed = glm::length(w.vel) / mToU(); // m/s
    const float k = za::clamp(vr_drips_fling.value, 0.f, 10.f);
    if(k <= 0.f || speed <= 2.2f || vrand::range(rng, 0.f, 1.f) >= (speed - 2.2f) * 1.5f * k * w.wet * dt)
    {
        return;
    }
    glm::vec3 at;
    if(!dropPoint(w, at) || liquidAt(at) != 0)
    {
        return;
    }
    const glm::vec4 c = dropColor(w.liquid, at);
    particles::bloodDroplet(at, w.vel * vrand::range(rng, 0.6f, 1.f), za::clamp(vr_drips_size.value, 0.05f, 3.f) * 1.4f,
        vrand::range(rng, 0.5f, 0.9f), glm::vec3{c} * c.a + glm::vec3{0.1f}, w.lo.z - 256.f);
    counters.emitted[static_cast<int>(w.liquid)]++;
    w.drops++;
}

// Dries `w` over `dt`, its drops owed added; false: dry (forgotten).
bool dry(Wet& w, float dt)
{
    if(cl.time - w.lastIn < inGrace)
    {
        w.pending = 0.f; // in it: none (nothing drips in or under a liquid)
        return true;
    }
    const float speed = glm::length(w.vel);
    const float faster = 1.f + za::min(1.5f, za::clamp(vr_drips_dry_motion.value, 0.f, 10.f) * speed / 400.f);
    w.wet -= dt / dryTime() * faster;
    if(w.wet <= 0.f)
    {
        if(vr_drips_debug.value)
        {
            Con_Printf("drips: %s%d dry after %d drops\n", own(w.what) ? "your own " : "entity ", w.num, w.drops);
        }
        w.wet = 0.f;
        return w.what == What::Probe; // (a probe kept: the test reads it)
    }
    const float rate = (testRate > 0.f ? testRate : za::clamp(vr_drips_rate.value, 0.f, 40.f)) *
                       za::pow(w.wet, za::clamp(vr_drips_curve.value, 0.1f, 8.f)) *
                       (own(w.what) ? za::clamp(vr_drips_view.value, 0.f, 2.f) : 1.f) *
                       (own(w.what) ? 1.f : za::clamp((w.hi.z - w.lo.z) / 56.f, 0.3f, 1.5f)); // (a gib drips less than a monster)
    w.pending = za::min(w.pending + rate * dt * vrand::range(rng, 0.5f, 1.5f), 3.f);
    if(w.what == What::Held || w.what == What::Hand)
    {
        fling(w, dt);
    }
    return true;
}

// ---- The wounded ----------------------------------------------------------------------------------

[[nodiscard]] bool onServer(int num)
{
    return sv.active && num > 0 && num < sv.qcvm.num_edicts;
}

// A living monster's health now (the local server's): -1 unknown, 0 dead or gone.
[[nodiscard]] float healthOf(int num)
{
    if(!onServer(num))
    {
        return -1.f;
    }
    progs::ServerVm serverVm;
    const edict_t* ed = EDICT_NUM(num);
    if(ed->free || !(static_cast<int>(ed->v.flags) & FL_MONSTER) || ed->v.health <= 0.f)
    {
        return 0.f;
    }
    return ed->v.health;
}

// Whether the local server's entity `num` is removed, or has no model (gibbed away); false when not the local server's.
[[nodiscard]] bool goneOnServer(int num)
{
    if(!onServer(num))
    {
        return false;
    }
    progs::ServerVm serverVm;
    const edict_t* ed = EDICT_NUM(num);
    return ed->free || ed->v.model == 0;
}

// (The test's: QC's test aids (vr_test_hurt) aim at entity `num` alone; 0 the nearest again.)
void testOnly(int num)
{
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    if(const func_t fn = progs::findFunction("VR_TestOnly"))
    {
        G_INT(OFS_PARM0) = EDICT_TO_PROG(EDICT_NUM(onServer(num) ? num : 0));
        PR_ExecuteProgram(fn);
    }
}

// (The test's: a monster hurt without a blow.)
void setHealth(int num, float health)
{
    if(onServer(num))
    {
        progs::ServerVm serverVm;
        EDICT_NUM(num)->v.health = health;
    }
}

// How long a corpse drips on, s (vr_drips_blood_ragdoll; the test's).
[[nodiscard]] float corpseSeconds()
{
    return testCorpseTime > 0.f ? testCorpseTime : za::clamp(vr_drips_blood_ragdoll.value, 0.f, 600.f);
}

[[nodiscard]] bool bloodOn()
{
    return vr_gore_wound_drips.value != 0.f && gore::level() > 0;
}

[[nodiscard]] glm::vec3 turn(const glm::vec3& v, float yawDeg)
{
    const float a = glm::radians(yawDeg);
    const float c = za::cos(a), s = za::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// Whether entity `num` is in the client's latest message (drawn now).
[[nodiscard]] bool present(int num)
{
    return num > 0 && num < cl.num_entities && cl_entities[num].model && cl_entities[num].msgtime == cl.mtime[0];
}

// Heals (forgotten), stays away too long (forgotten), or owes drops; false: forgotten. A wounded living monster drips
// all the while it is hurt, at vr_drips_blood_rate times (vr_drips_blood_min + the rest times the share of its health
// lost), more after a hit; its corpse (a ragdoll) on for vr_drips_blood_ragdoll s, tapering.
bool bleed(Bleeder& b, float dt)
{
    if(b.num <= 0 || b.num >= cl.num_entities)
    {
        return false;
    }
    const bool here = present(b.num);
    if(here)
    {
        b.lastSeen = cl.time;
    }
    else if(cl.time - b.lastSeen > 10.0)
    {
        return false; // (gone: removed, or out of sight a long while)
    }
    const float health = healthOf(b.num);
    if(goneOnServer(b.num))
    {
        return false; // removed, or gibbed (its gibs bleed their own: the gore's)
    }
    const float corpseTime = corpseSeconds();
    if(health == 0.f && b.died < 0.0)
    {
        b.died = cl.time; // dead: its corpse drips on a while
    }
    b.burst *= za::exp(-dt / 1.5f);
    float share;
    if(b.died >= 0.0)
    {
        const float t = static_cast<float>(cl.time - za::max(b.died, b.lastHit));
        if(t >= corpseTime)
        {
            return false;
        }
        share = 1.f - t / corpseTime; // (tapering)
    }
    else if(health < 0.f)
    {
        if(cl.time - b.lastHit > 30.0)
        {
            return false; // (health unknown: not the local server's)
        }
        share = 0.5f;
    }
    else
    {
        const float lost = b.start > 0.f ? za::clamp(1.f - health / b.start, 0.f, 1.f) : 0.f;
        if(lost <= 0.f || 1.f - lost >= za::clamp(vr_drips_blood_health.value, 0.f, 1.f) + 1e-4f)
        {
            if(lost <= 0.f && b.burst < 0.02f)
            {
                return false; // healed
            }
            share = 0.f; // (not hurt enough to drip: the hit's burst alone)
        }
        else
        {
            const float least = za::clamp(vr_drips_blood_min.value, 0.f, 1.f);
            share = least + (1.f - least) * lost;
        }
    }
    const float rate = (testBloodRate > 0.f ? testBloodRate : za::clamp(vr_drips_blood_rate.value, 0.f, 20.f)) * (share + b.burst);
    b.pending = here ? za::min(b.pending + rate * dt * vrand::range(rng, 0.5f, 1.5f), 3.f) : 0.f;
    return true;
}

// Where wound `i` of `b` drips from now: its vertex as drawn (bound the first time, and afresh when the body is drawn
// another way: it died into a ragdoll); else its point in the yaw frame.
[[nodiscard]] glm::vec3 woundNow(Bleeder& b, int i)
{
    const entity_t& e = cl_entities[b.num];
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    glm::vec3 at;
    if(b.vertex[i] >= 0 && bloodstream::anchorNow(b.num, b.vertex[i], b.poseKind[i], at))
    {
        b.lastAt[i] = at;
        return at;
    }
    const bool rebind = b.vertex[i] >= 0; // (drawn another way now)
    const glm::vec3 from = rebind ? b.lastAt[i] : o + turn(b.rel[i], e.angles[1]);
    if(b.vertex[i] != -1 && bloodstream::anchorPoint(b.num, from, rebind ? 48.f : 24.f, b.vertex[i], b.poseKind[i]) &&
        bloodstream::anchorNow(b.num, b.vertex[i], b.poseKind[i], at))
    {
        b.lastAt[i] = at;
        return at;
    }
    b.vertex[i] = -1; // (none: its point in the yaw frame from now on)
    b.lastAt[i] = from;
    return from;
}

void dropBlood(Bleeder& b)
{
    const entity_t& e = cl_entities[b.num];
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    glm::vec3 at;
    if(b.count > 0)
    {
        const int i = za::min(b.count - 1, static_cast<int>(vrand::range(rng, 0.f, static_cast<float>(b.count))));
        at = woundNow(b, i) + glm::vec3{vrand::range(rng, -1.f, 1.f), vrand::range(rng, -1.f, 1.f), -1.5f};
    }
    else
    {
        at = o + glm::vec3{vrand::range(rng, -6.f, 6.f), vrand::range(rng, -6.f, 6.f), vrand::range(rng, 0.f, 16.f)};
    }
    const float size = za::clamp(vr_drips_blood_size.value, 0.1f, 5.f);
    if(gore::dropFrom(0, at, vrand::range(rng, 0.5f, 0.75f) * size, glm::vec4{gore::bloodColorAt(at), 1.f},
           za::clamp(vr_drips_blood_marks.value, 0.f, 1.f)))
    {
        counters.emitted[0]++;
        b.drops++;
    }
}

[[nodiscard]] bool drawn(const Wet& w)
{
    if(w.what == What::Probe || own(w.what))
    {
        return true;
    }
    return w.num > 0 && w.num < static_cast<int>(drawnStamp.size()) && drawnStamp[static_cast<za::SizeT>(w.num)] == frameStamp &&
           glm::length(0.5f * (w.lo + w.hi) - eyes) < farAway;
}

[[nodiscard]] bool drawnNum(int num)
{
    if(num <= 0 || num >= static_cast<int>(drawnStamp.size()) || drawnStamp[static_cast<za::SizeT>(num)] != frameStamp)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    return glm::length(glm::vec3{e.origin[0], e.origin[1], e.origin[2]} - eyes) < farAway;
}

// ---- The test (vr_drips_test) -----------------------------------------------------------------------

struct Test
{
    int phase{0}; // 0 off
    double t0{0.0};
    double deadline{0.0};
    testkit::Tally tally;
    char savedWetTime[32]{};
    char savedGore[32]{};
    int window[4]{};
    int windows{0};
    int last{0};
    int monster{0};
    float savedHealth{0.f};
    int bloodLanded0{0}, bloodDrops0{0};
    double perfSum{0.0}, perfMax{0.0};
    int perfFrames{0}, perfDrops{0}, perfMostAFrame{0};
    // vr_drips_bleed_test: the hurt grunt, the unhurt one, the drops in each 5 s window.
    int other{0};
    float otherHealth{0.f};
    int winDrops[6]{};
    int win{0};
    int drops0{0};
};

Test test;
double frameWorkSeconds = 0.0; // the last frame's work (the test's perf)
int frameDrops = 0;

const char* testName = "drips_test";
constexpr glm::vec3 poolPoint{376.f, -264.f, -80.f}; // vrtesthall's pool (surface -9, 119 deep)
constexpr glm::vec3 dryPoint{184.f, -264.f, 60.f};   // west of it (its edge x 224), over the floor

Wet* probeWet(int i)
{
    return find(&probes[i]);
}

void testEnd()
{
    testRate = 0.f;
    testBloodRate = 0.f;
    Cvar_Set("vr_wounds_wet_time", test.savedWetTime);
    if(test.savedGore[0])
    {
        Cvar_Set("vr_gore", test.savedGore);
    }
    testCorpseTime = 0.f;
    if(test.phase >= 20)
    {
        testOnly(0);
    }
    if(test.monster > 0 && test.savedHealth > 0.f && healthOf(test.monster) > 0.f)
    {
        setHealth(test.monster, test.savedHealth);
    }
    za::vectorEraseIf(wets, [](const Wet& w) { return w.what == What::Probe; });
    test.tally.verdict(testName);
    test.phase = 0;
}

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", testName, what);
    }
    test.tally.check(ok, testName, what);
}

void placeProbe(int i, const glm::vec3& at)
{
    entity_t& p = probes[i];
    p.origin[0] = at.x;
    p.origin[1] = at.y;
    p.origin[2] = at.z;
}

// Each frame while the test runs: its steps, each on a condition or a counted span of game time.
void testStep()
{
    char line[160];
    const double now = cl.time;
    if(test.phase > 0 && now > test.deadline)
    {
        check(false, "timed out");
        testEnd();
        return;
    }
    switch(test.phase)
    {
        case 1: // the map in: a probe (the player's model) under the pool's surface
        {
            if(cls.signon != SIGNONS || !cl.worldmodel || cl.viewentity <= 0 || !cl_entities[cl.viewentity].model ||
                !strstr(cl.worldmodel->name, "vrtesthall"))
            {
                return; // (the map vr_scenario loads, not the one before it)
            }
            if(liquidAt(poolPoint) != 1 || liquidAt(dryPoint) != 0)
            {
                check(false, "no pool where vrtesthall's is (map vrtesthall)");
                testEnd();
                return;
            }
            q_strlcpy(test.savedWetTime, vr_wounds_wet_time.string, sizeof(test.savedWetTime));
            Cvar_Set("vr_wounds_wet_time", "6");
            testRate = 12.f;
            probes[0] = entity_t{};
            probes[0].model = cl_entities[cl.viewentity].model;
            placeProbe(0, poolPoint);
            add(&probes[0], 0, What::Probe, -1);
            test.phase = 2;
            test.t0 = now;
            return;
        }
        case 2: // a second under: soaked, no drop
        {
            if(now - test.t0 < 1.0)
            {
                return;
            }
            const Wet* w = probeWet(0);
            check(w && w->wet > 0.999f, "a thing under water is soaked (wetness 1)");
            check(w && w->drops == 0, "no drops while it is under water");
            placeProbe(0, dryPoint);
            test.phase = 3;
            test.t0 = now;
            test.last = 0;
            return;
        }
        case 3: // out: drops in three 2 s windows, fewer each; dry by 6.5 s (vr_wounds_wet_time 6), none after
        {
            const Wet* w = probeWet(0);
            const int drops = w ? w->drops : 0;
            const double t = now - test.t0;
            if(test.windows < 3 && t >= 2.0 * (test.windows + 1))
            {
                test.window[test.windows++] = drops - test.last;
                test.last = drops;
            }
            if(t < 6.6)
            {
                return;
            }
            const int a = test.window[0], b = test.window[1], c = test.window[2];
            q_snprintf(line, sizeof(line), "out of the water it drips fewer and fewer: %d, %d, %d drops in 2 s windows", a, b, c);
            check(a >= 8 && a > b && b >= c, line);
            check(w && w->wet <= 0.f, "dry after vr_wounds_wet_time (6 s)");
            test.last = drops;
            test.phase = 4;
            test.t0 = now;
            return;
        }
        case 4: // a second dry: no more drops; then 20 wet things at once, out of the water
        {
            if(now - test.t0 < 1.0)
            {
                return;
            }
            const Wet* w = probeWet(0);
            q_snprintf(line, sizeof(line), "dry: no more drops (%d in the last second)", (w ? w->drops : 0) - test.last);
            check(w && w->drops == test.last, line);
            testRate = 0.f; // (the shipped rate from here)
            for(int i = 1; i <= 20; i++)
            {
                probes[i] = entity_t{};
                probes[i].model = cl_entities[cl.viewentity].model;
                placeProbe(i, dryPoint + glm::vec3{static_cast<float>((i - 1) % 5) * -24.f, static_cast<float>((i - 1) / 5) * 24.f - 36.f, 0.f});
                if(Wet* p = add(&probes[i], 0, What::Probe, -1))
                {
                    p->wet = 1.f;
                    p->lastIn = now - 1.0;
                    p->waterline = 1e4f;
                }
            }
            Cvar_Set("vr_wounds_wet_time", "60");
            test.phase = 5;
            test.t0 = now;
            return;
        }
        case 5: // two seconds of the 20: the frame's cost, the cap held
        {
            test.perfSum += frameWorkSeconds;
            test.perfMax = za::max(test.perfMax, frameWorkSeconds);
            test.perfFrames++;
            test.perfDrops += frameDrops;
            test.perfMostAFrame = za::max(test.perfMostAFrame, frameDrops);
            if(now - test.t0 < 2.0)
            {
                return;
            }
            const double avgUs = test.perfSum / za::max(1, test.perfFrames) * 1e6;
            Con_Printf("%s: perf: 20 wet things (rate %.1f/s each): %.1f us a frame on average, %.1f at most; %d drops in %d "
                       "frames, at most %d a frame (cap %d)\n",
                testName, static_cast<double>(vr_drips_rate.value), avgUs, test.perfMax * 1e6, test.perfDrops, test.perfFrames,
                test.perfMostAFrame, static_cast<int>(vr_drips_max.value));
            check(test.perfMostAFrame <= static_cast<int>(za::clamp(vr_drips_max.value, 1.f, 128.f)), "20 wet things: the drops a frame within vr_drips_max");
            check(test.perfDrops > 0, "20 wet things drip");
            check(avgUs < 500.0, "20 wet things: under half a millisecond a frame");
            za::vectorEraseIf(wets, [](const Wet& w) { return w.what == What::Probe; });
            Cvar_Set("vr_wounds_wet_time", "6");
            // Blood still blood: a drop of the gore's lands as blood, not as water.
            test.bloodLanded0 = counters.landed[0];
            if(!gore::dropFrom(0, dryPoint, 0.7f, glm::vec4{gore::bloodColorAt(dryPoint), 1.f}))
            {
                gore::count(); // (why not: its landings full?)
                check(false, "a blood drop falls");
            }
            test.phase = 6;
            test.t0 = now;
            return;
        }
        case 6: // its landing (a fall of a second at most)
        {
            if(counters.landed[0] == test.bloodLanded0 && now - test.t0 < 1.5)
            {
                return;
            }
            check(counters.landed[0] > test.bloodLanded0, "a blood drop lands as blood (the gore's landing: specks, a mark, a drip)");
            test.phase = 7;
            test.t0 = now;
            return;
        }
        case 7: // a living monster on dry floor (the scenario's grunt): hurt to 30%, it drips blood
        {
            int best = 0;
            float bestD = 1e9f;
            for(int i = 1; i < cl.num_entities; i++)
            {
                const entity_t& e = cl_entities[i];
                const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
                if(healthOf(i) > 0.f && e.model && e.msgtime == cl.mtime[0] && liquidAt(o) == 0 && drawnNum(i))
                {
                    const float d = glm::length(o - eyes);
                    if(d < bestD)
                    {
                        bestD = d;
                        best = i;
                    }
                }
            }
            if(!best)
            {
                return; // (waits for the scenario's grunt: the deadline gives up)
            }
            test.monster = best;
            test.savedHealth = healthOf(best);
            const entity_t& e = cl_entities[best];
            onWound(best, {e.origin[0], e.origin[1], e.origin[2] + 8.f}, 0, 1);
            setHealth(best, test.savedHealth * 0.3f);
            testBloodRate = 6.f;
            test.bloodDrops0 = counters.emitted[0];
            test.phase = 8;
            test.t0 = now;
            return;
        }
        case 8: // 3 s of it; then with the gore off, a second of none
        {
            if(now - test.t0 < 3.0)
            {
                return;
            }
            const int n = counters.emitted[0] - test.bloodDrops0;
            q_snprintf(line, sizeof(line), "a wounded monster (70%% hurt) drips blood: %d drops in 3 s", n);
            check(n >= 4, line);
            q_strlcpy(test.savedGore, vr_gore.string, sizeof(test.savedGore));
            Cvar_Set("vr_gore", "0");
            test.bloodDrops0 = counters.emitted[0];
            test.phase = 9;
            test.t0 = now;
            return;
        }
        case 9:
        {
            if(now - test.t0 < 1.0)
            {
                return;
            }
            const int n = counters.emitted[0] - test.bloodDrops0;
            q_snprintf(line, sizeof(line), "with the gore off, no wounded drips (%d)", n);
            check(n == 0, line);
            testEnd();
            return;
        }
        case 20: // vr_drips_bleed_test: the scenario's grunts on dry floor; the nearest hit by a real blow to half health
        {
            if(cls.signon != SIGNONS || !cl.worldmodel || cl.viewentity <= 0 || !strstr(cl.worldmodel->name, "vrtesthall"))
            {
                return;
            }
            int best = 0, other = 0;
            float bestD = 1e9f;
            for(int i = 1; i < cl.num_entities; i++)
            {
                const entity_t& e = cl_entities[i];
                if(healthOf(i) > 0.f && e.model && e.msgtime == cl.mtime[0] && drawnNum(i))
                {
                    const float d = glm::length(glm::vec3{e.origin[0], e.origin[1], e.origin[2]} - eyes);
                    if(d < bestD)
                    {
                        other = best;
                        bestD = d;
                        best = i;
                    }
                    else if(!other)
                    {
                        other = i;
                    }
                }
            }
            if(!best || !other)
            {
                return; // (waits for both grunts: the deadline gives up)
            }
            test.monster = best;
            test.other = other;
            test.otherHealth = healthOf(other);
            // A shot of the player's, dealt in the server's frame (QC VR_TestHurt_Frame: its wound event goes out with
            // that frame; one written from here would be cleared before it is sent).
            test.savedHealth = healthOf(best);
            testOnly(best);
            Cvar_SetValue("vr_test_hurt", za::max(1.f, static_cast<float>(static_cast<int>(test.savedHealth * 0.6f))));
            test.phase = 22;
            return;
        }
        case 22: // the shot taken
        {
            const float h = healthOf(test.monster);
            if(h >= test.savedHealth)
            {
                return;
            }
            q_snprintf(line, sizeof(line), "grunt %d shot: health %.0f -> %.0f (grunt %d unhurt)", test.monster,
                static_cast<double>(test.savedHealth), static_cast<double>(h), test.other);
            check(h > 0.f, line);
            test.savedHealth = 0.f; // (not healed back at the end)
            test.phase = 21;
            test.t0 = now;
            test.drops0 = 0;
            return;
        }
        case 21: // 30 s, in 5 s windows: the hurt one drips in every one; the unhurt one never
        {
            const Bleeder* b = nullptr;
            for(const Bleeder& x : bleeders)
            {
                b = x.num == test.monster ? &x : b;
                if(x.num == test.other && healthOf(test.other) >= test.otherHealth && x.drops > 0)
                {
                    check(false, "the unhurt grunt (full health) drips blood");
                    testEnd();
                    return;
                }
            }
            if(now - test.t0 < 5.0 * (test.win + 1))
            {
                return;
            }
            const int d = b ? b->drops : -1;
            test.winDrops[test.win] = d < 0 ? 0 : d - test.drops0;
            test.drops0 = d < 0 ? test.drops0 : d;
            Con_Printf("%s: %2.0f s: health %.0f, %s, %d drops in 5 s\n", testName, 5.0 * (test.win + 1),
                static_cast<double>(healthOf(test.monster)), b ? "bleeding" : "not bleeding", test.winDrops[test.win]);
            if(++test.win < 6)
            {
                return;
            }
            int least = 1 << 30;
            for(const int n : test.winDrops)
            {
                least = za::min(least, n);
            }
            q_snprintf(line, sizeof(line), "the hurt grunt still drips after 30 s (%s; at least %d drops in every 5 s)",
                b ? "bleeding" : "forgotten", least);
            check(b != nullptr && least >= 6, line);
            q_snprintf(line, sizeof(line), "the unhurt grunt dripped nothing while at full health (now %.0f of %.0f)",
                static_cast<double>(healthOf(test.other)), static_cast<double>(test.otherHealth));
            check(true, line);
            // Then killed: its corpse drips on (the test's corpse time, 4 s), then stops.
            testCorpseTime = 4.f;
            Cvar_SetValue("vr_test_hurt", healthOf(test.monster) + 5.f); // (a kill, not a gibbing)
            test.phase = 23;
            return;
        }
        case 23: // dead (the server's word)
        {
            if(healthOf(test.monster) != 0.f)
            {
                return;
            }
            const Bleeder* b = nullptr;
            for(const Bleeder& x : bleeders)
            {
                b = x.num == test.monster ? &x : b;
            }
            test.drops0 = b ? b->drops : 0;
            test.t0 = now;
            test.phase = 24;
            return;
        }
        case 24: // 2 s of its corpse dripping, then 3 s more: past the corpse time, forgotten
        {
            const Bleeder* b = nullptr;
            for(const Bleeder& x : bleeders)
            {
                b = x.num == test.monster ? &x : b;
            }
            if(test.win < 7 && now - test.t0 >= 2.0)
            {
                q_snprintf(line, sizeof(line), "its corpse drips on: %d drops in 2 s", b ? b->drops - test.drops0 : 0);
                check(b != nullptr && b->drops - test.drops0 >= 1, line);
                test.win = 7;
            }
            if(now - test.t0 < 5.0)
            {
                return;
            }
            q_snprintf(line, sizeof(line), "its corpse stops dripping after the corpse time (4 s)%s",
                b ? va(": still, died %.1f s ago, hit %.1f s ago", now - b->died, now - b->lastHit) : "");
            check(b == nullptr, line);
            testEnd();
            return;
        }
        default: return;
    }
}

void test_f()
{
    testName = "drips_test";
    test = Test{};
    test.phase = 1;
    test.deadline = cl.time + 60.0;
    if(!cl.worldmodel)
    {
        test.deadline = 1e9; // (set again once the map is in: below)
    }
    Con_Printf("%s: started\n", testName);
}

// vr_drips_bleed_test (vr_scenario waterdrips first): a grunt hit to half health drips blood all through 30 s; the other,
// unhurt, none.
void bleedTest_f()
{
    testName = "drips_bleed_test";
    test = Test{};
    test.phase = 20;
    q_strlcpy(test.savedWetTime, vr_wounds_wet_time.string, sizeof(test.savedWetTime));
    test.deadline = cl.worldmodel ? cl.time + 90.0 : 1e9;
    Con_Printf("%s: started\n", testName);
}

// ---- Commands -----------------------------------------------------------------------------------

void info_f()
{
    Con_Printf("drips (vr_drips %d, wounded %s): %d wet, %d wounded; drops: %d blood, %d water, %d slime; landed: %d "
               "blood, %d water, %d slime; %d frames over budget\n",
        static_cast<int>(vr_drips.value), bloodOn() ? "on" : "off", static_cast<int>(wets.size()), static_cast<int>(bleeders.size()),
        counters.emitted[0], counters.emitted[1], counters.emitted[2], counters.landed[0], counters.landed[1], counters.landed[2],
        counters.capped);
    for(const Wet& w : wets)
    {
        Con_Printf("  %s %d: wet %.2f, waterline %.0f, %s, %d drops%s\n",
            w.what == What::Thing ? "entity" : w.what == What::Body ? "your body" : w.what == What::Hand ? "hand" :
            w.what == What::Held ? "held weapon" : w.what == What::Holstered ? "holstered" : "probe",
            w.what == What::Hand || w.what == What::Held ? w.hand : w.num, static_cast<double>(w.wet),
            static_cast<double>(w.waterline), w.liquid == Liquid::Slime ? "slime" : "water", w.drops,
            cl.time - w.lastIn < inGrace ? " (in it)" : "");
    }
    for(const Bleeder& b : bleeders)
    {
        Con_Printf("  wounded %d: health %.0f of %.0f, %d wounds, burst %.2f, %d drops\n", b.num, static_cast<double>(healthOf(b.num)),
            static_cast<double>(b.start), b.count, static_cast<double>(b.burst), b.drops);
    }
}

// Everything within 512 units and your own soaked, as just out of the water.
void soak_f()
{
    if(!cl.worldmodel || cl.viewentity <= 0)
    {
        return;
    }
    const glm::vec3 me{cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]};
    int n = 0;
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t& e = cl_entities[i];
        glm::vec3 lo, hi;
        if(i == cl.viewentity || e.msgtime != cl.mtime[0] || !boxOf(e, lo, hi) || glm::length(0.5f * (lo + hi) - me) > 512.f)
        {
            continue;
        }
        if(Wet* w = add(&e, i, What::Thing, -1))
        {
            look(*w, lo, hi, 0.f);
            w->wet = 1.f;
            w->waterline = hi.z - lo.z;
            w->lastIn = cl.time - 1.0;
            n++;
        }
    }
    // Your own: soaked as they are looked at next (their entries made now, a moment out of the water).
    entity_t* mine[3]{};
    view::woundTargets(mine);
    const auto soakOwn = [&](const entity_t* e, What what, int hand) {
        if(Wet* w = e ? add(e, 0, what, hand) : nullptr)
        {
            w->wet = 1.f;
            w->waterline = 1e4f;
            w->lastIn = cl.time - 1.0;
            w->seen = cl.time;
            n++;
        }
    };
    soakOwn(mine[0], What::Body, -1);
    for(int hand = 0; hand < 2; hand++)
    {
        soakOwn(mine[1 + hand], What::Hand, hand);
        const view::ViewEntity* held = view::heldWeapon(hand);
        soakOwn(held && held->visible ? &held->ent : nullptr, What::Held, hand);
    }
    Con_Printf("vr_drips_soak: %d things soaked\n", n);
}

} // namespace

void frame(const glm::vec3& head)
{
    if(host_framecount == lastFrame)
    {
        return;
    }
    lastFrame = host_framecount;
    eyes = head;
    // A new map, the same one loaded again too (a restart, a save of this level): every wet thing's entity pointer is
    // stale (cl_entities allocated again), so all are forgotten (a drop off one read freed memory: a crash).
    if(cl.worldmodel != lastWorld || worldGeneration() != lastWorldGen)
    {
        clear();
        lastWorld = cl.worldmodel;
        lastWorldGen = worldGeneration();
    }
    if(test.phase > 0)
    {
        if(test.deadline > 1e8 && cl.worldmodel)
        {
            test.deadline = cl.time + 90.0;
        }
        testStep();
    }
    if(!cl.worldmodel || cls.signon != SIGNONS || cl.viewentity <= 0)
    {
        return;
    }
    QVR_PROFILE("drips");
    const double t0 = Sys_DoubleTime();
    double dtd = lastTime < 0.0 ? 0.0 : cl.time - lastTime;
    lastTime = cl.time;
    if(dtd < 0.0 || dtd > 0.25)
    {
        dtd = 0.0; // (a load, a pause: no burst after it)
    }
    const float dt = static_cast<float>(dtd);

    const bool water = vr_drips.value != 0.f;
    const bool blood = bloodOn();
    if(!water)
    {
        za::vectorEraseIf(wets, [](const Wet& w) { return w.what != What::Probe; });
    }
    if(!blood)
    {
        bleeders.clear();
    }

    // What is drawn this frame (only that drips).
    frameStamp++;
    if(drawnStamp.size() < static_cast<za::SizeT>(cl.num_entities))
    {
        drawnStamp.resize(static_cast<za::SizeT>(cl.num_entities), 0u);
    }
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const za::SizeT n = static_cast<za::SizeT>(cl_visedicts[i] - cl_entities);
        if(cl_visedicts[i] >= cl_entities && n < drawnStamp.size())
        {
            drawnStamp[n] = frameStamp;
        }
    }

    // Looking: the client's models ten times a second, your own and the probes each frame.
    if(water)
    {
        if(cl.time >= nextScan || cl.time < nextScan - 1.0)
        {
            scan(static_cast<float>(scanEvery));
            nextScan = cl.time + scanEvery;
        }
        lookOwn(dt);
    }
    for(Wet& w : wets)
    {
        if(w.what == What::Probe)
        {
            glm::vec3 lo, hi;
            if(boxOf(*w.ent, lo, hi))
            {
                look(w, lo, hi, dt);
            }
        }
    }

    // Drying and bleeding: the drops owed.
    za::vectorEraseIf(wets, [dt](Wet& w) { return !dry(w, dt); });
    za::vectorEraseIf(bleeders, [dt](Bleeder& b) { return !bleed(b, dt); });

    // The drops, within the frame's budget, round robin (many wet monsters after a water fight share it).
    int budget = static_cast<int>(za::clamp(vr_drips_max.value, 0.f, 128.f));
    const za::SizeT total = wets.size() + bleeders.size();
    int dropped = 0;
    bool over = false;
    for(za::SizeT k = 0; k < total; k++)
    {
        const za::SizeT i = (cursor + k) % total;
        if(i < wets.size())
        {
            Wet& w = wets[i];
            while(w.pending >= 1.f)
            {
                if(budget <= 0)
                {
                    over = true;
                    break;
                }
                w.pending -= 1.f;
                if(drawn(w))
                {
                    dropWater(w);
                    budget--;
                    dropped++;
                }
            }
        }
        else
        {
            Bleeder& b = bleeders[i - wets.size()];
            while(b.pending >= 1.f)
            {
                if(budget <= 0)
                {
                    over = true;
                    break;
                }
                b.pending -= 1.f;
                if(drawnNum(b.num))
                {
                    dropBlood(b);
                    budget--;
                    dropped++;
                }
            }
        }
    }
    cursor = total > 0 ? (cursor + 1) % total : 0;
    counters.capped += over ? 1 : 0;
    frameDrops = dropped;
    frameWorkSeconds = Sys_DoubleTime() - t0;
}

void onWound(int num, const glm::vec3& org, int amount, int kind)
{
    // Blood: a shot, a nail, a blow, a blast (not a burn, a zap, lava, slime, a liquid).
    if((kind < 1 || kind > 4) || !bloodOn() || num <= 0 || num >= cl.num_entities || num == cl.viewentity)
    {
        return;
    }
    const float health = healthOf(num);
    if(health == 0.f && (!onServer(num) || goneOnServer(num) || corpseSeconds() <= 0.f))
    {
        return; // (gone; or a corpse, and corpses don't drip)
    }
    Bleeder* b = nullptr;
    for(Bleeder& x : bleeders)
    {
        if(x.num == num)
        {
            b = &x;
        }
    }
    if(!b)
    {
        if(bleeders.size() >= maxBleeders)
        {
            // The one hit longest ago makes way.
            za::SizeT oldest = 0;
            for(za::SizeT i = 1; i < bleeders.size(); i++)
            {
                oldest = bleeders[i].lastHit < bleeders[oldest].lastHit ? i : oldest;
            }
            bleeders[oldest] = bleeders.back();
            bleeders.popBack();
        }
        bleeders.pushBack(Bleeder{});
        b = &bleeders.back();
        b->num = num;
        b->start = health > 0.f ? health : 0.f;
        b->firstHit = cl.time;
        b->lastSeen = cl.time;
        b->died = health == 0.f ? cl.time : -1.0;
    }
    if(b->firstHit == cl.time && health > 0.f)
    {
        b->start += static_cast<float>(amount); // (the first frame's hits: a shotgun blast's pellets each their own)
    }
    const entity_t& e = cl_entities[num];
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    b->rel[b->next] = turn(org - o, -e.angles[1]);
    b->vertex[b->next] = -2; // (bound as it first drips)
    b->next = (b->next + 1) % 4;
    b->count = za::min(b->count + 1, 4);
    b->lastHit = cl.time;
    b->burst = za::min(3.f, b->burst + za::clamp(vr_drips_blood_hit.value, 0.f, 10.f) * static_cast<float>(amount) / 20.f);
}

void landed(int liquid, const glm::vec3& where, const glm::vec3& normal, const glm::vec4& color)
{
    if(liquid < 1 || liquid > 2)
    {
        return;
    }
    counters.landed[liquid]++;
    if(normal.z < 0.6f)
    {
        return; // a wall: it runs down out of sight
    }
    if(vr_drips_splash.value != 0.f)
    {
        particles::bloodSpecks(where, normal, 2 + static_cast<int>(vrand::range(rng, 0.f, 1.99f)), glm::vec3{color},
            za::clamp(color.a * 1.2f, 0.f, 1.f));
    }
    goresound::waterDrip(where, vrand::range(rng, 0.6f, 1.f));
}

void noteBloodLanding()
{
    counters.landed[0]++;
}

float wetness(const entity_s* e)
{
    const Wet* w = find(e);
    return w ? w->wet : 0.f;
}

void clear()
{
    za::vectorEraseIf(wets, [](const Wet& w) { return w.what == What::Probe && test.phase > 0; });
    if(test.phase == 0)
    {
        wets.clear();
    }
    bleeders.clear();
    lastTime = -1.0;
    nextScan = 0.0;
    cursor = 0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_drips_info", info_f);
    Cmd_AddCommand("vr_drips_soak", soak_f);
    Cmd_AddCommand("vr_drips_test", test_f);
    Cmd_AddCommand("vr_drips_bleed_test", bleedTest_f);
}

} // namespace qvr::drips
