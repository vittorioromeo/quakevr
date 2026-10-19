// vr_flamewobble.cpp -- see vr_flamewobble.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_flamewobble.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>

namespace qvr::flamewobble
{
namespace
{

constexpr float omega = 8.8f;          // the spring's own frequency, radians a second (1.4 Hz)
constexpr float damping = 0.45f;       // its damping ratio: a little overshoot back (a fifth), settled in about a second
constexpr float push = 16.f;           // a push's acceleration (lean a second squared) a m/s of excess speed, at the flame
constexpr float mostLean = 0.8f;       // the most a flame leans (sideways units a unit up: 39 degrees)
constexpr float reachBase = 5.f;       // units: a hand pushes a flame within this of its axis, and
constexpr float reachPerHeight = 0.25f; // this share of its height more
constexpr float farthest = 64.f;       // units: a flame model's origin farther than this from every fast hand is not looked at
constexpr float stillLean = 1e-3f;     // a spring this still is dropped
constexpr float stillRate = 1e-2f;
constexpr float gustReach = 14.f;      // units round a fast hand the fire and smoke particles are carried along
constexpr float gustShare = 0.5f;      // at this share of the hand's speed
constexpr float jumpSpeed = 15.f;      // m/s: faster is a jump (a teleport, tracking lost and found): no push
constexpr float subStep = 1.f / 120.f; // seconds: the springs' longest step
constexpr float handSmooth = 0.03f;    // seconds: the hands' speeds smoothed over about this (a frame's jitter is no push)
constexpr std::uintptr_t sourceBit = std::uintptr_t{1} << (sizeof(std::uintptr_t) * 8 - 1); // sources' keys (not pointers)

struct Wobble
{
    glm::vec2 lean{0.f}, rate{0.f};
    glm::vec2 force{0.f};   // this frame's push
    int pushedFrame{-1};    // host_framecount it was last pushed in
    double pushedTime{-1.0}; // cl.time ...
    float peak{0.f};        // the most it leant since vr_flame_wobble_info
};

struct Source
{
    std::uintptr_t key;
    glm::vec3 foot;
    float height;
    int carrier;
};

struct Springs
{
    ankerl::unordered_dense::map<std::uintptr_t, Wobble> wobbles;
    za::Vector<Source> sources;     // the flames drawn by their own code, registered since the last frame()
    za::Vector<Source> lastSources; // ... the last frame's (vr_flame_wobble_info)
    auto members() { return mem::list(wobbles, sources, lastSources); }
};
mem::Cache<Springs> springs{"flame wobble", mem::MapChange | mem::GameDirChange | mem::ModelReload};

struct Hand
{
    glm::vec3 pos{0.f};  // world
    glm::vec3 room{0.f}; // in the play space (its own motion left out)
    glm::vec3 vel{0.f};  // m/s, world axes, the play space's motion left out
    float speed{0.f};    // m/s, horizontal
    bool fast{false};    // past vr_flame_wobble_speed: it pushes
};
Hand handNow[2];
bool roomKnown = false;
double lastTime = -1.0;
int lastFrame = -1;
int pushes = 0, looked = 0; // since vr_flame_wobble_info

[[nodiscard]] float strength()
{
    return za::clamp(vr_flame_wobble_strength.value, 0.f, 4.f);
}

// A map flame model (progs/flame2.mdl, which has no stick): its frame's foot and top along its own z (model units).
[[nodiscard]] bool flameModel(const entity_t& e, float& base, float& top)
{
    if(!e.model || e.model->type != mod_alias || e.alpha == ENTALPHA_ZERO || !modelmeta::is(e.model, modelmeta::Id::Flame2))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numframes <= 0)
    {
        return false;
    }
    const auto& frame = hdr->frames[za::clamp(int(e.frame), 0, hdr->numframes - 1)];
    base = hdr->scale_origin[2] + hdr->scale[2] * frame.bboxmin.v[2];
    top = hdr->scale_origin[2] + hdr->scale[2] * frame.bboxmax.v[2];
    return top > base;
}

[[nodiscard]] float distanceToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::distance(p, a + ab * t);
}

