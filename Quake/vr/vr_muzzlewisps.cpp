// vr_muzzlewisps.cpp -- see vr_muzzlewisps.hpp.

#include "vr_ambient.hpp"
#include "vr_muzzlewisps.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_portals.hpp"
#include "vr_profile.hpp"
#include "vr_ribbons.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"
#include "vr_voicenotes.hpp"

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
    glm::vec3 tint{1.f};    // their gun's tint as they started (Gun::tint)
    double activeSince{-1.0}; // cl.time they started (-1: none left)
    int peakPoints{0}, peakDrawn{0};
};
constexpr int kSources = 2 + maxLoose;
HandWisps hands[kSources]; // your hands', then the loose guns' (2 + their index)

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
    return za::clamp(static_cast<float>(ambient::lightPoint(at, lightCache)) / 128.f, 0.f, 1.6f);
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

// Your breath this frame (vr_smoke_blow).
struct Blow
{
    float power{0.f}; // 0 not blowing, 0.5 at the threshold .. 1 at twice it
    glm::vec3 mouth{0.f};
    glm::vec3 facing{1.f, 0.f, 0.f};
    float reach{0.f}; // units
};
float blowHeld = 0.f; // seconds the level has been above the threshold
int blowListens = 0;  // frames the microphone was wanted (vr_smoke_blow_check)

