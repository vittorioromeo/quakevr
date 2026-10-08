// Explosion chunks: small-gib-style scaled models and bounded oldest-first lifetime management,
// with the casings' client-side physical simulation. Swept spheres collide with world/brush geometry;
// characters are deliberately absent from the collision world, so the chunks cannot hurt or push them.
#include "vr_explosiondebris.hpp"
#include <string.h>
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_particles.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

namespace qvr::explosiondebris
{
namespace
{
constexpr int capacity = 256;
constexpr int lightKey = -19000;
constexpr const char* modelName = "progs/vr_explosion_debris.mdl";
struct Chunk
{
    bool active{false};
    glm::vec3 pos{0.f}, vel{0.f}, angles{0.f}, spin{0.f};
    double born{0.0}, die{0.0};
    float size{1.f};
    entity_t ent{};
};
// Fixed-size, main-thread simulation state; scene pointers stay valid until the next frame.
Chunk chunks[capacity];
double lastRun = -1.0;
int lastFrame = -1;
za::FastNonCryptoRng rng{1};
struct Counters
{
    int made{0}, expired{0}, evicted{0}, bounced{0}, trailSamples{0}, lights{0}, rendered{0};
    float minSpeed{1e30f}, maxSpeed{0.f}, minLife{1e30f}, maxLife{0.f};
} counters;

float rnd(float lo, float hi) { return rng.getF(lo, hi); }
int limit() { return static_cast<int>(za::clamp(vr_explosion_debris_max.value, 0.f, float(capacity))); }
int oldest()
{
    int result = -1;
    for(int i = 0; i < capacity; ++i)
    {
        if(chunks[i].active && (result < 0 || chunks[i].born < chunks[result].born)) { result = i; }
    }
    return result;
}
void retire(int i)
{
    chunks[i].active = false;
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key == lightKey - i) { dl.radius = 0.f; dl.die = 0.f; }
    }
}
void trim(int max)
{
    while(liveCount() > max) { retire(oldest()); ++counters.evicted; }
}
glm::vec3 direction()
{
    const float z = rnd(-1.f, 1.f), angle = rnd(0.f, 6.2831853f);
    const float r = za::sqrt(za::max(0.f, 1.f - z * z));
    glm::vec3 v{r * za::cos(angle), r * za::sin(angle), z + za::clamp(vr_explosion_debris_up.value, 0.f, 2.f)};
    return glm::length(v) > 1e-4f ? glm::normalize(v) : glm::vec3{0.f, 0.f, 1.f};
}

// Seven offset line sweeps approximate the tiny sphere, including a centre sweep to avoid tunnelling.
trace_t sweep(const glm::vec3& from, const glm::vec3& to, float radius)
{
    // (The centre sweep once: in solid, it is the answer, as when the six were left after it.)
    constexpr glm::vec3 offsets[] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
    trace_t best = worldtrace::world(from, to, true, true);
    if(best.startsolid || best.allsolid) { return best; }
    for(const glm::vec3& axis : offsets)
    {
        const glm::vec3 offset = axis * radius;
        trace_t tr = worldtrace::world(from + offset, to + offset, true, true);
        if(tr.startsolid || tr.allsolid || tr.fraction < best.fraction)
        {
            best = tr;
            const glm::vec3 at = worldtrace::endPos(tr) - offset;
            for(int k = 0; k < 3; ++k) { best.endpos[k] = at[k]; }
            if(tr.startsolid || tr.allsolid) { break; }
        }
    }
    return best;
}

void advance(Chunk& c, float dt)
{
    const float radius = c.size * 0.5f;
    const glm::vec3 from = c.pos;
    c.vel.z -= za::max(sv_gravity.value, 0.f) * dt;
    c.vel *= za::exp(-0.15f * dt);
    float remaining = dt;
    for(int hit = 0; hit < 4 && remaining > 1e-5f; ++hit)
    {
        const glm::vec3 to = c.pos + c.vel * remaining;
        const trace_t tr = sweep(c.pos, to, radius);
        if(tr.startsolid || tr.allsolid) { c.vel = glm::vec3{0.f}; break; }
        c.pos = worldtrace::endPos(tr);
        if(tr.fraction >= 1.f) { break; }
        const glm::vec3 normal = worldtrace::normal(tr);
        c.pos += normal * 0.05f;
        const float into = glm::dot(c.vel, normal);
        if(into < 0.f)
        {
            const glm::vec3 tangent = c.vel - normal * into;
            const float bounce = -into * za::clamp(vr_explosion_debris_bounce.value, 0.f, 1.f);
            c.vel = tangent * 0.75f + normal * bounce;
            if(normal.z > 0.7f && bounce < 8.f * units::worldScale())
            {
                c.vel = tangent * za::exp(-12.f * dt);
                c.spin *= za::exp(-12.f * dt);
            }
            if(-into > 12.f * units::worldScale()) { ++counters.bounced; }
        }
        remaining *= 1.f - tr.fraction;
    }
    c.angles += c.spin * dt;
    const float heat = za::clamp(float((c.die - cl.time) / (c.die - c.born)), 0.f, 1.f);
    if(glm::length(c.pos - from) > 0.01f && vr_explosion_debris_trail.value > 0.f && vr_particles.value && r_particles.value)
    {
        particles::explosionDebrisTrail(from, c.pos, c.size, heat);
        ++counters.trailSamples;
    }
}