void describe(std::uintptr_t key, char* out, int size)
{
    const auto* e = reinterpret_cast<const entity_t*>(key);
    if(key & sourceBit)
    {
        const unsigned k = static_cast<unsigned>(key & 0xffffffffu);
        q_snprintf(out, size, k >= 0x30000u ? "pilot light %u" : k >= 0x20000u ? "wall torch %u" : "source %u", k & 0xffffu);
    }
    else if(e >= cl_static_entities && e < cl_static_entities + MAX_STATIC_ENTITIES)
    {
        q_snprintf(out, size, "static flame %d", static_cast<int>(e - cl_static_entities));
    }
    else if(cl_entities && e >= cl_entities && e < cl_entities + cl.num_entities)
    {
        q_snprintf(out, size, "flame entity %d", static_cast<int>(e - cl_entities));
    }
    else
    {
        q_snprintf(out, size, "flame %p", static_cast<const void*>(e));
    }
}

// The fast hands near a flame from `foot` to `tip`: their push on its spring this frame.
void pushFlame(std::uintptr_t key, const glm::vec3& foot, const glm::vec3& tip, float height, int carrier)
{
    looked++;
    const float threshold = za::max(0.f, vr_flame_wobble_speed.value);
    const float reach = reachBase + reachPerHeight * za::max(0.f, height);
    glm::vec2 force{0.f};
    int by = -1;
    float nearest = 0.f;
    for(int h = 0; h < 2; h++)
    {
        const Hand& hd = handNow[h];
        if(h == carrier || !hd.fast)
        {
            continue;
        }
        const float d = distanceToSegment(hd.pos, foot, tip);
        if(d >= reach)
        {
            continue;
        }
        const float w = 1.f - d / reach;
        const glm::vec2 dir = glm::vec2{hd.vel.x, hd.vel.y} / hd.speed;
        force += dir * (push * strength() * w * (hd.speed - threshold));
        by = h;
        nearest = d;
    }
    if(by < 0)
    {
        return;
    }
    Wobble& wb = springs.wobbles[key];
    if(vr_debug_flame_wobble.value >= 1.f && (wb.pushedTime < 0.0 || cl.time - wb.pushedTime > 0.1 || cl.time < wb.pushedTime))
    {
        char what[48];
        describe(key, what, sizeof(what));
        Con_Printf("flame wobble: %s pushed by the %s hand, %.1f units off its axis (reach %.1f), %.2f m/s\n", what,
            by == HAND_MAIN ? "main" : "off", nearest, reach, handNow[by].speed);
    }
    wb.force += force;
    wb.pushedFrame = host_framecount;
    wb.pushedTime = cl.time;
    pushes++;
}

void readHands(const hands::State& s, float dt)
{
    const float threshold = za::max(0.f, vr_flame_wobble_speed.value);
    const float perMetre = units::metresToUnits();
    for(int h = 0; h < 2; h++)
    {
        Hand& hd = handNow[h];
        const glm::vec3 room = hands::rotateYaw(s.pos[h] - s.spaceOrigin, -s.turnYaw);
        if(!s.valid || !roomKnown)
        {
            hd.vel = glm::vec3{0.f};
        }
        else if(dt > 0.f)
        {
            const glm::vec3 raw = hands::rotateYaw((room - hd.room) / (dt * perMetre), s.turnYaw);
            hd.vel = glm::length(raw) > jumpSpeed ? glm::vec3{0.f} : glm::mix(hd.vel, raw, 1.f - za::exp(-dt / handSmooth));
        }
        hd.pos = s.pos[h];
        hd.room = room;
        hd.speed = glm::length(glm::vec2{hd.vel.x, hd.vel.y});
        hd.fast = hd.speed > threshold && hd.speed > 1e-3f;
    }
    roomKnown = s.valid;
}

// The map's flame models near a fast hand (the statics and the live entities).
void pushModels()
{
    const auto look = [](const entity_t& e) {
        const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
        const float scale = ENTSCALE_DECODE(e.scale);
        const float within = farthest * za::max(1.f, scale);
        if((!handNow[0].fast || glm::distance(origin, handNow[0].pos) > within) &&
            (!handNow[1].fast || glm::distance(origin, handNow[1].pos) > within))
        {
            return;
        }
        float base = 0.f, top = 0.f;
        if(!flameModel(e, base, top))
        {
            return;
        }
        const glm::vec3 up = held::axesFromAngles(e.angles, false)[2];
        pushFlame(reinterpret_cast<std::uintptr_t>(&e), origin + up * (base * scale), origin + up * (top * scale),
            (top - base) * scale, -1);
    };
    for(int i = 0; i < cl.num_statics; i++)
    {
        look(cl_static_entities[i]);
    }
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(e.msgtime == cl.mtime[0])
        {
            look(e);
        }
    }
}

