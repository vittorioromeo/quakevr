// vr_brainjiggle.cpp -- see vr_brainjiggle.hpp.

#include "vr_brainjiggle.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>

namespace qvr::brainjiggle
{
namespace
{

constexpr float omega = 30.f;          // the spring's own frequency, radians a second (about 4.8 Hz: a soft lump)
constexpr float damping = 0.22f;       // its damping ratio: two or three overshoots, settled in about half a second
constexpr float kick = 2.5f;           // the spring's rate (squash a second) a m/s of the change past the threshold
constexpr float mostSquash = 0.3f;     // the most it squashes (a share of its size along the change), and
constexpr float mostStretch = 0.2f;    // stretches
constexpr float jumpSpeed = 25.f;      // m/s: faster is a jump (spawned, teleported, its slot reused): no kick
constexpr float longestGap = 0.25f;    // seconds: a chunk not drawn for longer starts over (no kick from the gap)
constexpr float smoothing = 0.02f;     // seconds: its speed smoothed over about this (the drawn motion's jitter no kick)
constexpr float stillSquash = 1e-3f;   // a spring this still is at rest (drawn as is)
constexpr float stillRate = 1e-2f;
constexpr float subStep = 1.f / 120.f; // seconds: the spring's longest step
constexpr double forgetAfter = 1.0;    // seconds: a chunk not drawn for this long is forgotten

struct Spring
{
    glm::vec3 pos{0.f}, vel{0.f};  // where it was drawn last, its smoothed speed then (units a second)
    glm::vec3 axis{0.f, 0.f, 1.f}; // the squash's direction
    float squash{0.f}, rate{0.f};  // the squash (a share of its size along axis; negative: stretched), its rate
    double time{-1.0};             // cl.time of pos
    int samples{0};                // positions seen (its speed known from the second, its change from the third)
};

struct Springs
{
    ankerl::unordered_dense::map<std::uintptr_t, Spring> springs;
    auto members() { return mem::list(springs); }
};
mem::Cache<Springs> cache{"brain jiggle", mem::MapChange | mem::GameDirChange | mem::ModelReload};

double lastPrune = -1.0; // cl.time the forgotten chunks were last dropped
int kicks = 0;           // since vr_brain_jiggle_info: kicks given, and
float peak = 0.f;        // the most squash drawn

[[nodiscard]] bool isBrain(const entity_t* e)
{
    return e && e->model && e->model->type == mod_alias && strncmp(e->model->name, "progs/gib_brain", 15) == 0;
}

void step(Spring& s, float dt)
{
    while(dt > 0.f)
    {
        const float h = za::min(dt, subStep);
        s.rate += (-omega * omega * s.squash - 2.f * damping * omega * s.rate) * h;
        s.squash = za::clamp(s.squash + s.rate * h, -mostStretch, mostSquash);
        dt -= h;
    }
}

void restart(Spring& s, const glm::vec3& pos, double now)
{
    s.pos = pos;
    s.vel = glm::vec3{0.f};
    s.time = now;
    s.samples = 1;
}

// `e`'s spring stepped to cl.time, kicked by its speed's change since its last drawing.
Spring& update(const entity_t* e, float strength)
{
    const double now = cl.time;
    if(now < lastPrune || now - lastPrune > forgetAfter)
    {
        if(now >= lastPrune)
        {
            za::Vector<std::uintptr_t> stale;
            for(const auto& [key, s] : cache.springs)
            {
                if(now - s.time > forgetAfter)
                {
                    stale.pushBack(key);
                }
            }
            for(const std::uintptr_t key : stale)
            {
                cache.springs.erase(key);
            }
        }
        lastPrune = now;
    }
    Spring& s = cache.springs[reinterpret_cast<std::uintptr_t>(e)];
    if(s.time == now)
    {
        return s;
    }
    const glm::vec3 pos{e->origin[0], e->origin[1], e->origin[2]};
    const float dt = static_cast<float>(now - s.time);
    if(s.samples == 0 || dt <= 0.f || dt > longestGap)
    {
        restart(s, pos, now);
        return s;
    }
    const float perMetre = units::metresToUnits();
    const glm::vec3 raw = (pos - s.pos) / dt;
    if(glm::length(raw) > jumpSpeed * perMetre)
    {
        restart(s, pos, now);
        return s;
    }
    step(s, dt);
    const glm::vec3 vel = s.samples < 2 ? raw : glm::mix(s.vel, raw, 1.f - za::exp(-dt / smoothing));
    if(s.samples >= 2)
    {
        const glm::vec3 dv = vel - s.vel;
        const float change = glm::length(dv) / perMetre; // m/s
        const float threshold = za::max(0.f, vr_smallgibs_brains_jiggle_speed.value);
        if(change > threshold && change > 1e-4f)
        {
            // Along the new change (a wobble still going folded onto it: one spring, one axis).
            s.axis = dv / glm::length(dv);
            s.rate += kick * strength * (change - threshold);
            kicks++;
        }
    }
    s.vel = vel;
    s.pos = pos;
    s.time = now;
    s.samples = za::min(s.samples + 1, 3);
    return s;
}

void info_f()
{
    int wobbling = 0;
    for(const auto& [key, s] : cache.springs)
    {
        wobbling += za::fabs(s.squash) > stillSquash || za::fabs(s.rate) > stillRate ? 1 : 0;
    }
    Con_Printf("brain jiggle: %d chunks followed, %d wobbling; %d kicks, the most squash %.3f since the last\n",
        static_cast<int>(cache.springs.size()), wobbling, kicks, static_cast<double>(peak));
    kicks = 0;
    peak = 0.f;
}

} // namespace

void squashModel(const entity_t* e, float matrix[16])
{
    const float strength = za::clamp(vr_smallgibs_brains_jiggle.value, 0.f, 4.f);
    if(strength <= 0.f || !isBrain(e))
    {
        return;
    }
    const Spring& s = update(e, strength);
    if(za::fabs(s.squash) <= stillSquash)
    {
        return;
    }
    peak = za::max(peak, s.squash);
    // Squashed along the axis by `squash`, bulged across it as much as keeps its volume, about its origin (its box's
    // middle: make_brains.py).
    const float along = 1.f - s.squash;
    const float across = 1.f / glm::sqrt(along);
    const glm::mat3 squash = glm::mat3{across} + (along - across) * glm::outerProduct(s.axis, s.axis);
    const glm::vec3 o{e->origin[0], e->origin[1], e->origin[2]};
    const glm::mat4 m = glm::translate(glm::mat4{1.f}, o) * glm::mat4{squash} * glm::translate(glm::mat4{1.f}, -o) *
                        glm::make_mat4(matrix);
    memcpy(matrix, glm::value_ptr(m), sizeof(float) * 16);
}

void registerCommands()
{
    Cmd_AddCommand("vr_brain_jiggle_info", info_f);
}

} // namespace qvr::brainjiggle