void scene(Chunk& c, qmodel_t* model)
{
    if(!model || cl_numvisedicts >= MAX_VISEDICTS) { return; }
    const float fadeTime = za::min(0.5f, float(c.die - c.born) * 0.33f);
    const float alpha = za::clamp(float(c.die - cl.time) / fadeTime, 0.f, 1.f);
    entity_t& e = c.ent;
    e.model = model;
    e.colormap = vid.colormap;
    const float extent = za::max(model->maxs[0] - model->mins[0], model->maxs[1] - model->mins[1], model->maxs[2] - model->mins[2], 1.f);
    e.scale = static_cast<unsigned char>(CLAMP(1.f, c.size / extent * ENTSCALE_DEFAULT + 0.5f, 255.f));
    e.alpha = alpha >= 1.f ? ENTALPHA_DEFAULT : static_cast<unsigned char>(ENTALPHA_ENCODE(alpha));
    e.lerpflags = LERP_RESETANIM | LERP_RESETMOVE;
    for(int k = 0; k < 3; ++k) { e.origin[k] = c.pos[k]; e.angles[k] = c.angles[k]; }
    cl_visedicts[cl_numvisedicts++] = &e;
    ++counters.rendered;
}

void lights()
{
    // Remove last frame's glows before choosing the nearest ones: disabling/reducing the budget takes effect now.
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key <= lightKey && dl.key > lightKey - capacity) { dl.radius = 0.f; dl.die = 0.f; }
    }
    counters.lights = 0;
    bool chosen[capacity]{};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const int count = static_cast<int>(za::clamp(vr_explosion_debris_lights.value, 0.f, 16.f));
    const float radius = za::clamp(vr_explosion_debris_light_radius.value, 0.f, 200.f) * units::worldScale();
    if(radius <= 0.f) { return; }
    for(int n = 0; n < count; ++n)
    {
        int nearest = -1;
        float distance = 1e30f;
        for(int i = 0; i < capacity; ++i)
        {
            if(!chunks[i].active || chosen[i]) { continue; }
            const glm::vec3 delta = chunks[i].pos - eye;
            const float d = glm::dot(delta, delta);
            if(d < distance) { distance = d; nearest = i; }
        }
        if(nearest < 0) { break; }
        chosen[nearest] = true;
        const Chunk& c = chunks[nearest];
        const float heat = za::clamp(float((c.die - cl.time) / za::min(1.5, c.die - c.born)), 0.f, 1.f);
        dlight_t* dl = CL_AllocDlight(lightKey - nearest);
        for(int k = 0; k < 3; ++k) { dl->origin[k] = c.pos[k]; }
        dl->radius = radius * heat;
        dl->die = cl.time + 0.05;
        dl->color[0] = 1.f;
        dl->color[1] = vr_colored_lights.value ? 0.35f : 1.f;
        dl->color[2] = vr_colored_lights.value ? 0.05f : 1.f;
        lighting::dlightLook(dl, 0.f, 0.f);
        lighting::dlightNoShadow(dl);
        ++counters.lights;
    }
}

void stats_f()
{
    Con_Printf("explosiondebris: live=%d made=%d expired=%d evicted=%d bounces=%d trails=%d lights=%d rendered=%d speed=%.3f..%.3f life=%.3f..%.3f health=%d fireballs=%d\n",
        liveCount(), counters.made, counters.expired, counters.evicted, counters.bounced, counters.trailSamples,
        counters.lights, counters.rendered, counters.made ? counters.minSpeed : 0.f, counters.maxSpeed,
        counters.made ? counters.minLife : 0.f, counters.maxLife, cl.stats[STAT_HEALTH], particles::largeExplosionCount());
}
void test_f()
{
    if(cls.state != ca_connected || !cl.worldmodel) { return; }
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "clear")) { clear(); return; }
    vec3_t forward, right, up;
    AngleVectors(r_refdef.viewangles, forward, right, up);
    vec3_t pos;
    for(int k = 0; k < 3; ++k) { pos[k] = r_refdef.vieworg[k] + forward[k] * 64.f; }
    Con_DPrintf("explosiondebris test: at %.1f %.1f %.1f, flags=%u enabled=%.0f\n", pos[0], pos[1], pos[2], cl.protocolflags, vr_explosion_debris.value);
    const char* kind = Cmd_Argc() > 1 ? Cmd_Argv(1) : "normal";
    if(!strcmp(kind, "colored")) { R_ParticleExplosion2(pos, 229, 8); }
    else if(!strcmp(kind, "tar")) { R_BlobExplosion(pos); }
    else if(!strcmp(kind, "preset")) { particles::spawn({pos[0], pos[1], pos[2]}, glm::vec3{0.f}, particles::Preset::Explosion, 1); }
    else { R_ParticleExplosion(pos); }
}
} // namespace

