// vr_fgfx.cpp -- see vr_fgfx.hpp.

#include "vr_fgfx.hpp"
#include "vr_cvars.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_particles.hpp"
#include "vr_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace qvr::fgfx
{
namespace
{

enum State : int
{
    None,
    Aimed,
    Locked,
    Flying
};

struct Target
{
    int ent{0};
    int state{None};
};

std::unordered_map<int, float> glows; // entity -> current glow
double lastTime = -1.0;

[[nodiscard]] Target target(int stat)
{
    const int v = cl.stats[stat];
    return {v >> 2, v & 3};
}

[[nodiscard]] bool valid(int ent)
{
    return ent > 0 && ent < cl.num_entities && cl_entities[ent].model;
}

// The middle of an entity as drawn: its model's box, turned with it, with the networked scale and
// offset and the weapon scaling (dropped weapons and backpacks are drawn well off their origin).
[[nodiscard]] glm::vec3 centre(int ent)
{
    return held::drawnCentre(ent);
}

// A small hash noise, -1..1.
[[nodiscard]] float noise(int a, int b)
{
    unsigned h = static_cast<unsigned>(a) * 73856093u ^ static_cast<unsigned>(b) * 19349663u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xFFFF) / 32767.5f - 1.f;
}

// A crackling tendril from `a` to `b`: jittered segments, redrawn a few dozen times a second, a
// bright core inside a wide soft glow, bulging in the middle.
void tendril(const glm::vec3& a, const glm::vec3& b, float strength, int seed)
{
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    if(len < 1.f)
    {
        return;
    }
    const glm::vec3 dir = d / len;
    const glm::vec3 side1 = glm::normalize(glm::cross(dir, std::fabs(dir.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0}));
    const glm::vec3 side2 = glm::cross(dir, side1);

    const int tick = static_cast<int>(realtime * 24.0);
    const int segments = std::clamp(static_cast<int>(len / 6.f), 6, 24);
    const float amplitude = std::min(4.f, len * 0.06f);
    for(int strand = 0; strand < 2; strand++)
    {
        glm::vec3 prev = a;
        for(int i = 1; i <= segments; i++)
        {
            const float t = static_cast<float>(i) / segments;
            const float bulge = std::sin(t * 3.14159265f) * amplitude * (strand == 0 ? 1.f : 0.6f);
            glm::vec3 p = a + d * t;
            if(i < segments)
            {
                p += side1 * (noise(tick + seed * 31 + strand * 7, i) * bulge) +
                     side2 * (noise(tick + seed * 17 + strand * 3, i + 97) * bulge);
            }
            const float fade = strength * (strand == 0 ? 1.f : 0.5f);
            const glm::vec4 halo = glm::vec4{0.15f, 0.35f, 0.9f, 1.f} * (0.22f * fade);
            const glm::vec4 core = glm::vec4{0.7f, 0.9f, 1.f, 1.f} * (0.9f * fade);
            lines::glow(prev, p, 1.4f, halo, halo);
            lines::glow(prev, p, 0.35f, core, core);
            prev = p;
        }
    }
    lines::glowPoint(a, 2.5f * strength, glm::vec4{0.4f, 0.7f, 1.f, 1.f} * (0.5f * strength));
}

} // namespace

void queue(const hands::State& s)
{
    const double now = cl.time;
    const float dt = lastTime < 0.0 ? 0.f : static_cast<float>(std::clamp(now - lastTime, 0.0, 0.1));
    lastTime = now;

    // The glows fade towards each hand's target: aimed at, softly; locked on or flying, fully.
    Target targets[2] = {target(protocol::STAT_QVR_FGOFF), target(protocol::STAT_QVR_FGMAIN)};
    // A hand holding the flashlight does not force grab (the server is told; this spares the
    // round trip).
    for(int hand = 0; hand < 2; hand++)
    {
        if(flashlight::holds(hand))
        {
            targets[hand] = Target{};
        }
    }
    const auto wanted = [](const Target& t) { return t.state != None && valid(t.ent); };
    const auto goalOf = [&](int ent) {
        float goal = 0.f;
        for(const Target& t : targets)
        {
            if(t.ent == ent && wanted(t))
            {
                goal = std::max(goal, t.state == Aimed ? 0.55f : 1.f);
            }
        }
        return goal;
    };
    for(const Target& t : targets)
    {
        if(wanted(t))
        {
            glows.try_emplace(t.ent, 0.f);
        }
    }
    const float k = 1.f - std::exp(-dt * 8.f);
    for(auto it = glows.begin(); it != glows.end();)
    {
        const float goal = goalOf(it->first);
        it->second += (goal - it->second) * k;
        if(goal == 0.f && it->second < 0.01f)
        {
            it = glows.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // The beams, from the palm.
    if(!s.valid || !vr_forcegrab_fx.value)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        const Target& t = targets[hand];
        if(!wanted(t))
        {
            continue;
        }
        const glm::vec3 palm = s.pos[hand] + hands::forward(s.rot[hand]) * 2.f;
        const glm::vec3 to = centre(t.ent);
        if(t.state == Aimed)
        {
            lines::glow(palm, to, 0.3f, glm::vec4{0.3f, 0.6f, 1.f, 1.f} * 0.18f, glm::vec4{0.3f, 0.6f, 1.f, 1.f} * 0.03f);
            continue;
        }
        const float pulse = 0.85f + 0.15f * std::sin(static_cast<float>(realtime) * 17.f);
        tendril(palm, to, (t.state == Flying ? 1.f : 0.75f) * pulse, t.ent + hand * 1000);
        if(t.state == Flying && dt > 0.f)
        {
            particles::spawn(to, glm::vec3{0.f}, particles::Preset::ForceGrabTrail, 2);
        }
    }
}

float entityGlow(const entity_t* e)
{
    if(!e || e < cl_entities || e >= cl_entities + cl.num_entities || glows.empty())
    {
        return 0.f;
    }
    const auto it = glows.find(static_cast<int>(e - cl_entities));
    if(it == glows.end())
    {
        return 0.f;
    }
    const float breathe = 0.88f + 0.12f * std::sin(static_cast<float>(realtime) * 4.f);
    return std::clamp(it->second * breathe * vr_forcegrab_outline.value, 0.f, 1.f);
}

} // namespace qvr::fgfx

extern "C" float VR_EntityGlow(const entity_t* e)
{
    return qvr::fgfx::entityGlow(e);
}