void step(float dt)
{
    const int n = dt > 0.f ? za::clamp(static_cast<int>(za::ceil(dt / subStep)), 1, 16) : 0;
    const float h = n > 0 ? dt / static_cast<float>(n) : 0.f;
    for(auto& [key, wb] : springs.wobbles)
    {
        for(int i = 0; i < n; i++)
        {
            const glm::vec2 acc = wb.force - (omega * omega) * wb.lean - (2.f * damping * omega) * wb.rate;
            wb.rate += acc * h;
            wb.lean += wb.rate * h;
            if(const float l = glm::length(wb.lean); l > mostLean)
            {
                const glm::vec2 out = wb.lean / l;
                wb.lean = out * mostLean;
                wb.rate -= out * za::max(0.f, glm::dot(wb.rate, out)); // (no further out)
            }
        }
        wb.force = glm::vec2{0.f};
        wb.peak = za::max(wb.peak, glm::length(wb.lean));
        if(vr_debug_flame_wobble.value >= 2.f)
        {
            char what[48];
            describe(key, what, sizeof(what));
            Con_Printf("flame wobble: t %.3f %s lean %.3f %.3f (%.3f)\n", cl.time, what, wb.lean.x, wb.lean.y,
                glm::length(wb.lean));
        }
    }
    erase_if(springs.wobbles, [](const auto& pair) {
        const Wobble& wb = pair.second;
        return wb.pushedFrame != host_framecount && glm::length(wb.lean) < stillLean && glm::length(wb.rate) < stillRate;
    });
}

void info_f()
{
    Con_Printf("flame wobble: %s, strength %.2f, from %.2f m/s; %d swinging, %d pushes over %d flames looked at since the last\n",
        vr_flame_wobble.value ? "on" : "off", strength(), vr_flame_wobble_speed.value, static_cast<int>(springs.wobbles.size()),
        pushes, looked);
    pushes = looked = 0;
    for(int h = 0; h < 2; h++)
    {
        Con_Printf("  %s hand at %.1f %.1f %.1f, %.2f m/s%s\n", h == HAND_MAIN ? "main" : "off", handNow[h].pos.x,
            handNow[h].pos.y, handNow[h].pos.z, handNow[h].speed, handNow[h].fast ? " (pushing)" : "");
    }
    for(auto& [key, wb] : springs.wobbles)
    {
        char what[48];
        describe(key, what, sizeof(what));
        Con_Printf("  %s: lean %.3f %.3f, the most %.3f\n", what, wb.lean.x, wb.lean.y, wb.peak);
        wb.peak = 0.f;
    }
    // The nearest flame model and wall torch to you (for tests: where to sweep a hand).
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    float best = 1e9f;
    glm::vec3 foot{0.f}, tip{0.f};
    const char* kind = nullptr;
    int index = -1;
    const auto consider = [&](const entity_t& e, const char* k, int i) {
        float base = 0.f, top = 0.f;
        const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
        if(flameModel(e, base, top) && glm::distance(origin, eye) < best)
        {
            const float scale = ENTSCALE_DECODE(e.scale);
            const glm::vec3 up = held::axesFromAngles(e.angles, false)[2];
            best = glm::distance(origin, eye);
            foot = origin + up * (base * scale);
            tip = origin + up * (top * scale);
            kind = k;
            index = i;
        }
    };
    for(int i = 0; i < cl.num_statics; i++)
    {
        consider(cl_static_entities[i], "static flame", i);
    }
    for(int i = 1; i < cl.num_entities; i++)
    {
        if(cl_entities[i].msgtime == cl.mtime[0])
        {
            consider(cl_entities[i], "flame entity", i);
        }
    }
    if(kind)
    {
        Con_Printf("  nearest flame model: %s %d, %.0f units off, foot %.1f %.1f %.1f, top %.1f %.1f %.1f\n", kind, index,
            best, foot.x, foot.y, foot.z, tip.x, tip.y, tip.z);
    }
    best = 1e9f;
    const Source* closest = nullptr;
    for(const Source& src : springs.lastSources)
    {
        if(glm::distance(src.foot, eye) < best)
        {
            best = glm::distance(src.foot, eye);
            closest = &src;
        }
    }
    if(closest)
    {
        char what[48];
        describe(closest->key, what, sizeof(what));
        Con_Printf("  nearest of %d drawn flames: %s, %.0f units off, foot %.1f %.1f %.1f, height %.1f\n",
            static_cast<int>(springs.lastSources.size()), what, best, closest->foot.x, closest->foot.y, closest->foot.z, closest->height);
    }
}

} // namespace

