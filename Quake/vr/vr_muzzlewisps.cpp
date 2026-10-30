// vr_muzzlewisps.cpp -- see vr_muzzlewisps.hpp.

#include "vr_muzzlewisps.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_portals.hpp"
#include "vr_profile.hpp"
#include "vr_ribbons.hpp"
#include "vr_units.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"

#include <math.h>

namespace qvr::muzzlewisps
{
namespace
{

constexpr int kMaxNodes = 128;           // points a barrel's trail keeps (the oldest let go first)
constexpr float kEmitInterval = 1.f / 30.f; // seconds between the points let go at the muzzle
constexpr float kEmitDistance = 2.5f;    // ... or units the muzzle moved
constexpr float kEmerge = 0.12f;         // seconds a point takes to show (out of the barrel softly)
constexpr float kRelax = 1.5f;           // a second: how fast a point takes to the air's rise (from its puff forward)
constexpr float kPuff = 1.5f;            // units a second a point leaves the muzzle forward at
constexpr float kFlowScale = 0.75f;      // the swirling flow's: radians a unit (about 8 units a swirl)
constexpr float kFlowSpeed = 1.f;        // ... and a second (it changes slowly)
constexpr float kFlowAccel = 9.f;        // units a second squared at vr_gunfx_turbulence 1
constexpr float kSway = 1.6f;            // units a second the stream let go sways at (the air wavering at the muzzle)
constexpr float kJump = 64.f;           // a muzzle further than this from its last point: a new trail (a teleport)

// A point of smoke let go at a muzzle.
struct Node
{
    glm::vec3 pos{0.f};
    float age{0.f}; // seconds
    glm::vec3 vel{0.f};
    float strength{0.f}; // its opacity at full (the gun's thickness times the barrel's smoke then)
    bool first{false};   // the first of a trail (the muzzle let go of the last before it)
};

// A barrel's trail: a ring of points, the oldest at `first`.
struct Strand
{
    Node nodes[kMaxNodes];
    int first{0};
    int count{0};
    float phase{0.f}; // its own offset in the flow
    bool restart{true}; // the next point starts a new trail

