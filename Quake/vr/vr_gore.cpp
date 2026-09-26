// vr_gore.cpp -- see vr_gore.hpp.

#include "vr_gore.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>
#include <vector>

namespace qvr::gore
{
namespace
{

using decals::Mark;
using decals::MarkOptions;

std::mt19937 rng{std::random_device{}()};

[[nodiscard]] float rnd(float lo, float hi)
{
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

[[nodiscard]] glm::vec3 onSphere()
{
    const float z = rnd(-1.f, 1.f);
    const float a = rnd(0.f, 6.2831853f);
    const float r = std::sqrt(std::max(0.f, 1.f - z * z));
    return {r * std::cos(a), r * std::sin(a), z};
}

// A count from a rate: rounded at random, so fractions still come out right on average.
[[nodiscard]] int stochastic(float n)
{
    return static_cast<int>(std::floor(n + rnd(0.f, 1.f)));
}

[[nodiscard]] float sizeMult()
{
    return std::clamp(vr_gore_size.value, 0.25f, 4.f);
}

[[nodiscard]] float dripMult()
{
    return std::clamp(vr_gore_drips.value, 0.f, 4.f);
}

[[nodiscard]] float gravity()
{
    return std::max(100.f, sv_gravity.value);
}

// The colour of a drop falling through `p`: blood, as lit as the place is (a particle takes no light
// of its own; the lightmap under it).
[[nodiscard]] glm::vec3 dropColor(const glm::vec3& p)
{
    static lightcache_t cache{};
    vec3_t v = {p.x, p.y, p.z};
    const float light = cl.worldmodel ? static_cast<float>(R_LightPoint(v, 0.f, &cache)) : 110.f; // 128: full light
    const float k = std::clamp(std::max(light, 40.f) / 100.f, 0.4f, 1.4f);
    return glm::min(glm::vec3{0.42f, 0.02f, 0.015f} * k, glm::vec3{1.f});
}

[[nodiscard]] bool inLiquid(const glm::vec3& p)
{
    vec3_t v = {p.x, p.y, p.z};
    const mleaf_t* leaf = Mod_PointInLeaf(v, cl.worldmodel);
    return leaf && leaf->contents < CONTENTS_SOLID; // water, slime, lava
}

// ---- Lines of blood flung out by a hit or a burst, traced a few a frame --------------------------

struct Ray
{
    glm::vec3 from, dir;
    float reach, size;
    float dripChance;   // a mark on the ceiling drips
    float streakChance; // a mark on a wall runs down it
    bool big;           // a burst's (splotches head on)
};

std::deque<Ray> rays;
constexpr std::size_t maxRays = 320;
constexpr int raysPerFrame = 24;

void queue(const Ray& r)
{
    if(rays.size() < maxRays)
    {
        rays.push_back(r);
    }
}

// ---- Dripping: drops falling from a source (a splat on the ceiling, a hanging gib) --------------

struct Source
{
    int ent = 0; // a hanging gib's (kept alive while it hangs), 0 a splat's
    glm::vec3 pos{0.f};
    double until = 0.0, next = 0.0;
    float interval = 0.5f; // seconds between drops, about
    float spread = 1.f;    // drops fall from within this far of `pos`
    int dropsLeft = 0;     // it bleeds dry
    int marks = 0;         // drops that marked the floor (the first few do)
    bool puddle = false;   // the first drop's landing starts a puddle
    glm::vec3 color{0.4f, 0.02f, 0.015f};
};

std::vector<Source> sources;
constexpr std::size_t maxSources = 48;

struct Landing
{
    double time;
    glm::vec3 where, normal, color;
    float size;       // its mark's (0: none)
    float puddleGrow; // > 0: a puddle spreading over this long, not a drop's mark
};

std::vector<Landing> landings;
constexpr std::size_t maxLandings = 192;

// The drops' marks, at most so many a second (a steady drip would fill the decal budget).
double markSecond = -1.0;
int marksThisSecond = 0;

[[nodiscard]] bool markBudget()
{
    if(cl.time < markSecond || cl.time - markSecond >= 1.0)
    {
        markSecond = cl.time;
        marksThisSecond = 0;
    }
    return ++marksThisSecond <= 20;
}

// A drip from `pos` for `seconds` (starting after `delay`), a drop every `interval` or so.
void addSource(int ent, const glm::vec3& pos, float seconds, float spread, float interval, int drops, float delay = 0.f)
{
    if(seconds <= 0.f || drops <= 0 || sources.size() >= maxSources)
    {
        return;
    }
    Source s;
    s.ent = ent;
    s.pos = pos;
    s.until = cl.time + delay + seconds;
    s.interval = interval;
    s.next = cl.time + delay + rnd(0.1f, 0.6f) * interval;
    s.spread = spread;
    s.dropsLeft = drops;
    s.color = dropColor(pos);
    sources.push_back(s);
}

// A drop falls from `p`: one trace down for where it lands and when (it falls straight: real
// enough for blood from a ceiling), the falling drop a particle, its landing later.
bool fall(const glm::vec3& p, float dropSize, const glm::vec3& color, float markSize, float puddleGrow)
{
    glm::vec3 where, normal;
    float f;
    if(landings.size() >= maxLandings || inLiquid(p) || !decals::trace(p, p - glm::vec3{0.f, 0.f, 768.f}, where, normal, f))
    {
        return false;
    }
    const float h = std::max(0.f, p.z - where.z);
    const float t = std::sqrt(2.f * h / gravity());
    particles::bloodDrip(p, t, where.z, dropSize, color);
    landings.push_back({cl.time + t, where, normal, color, markSize, puddleGrow});
    return true;
}

void drip(Source& s)
{
    const float a = rnd(0.f, 6.2831853f);
    const float r = s.spread * std::sqrt(rnd(0.f, 1.f));
    const glm::vec3 p = s.pos + glm::vec3{std::cos(a) * r, std::sin(a) * r, 0.f};
    // The first few mark the floor (the first starts a puddle spreading while it drips), the
    // others splash on them.
    const float puddle = !s.puddle ? std::clamp(static_cast<float>(s.until - cl.time), 4.f, 12.f) : 0.f;
    const float mark = s.puddle && s.marks < 5 && rnd(0.f, 1.f) < 0.7f ? rnd(2.5f, 4.5f) * sizeMult() : 0.f;
    if(fall(p, rnd(0.6f, 0.85f), s.color, mark, puddle))
    {
        s.puddle = true;
        s.marks += mark > 0.f ? 1 : 0;
    }
    s.dropsLeft--;
}

// A run of blood down the wall from `where` (normal `n`), `width` across, as far as the wall
// goes (to the floor), showing after `delay`, running down over a few seconds.
void streak(const glm::vec3& where, const glm::vec3& n, float width, float delay)
{
    glm::vec3 down = glm::vec3{0.f, 0.f, -1.f} - n * n.z;
    if(glm::length(down) < 0.5f || width < 2.f)
    {
        return;
    }
    down = glm::normalize(down);
    float length = width * rnd(2.f, 3.8f);
    glm::vec3 w, hn;
    float f;
    const glm::vec3 start = where + n * 1.f;
    if(decals::trace(start, start + down * length, w, hn, f))
    {
        length *= f; // down to the floor (or a ledge)
    }
    if(length < width * 1.2f)
    {
        return;
    }
    MarkOptions o;
    o.along = down;
    o.aspect = length / width;
    o.delay = delay + rnd(0.1f, 0.4f);
    o.grow = rnd(2.f, 5.f);
    o.growFrom = 0.15f;
    o.fromStart = true;
    o.darken = 0.25f;
    decals::place(Mark::Streak, where - down * (width * 0.15f), n, width, o);
}

void cast(const Ray& r)
{
    glm::vec3 where, n;
    float f;
    const glm::vec3 end = r.from + r.dir * r.reach;
    float flight = r.reach;
    if(decals::trace(r.from, end, where, n, f))
    {
        flight *= f;
    }
    else if(decals::trace(end, end - glm::vec3{0.f, 0.f, 256.f}, where, n, f) && n.z > 0.6f)
    {
        // Nothing near that way: it falls to the floor at the end of its flight (a spray landing
        // with its momentum: drawn out along it).
        flight += f * 256.f;
        f = 0.5f; // (spread less: its flight was mostly up or its momentum spent)
    }
    else
    {
        return;
    }
    // It shows as the blood would arrive; wider the further it flew.
    const float delay = flight / 450.f;
    const float size = r.size * (0.8f + 0.4f * f);
    // Head on, a splat all round; glancing, a spray along the way it went.
    const bool headOn = std::fabs(glm::dot(r.dir, n)) > 0.85f;
    MarkOptions o;
    o.along = r.dir;
    o.delay = delay;
    o.darken = 0.2f;
    const bool placed = decals::place(headOn || r.big ? Mark::Splotch : Mark::Splatter, where, n, size, o);
    if(developer.value >= 2)
    {
        Con_Printf("gore: blood %.0f units %s, a %.0f-unit mark on (%.1f %.1f %.1f)%s\n", flight, f < 1.f ? "on" : "then down",
            size, n.x, n.y, n.z, placed ? "" : ": none fits");
    }
    if(!placed)
    {
        return;
    }
    if(n.z < -0.6f && rnd(0.f, 1.f) < r.dripChance && dripMult() > 0.f)
    {
        // On the ceiling: it drips a while.
        const float seconds = rnd(2.5f, 6.f) * dripMult();
        addSource(0, where + n * 1.5f, seconds, size * 0.2f, rnd(0.25f, 0.5f), static_cast<int>(seconds * 4.f), delay);
    }
    else if(std::fabs(n.z) < 0.35f && rnd(0.f, 1.f) < r.streakChance)
    {
        streak(where, n, size * rnd(0.3f, 0.5f), delay);
    }
}

// ---- Pools under corpses ------------------------------------------------------------------------

struct PendingPool
{
    glm::vec3 org;
    double start;
    float size;
    int ent;
    const qmodel_t* model;
};

std::vector<PendingPool> pending;
constexpr std::size_t maxPending = 32;

void pool(const PendingPool& p)
{
    // Where the corpse lies now (its death has moved it a little).
    glm::vec3 org = p.org;
    if(p.ent > 0 && p.ent < cl.num_entities && cl_entities[p.ent].model == p.model && p.model)
    {
        const entity_t& e = cl_entities[p.ent];
        if(glm::distance(glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, p.org) < 96.f)
        {
            org = {e.origin[0], e.origin[1], e.origin[2]};
        }
    }
    glm::vec3 where, n;
    float f;
    if(!decals::trace(org + glm::vec3{0.f, 0.f, 8.f}, org - glm::vec3{0.f, 0.f, 96.f}, where, n, f) || n.z < 0.7f ||
        inLiquid(where + n * 2.f))
    {
        return;
    }
    // Spreading slowly, darkening as it thickens; a second, off to one side, a little later.
    MarkOptions o;
    o.grow = rnd(12.f, 18.f);
    o.growFrom = 0.12f;
    o.darken = 0.35f;
    decals::place(Mark::Pool, where, n, p.size, o);
    const float a = rnd(0.f, 6.2831853f);
    const glm::vec3 side = glm::vec3{std::cos(a), std::sin(a), 0.f} * (p.size * 0.25f);
    o.delay = rnd(1.f, 2.5f);
    o.grow = rnd(14.f, 20.f);
    o.growFrom = 0.1f;
    o.darken = 0.4f;
    if(decals::trace(where + side + n * 4.f, where + side - n * 8.f, where, n, f) && n.z > 0.7f)
    {
        decals::place(Mark::Pool, where, n, p.size * rnd(0.55f, 0.75f), o);
    }
}

// ---- The events ---------------------------------------------------------------------------------

enum HitKind
{
    HitShot,
    HitPellets,
    HitRadial,
    HitMelee
};

void hit(const glm::vec3& org, const glm::vec3& dir, int damage, int kind, int lvl)
{
    const float mult = std::clamp(vr_gore_spray.value, 0.f, 4.f);
    if(mult <= 0.f || damage <= 0)
    {
        return;
    }
    const glm::vec3 d = glm::length(dir) > 0.1f ? glm::normalize(dir) : glm::vec3{0.f, 0.f, -1.f};
    float n = lvl >= 2 ? std::min(10.f, 3.f + damage / 8.f) : std::min(3.f, 1.f + damage / 25.f);
    if(kind == HitRadial)
    {
        n *= 1.5f;
    }
    const int count = stochastic(n * mult);
    if(developer.value >= 2)
    {
        Con_Printf("gore: hit for %d (kind %d): %d lines of blood\n", damage, kind, count);
    }
    const float spread = kind == HitPellets ? 0.45f : kind == HitRadial ? 1.1f : kind == HitMelee ? 0.5f : 0.3f;
    const float reach = std::min(96.f + damage * 1.5f, 200.f) * (lvl >= 2 ? 1.f : 0.8f);
    const float size = std::clamp(28.f + damage * 1.2f, 28.f, 96.f) * sizeMult() * (lvl >= 2 ? 1.f : 0.7f);
    for(int i = 0; i < count; i++)
    {
        // Behind the hit, spreading, falling a little.
        const glm::vec3 r = d + onSphere() * (spread * rnd(0.3f, 1.f)) + glm::vec3{0.f, 0.f, -0.25f};
        queue({org, glm::normalize(r), reach * rnd(0.6f, 1.f), size * rnd(0.7f, 1.3f), lvl >= 2 ? 0.35f : 0.1f,
            lvl >= 2 ? 0.5f : 0.15f, false});
    }
}

void burst(const glm::vec3& org, const glm::vec3& dir, float size, int lvl)
{
    const float scale = std::clamp(size, 0.7f, 2.f);
    const float mult = std::clamp(vr_gore_spray.value, 0.f, 4.f);
    const int count = stochastic((lvl >= 2 ? 14.f : 5.f) * scale * mult);
    const glm::vec3 d = glm::length(dir) > 0.1f ? glm::normalize(dir) : glm::vec3{0.f};
    const float reach = lvl >= 2 ? 170.f : 120.f;
    // Where the blow drives it (a gib thrown at a wall: that wall), a big splat first, dripping
    // from a ceiling.
    if(glm::dot(d, d) > 0.f && mult > 0.f)
    {
        queue({org, d, 64.f, rnd(56.f, 80.f) * std::sqrt(scale) * sizeMult() * (lvl >= 2 ? 1.f : 0.7f), 1.f,
            lvl >= 2 ? 1.f : 0.3f, true});
    }
    for(int i = 0; i < count; i++)
    {
        glm::vec3 r;
        float range = reach;
        if(lvl >= 2 && i < count / 4)
        {
            r = glm::vec3{0.f, 0.f, 1.f} + onSphere() * 0.5f; // up, onto the ceiling
            range = 320.f;
        }
        else if(glm::dot(d, d) > 0.f && (i & 1))
        {
            r = d + onSphere() * 0.7f; // along the blow
        }
        else
        {
            r = onSphere();
            r.z *= 0.8f;
        }
        const float s = rnd(32.f, 56.f) * std::sqrt(scale) * sizeMult() * (lvl >= 2 ? 1.f : 0.7f);
        queue({org, glm::normalize(r), range * rnd(0.6f, 1.f), s, lvl >= 2 ? 1.f : 0.4f, lvl >= 2 ? 0.6f : 0.2f, (i % 3) == 0});
    }

    // A pool under it, spreading.
    const float pools = std::clamp(vr_gore_pools.value, 0.f, 3.f);
    glm::vec3 where, n;
    float f;
    if(pools > 0.f && decals::trace(org, org - glm::vec3{0.f, 0.f, 160.f}, where, n, f) && n.z > 0.7f &&
        !inLiquid(where + n * 2.f))
    {
        MarkOptions o;
        o.delay = 0.2f + f * 0.3f;
        o.grow = rnd(4.f, 7.f);
        o.growFrom = 0.3f;
        o.darken = 0.3f;
        decals::place(Mark::Pool, where, n, rnd(48.f, 72.f) * scale * sizeMult() * pools * (lvl >= 2 ? 1.f : 0.7f), o);
    }
}

void corpse(const glm::vec3& org, float radius, int lvl)
{
    const float mult = std::clamp(vr_gore_pools.value, 0.f, 3.f);
    if(mult <= 0.f || pending.size() >= maxPending)
    {
        return;
    }
    // The corpse: the entity where the monster died.
    int ent = 0;
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(e.model && std::fabs(e.origin[0] - org.x) < 1.f && std::fabs(e.origin[1] - org.y) < 1.f &&
            std::fabs(e.origin[2] - org.z) < 1.f)
        {
            ent = i;
            break;
        }
    }
    const float size = std::clamp(radius * 4.5f, 56.f, 160.f) * mult * sizeMult() * (lvl >= 2 ? 1.f : 0.6f);
    // Once it has fallen.
    pending.push_back({org, cl.time + 1.2, size, ent, ent ? cl_entities[ent].model : nullptr});
}

// ---- The player's wounds: drops round the feet ---------------------------------------------------

double playerTime = -1.0;
int playerHealth = 0;
float playerBurst = 0.f; // a recent hit: more, fading
float playerOwed = 0.f;  // drops owed

void playerBleed()
{
    const double dtd = playerTime < 0.0 ? 0.0 : cl.time - playerTime;
    playerTime = cl.time;
    const int health = cl.stats[STAT_HEALTH];
    const float mult = std::clamp(vr_body_blood_floor.value, 0.f, 4.f);
    if(dtd <= 0.0 || dtd > 0.25 || health <= 0 || cl.intermission || mult <= 0.f || cl.viewentity <= 0 ||
        cl.viewentity >= cl.num_entities)
    {
        playerHealth = health;
        playerOwed = 0.f;
        return;
    }
    const float dt = static_cast<float>(dtd);
    if(playerHealth > 0 && health < playerHealth)
    {
        playerBurst = std::min(3.f, playerBurst + static_cast<float>(playerHealth - health) / 12.f);
    }
    playerHealth = health;
    playerBurst *= std::exp(-dt / 1.5f);

    // As the wound skins (vr_view.cpp's damageLevel): from 75 health down.
    constexpr float rates[4] = {0.f, 0.5f, 1.1f, 2.2f};
    const int wounds = health > 75 ? 0 : health > 50 ? 1 : health > 25 ? 2 : 3;
    playerOwed += (rates[wounds] + playerBurst * 1.5f) * mult * dt;
    const float amount = std::clamp(vr_body_blood_amount.value, 0.25f, 4.f);
    const float markChance = std::clamp(vr_body_blood_marks.value, 0.f, 1.f);
    const float markSize = std::clamp(vr_body_blood_mark_size.value, 0.25f, 4.f);
    int n = 0;
    while(playerOwed >= 1.f && n++ < 4)
    {
        playerOwed -= 1.f;
        // From the body (about the waist), round it: in view looking down, never before the eyes.
        const entity_t& pl = cl_entities[cl.viewentity];
        const float a = rnd(0.f, 6.2831853f);
        const float r = rnd(4.f, 11.f);
        const glm::vec3 p{pl.origin[0] + std::cos(a) * r, pl.origin[1] + std::sin(a) * r, pl.origin[2] + rnd(-6.f, 6.f)};
        const bool mark = rnd(0.f, 1.f) < markChance;
        fall(p, rnd(0.5f, 0.7f) * amount, dropColor(p), mark ? rnd(3.5f, 6.5f) * markSize * std::sqrt(amount) : 0.f, 0.f);
    }
    playerOwed = std::min(playerOwed, 2.f);
}

} // namespace

int level()
{
    return std::clamp(static_cast<int>(vr_gore.value), 0, 2);
}

bool event(const glm::vec3& org, const glm::vec3& dir, int preset, int count)
{
    if(preset < EventHit || preset > EventCorpse)
    {
        return false;
    }
    const int lvl = level();
    if(lvl <= 0 || !vr_decals.value || !cl.worldmodel)
    {
        return true;
    }
    switch(preset)
    {
        case EventHit: hit(org, dir, count % 1000, count / 1000, lvl); break;
        case EventBurst: burst(org, dir, static_cast<float>(count) / 10.f, lvl); break;
        case EventCorpse: corpse(org, dir.x * 8.f, lvl); break;
        default: break;
    }
    return true;
}

bool gibImpact(const glm::vec3& where, const glm::vec3& normal, float strength, const glm::vec3& velocity)
{
    const int lvl = level();
    if(lvl <= 0)
    {
        return false;
    }
    const float size = std::clamp(24.f + strength * 0.05f, 24.f, 64.f) * sizeMult() * (lvl >= 2 ? 1.f : 0.7f);
    MarkOptions o;
    o.along = velocity;
    o.darken = 0.2f;
    if(!decals::place(Mark::Splotch, where, normal, size, o))
    {
        return true;
    }
    if(normal.z < -0.6f && (lvl >= 2 || rnd(0.f, 1.f) < 0.5f) && dripMult() > 0.f)
    {
        const float seconds = rnd(3.f, 6.f) * dripMult();
        addSource(0, where + normal * 1.5f, seconds, size * 0.2f, rnd(0.25f, 0.45f), static_cast<int>(seconds * 4.f));
    }
    else if(std::fabs(normal.z) < 0.35f && lvl >= 2)
    {
        streak(where, normal, size * rnd(0.35f, 0.5f), 0.f);
    }
    return true;
}

void gibRest(const glm::vec3& where, const glm::vec3& normal)
{
    const int lvl = level();
    const float pools = std::clamp(vr_gore_pools.value, 0.f, 3.f);
    if(lvl <= 0 || pools <= 0.f || inLiquid(where + normal * 2.f))
    {
        return;
    }
    MarkOptions o;
    o.grow = rnd(5.f, 8.f);
    o.growFrom = 0.2f;
    o.darken = 0.3f;
    decals::place(Mark::Pool, where, normal, rnd(16.f, 26.f) * sizeMult() * pools * (lvl >= 2 ? 1.f : 0.7f), o);
}

void gibHanging(int ent, const glm::vec3& org)
{
    if(dripMult() <= 0.f)
    {
        return;
    }
    for(Source& s : sources)
    {
        if(s.ent == ent)
        {
            s.pos = org - glm::vec3{0.f, 0.f, 3.f};
            s.until = std::max(s.until, cl.time + 0.25);
            return;
        }
    }
    // It bleeds a while (dry after so many drops; hanging again later, it bleeds again).
    addSource(ent, org - glm::vec3{0.f, 0.f, 3.f}, 0.25f, 2.f, rnd(0.25f, 0.45f) / std::max(0.25f, dripMult()),
        static_cast<int>(24.f * dripMult()));
}

void frame()
{
    QVR_PROFILE("gore");
    if(!cl.worldmodel || level() <= 0)
    {
        if(!rays.empty() || !sources.empty() || !pending.empty())
        {
            rays.clear();
            sources.clear();
            pending.clear();
        }
        // Landings already on their way still land (their drops are falling).
    }

    // Pools whose corpse has fallen.
    for(std::size_t i = 0; i < pending.size();)
    {
        if(cl.time >= pending[i].start || cl.time < pending[i].start - 5.0)
        {
            pool(pending[i]);
            pending[i] = pending.back();
            pending.pop_back();
        }
        else
        {
            i++;
        }
    }

    // The lines of blood, a few a frame.
    for(int i = 0; i < raysPerFrame && !rays.empty(); i++)
    {
        const Ray r = rays.front();
        rays.pop_front();
        cast(r);
    }

    // Drips.
    for(Source& s : sources)
    {
        for(int n = 0; n < 3 && cl.time >= s.next && cl.time < s.until && s.dropsLeft > 0; n++)
        {
            drip(s);
            s.next += s.interval * rnd(0.6f, 1.4f);
        }
        if(s.next < cl.time - 1.0)
        {
            s.next = cl.time; // a long frame (a load): not a burst of drops after it
        }
    }
    std::erase_if(sources, [](const Source& s) { return cl.time >= s.until || s.dropsLeft <= 0 || cl.time < s.until - 60.0; });

    if(cl.worldmodel && level() > 0)
    {
        playerBleed();
    }

    // Drops landing: a splash, a mark or a puddle.
    for(std::size_t i = 0; i < landings.size();)
    {
        const Landing& l = landings[i];
        if(cl.time < l.time && cl.time > l.time - 10.0)
        {
            i++;
            continue;
        }
        if(l.normal.z > 0.6f && cl.worldmodel)
        {
            if(l.puddleGrow > 0.f)
            {
                MarkOptions o;
                o.grow = l.puddleGrow;
                o.growFrom = 0.2f;
                o.darken = 0.3f;
                decals::place(Mark::Pool, l.where, l.normal, rnd(9.f, 15.f) * sizeMult(), o);
            }
            else if(l.size > 0.f && markBudget())
            {
                decals::place(Mark::Drop, l.where, l.normal, l.size);
            }
            particles::bloodSpecks(l.where, l.normal, 3, l.color);
        }
        landings[i] = landings.back();
        landings.pop_back();
    }
}

void test_f()
{
    if(!cl.worldmodel || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        return;
    }
    // As if a monster 64 units ahead were hit by a shot of vr_gore_test's damage (default 40) from
    // here, or gibbed ("burst").
    vec3_t fwd, right, up;
    AngleVectors(cl.viewangles, fwd, right, up);
    const glm::vec3 f{fwd[0], fwd[1], 0.f};
    const glm::vec3 dir = glm::length(f) > 0.01f ? glm::normalize(f) : glm::vec3{1.f, 0.f, 0.f};
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 org = glm::vec3{pl.origin[0], pl.origin[1], pl.origin[2]} + dir * 64.f + glm::vec3{0.f, 0.f, 8.f};
    const int lvl = std::max(1, level());
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "burst"))
    {
        burst(org, dir, 2.f, lvl);
    }
    else if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "corpse"))
    {
        pending.push_back({org, cl.time, std::clamp(16.f * 4.5f, 56.f, 160.f) * sizeMult(), 0, nullptr});
    }
    else
    {
        hit(org, dir, Cmd_Argc() > 1 ? std::max(1, atoi(Cmd_Argv(1))) : 40, HitShot, lvl);
    }
}

void clear()
{
    rays.clear();
    sources.clear();
    landings.clear();
    pending.clear();
    playerTime = -1.0;
    playerOwed = 0.f;
    playerBurst = 0.f;
}

void count()
{
    Con_Printf("gore (vr_gore %d): %d lines of blood queued, %d dripping, %d drops falling, %d pools to come\n", level(),
        static_cast<int>(rays.size()), static_cast<int>(sources.size()), static_cast<int>(landings.size()),
        static_cast<int>(pending.size()));
}

} // namespace qvr::gore