void frame()
{
    if(lastFrame == host_framecount)
    {
        return;
    }
    lastFrame = host_framecount;
    if(!vr_flame_wobble.value || !cl.worldmodel || cls.state != ca_connected)
    {
        springs.wobbles.clear();
        springs.sources.clear();
        roomKnown = false;
        lastTime = -1.0;
        return;
    }
    const double now = cl.time;
    const float dt = lastTime >= 0.0 ? static_cast<float>(za::clamp(now - lastTime, 0.0, 0.1)) : 0.f;
    lastTime = now;
    readHands(hands::current(), dt);
    if((handNow[0].fast || handNow[1].fast) && strength() > 0.f && dt > 0.f)
    {
        pushModels();
        for(const Source& src : springs.sources)
        {
            pushFlame(src.key, src.foot, src.foot + glm::vec3{0.f, 0.f, src.height}, src.height, src.carrier);
        }
        // The air the hand moves carries the fire's and the smoke's particles near it.
        for(int h = 0; h < 2; h++)
        {
            if(handNow[h].fast)
            {
                particles::gust(handNow[h].pos, gustReach, handNow[h].vel * (units::metresToUnits() * gustShare * strength()), dt);
            }
        }
    }
    step(dt);
    springs.lastSources.clear();
    for(const Source& src : springs.sources)
    {
        springs.lastSources.pushBack(src); // (for vr_flame_wobble_info)
    }
    springs.sources.clear();
}

glm::vec2 source(std::uint32_t key, const glm::vec3& foot, float height, int carrier)
{
    if(!vr_flame_wobble.value)
    {
        return glm::vec2{0.f};
    }
    const std::uintptr_t k = sourceBit | key;
    if(springs.sources.size() < 512)
    {
        springs.sources.pushBack(Source{k, foot, height, carrier});
    }
    const auto it = springs.wobbles.find(k);
    return it != springs.wobbles.end() ? it->second.lean : glm::vec2{0.f};
}

void leanModel(const entity_t* e, float matrix[16])
{
    if(springs.wobbles.empty() || !e)
    {
        return;
    }
    const auto it = springs.wobbles.find(reinterpret_cast<std::uintptr_t>(e));
    float base = 0.f, top = 0.f;
    if(it == springs.wobbles.end() || !flameModel(*e, base, top))
    {
        return;
    }
    // Sheared over its foot in the world: each point moved sideways by the lean times its height over the foot (an
    // upside-down flame's the other way, so that it still leans the way the air went).
    const float scale = ENTSCALE_DECODE(e->scale);
    const glm::vec3 up = held::axesFromAngles(e->angles, false)[2];
    const glm::vec3 foot = glm::vec3{e->origin[0], e->origin[1], e->origin[2]} + up * (base * scale);
    const glm::vec2 lean = it->second.lean * (up.z < 0.f ? -1.f : 1.f);
    glm::mat4 shear{1.f};
    shear[2][0] = lean.x;
    shear[2][1] = lean.y;
    const glm::mat4 m = glm::translate(glm::mat4{1.f}, foot) * shear * glm::translate(glm::mat4{1.f}, -foot) * glm::make_mat4(matrix);
    memcpy(matrix, glm::value_ptr(m), sizeof(float) * 16);
}

void clear()
{
    springs.wobbles.clear();
    springs.sources.clear();
    roomKnown = false;
    lastTime = -1.0;
    lastFrame = -1;
}

void registerCommands()
{
    Cmd_AddCommand("vr_flame_wobble_info", info_f);
}

} // namespace qvr::flamewobble