    [[nodiscard]] Node& at(int i) { return nodes[(first + i) % kMaxNodes]; }
    void push(const Node& n)
    {
        if(count == kMaxNodes)
        {
            first = (first + 1) % kMaxNodes;
            count--;
        }
        nodes[(first + count) % kMaxNodes] = n;
        count++;
    }
};

struct HandWisps
{
    Strand strands[maxBarrels];
    Gun gun;
    int seenFrame{-1};     // host_framecount of the last handFrame
    double lastShot{-1.0}; // cl.time
    float charge{0.f};     // 0 .. 1: the smoke the shots left in the barrels
    bool emitting{false};
    float sinceEmit{0.f};
    glm::vec3 lastEmitAt{0.f};
    float light{1.f};       // the place's light where they started (their grey)
    double activeSince{-1.0}; // cl.time they started (-1: none left)
    int peakPoints{0}, peakDrawn{0};
};
HandWisps hands[2];

int advancedFrame = -1;
double lastTime = -1.0;
lightcache_t lightCache; // (placeLight's)

struct WispScratch
{
    za::Vector<ribbons::Point> points;
    za::Vector<gfx::Vertex> triangles;
    auto members() { return qvr::mem::list(points, triangles); }
};
mem::Scratch<WispScratch> scratch{"muzzlewisps"};

[[nodiscard]] bool wanted()
{
    return vr_gunfx.value != 0.f && vr_gunfx_wisps.value != 0.f;
}

[[nodiscard]] float fadeSeconds()
{
    return za::clamp(vr_gunfx_wisp_fade.value, 0.2f, 4.f);
}

// The place's light at `at` (1 at Quake's full light), as the gunfire smoke's grey (vr_gunfx.cpp placeLight).
[[nodiscard]] float placeLight(const glm::vec3& at)
{
    if(!cl.worldmodel)
    {
        return 1.f;
    }
    vec3_t v{at.x, at.y, at.z};
    return za::clamp(static_cast<float>(R_LightPoint(v, 0.f, &lightCache)) / 128.f, 0.f, 1.6f);
}

// A slow swirling flow (each component -1.35 .. 1.35): each a wave across the other two axes (an ABC flow, two octaves),
// so it neither gathers nor spreads the smoke (no divergence) but curls it.
[[nodiscard]] glm::vec3 flow(const glm::vec3& p, float t)
{
    const glm::vec3 q = p * kFlowScale;
    const glm::vec3 r = p * (kFlowScale * 2.3f);
    const float s = t * kFlowSpeed;
    return glm::vec3{za::sin(q.z + s) + za::cos(q.y - 0.7f * s), za::sin(q.x + 1.3f * s) + za::cos(q.z + 0.4f * s),
               za::sin(q.y - 0.9f * s) + za::cos(q.x + s)} *
               0.5f +
           glm::vec3{za::sin(r.y + 1.7f * s), za::sin(r.z - 1.1f * s), za::sin(r.x + 1.4f * s)} * 0.35f;
}

void report(const char* what, int hand, const HandWisps& h, double since)
{
    if(vr_debug_gunfx.value != 0.f)
    {
        Con_Printf("gunfx wisps %s hand %d barrels %d after %.2f s charge %.2f points %d drawn %d\n", what, hand, h.gun.barrels,
            since, h.charge, h.peakPoints, h.peakDrawn);
    }
}

// Once a frame (`dt` seconds of the game's time): each hand's barrels let go of points while they smoke, the points
// carried up and about, the old ones forgotten.
void advance(float dt)
{
    const float m2u = units::metresToUnits();
    const float fade = fadeSeconds();
    const float delay = za::clamp(vr_gunfx_wisp_delay.value, 0.f, 3.f);
    const float time = za::clamp(vr_gunfx_wisp_time.value, 0.f, 30.f);
    const float rise = za::clamp(vr_gunfx_wisp_rise.value, 0.f, 200.f) * 0.01f * m2u;
    const float turb = kFlowAccel * za::clamp(vr_gunfx_turbulence.value, 0.f, 3.f);
    const float clock = static_cast<float>(fmod(cl.time, 2000.0));
    const float relax = za::min(1.f, kRelax * dt);
    for(int hand = 0; hand < 2; hand++)
    {
        HandWisps& h = hands[hand];
        // Smoking: the gun in the hand (drawn this frame), its last shot a while ago and its smoke not spent.
        const bool seen = h.seenFrame == host_framecount && h.gun.barrels > 0;
        const float since = h.lastShot >= 0.0 ? static_cast<float>(cl.time - h.lastShot) : 1e9f;
        const float t = since - delay;
        const float lasts = time * (0.4f + 0.6f * h.charge);
        float strength = 0.f;
        if(h.charge > 0.f && t >= lasts)
        {
            h.charge = 0.f; // (spent)
        }
        if(seen && wanted() && h.charge > 0.f && t >= 0.f && t < lasts)
        {
            strength = h.charge * glm::smoothstep(0.f, 0.3f, t) * (1.f - glm::smoothstep(0.25f, 1.f, t / lasts));
        }
        const bool emitting = strength > 0.f;
        if(emitting && !h.emitting)
        {
            for(Strand& s : h.strands)
            {
                s.restart = true;
            }
            h.light = placeLight(h.gun.at[0]);
            h.sinceEmit = kEmitInterval;
            h.lastEmitAt = h.gun.at[0];
            if(h.activeSince < 0.0)
            {
                h.activeSince = cl.time;
                h.peakPoints = h.peakDrawn = 0;
            }
            report("start", hand, h, since);
        }
        h.emitting = emitting;
        if(emitting)
        {
            h.sinceEmit += dt;
            const float moved = glm::distance(h.gun.at[0], h.lastEmitAt);
            if(h.sinceEmit >= kEmitInterval || moved >= kEmitDistance)
            {
                h.sinceEmit = h.sinceEmit >= kEmitInterval ? h.sinceEmit - kEmitInterval : h.sinceEmit;
                h.sinceEmit = za::min(h.sinceEmit, kEmitInterval);
                h.lastEmitAt = h.gun.at[0];
                for(int b = 0; b < h.gun.barrels; b++)
                {
                    Strand& s = h.strands[b];
                    Node n;
                    n.pos = h.gun.at[b] + h.gun.fwd * 0.2f;
                    // (the air at the muzzle wavers: the stream let go sways, so even a still gun's wisp snakes up)
                    const float w = clock * 1.f + s.phase * 3.f;
                    const glm::vec3 sway{za::sin(w * 2.1f) + 0.6f * za::sin(w * 3.7f + 1.f), za::cos(w * 1.7f) + 0.6f * za::sin(w * 4.3f), 0.f};
                    n.vel = h.gun.fwd * kPuff + sway * kSway;
                    n.strength = strength * h.gun.alpha;
                    n.first = s.restart || moved >= kJump;
                    s.restart = false;
                    s.push(n);
                }
            }
        }
        // The points: up with the warm air, about in the flow (more as they age: a plume breaking up), forgotten once faded.
        int points = 0;
        for(Strand& s : h.strands)
        {
            for(int i = 0; i < s.count; i++)
            {
                Node& n = s.at(i);
                n.age += dt;
                const float swirl = turb * glm::smoothstep(0.f, 0.5f, n.age) * (0.6f + n.age / fade);
                n.vel += (glm::vec3{0.f, 0.f, rise} - n.vel) * relax + flow(n.pos, clock + s.phase) * (swirl * dt);
                n.pos += n.vel * dt;
            }
            while(s.count > 0 && s.at(0).age >= fade)
            {
                s.first = (s.first + 1) % kMaxNodes;
                s.count--;
            }
            points += s.count;
        }
        h.peakPoints = za::max(h.peakPoints, points);
        if(h.activeSince >= 0.0 && points == 0 && !h.emitting)
        {
            report("faded", hand, h, cl.time - h.activeSince);
            h.activeSince = -1.0;
        }
    }
}

// A trail's points from its oldest, a ribbon each time a trail starts, the newest at the muzzle now while it smokes.
int build(HandWisps& h, Strand& s, int barrel, const ribbons::View& view, za::Vector<gfx::Vertex>& out)
{
    za::Vector<ribbons::Point>& points = scratch.points;
    const float fade = fadeSeconds();
    const float spread = za::clamp(vr_gunfx_wisp_spread.value, 1.f, 10.f) - 1.f;
    const float halfWidth = 0.5f * za::clamp(vr_gunfx_wisp_width.value, 0.05f, 20.f) * 0.01f * units::metresToUnits();
    const float amount = za::clamp(vr_gunfx_wisp_amount.value, 0.f, 4.f);
    const float grey = za::clamp(0.48f + 0.3f * h.light, 0.48f, 0.9f);
    const glm::vec3 colour = glm::vec3{0.94f, 0.97f, 1.f} * grey;
    int drawn = 0;
    const auto flush = [&]() {
        if(points.size() >= 2)
        {
            drawn += ribbons::append(out, view, {points.data(), points.size()}, 3);
        }
        points.clear();
    };
    const auto point = [&](const glm::vec3& pos, float age, float strength) {
        const float a = za::clamp(age / fade, 0.f, 1.f);
        const float widen = 1.f + spread * a;
        const float alpha = amount * strength * glm::smoothstep(0.f, kEmerge, age) * za::pow(1.f - a, 1.5f) / (0.4f + 0.6f * widen);
        points.pushBack(ribbons::Point{pos, halfWidth * widen, glm::vec4{colour, za::min(alpha, 1.f)}});
    };
    points.clear();
    for(int i = 0; i < s.count; i++)
    {
        const Node& n = s.at(i);
        if(n.first)
        {
            flush();
        }
        point(n.pos, n.age, n.strength);
    }
    if(h.emitting && s.count > 0 && barrel < h.gun.barrels)
    {
        point(h.gun.at[barrel], 0.f, s.at(s.count - 1).strength); // (the muzzle now: the trail follows it)
    }
    flush();
    return drawn;
}

} // namespace

void handFrame(int hand, const Gun& gun, bool fired)
{
    if(hand < 0 || hand > 1)
    {
        return;
    }
    HandWisps& h = hands[hand];
    if(gun.barrels != h.gun.barrels || gun.charge != h.gun.charge || gun.alpha != h.gun.alpha)
    {
        h.charge = 0.f; // (another gun: the last one's smoke stays with what it let go)
        h.lastShot = -1.0;
    }
    h.gun = gun;
    h.gun.barrels = za::clamp(gun.barrels, 0, maxBarrels);
    h.seenFrame = host_framecount;
    if(fired && gun.charge > 0.f)
    {
        h.lastShot = cl.time;
        h.charge = za::min(1.f, h.charge + gun.charge);
        if(h.emitting && vr_debug_gunfx.value >= 2.f)
        {
            Con_Printf("gunfx wisps stop hand %d (fired)\n", hand);
        }
    }
}

bool smoking(int hand)
{
    return hand >= 0 && hand < 2 && hands[hand].charge > 0.f && wanted();
}

void drawTranslucent()
{
    if(advancedFrame != host_framecount)
    {
        advancedFrame = host_framecount;
        const float dt = lastTime >= 0.0 ? za::clamp(static_cast<float>(cl.time - lastTime), 0.f, 0.1f) : 0.f;
        lastTime = cl.time;
        QVR_PROFILE("muzzle wisps");
        advance(dt);
    }
    if(!wanted() || portals::viewing())
    {
        return;
    }
    bool any = false;
    for(const HandWisps& h : hands)
    {
        for(const Strand& s : h.strands)
        {
            any = any || s.count > 0;
        }
    }
    if(!any)
    {
        return;
    }
    QVR_GPU_PROFILE("muzzle wisps");
    za::Vector<gfx::Vertex>& out = scratch.triangles;
    out.clear();
    const ribbons::View view = ribbons::sceneView(za::clamp(vr_gunfx_wisp_pixels.value, 0.5f, 8.f));
    for(HandWisps& h : hands)
    {
        int drawn = 0;
        for(int b = 0; b < maxBarrels; b++)
        {
            drawn += build(h, h.strands[b], b, view, out);
        }
        h.peakDrawn = za::max(h.peakDrawn, drawn);
    }
    ribbons::draw({out.data(), out.size()}, false);
}

void clear()
{
    for(HandWisps& h : hands)
    {
        h = HandWisps{};
    }
    for(int i = 0; i < 2; i++)
    {
        for(int b = 0; b < maxBarrels; b++)
        {
            hands[i].strands[b].phase = 1.7f * static_cast<float>(i * maxBarrels + b);
        }
    }
    advancedFrame = -1;
    lastTime = -1.0;
}

} // namespace qvr::muzzlewisps