// The microphone is wanted only with the setting on and smoke within twice the reach of your mouth.
[[nodiscard]] Blow blowFrame(float dt)
{
    Blow b;
    b.reach = za::clamp(vr_smoke_blow_reach.value, 0.01f, 2.f) * units::metresToUnits();
    bool closeBy = false;
    if(vr_smoke_blow.value != 0.f && wanted() && voicenotes::mouth(b.mouth, b.facing))
    {
        for(HandWisps& h : hands)
        {
            for(Strand& s : h.strands)
            {
                for(int i = 0; i < s.count && !closeBy; i++)
                {
                    closeBy = glm::distance(s.at(i).pos, b.mouth) < 2.f * b.reach;
                }
            }
        }
    }
    if(!closeBy)
    {
        blowHeld = 0.f;
        return b;
    }
    blowListens++;
    voicenotes::wantMicLevel();
    const float level = voicenotes::micLevel();
    const float threshold = za::max(vr_smoke_blow_threshold.value, 0.001f);
    blowHeld = level >= threshold ? blowHeld + dt : 0.f;
    if(blowHeld >= vr_smoke_blow_hold.value && level >= threshold)
    {
        b.power = za::clamp(0.5f * level / threshold, 0.5f, 1.f);
    }
    return b;
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
    const Blow blow = blowFrame(dt);
    const float blowPush = za::clamp(vr_smoke_blow_push.value, 0.f, 100.f) * m2u;
    const float blowFade = za::clamp(vr_smoke_blow_fade.value, 0.f, 50.f);
    for(int hand = 0; hand < kSources; hand++)
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
            h.tint = h.gun.tint;
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
                if(blow.power > 0.f)
                {
                    // (your breath: full within half the reach, nothing past it)
                    const float d = glm::distance(n.pos, blow.mouth);
                    const float felt = blow.power * (1.f - glm::smoothstep(0.5f * blow.reach, blow.reach, d));
                    n.vel += blow.facing * (blowPush * felt * dt);
                    n.age = za::min(n.age + blowFade * felt * dt, fade);
                }
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
    const float most = za::clamp(vr_gunfx_wisp_opacity.value, 0.02f, 1.f);
    const float grey = za::clamp(0.48f + 0.3f * h.light, 0.48f, 0.9f);
    const glm::vec3 colour = glm::vec3{0.94f, 0.97f, 1.f} * grey * h.tint;
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
        points.pushBack(ribbons::Point{pos, halfWidth * widen, glm::vec4{colour, za::min(alpha, most)}});
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

// vr_smoke_blow_check: the real advance() on a fake trail with a fake level: a point at your mouth and one past the
// reach, against the same with no breath (exactly the same where nothing should change).
struct Probe
{
    glm::vec3 inReach{0.f}, outReach{0.f};
    float inAge{0.f}, outAge{0.f};
    int listens{0};
};

[[nodiscard]] Probe probe(float setting, float level, int frames, bool withNear = true)
{
    Cvar_SetValueQuick(&vr_smoke_blow, setting);
    Cvar_SetValueQuick(&vr_test_mic_level, level);
    glm::vec3 mouth{0.f}, facing{1.f, 0.f, 0.f};
    (void)voicenotes::mouth(mouth, facing);
    const float reach = za::clamp(vr_smoke_blow_reach.value, 0.01f, 2.f) * units::metresToUnits();
    Strand& s = hands[kSources - 1].strands[0];
    s = Strand{};
    Node n;
    n.strength = 1.f;
    n.first = true;
    n.pos = mouth + facing * (3.f * reach);
    s.push(n);
    n.first = false;
    n.pos = mouth + facing * (withNear ? 0.3f * reach : 4.f * reach);
    s.push(n);
    blowHeld = 0.f;
    const int before = blowListens;
    for(int i = 0; i < frames; i++)
    {
        advance(1.f / 72.f);
    }
    Probe p;
    p.outReach = s.at(0).pos;
    p.outAge = s.at(0).age;
    p.inReach = s.at(1).pos;
    p.inAge = s.at(1).age;
    p.listens = blowListens - before;
    s = Strand{};
    return p;
}

void check_f()
{
    glm::vec3 mouth, facing;
    if(!voicenotes::mouth(mouth, facing) || !wanted())
    {
        Con_Printf("vr_smoke_blow_check: FAIL no head, or no wisps wanted\n");
        testkit::verdict("vr_smoke_blow_check", 0, 1);
        return;
    }
    const float setting = vr_smoke_blow.value;
    const float fake = vr_test_mic_level.value;
    const float threshold = za::max(vr_smoke_blow_threshold.value, 0.001f);
    constexpr int kFrames = 72;
    const int shortFrames = static_cast<int>(vr_smoke_blow_hold.value * 72.f) - 1;
    const Probe still = probe(0.f, 2.f * threshold, kFrames);
    const Probe hard = probe(1.f, 2.f * threshold, kFrames);
    const Probe soft = probe(1.f, threshold, kFrames);
    const Probe quiet = probe(1.f, 0.5f * threshold, kFrames);
    const Probe shortBlow = probe(1.f, 2.f * threshold, shortFrames);
    const Probe shortStill = probe(0.f, 2.f * threshold, shortFrames);
    const Probe atOnce = probe(1.f, 2.f * threshold, 1);
    const Probe farOnly = probe(1.f, 2.f * threshold, kFrames, false);
    Cvar_SetValueQuick(&vr_smoke_blow, setting);
    Cvar_SetValueQuick(&vr_test_mic_level, fake);
    const auto ahead = [&](const Probe& p) { return glm::dot(p.inReach - still.inReach, facing); };
    const auto same = [](const Probe& a, const Probe& b) {
        return a.inReach == b.inReach && a.outReach == b.outReach && a.inAge == b.inAge && a.outAge == b.outAge;
    };
    constexpr const char* kName = "vr_smoke_blow_check";
    testkit::Tally tally;
    Con_Printf("vr_smoke_blow_check: near ahead hard %.2f soft %.2f units, age still %.2f hard %.2f soft %.2f; far moved %.3f\n",
        ahead(hard), ahead(soft), still.inAge, hard.inAge, soft.inAge, glm::distance(hard.outReach, still.outReach));
    tally.check(ahead(hard) > 1.f, kName, "a blow pushes the smoke at the mouth forward");
    tally.check(hard.inAge > still.inAge + 0.05f, kName, "a blow ages the smoke at the mouth");
    tally.check(ahead(hard) > ahead(soft) && ahead(soft) > 0.f && hard.inAge > soft.inAge, kName, "a harder blow does more");
    tally.check(hard.outReach == still.outReach && hard.outAge == still.outAge, kName, "smoke past the reach is untouched");
    tally.check(same(quiet, still), kName, "below the threshold nothing changes");
    tally.check(same(shortBlow, shortStill), kName, "shorter than the hold nothing changes");
    tally.check(still.listens == 0, kName, "off: the microphone is not wanted");
    tally.check(atOnce.listens == 1, kName, "on, smoke at the mouth: it is");
    tally.check(farOnly.listens == 0, kName, "on, smoke far off: it is not");
    tally.verdict(kName);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_smoke_blow_check", check_f);
}

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

int release(int hand)
{
    if(!smoking(hand))
    {
        return -1;
    }
    // A free loose gun's place, else the one with the least smoke left (its wisps let go fade on with it).
    int best = 0;
    for(int i = 0; i < maxLoose; i++)
    {
        const HandWisps& l = hands[2 + i];
        const bool free = l.charge <= 0.f && l.activeSince < 0.0;
        if(free || l.charge < hands[2 + best].charge)
        {
            best = i;
            if(free)
            {
                break;
            }
        }
    }
    HandWisps& h = hands[hand];
    HandWisps& l = hands[2 + best];
    float phases[maxBarrels];
    for(int b = 0; b < maxBarrels; b++)
    {
        phases[b] = l.strands[b].phase;
    }
    l = h; // (its trails, its smoke and time left: the trail goes on to where the gun is found, a new one if it went far)
    for(int b = 0; b < maxBarrels; b++)
    {
        const float own = h.strands[b].phase;
        h.strands[b] = Strand{};
        h.strands[b].phase = own;
        l.strands[b].phase = phases[b];
    }
    h.gun = Gun{};
    h.charge = 0.f;
    h.lastShot = -1.0;
    h.emitting = false;
    h.activeSince = -1.0;
    if(vr_debug_gunfx.value != 0.f)
    {
        Con_Printf("gunfx wisps loose hand %d as %d charge %.2f\n", hand, best, l.charge);
    }
    return best;
}

void looseFrame(int index, const Gun& gun)
{
    if(index < 0 || index >= maxLoose)
    {
        return;
    }
    HandWisps& l = hands[2 + index];
    const int barrels = l.gun.barrels;
    l.gun = gun;
    l.gun.barrels = za::clamp(barrels, 0, maxBarrels); // (as many as it had in the hand)
    l.seenFrame = host_framecount;
}

bool looseSmoking(int index)
{
    return index >= 0 && index < maxLoose && hands[2 + index].charge > 0.f && wanted();
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
    ribbons::View view = ribbons::sceneView(za::clamp(vr_gunfx_wisp_pixels.value, 0.5f, 8.f));
    // Faded out only this near the eye (vr_gunfx_wisp_near; the ribbons' own 6 units, about 18 cm, hid the smoke of a
    // gun held at the mouth, where you blow on it: his message, 2026-11-08).
    view.nearFade = za::clamp(vr_gunfx_wisp_near.value, 0.5f, 30.f) * 0.01f * units::metresToUnits();
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
    for(int i = 0; i < kSources; i++)
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