int liveCount()
{
    int n = 0;
    for(const Chunk& c : chunks) { n += c.active ? 1 : 0; }
    return n;
}
void spawn(const glm::vec3& org)
{
    if(!vr_explosion_debris.value || !(cl.protocolflags & PRFL_QUAKEVR) || !cl.worldmodel) { return; }
    trim(limit());
    const int count = za::min(static_cast<int>(za::clamp(vr_explosion_debris_count.value, 0.f, 64.f)), limit());
    const float a = za::clamp(vr_explosion_debris_speed_min.value, 0.f, 40.f);
    const float b = za::clamp(vr_explosion_debris_speed_max.value, 0.f, 40.f);
    const float lo = za::min(a, b), hi = za::max(a, b);
    const float la = za::clamp(vr_explosion_debris_life_min.value, 0.1f, 30.f);
    const float lb = za::clamp(vr_explosion_debris_life_max.value, 0.1f, 30.f);
    const float sa = za::clamp(vr_explosion_debris_size_min.value, 0.8f, 8.f);
    const float sb = za::clamp(vr_explosion_debris_size_max.value, 0.8f, 8.f);
    for(int n = 0; n < count; ++n)
    {
        Chunk c;
        c.size = rnd(za::min(sa, sb), za::max(sa, sb)) * units::worldScale();
        const glm::vec3 dir = direction();
        c.pos = org;
        // A blast exactly at a wall/floor: move towards a free launch point before giving up on this chunk.
        bool free = false;
        for(int attempt = 0; attempt < 6; ++attempt)
        {
            const glm::vec3 candidate = org + dir * (c.size + attempt * 2.f * units::worldScale());
            const trace_t route = worldtrace::world(org, candidate, true, true);
            if(!route.startsolid && route.fraction < 1.f) { break; }
            const trace_t tr = sweep(candidate, candidate, c.size * 0.5f);
            if(!tr.startsolid && !tr.allsolid) { c.pos = candidate; free = true; break; }
        }
        if(!free) { continue; }
        if(liveCount() >= limit()) { retire(oldest()); ++counters.evicted; }
        int slot = 0;
        while(chunks[slot].active) { ++slot; }
        const float speed = rnd(lo, hi), life = rnd(za::min(la, lb), za::max(la, lb));
        c.active = true;
        c.born = cl.time;
        c.die = cl.time + life;
        c.vel = dir * speed * units::metresToUnits();
        c.angles = {rnd(0.f, 360.f), rnd(0.f, 360.f), rnd(0.f, 360.f)};
        c.spin = {rnd(-600.f, 600.f), rnd(-600.f, 600.f), rnd(-600.f, 600.f)};
        chunks[slot] = c;
        ++counters.made;
        counters.minSpeed = za::min(counters.minSpeed, speed); counters.maxSpeed = za::max(counters.maxSpeed, speed);
        counters.minLife = za::min(counters.minLife, life); counters.maxLife = za::max(counters.maxLife, life);
    }
}
void frame()
{
    if(lastFrame == host_framecount) { return; }
    lastFrame = host_framecount;
    const float elapsed = lastRun >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - lastRun, 0.1)) : 0.f;
    lastRun = cl.time;
    trim(vr_explosion_debris.value ? limit() : 0);
    qmodel_t* model = liveCount() ? Mod_ForName(modelName, false) : nullptr;
    counters.rendered = 0;
    for(int i = 0; i < capacity; ++i)
    {
        Chunk& c = chunks[i];
        if(!c.active) { continue; }
        if(cl.time >= c.die || cl.time < c.born) { retire(i); ++counters.expired; continue; }
        float remaining = elapsed;
        while(remaining > 1e-5f)
        {
            const float dt = za::min(remaining, 1.f / 120.f);
            advance(c, dt);
            remaining -= dt;
        }
        scene(c, model);
    }
    lights();
}
void prepare() { (void)Mod_ForName(modelName, false); }
void clear()
{
    for(int i = 0; i < capacity; ++i) { retire(i); }
    lastRun = -1.0; lastFrame = -1; counters = Counters{};
    rng = za::FastNonCryptoRng{vr_particle_seed.value > 0.f ? static_cast<za::U64>(vr_particle_seed.value) : static_cast<za::U64>(za::Clock::nowNanoseconds())};
}
void registerCommands()
{
    Cmd_AddCommand("vr_explosion_debris_test", test_f);
    Cmd_AddCommand("vr_explosion_debris_stats", stats_f);
}
} // namespace qvr::explosiondebris
