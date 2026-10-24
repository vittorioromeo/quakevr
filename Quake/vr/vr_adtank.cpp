// vr_adtank.cpp -- see vr_adtank.hpp.

#include "vr_adtank.hpp"

#include "vr_adguns.hpp"
#include "vr_flamewobble.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_box3d.hpp"
#include "vr_bodyclone.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_flail.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_verlet.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::adtank
{
namespace
{

constexpr int nodes = 26;      // the hose's points (tank .. clip .. gun)
constexpr int clipNode = 9;    // the one held at the hip's or shoulder's clip
constexpr float hoseRadius = 0.011f; // metres
constexpr float tankRadius = 0.06f, tankHeight = 0.27f;
const glm::vec3 keyLight = glm::normalize(glm::vec3{0.3f, 0.2f, 1.f});
const glm::vec3 hazardBand{0.78f, 0.6f, 0.08f};

[[nodiscard]] glm::vec3 tankColour(int variant)
{
    return variant == adguns::Pyro ? glm::vec3{0.48f, 0.09f, 0.05f} : glm::vec3{0.15f, 0.34f, 0.08f};
}

[[nodiscard]] glm::vec3 hoseColour(int variant)
{
    return variant == adguns::Pyro ? glm::vec3{0.09f, 0.05f, 0.04f} : glm::vec3{0.05f, 0.08f, 0.04f};
}

constexpr int worldNodes = 22;   // a world hose's points (the tank's outlet .. the gun's inlet; a flail's chain's: the ring .. the tip)
constexpr int worldHoseMax = 8;  // the world hoses drawn (the nearest pairs: the tanks' and the flails' lying about)
constexpr int flailVariant = -2; // a world line that is a flail's chain lying about (vr_flail.cpp's links)
constexpr float flailChainRadius = 0.006f; // metres: its links lying on the floor (vr_flail.cpp's 1.1 cm wide)
constexpr float worldReach = 1500.f; // units: a pair farther from the eyes has no hose
constexpr int carriedCount = 2 + body::HolsterCount; // the hands, then the holsters

// A hose: a carried gun's (a hand's, from the back tank through a clip; a holster's, straight to it) or a world one's
// (a gun lying about and its tank: QC vr_ad_enemyguns.qc "Fuel tanks").
struct Hose
{
    int variant{-1};
    bool live{false};
    za::Vector<glm::vec3> p, prev, from;
    za::Vector<char> touching;
    float lastDt{0.f};
    double time{0.0};
    glm::vec3 lastTank{0.f};  // carried: the back tank's place at the last step (its move carries the hose)
    glm::vec3 outlet{0.f};    // the hose's first end, under the tank
    glm::vec3 clip{0.f};      // a hand's: the clip's
    glm::vec3 inlet{0.f};     // the gun's
    bool clipped{false};      // through the clip (a hand's)
    // World hoses: the pair (edict numbers), the hose's length (units), its ends at the last step and the steps they
    // have stayed (a hose lying still is not stepped), the frame it was last found.
    int gun{0}, tank{0};
    float length{0.f};
    glm::vec3 lastOutlet{0.f}, lastInlet{0.f};
    int still{0};
    int seen{-1};
    bool lying{false}; // a world hose between a gun and its tank lying about (a blade cuts it: crossHose)
    coil::Cord cord;
    // The draw's (built once a frame, uploaded once, drawn in both eyes; the mirror image's too).
    gfx::TubeBatch hoseRings, hoseMirror;
    int sides{0};
    int builtFrame{-1};
};
Hose carried[carriedCount];
Hose worldHoses[worldHoseMax];
// The hose cut's stubs (setupStubs): a cut gun's and tank's short free end of the hose.
constexpr int stubNodes = 6;
constexpr int stubMax = 8;
constexpr float stubLength = 0.15f; // metres (some 3-4 of the world hose's links)
Hose stubs[stubMax];
int stubsDrawn = 0; // (vr_adgun_hose_info)

// The one tank on the back (Vittorio, 2026-10-23: several fuelled guns carried, in the hands or the holsters, share
// one, the last held one's; every gun's hose runs to it), while any is carried.
struct BackTank
{
    int variant{-1};
    bool live{false};
    glm::vec3 tank{0.f}; // its bottom middle (world)
    glm::vec3 axis{0.f, 0.f, 1.f}; // its up (upright; another way while it flies there: Flight)
    gfx::TubeBatch rings, mirror;
    int builtFrame{-1};
};
BackTank back;
int lastHeld[2]{-1, -1}; // each hand's fuelled gun last frame (-1: none): a gun newly in a hand gives the tank its colour
int lastWant[carriedCount]{}; // every carried slot's fuelled gun last frame, its variant + 1 (0 none): a draw from a holster

// The pickup's flight (Vittorio, 2026-10-23, vrfiringrange_2026-10-23_21-24-32: the tank shouldn't teleport to the back):
// a gun taken into a hand whose tank lay about (or was on an armed monster's back), the tank flies from where it was
// drawn last to its place on the back over vr_adgun_tank_pickup_time (rising a little on the way, turning upright), its
// hose following; then it is the back tank. The tanks drawn lately are remembered (where, which way up, when last drawn)
// so the one that just went is found: the nearest of the gun's kind to the gun, drawn within the last 0.3 s.
struct SeenTank
{
    int num{0};
    int variant{-1};
    glm::vec3 bottom{0.f}, axis{0.f, 0.f, 1.f};
    double seen{-1.0}; // vr_gametime last drawn (-1: none)
};
constexpr int seenMax = 16;
SeenTank seenTanks[seenMax];
struct Flight
{
    bool on{false};
    bool own{false};  // it is the back tank (none was on the back): the hoses run to it; else one more tank, drawn alone
    int variant{-1};
    glm::vec3 from{0.f}, fromAxis{0.f, 0.f, 1.f};
    double start{0.0};
    glm::vec3 tank{0.f}, axis{0.f, 0.f, 1.f}; // where it is now (drawn alone)
    gfx::TubeBatch rings;
    int builtFrame{-1};
    int flights{0};   // flights flown (vr_adgun_hose_info)
    float firstGap{0.f}; // its first frame: how far from where the tank lay (units: 0 when seamless)
};
Flight flight;

// The armed monsters' back tanks (QC vr_adtankworn.qc; a listen server's), found each frame (relinkWorn): the tank, its
// gun as held, its wearer (edict numbers).
struct Worn
{
    int tank{0}, gun{0}, wearer{0};
};
constexpr int wornMax = 16;
Worn worn[wornMax];
int wornCount = 0;

// The body's frame this frame (forward, right; up the world's).
glm::vec3 bodyFwd{1.f, 0.f, 0.f}, bodyRight{0.f, -1.f, 0.f};

// The pyro's pilot light (Vittorio, 2026-10-18: a small subtle continuous stream, not a modelled flame): each hand's
// particles owed (fractions carried over frames), the game time it last ran, and the count made (vr_adgun_hose_info).
struct Pilot
{
    float owed{0.f};
    double time{-1.0};
    int made{0};
};
Pilot pilots[2];
constexpr float pilotRate = 34.f; // particles a second: about 3 in 4 small yellow tongues, the rest blue at the tip; a spark

// Visual randomness (no sequence that matters): 0..1.
constexpr float pilotHeight = 3.f; // units: its flames' height (vr_flamewobble.cpp's reach round them)
constexpr float pilotStir = 40.f;  // units/s its flames drift a unit of a stirring hand's lean
unsigned pilotSeed = 0x9E3779B9u;
[[nodiscard]] float pilotRandom()
{
    pilotSeed = pilotSeed * 1664525u + 1013904223u;
    return static_cast<float>(pilotSeed >> 8) / static_cast<float>(1u << 24);
}

// `ve` (the pyro's gun, held in hand `i`) burning its pilot light this frame: small flames off its pilot pipe's tip,
// drifting forward and up, now and then a spark. None while paused or under water, nor with its fuel gone (Vittorio,
// 2026-10-18: the empty gun's tell; its clip is its fuel).
void pilotFlames(int i, const view::ViewEntity* ve)
{
    Pilot& p = pilots[i];
    glm::vec3 tipModel;
    const int fuel = cl.stats[i == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
    if(!ve || !ve->ent.model || !adguns::pilotLight(adguns::variantOf(ve->ent.model->name), tipModel) || !cl.worldmodel ||
        fuel <= 0)
    {
        p.time = -1.0;
        return;
    }
    const double now = vr_gametime;
    const float dt = p.time < 0.0 ? 0.f : static_cast<float>(za::clamp(now - p.time, 0.0, 0.1));
    p.time = now;
    const glm::vec3 tip = view::modelPoint(*ve, tipModel);
    const glm::vec3 ahead = view::modelPoint(*ve, tipModel + glm::vec3{1.f, 0.f, 0.f}) - tip;
    glm::vec3 probe = tip;
    if(glm::length(ahead) < 1e-6f || Mod_PointInLeaf(&probe.x, cl.worldmodel)->contents <= CONTENTS_WATER)
    {
        return;
    }
    const glm::vec3 fwd = glm::normalize(ahead);
    // Stirred by the other hand passing it fast (vr_flamewobble.cpp): its flames carried the way the air went.
    const glm::vec2 lean = flamewobble::source(0x30000u + static_cast<unsigned>(i), tip, pilotHeight, i);
    const glm::vec3 stir = glm::vec3{lean, 0.f} * pilotStir;
    p.owed += dt * pilotRate;
    const glm::vec3 blue{0.42f, 0.62f, 1.f}, yellow{1.f, 0.86f, 0.32f};
    while(p.owed >= 1.f)
    {
        p.owed -= 1.f;
        p.made++;
        const float r = pilotRandom();
        const glm::vec3 wobble{pilotRandom() - 0.5f, pilotRandom() - 0.5f, pilotRandom() - 0.5f};
        if(r < 0.08f)
        {
            particles::hotTrail(tip, fwd * 30.f + wobble * 30.f + glm::vec3{0.f, 0.f, 12.f} + stir, yellow, 0.8f, 0.3f, true);
        }
        else if(r < 0.32f)
        {
            particles::hotTrail(tip + fwd * 0.2f, fwd * 4.f + wobble * 2.f + stir, blue, 0.45f, 0.12f, false);
        }
        else
        {
            particles::hotTrail(tip + fwd * 0.5f, fwd * 9.f + wobble * 4.f + glm::vec3{0.f, 0.f, 5.f} + stir, yellow, 0.5f, 0.2f, false);
        }
    }
}

// The cost (vr_adgun_hose_info): the last setup's seconds, smoothed.
double setupCost = 0.0;
double buildCost = 0.0; // the rings' (hose, tank, their mirror image's), built once a frame for both eyes
int setupHoses = 0;

struct AdTankScratch
{
    za::Vector<gfx::TubeRing> rings;
    za::Vector<gfx::TubeRing> mirrored;
    za::Vector<char> pinned;
    za::Vector<float> rests;
    auto members() { return qvr::mem::list(rings, mirrored, pinned, rests); }
};
mem::Scratch<AdTankScratch> scratch{"adtank"};

[[nodiscard]] bool fuelled(int variant)
{
    return variant == adguns::Pyro || variant == adguns::Fumigator;
}

// A carried gun whose tank burst lying about (QC's QVR_WPNFLAG_TANK_BURST, vr_defs.qc; vr_ad_enemyguns.qc
// VR_FuelTank_Burst): spent, no tank on the back for it, no hose. `i`: 0..1 the hands, 2.. the drawn holsters.
constexpr int weaponFlagTankBurst = 4096;

[[nodiscard]] bool tankBurst(int i)
{
    const int flags = i < 2 ? cl.stats[i == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2]
                            : view::holsterFlags(i - 2);
    return (flags & weaponFlagTankBurst) != 0;
}

// The torso as an upright elliptic cylinder round `axis` (its middle line, world), from `bottom` to `top` (world z): a
// point inside pushed out to its surface (sideways semi-axis `side`, forwards `depth`, metres in units).
// (`fwd`, `right`: the body's frame; yours by default, an armed monster's for its hose.)
void pushOutOfTorso(glm::vec3& p, const glm::vec3& axis, float bottom, float top, float side, float depth,
    const glm::vec3& fwd = bodyFwd, const glm::vec3& right = bodyRight)
{
    if(p.z < bottom || p.z > top)
    {
        return;
    }
    const glm::vec3 d{p.x - axis.x, p.y - axis.y, 0.f};
    const float x = glm::dot(d, right) / side, y = glm::dot(d, fwd) / depth;
    const float k = x * x + y * y;
    if(k >= 1.f)
    {
        return;
    }
    const float s = k > 1e-8f ? 1.f / sqrtf(k) : 0.f;
    const glm::vec3 out = k > 1e-8f ? (right * (x * s * side) + fwd * (y * s * depth)) : right * side;
    p.x = axis.x + out.x;
    p.y = axis.y + out.y;
}

// An armed monster's torso round which its hose goes (pushOutOfTorso's): from its front to its back tank.
struct WornBody
{
    glm::vec3 axis{0.f}, fwd{1.f, 0.f, 0.f}, right{0.f, -1.f, 0.f};
    float bottom{0.f}, top{0.f}, side{0.f}, depth{0.f};
};

// The drawn body's arms as capsules (Vittorio, 2026-10-18: the hose went through the arms), from the body as last posed
// (avatar::skeleton; sized as vr_selfcollide.cpp's: the upper arm, the forearm's thick half and its bracer), the hose's
// radius added. Each hose segment is kept out of them as the torso keeps its points out (vr_adgun_hose_arms), and how
// deep the hose lies in them is measured either way (vr_adgun_hose_info).
struct ArmCapsule
{
    glm::vec3 a{0.f}, b{0.f};
    float r{0.f};
};
constexpr int armCapsuleCount = 6;
ArmCapsule armCaps[armCapsuleCount];
int armCapCount = 0;

// Each hose's depth in the arms after its step (units), its segments in them, the steps measured.
struct ArmMetric
{
    float worst{0.f};    // this step's deepest segment
    int inside{0};       // ... its segments in an arm
    float peak{0.f};     // the deepest since vr_adgun_hose_info last printed
    double sum{0.0};     // ... the steps' worst, summed (the mean)
    int steps{0};
};
ArmMetric armMetrics[2];

void gatherArms()
{
    armCapCount = 0;
    avatar::Skeleton sk;
    if(!avatar::skeleton(sk))
    {
        return;
    }
    const float k = sk.m2w;
    for(int h = 0; h < 2; h++)
    {
        const glm::vec3 mid = glm::mix(sk.elbow[h], sk.wrist[h], 0.45f);
        armCaps[armCapCount++] = {sk.shoulder[h], sk.elbow[h], (0.056f + hoseRadius) * k};
        armCaps[armCapCount++] = {sk.elbow[h], mid, (0.05f + hoseRadius) * k};
        armCaps[armCapCount++] = {mid, sk.wrist[h], (0.036f + hoseRadius) * k};
    }
}

// The closest points of segments p0-p1 and q0-q1: their shares along each (s on p, t on q). Ericson's.
void closestSegments(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& q0, const glm::vec3& q1, float& s, float& t)
{
    const glm::vec3 d1 = p1 - p0, d2 = q1 - q0, r = p0 - q0;
    const float a = glm::dot(d1, d1), e = glm::dot(d2, d2), f = glm::dot(d2, r);
    if(a <= 1e-8f && e <= 1e-8f)
    {
        s = t = 0.f;
        return;
    }
    if(a <= 1e-8f)
    {
        s = 0.f;
        t = za::clamp(f / e, 0.f, 1.f);
        return;
    }
    const float c = glm::dot(d1, r);
    if(e <= 1e-8f)
    {
        t = 0.f;
        s = za::clamp(-c / a, 0.f, 1.f);
        return;
    }
    const float b = glm::dot(d1, d2), denom = a * e - b * b;
    s = denom > 1e-8f ? za::clamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;
    t = (b * s + f) / e;
    if(t < 0.f)
    {
        t = 0.f;
        s = za::clamp(-c / a, 0.f, 1.f);
    }
    else if(t > 1.f)
    {
        t = 1.f;
        s = za::clamp((b - c) / a, 0.f, 1.f);
    }
}

// How deep segment p0-p1 lies in capsule `c` (0: not in it), the way out of it at its deepest and its share there.
[[nodiscard]] float depthIn(const ArmCapsule& c, const glm::vec3& p0, const glm::vec3& p1, glm::vec3& out, float& s)
{
    float t;
    closestSegments(p0, p1, c.a, c.b, s, t);
    const glm::vec3 onHose = glm::mix(p0, p1, s), onArm = glm::mix(c.a, c.b, t);
    const glm::vec3 d = onHose - onArm;
    const float dist = glm::length(d);
    if(dist >= c.r)
    {
        return 0.f;
    }
    out = dist > 1e-5f ? d / dist : bodyRight;
    return c.r - dist;
}

// The hose's points (`pinned` held) pushed out of the arms: each segment in an arm moved out along the way out at its
// deepest, split between its free ends by their shares (a segment whose held end is in the arm is left: the gun's own
// hand holds it there).
void pushOutOfArms(glm::vec3* p, const char* pinned, int n)
{
    for(int k = 0; k + 1 < n; k++)
    {
        const bool f0 = !pinned[k], f1 = !pinned[k + 1];
        if(!f0 && !f1)
        {
            continue;
        }
        for(int c = 0; c < armCapCount; c++)
        {
            glm::vec3 out;
            float s;
            const float depth = depthIn(armCaps[c], p[k], p[k + 1], out, s);
            if(depth <= 0.f)
            {
                continue;
            }
            const float w0 = f0 ? 1.f - s : 0.f, w1 = f1 ? s : 0.f;
            const float inv = w0 * w0 + w1 * w1;
            if(inv < 0.05f)
            {
                continue; // (its held end's the deepest: in the holding hand's arm)
            }
            const float lambda = depth / inv;
            p[k] += out * (lambda * w0);
            p[k + 1] += out * (lambda * w1);
        }
    }
}

// The step's depth of the hose in the arms (its segments with a free end).
void measureArms(int hand, const glm::vec3* p, const char* pinned, int n)
{
    ArmMetric& m = armMetrics[hand];
    m.worst = 0.f;
    m.inside = 0;
    if(armCapCount == 0)
    {
        return;
    }
    for(int k = 0; k + 1 < n; k++)
    {
        if(pinned[k] && pinned[k + 1])
        {
            continue;
        }
        float deepest = 0.f;
        for(int c = 0; c < armCapCount; c++)
        {
            glm::vec3 out;
            float s;
            const float d = depthIn(armCaps[c], p[k], p[k + 1], out, s);
            const bool heldEnd = (pinned[k] && s < 0.2f) || (pinned[k + 1] && s > 0.8f);
            if(!heldEnd)
            {
                deepest = za::max(deepest, d);
            }
        }
        if(deepest > 0.f)
        {
            m.inside++;
            m.worst = za::max(m.worst, deepest);
        }
    }
    m.peak = za::max(m.peak, m.worst);
    m.sum += m.worst;
    m.steps++;
}

void hide(Hose& h)
{
    h.live = false;
    h.variant = -1;
    h.cord.hide();
    h.p.clear();
    h.prev.clear();
    h.gun = h.tank = 0;
    h.lying = false;
}

// The tank's rings: an upright bottle (an outlet's neck under it, domes, a valve on top), its band in the hazard colour
// (the rings' rust). (Its model lying about, vr_adgunart.cpp makeTank, is turned from the same profile.)
// `up`: its axis (upright on the back; another way flying there from where it lay: the pickup's flight).
void buildTank(const glm::vec3& tank, const glm::vec3& up, za::Vector<gfx::TubeRing>& out)
{
    out.clear();
    const float m2u = units::metresToUnits();
    const glm::vec3 side = glm::cross(bodyFwd, up); // (upright: the body's right)
    const glm::vec3 across = glm::dot(side, side) > 1e-6f ? glm::normalize(side) : bodyRight;
    const glm::vec3 light = coil::lightAt(tank + up * (tankHeight * 0.5f * m2u));
    // (height from the bottom, radius's share, the band's)
    constexpr float r = tankRadius;
    const float profile[][3] = {{-0.03f, 0.f, 0.f}, {-0.03f, 0.016f / r, 0.f}, {-0.004f, 0.016f / r, 0.f}, {0.f, 0.55f, 0.f},
        {0.014f, 0.9f, 0.f}, {0.03f, 1.f, 0.f}, {0.11f, 1.f, 0.f}, {0.11f, 1.005f, 1.f}, {0.16f, 1.005f, 1.f}, {0.16f, 1.f, 0.f},
        {tankHeight - 0.035f, 1.f, 0.f}, {tankHeight - 0.012f, 0.82f, 0.f}, {tankHeight, 0.45f, 0.f}, {tankHeight, 0.2f, 0.f},
        {tankHeight + 0.022f, 0.2f, 0.f}, {tankHeight + 0.022f, 0.42f, 1.f}, {tankHeight + 0.032f, 0.42f, 1.f},
        {tankHeight + 0.032f, 0.f, 1.f}};
    for(const auto& a : profile)
    {
        gfx::TubeRing ring{};
        ring.mid = glm::vec4{tank + up * (a[0] * m2u), za::max(a[1] * r * m2u, 1e-4f)};
        ring.across = glm::vec4{across, 0.f};
        ring.along = glm::vec4{up, 0.f};
        ring.ambient = glm::vec4{light, a[2]};
        ring.lamp = glm::vec4{0.f};
        ring.lampDir = glm::vec4{0.f, 0.f, 1.f, 0.f};
        out.pushBack(ring);
    }
}

// The rings reflected by the body's mirror image.
void reflect(const za::Vector<gfx::TubeRing>& in, const glm::mat4& mirror, za::Vector<gfx::TubeRing>& out)
{
    out.clear();
    const glm::mat3 turn{mirror};
    for(const gfx::TubeRing& r : in)
    {
        gfx::TubeRing m = r;
        m.mid = glm::vec4{glm::vec3{mirror * glm::vec4{glm::vec3{r.mid}, 1.f}}, r.mid.w};
        m.across = glm::vec4{turn * glm::vec3{r.across}, 0.f};
        m.along = glm::vec4{turn * glm::vec3{r.along}, 0.f};
        m.lampDir = glm::vec4{turn * glm::vec3{r.lampDir}, 0.f};
        out.pushBack(m);
    }
}

[[nodiscard]] float hoseLength(const Hose& h)
{
    float length = 0.f;
    for(za::SizeT k = 0; k + 1 < h.p.size(); k++)
    {
        length += glm::distance(h.p[k], h.p[k + 1]);
    }
    return length;
}

// The server's tanks and fuelled guns lying about (a listen server's; vr_adgun_hose_info): how many of each, how many
// paired (each naming the other), the tanks without their gun, the guns without a tank.
void serverCounts(int& tanks, int& guns, int& paired, int& orphans, int& bare)
{
    tanks = guns = paired = orphans = bare = 0;
    const int link = progs::fields().vr_fuellink;
    if(!sv.active || link < 0)
    {
        return;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    for(int n = svs.maxclients + 1; n < qcvm->num_edicts; n++)
    {
        edict_t* e = EDICT_NUM(n);
        if(e->free)
        {
            continue;
        }
        const char* cls = PR_GetString(e->v.classname);
        const bool tank = !strcmp(cls, "vr_fueltank");
        const bool gun = !strcmp(cls, "thrown_weapon") && e->v.model && adguns::variantOf(PR_GetString(e->v.model)) >= 0 &&
                         fuelled(adguns::variantOf(PR_GetString(e->v.model)));
        if(!tank && !gun)
        {
            continue;
        }
        const int ofs = progs::fieldInt(e, link);
        edict_t* other = ofs ? PROG_TO_EDICT(ofs) : nullptr;
        const bool mutual = other && !other->free && progs::fieldInt(other, link) == EDICT_TO_PROG(e) &&
                            !strcmp(PR_GetString(other->v.classname), tank ? "thrown_weapon" : "vr_fueltank");
        if(tank)
        {
            tanks++;
            paired += mutual ? 1 : 0;
            orphans += mutual ? 0 : 1;
        }
        else
        {
            guns++;
            bare += mutual ? 0 : 1;
        }
    }
    PR_PopQCVM(old);
}

// vr_adgun_hose_info: the back tank, each carried gun's hose (its gun, its ends, its length and sag, how far into the
// torso its worst point lies), the world's tanks, ropes and hoses, and the cost.
void info_f()
{
    Con_Printf("adgun hose: on %g, route %s, tank at %g %g %g, %.3f ms a frame stepping %d hose(s), %.3f ms building their "
               "rings, %.1f units a metre\n",
        vr_adgun_hose.value, vr_adgun_hose_route.value != 0.f ? "shoulder" : "hip", vr_adgun_tank_x.value, vr_adgun_tank_y.value,
        vr_adgun_tank_z.value, setupCost * 1000.0, setupHoses, buildCost * 1000.0, units::metresToUnits());
    Con_Printf("adgun pilot light: %d and %d particles made (main, off hand)\n", pilots[HAND_MAIN].made, pilots[1 - HAND_MAIN].made);
    int lyingHoses = 0;
    for(const Hose& h : worldHoses)
    {
        lyingHoses += h.live && h.lying ? 1 : 0;
    }
    Con_Printf("adgun hose cut: on %g, least speed %g m/s, %d lying hoses to cut, %d stubs drawn\n", vr_adgun_hosecut.value,
        vr_adgun_hosecut_speed.value, lyingHoses, stubsDrawn);
    for(const Hose& h : stubs)
    {
        if(h.live && h.p.size() > 0)
        {
            Con_Printf("adgun hose stub: entity %d, variant %d, from %.1f %.1f %.1f, its free end %.1f %.1f %.1f (%.1f units "
                       "below its start), %s\n",
                h.gun, h.variant, h.p[0].x, h.p[0].y, h.p[0].z, h.p.back().x, h.p.back().y, h.p.back().z, h.p[0].z - h.p.back().z,
                h.cord.visible() ? "drawn" : "hidden");
        }
    }
    const float u2cm = 100.f / units::metresToUnits();
    Con_Printf("adgun hose arms: %s, %d arm capsules\n", vr_adgun_hose_arms.value != 0.f ? "kept out" : "not collided", armCapCount);
    for(int i = 0; i < 2; i++)
    {
        ArmMetric& m = armMetrics[i];
        Con_Printf("adgun hose arms %s: now %.2f cm deep (%d segments in), deepest %.2f cm, mean %.2f cm over %d steps\n",
            i == HAND_MAIN ? "main" : "off", m.worst * u2cm, m.inside, m.peak * u2cm,
            m.steps ? static_cast<float>(m.sum / m.steps) * u2cm : 0.f, m.steps);
        m = ArmMetric{};
    }
    int carriedLive = 0;
    for(const Hose& h : carried)
    {
        carriedLive += h.live ? 1 : 0;
    }
    Con_Printf("adgun tank flight: %s (%s), variant %d, %.2f s in, at %.1f units from where it lay, %.1f from its place on the "
               "back, its up %.2f; %d flown, the first frame %.2f units from where it lay; worn tanks %d\n",
        flight.on ? "flying" : "none", flight.own ? "the back tank" : "one more", flight.variant,
        flight.on ? static_cast<float>(vr_gametime - flight.start) : 0.f,
        glm::distance(flight.own ? back.tank : flight.tank, flight.from),
        glm::distance(flight.own ? back.tank : flight.tank,
            body::chestAnchor(hands::current(), glm::vec3{vr_adgun_tank_x.value, vr_adgun_tank_y.value, vr_adgun_tank_z.value}) -
                glm::vec3{0.f, 0.f, tankHeight * 0.5f * units::metresToUnits()}),
        (flight.own ? back.axis : flight.axis).z, flight.flights, flight.firstGap, wornCount);
    for(int k = 0; k < wornCount; k++)
    {
        const entity_t& t = cl_entities[worn[k].tank];
        const entity_t& m = cl_entities[worn[k].wearer];
        const Hose* drawn = nullptr;
        for(const Hose& h : worldHoses)
        {
            drawn = h.live && h.tank == worn[k].tank ? &h : drawn;
        }
        Con_Printf("adgun worn tank %d on %d (gun %d): %.1f units from its wearer's origin; hose %s, %d rings\n", worn[k].tank,
            worn[k].wearer, worn[k].gun,
            glm::distance(glm::vec3{t.origin[0], t.origin[1], t.origin[2]}, glm::vec3{m.origin[0], m.origin[1], m.origin[2]}),
            drawn ? "drawn" : "none", drawn ? static_cast<int>(drawn->hoseRings.count) : 0);
    }
    Con_Printf("adgun back tank: %s, variant %d, %d hose(s) to it, %d tank rings drawn (%d in the body's mirror image)\n",
        back.live ? "on the back" : "none", back.variant, carriedLive, static_cast<int>(back.rings.count),
        static_cast<int>(back.mirror.count));
    const float m2u = units::metresToUnits();
    for(int i = 0; i < carriedCount; i++)
    {
        const Hose& h = carried[i];
        char where[24];
        if(i < 2)
        {
            q_snprintf(where, sizeof(where), "%s", i == HAND_MAIN ? "main" : "off");
        }
        else
        {
            q_snprintf(where, sizeof(where), "holster %d", i - 2);
        }
        if(!h.live || h.p.size() < 2)
        {
            if(i < 2)
            {
                Con_Printf("adgun hose %s: none\n", where);
            }
            continue;
        }
        float lowest = 1e9f;
        for(const glm::vec3& q : h.p)
        {
            lowest = za::min(lowest, q.z);
        }
        Con_Printf("adgun hose %s: variant %d, outlet %.2f, clip %s %.2f, inlet %.2f (units off), length %.2f m, lowest %.2f m "
                   "under the inlet, %d rings; drawn %d hose rings, %d in the body's mirror image\n",
            where, h.variant, glm::distance(h.p[0], h.outlet), h.clipped ? "on" : "none",
            h.clipped ? glm::distance(h.p[static_cast<za::SizeT>(clipNode)], h.clip) : 0.f, glm::distance(h.p.back(), h.inlet),
            hoseLength(h) / m2u, (h.inlet.z - lowest) / m2u, h.cord.rings(), static_cast<int>(h.hoseRings.count),
            static_cast<int>(h.hoseMirror.count));
    }
    int tanks, guns, paired, orphans, bare;
    serverCounts(tanks, guns, paired, orphans, bare);
    int ropes = 0, joined = 0;
    box3d::forEachFuelRope([&](int, int, float, bool j) {
        ropes++;
        joined += j ? 1 : 0;
    });
    Con_Printf("adgun world tanks: %d tanks, %d fuelled guns lying about, %d paired, %d orphan tanks, %d guns without a tank; "
               "%d ropes (%d joined)\n",
        tanks, guns, paired, orphans, bare, ropes, joined);
    box3d::forEachFuelRope([&](int gun, int tank, float length, bool j) {
        const bool seen = gun > 0 && gun < cl.num_entities && tank > 0 && tank < cl.num_entities;
        const float apart = seen ? glm::distance(glm::vec3{cl_entities[gun].origin[0], cl_entities[gun].origin[1], cl_entities[gun].origin[2]},
                                       glm::vec3{cl_entities[tank].origin[0], cl_entities[tank].origin[1], cl_entities[tank].origin[2]})
                                 : -1.f;
        const Hose* drawn = nullptr;
        for(const Hose& h : worldHoses)
        {
            if(h.live && h.gun == gun && h.tank == tank)
            {
                drawn = &h;
            }
        }
        int inSolid = 0;
        if(drawn && cl.worldmodel)
        {
            for(glm::vec3 q : drawn->p)
            {
                inSolid += Mod_PointInLeaf(&q.x, cl.worldmodel)->contents == CONTENTS_SOLID ? 1 : 0;
            }
        }
        Con_Printf("adgun world rope: gun %d, tank %d, %s, most %.2f m, apart %.2f m (%s); hose %s: %.2f m of %.2f m, %d points in "
                   "the world, %d hose rings\n",
            gun, tank, j ? "joined" : "not joined", length / m2u, apart / m2u, apart <= length + 0.5f ? "within" : "PAST",
            drawn ? "drawn" : "none", drawn ? hoseLength(*drawn) / m2u : 0.f, drawn ? drawn->length / m2u : 0.f, inSolid,
            drawn ? static_cast<int>(drawn->hoseRings.count) : 0);
    });
    // The flails' chains lying about (the same world lines: vr_flail_info has their ropes).
    for(const Hose& h : worldHoses)
    {
        if(!h.live || h.variant != flailVariant)
        {
            continue;
        }
        int inSolid = 0;
        for(glm::vec3 q : h.p)
        {
            inSolid += cl.worldmodel && Mod_PointInLeaf(&q.x, cl.worldmodel)->contents == CONTENTS_SOLID ? 1 : 0;
        }
        Con_Printf("adgun world flail chain: handle %d, head %d: %.2f m of %.2f m, %d points in the world, %d links, %s\n",
            h.gun, h.tank, hoseLength(h) / m2u, h.length / m2u, inSolid, h.cord.links(), h.still > 90 ? "lying still" : "moving");
    }
}

// A carried gun's hose (in hand `i` < 2, or holster i - 2) stepped from the back tank to the gun `ve` (variant
// `variant`): a hand's through its clip, a holster's straight.
void stepCarried(int i, int variant, const view::ViewEntity& ve, const glm::vec3& torso, bool armsCollide)
{
    Hose& h = carried[i];
    const float m2u = units::metresToUnits();
    const glm::vec3 worldUp{0.f, 0.f, 1.f};
    const bool hand = i < 2;
    const float side = i == HAND_MAIN ? 1.f : -1.f; // the body's right for the main hand
    const bool shoulder = vr_adgun_hose_route.value != 0.f;
    // (each hose's outlet a little apart from the others': not one inside another)
    const float spread = (static_cast<float>(i % 4) - 1.5f) * 0.006f * m2u;
    h.outlet = back.tank - back.axis * (0.03f * m2u) + bodyRight * spread;
    h.clipped = hand;
    h.clip = shoulder ? torso + bodyRight * (side * 0.14f * m2u) + worldUp * (0.24f * m2u)
                      : torso + bodyRight * (side * 0.22f * m2u) - worldUp * (0.3f * m2u);
    glm::vec3 inletModel, inletDirModel;
    (void)adguns::hoseInlet(variant, inletModel, inletDirModel);
    h.inlet = view::modelPoint(ve, inletModel);
    const glm::vec3 inletDir = view::modelPoint(ve, inletModel + inletDirModel) - h.inlet;
    // A hand's two pieces' lengths: the tank's to the clip a little slack, the clip's to the gun as far as the gun is
    // (stretching: a hose pays out through its clip) with slack, at least 30 cm. A holster's: one piece, slack.
    const float lenA = glm::distance(h.outlet, h.clip) * 1.15f + 0.04f * m2u;
    const float lenB = za::max(glm::distance(h.clip, h.inlet) * 1.15f, 0.3f * m2u);
    const float lenAll = za::max(glm::distance(h.outlet, h.inlet) * 1.2f + 0.08f * m2u, 0.3f * m2u);
    const auto pieceRest = [&](int k) {
        if(!hand)
        {
            return lenAll / static_cast<float>(nodes - 1);
        }
        return k < clipNode ? lenA / static_cast<float>(clipNode) : lenB / static_cast<float>(nodes - 1 - clipNode);
    };

    const bool fresh = !h.live || h.variant != variant || h.p.size() != static_cast<za::SizeT>(nodes) ||
                       glm::distance(back.tank, h.lastTank) > 1.f * m2u;
    if(fresh)
    {
        // Laid along the way: the tank to the clip, the clip to the gun (a holster's: the tank to the gun), sagging.
        h.p.clear();
        h.prev.clear();
        for(int k = 0; k < nodes; k++)
        {
            glm::vec3 a = h.outlet, b = h.inlet;
            float t = static_cast<float>(k) / static_cast<float>(nodes - 1);
            if(hand)
            {
                t = k <= clipNode ? static_cast<float>(k) / static_cast<float>(clipNode)
                                  : static_cast<float>(k - clipNode) / static_cast<float>(nodes - 1 - clipNode);
                a = k <= clipNode ? h.outlet : h.clip;
                b = k <= clipNode ? h.clip : h.inlet;
            }
            const glm::vec3 q = glm::mix(a, b, t) - worldUp * (sinf(t * 3.14159265f) * 0.05f * m2u);
            h.p.pushBack(q);
            h.prev.pushBack(q);
        }
        h.lastDt = 0.f;
        h.time = vr_gametime;
        h.lastTank = back.tank;
    }
    h.live = true;
    h.variant = variant;
    const float dt = fresh ? 1.f / 90.f : static_cast<float>(za::clamp(vr_gametime - h.time, 1.0 / 500.0, 1.0 / 20.0));
    if(!fresh && vr_gametime == h.time)
    {
        return; // (paused: no step)
    }

    // The body's movement carries the hose along whole (it is all on the body); the gun's own swings it.
    const glm::vec3 carriedBy = back.tank - h.lastTank;
    for(int k = 1; k < nodes - 1; k++)
    {
        h.p[static_cast<za::SizeT>(k)] += carriedBy;
        h.prev[static_cast<za::SizeT>(k)] += carriedBy;
    }
    za::Vector<char>& pinned = scratch.pinned;
    pinned.clear();
    pinned.resize(static_cast<za::SizeT>(nodes), static_cast<char>(0));
    pinned[0] = 1;
    pinned[static_cast<za::SizeT>(nodes - 1)] = 1;
    h.p[0] = h.outlet;
    if(hand)
    {
        pinned[static_cast<za::SizeT>(clipNode)] = 1;
        h.p[static_cast<za::SizeT>(clipNode)] = h.clip;
    }
    h.p.back() = h.inlet;
    // Leaving the gun along its inlet's way (a stiff stub at each end).
    h.p[static_cast<za::SizeT>(nodes - 2)] = h.inlet + glm::normalize(inletDir + glm::vec3{1e-6f}) * pieceRest(nodes - 2);
    pinned[static_cast<za::SizeT>(nodes - 2)] = 1;
    h.p[1] = h.outlet - back.axis * pieceRest(0);
    pinned[1] = 1;
    verlet::fall(h.p.data(), h.prev.data(), pinned.data(), static_cast<za::SizeT>(nodes), dt, h.lastDt, 2.5f, 9.81f * m2u);
    za::Vector<float>& rests = scratch.rests;
    rests.clear();
    for(int k = 0; k + 1 < nodes; k++)
    {
        rests.pushBack(pieceRest(k));
    }
    for(int pass = 0; pass < 3; pass++)
    {
        verlet::holdLengths(h.p.data(), pinned.data(), static_cast<za::SizeT>(nodes), rests.data(), 6, 1.f);
        for(int k = 0; k < nodes; k++)
        {
            if(!pinned[static_cast<za::SizeT>(k)])
            {
                pushOutOfTorso(h.p[static_cast<za::SizeT>(k)], torso, torso.z - 0.55f * m2u, torso.z + 0.22f * m2u,
                    (0.2f + hoseRadius) * m2u, (0.14f + hoseRadius) * m2u);
            }
        }
        if(armsCollide)
        {
            pushOutOfArms(h.p.data(), pinned.data(), nodes);
        }
    }
    if(hand)
    {
        measureArms(i, h.p.data(), pinned.data(), nodes);
    }
    h.lastDt = dt;
    h.time = vr_gametime;
    h.lastTank = back.tank;
    coil::Style style;
    style.turns = 0;
    style.wireRadius = hoseRadius;
    style.albedo = hoseColour(back.variant);
    h.cord.shape(h.p.data(), h.p.size(), style);
}

// A client entity as the view draws it (its networked scale and offset), for modelPoint; false if it is not there now.
[[nodiscard]] bool drawnEntity(int num, view::ViewEntity& out)
{
    if(num <= 0 || num >= cl.num_entities)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    if(!e.model || e.model->type != mod_alias || e.msgtime != cl.mtime[0])
    {
        return false;
    }
    out = view::ViewEntity{};
    out.ent = e;
    out.visible = true;
    out.netEntity = num;
    return true;
}

// The world hose of the pair (`gun`, `tank`) lying about, its rope `rope` units at most, stepped into `h`: from the
// tank's outlet to the gun's inlet, as long as the rope and its ends' reach from the bodies' middles (never stretched),
// falling, lying on the world (Verlet's collide). Left as it lies once its ends have stayed still a while.
// A world line (a hose, a flail's chain lying about) of `length` units from `outlet` (leaving it along `outletDir`) to
// `inlet` (`inletDir`), `radius` metres thick, stepped into `h` and shaped in `style`.
void stepLine(Hose& h, int gun, int tank, int variant, const glm::vec3& outlet, const glm::vec3& outletDir, const glm::vec3& inlet,
    const glm::vec3& inletDir, float length, float radius, const coil::Style& style, const WornBody* body = nullptr);

void stepWorld(Hose& h, int gun, int tank, float rope, const view::ViewEntity& g, const view::ViewEntity& t,
    const WornBody* body = nullptr)
{
    const int variant = adguns::variantOf(g.ent.model->name);
    glm::vec3 inletModel, inletDirModel;
    (void)adguns::hoseInlet(variant, inletModel, inletDirModel);
    const glm::vec3 inlet = view::modelPoint(g, inletModel);
    const glm::vec3 inletDir = view::modelPoint(g, inletModel + inletDirModel) - inlet;
    const glm::vec3 outletModel = adguns::tankOutlet();
    const glm::vec3 outlet = view::modelPoint(t, outletModel);
    const glm::vec3 outletDir = view::modelPoint(t, outletModel - glm::vec3{0.f, 0.f, 1.f}) - outlet;
    const glm::vec3 gunMid{g.ent.origin[0], g.ent.origin[1], g.ent.origin[2]}, tankMid{t.ent.origin[0], t.ent.origin[1], t.ent.origin[2]};
    const float length = (rope + glm::distance(gunMid, inlet) + glm::distance(tankMid, outlet)) * 1.02f;
    coil::Style style;
    style.turns = 0;
    style.wireRadius = hoseRadius;
    style.albedo = hoseColour(variant);
    stepLine(h, gun, tank, variant, outlet, outletDir, inlet, inletDir, length, hoseRadius, style, body);
}

// A flail lying about's chain (QC vr_ad_melee.qc "The flail lying about"): from the head's ring to the handle's tip, as
// long as its rope (vr_box3d.cpp syncPropRopes: tip to ring), the hands' chain's links (vr_flail.cpp).
void stepFlail(Hose& h, int handle, int head, float rope, const view::ViewEntity& g, const view::ViewEntity& t)
{
    glm::vec3 tip, tipDir, ring, ringDir;
    if(!flail::lyingEnds(g, t, tip, tipDir, ring, ringDir))
    {
        hide(h);
        return;
    }
    stepLine(h, handle, head, flailVariant, ring, ringDir, tip, tipDir, rope * 1.02f, flailChainRadius, flail::chainStyle());
}

void stepLine(Hose& h, int gun, int tank, int variant, const glm::vec3& outlet, const glm::vec3& outletDir, const glm::vec3& inlet,
    const glm::vec3& inletDir, float length, float radius, const coil::Style& style, const WornBody* body)
{
    const float m2u = units::metresToUnits();
    const float rest = length / static_cast<float>(worldNodes - 1);
    const bool fresh = !h.live || h.gun != gun || h.tank != tank || h.variant != variant ||
                       h.p.size() != static_cast<za::SizeT>(worldNodes) || glm::distance(outlet, h.lastOutlet) > 2.f * m2u ||
                       glm::distance(inlet, h.lastInlet) > 2.f * m2u;
    if(fresh)
    {
        // Laid from the tank to the gun, sagging (the slack's middle hangs, then falls onto the floor).
        h.p.clear();
        h.prev.clear();
        for(int k = 0; k < worldNodes; k++)
        {
            const float s = static_cast<float>(k) / static_cast<float>(worldNodes - 1);
            const glm::vec3 q = glm::mix(outlet, inlet, s);
            h.p.pushBack(q);
            h.prev.pushBack(q);
        }
        h.touching.clear();
        h.touching.resize(static_cast<za::SizeT>(worldNodes), static_cast<char>(0));
        h.lastDt = 0.f;
        h.time = vr_gametime;
        h.still = 0;
    }
    const bool moved = fresh || glm::distance(outlet, h.lastOutlet) > 0.05f || glm::distance(inlet, h.lastInlet) > 0.05f ||
                       h.length != length;
    h.still = moved ? 0 : h.still + 1;
    h.live = true;
    h.variant = variant;
    h.gun = gun;
    h.tank = tank;
    h.length = length;
    h.lastOutlet = outlet;
    h.lastInlet = inlet;
    const float dt = fresh ? 1.f / 90.f : static_cast<float>(za::clamp(vr_gametime - h.time, 1.0 / 500.0, 1.0 / 20.0));
    if((!fresh && vr_gametime == h.time) || h.still > 90)
    {
        return; // (paused, or lying still: as it lies)
    }
    za::Vector<char>& pinned = scratch.pinned;
    pinned.clear();
    pinned.resize(static_cast<za::SizeT>(worldNodes), static_cast<char>(0));
    pinned[0] = pinned[1] = 1;
    pinned[static_cast<za::SizeT>(worldNodes - 2)] = pinned[static_cast<za::SizeT>(worldNodes - 1)] = 1;
    h.p[0] = outlet;
    h.p[1] = outlet + glm::normalize(outletDir + glm::vec3{1e-6f}) * rest;
    h.p.back() = inlet;
    h.p[static_cast<za::SizeT>(worldNodes - 2)] = inlet + glm::normalize(inletDir + glm::vec3{1e-6f}) * rest;
    h.from.clear();
    for(const glm::vec3& q : h.p)
    {
        h.from.pushBack(q);
    }
    verlet::fall(h.p.data(), h.prev.data(), pinned.data(), static_cast<za::SizeT>(worldNodes), dt, h.lastDt, 2.5f, 9.81f * m2u);
    za::Vector<float>& rests = scratch.rests;
    rests.clear();
    rests.resize(static_cast<za::SizeT>(worldNodes - 1), rest);
    verlet::holdLengths(h.p.data(), pinned.data(), static_cast<za::SizeT>(worldNodes), rests.data(), 30, 1.f);
    for(int pass = 0; body && pass < 3; pass++)
    {
        // (an armed monster's: round its torso, from its back tank to its gun in front)
        for(int k = 0; k < worldNodes; k++)
        {
            if(!pinned[static_cast<za::SizeT>(k)])
            {
                pushOutOfTorso(h.p[static_cast<za::SizeT>(k)], body->axis, body->bottom, body->top, body->side, body->depth,
                    body->fwd, body->right);
            }
        }
        verlet::holdLengths(h.p.data(), pinned.data(), static_cast<za::SizeT>(worldNodes), rests.data(), 8, 1.f);
    }
    verlet::collide(h.p.data(), h.prev.data(), h.from.data(), pinned.data(), static_cast<za::SizeT>(worldNodes), radius * m2u,
        0.2f, nullptr, h.touching.data());
    h.lastDt = dt;
    h.time = vr_gametime;
    h.cord.shape(h.p.data(), h.p.size(), style);
}

// The world hoses: each rope the server holds (the nearest worldHoseMax within worldReach of the eyes) a hose.
void setupWorld(const hands::State& s)
{
    const glm::vec3 eye = 0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]);
    struct Pair
    {
        int gun, tank;
        float rope, dist;
        bool flail; // a flail lying about's chain (gun its handle, tank its head)
        int wearer; // an armed monster's back tank and the gun it holds: the monster (0: lying about)
    };
    Pair pairs[worldHoseMax];
    int count = 0;
    const auto consider = [&](int gun, int tank, float rope, bool flail, int wearer) {
        if(tank <= 0 || tank >= cl.num_entities)
        {
            return;
        }
        const float d = glm::distance(eye, glm::vec3{cl_entities[tank].origin[0], cl_entities[tank].origin[1], cl_entities[tank].origin[2]});
        if(d > worldReach)
        {
            return;
        }
        int at = count < worldHoseMax ? count++ : -1;
        if(at < 0)
        {
            // (full: the farthest given way)
            int farthest = 0;
            for(int k = 1; k < worldHoseMax; k++)
            {
                farthest = pairs[k].dist > pairs[farthest].dist ? k : farthest;
            }
            at = pairs[farthest].dist > d ? farthest : -1;
        }
        if(at >= 0)
        {
            pairs[at] = Pair{gun, tank, rope, d, flail, wearer};
        }
    };
    if(cl.worldmodel && sv.active && vr_adgun_tank_world.value != 0.f)
    {
        box3d::forEachFuelRope([&](int gun, int tank, float rope, bool) { consider(gun, tank, rope, false, 0); });
    }
    if(cl.worldmodel && sv.active && vr_flail_world.value != 0.f)
    {
        box3d::forEachFlailRope([&](int handle, int head, float rope, bool, float) { consider(handle, head, rope, true, 0); });
    }
    const float m2u = units::metresToUnits();
    for(int k = 0; k < wornCount; k++)
    {
        // (its "rope": round the body from the back to the gun in front, as far as they are apart and some)
        const Worn& w = worn[k];
        if(w.gun > 0 && w.gun < cl.num_entities && w.tank < cl.num_entities)
        {
            const entity_t& g = cl_entities[w.gun];
            const entity_t& t = cl_entities[w.tank];
            const float apart = glm::distance(glm::vec3{g.origin[0], g.origin[1], g.origin[2]}, glm::vec3{t.origin[0], t.origin[1], t.origin[2]});
            consider(w.gun, w.tank, apart * 1.15f + 0.12f * m2u, false, w.wearer);
        }
    }
    const int frame = host_framecount;
    for(int k = 0; k < count; k++)
    {
        view::ViewEntity g, t;
        if(!drawnEntity(pairs[k].gun, g) || !drawnEntity(pairs[k].tank, t) ||
            (!pairs[k].flail && !fuelled(adguns::variantOf(g.ent.model->name))))
        {
            continue;
        }
        // Its hose of the last frame, else a free one.
        Hose* h = nullptr;
        for(Hose& x : worldHoses)
        {
            if(x.live && x.gun == pairs[k].gun && x.tank == pairs[k].tank)
            {
                h = &x;
                break;
            }
        }
        for(int f = 0; !h && f < worldHoseMax; f++)
        {
            h = worldHoses[f].live || worldHoses[f].seen == frame ? nullptr : &worldHoses[f];
        }
        if(!h)
        {
            continue;
        }
        h->seen = frame;
        h->lying = !pairs[k].flail && pairs[k].wearer <= 0;
        if(pairs[k].flail)
        {
            stepFlail(*h, pairs[k].gun, pairs[k].tank, pairs[k].rope, g, t);
        }
        else if(pairs[k].wearer > 0 && pairs[k].wearer < cl.num_entities)
        {
            // Round its torso: an upright ellipse from its chest's front (8 units ahead of its origin) to its tank's
            // front, as wide as its shoulders, from under the tank's bottom to over its top.
            const entity_t& m = cl_entities[pairs[k].wearer];
            WornBody b;
            glm::vec3 fwd, right, up;
            hands::angleVectors({0.f, m.angles[1], 0.f}, fwd, right, up);
            b.fwd = glm::normalize(glm::vec3{fwd.x, fwd.y, 0.f});
            b.right = glm::normalize(glm::vec3{right.x, right.y, 0.f});
            const glm::vec3 o{m.origin[0], m.origin[1], m.origin[2]};
            const glm::vec3 tm{t.ent.origin[0], t.ent.origin[1], t.ent.origin[2]};
            const float backFace = glm::dot(tm - o, b.fwd) + (tankRadius * m2u + 0.5f);
            const float front = 8.f;
            b.depth = za::max((front - backFace) * 0.5f, 4.f) + hoseRadius * m2u;
            b.axis = o + b.fwd * ((front + backFace) * 0.5f);
            b.side = 9.f + hoseRadius * m2u;
            b.bottom = tm.z - tankHeight * 0.5f * m2u - 4.f;
            b.top = tm.z + tankHeight * 0.5f * m2u + 6.f;
            stepWorld(*h, pairs[k].gun, pairs[k].tank, pairs[k].rope, g, t, &b);
        }
        else
        {
            stepWorld(*h, pairs[k].gun, pairs[k].tank, pairs[k].rope, g, t);
        }
        setupHoses++;
    }
    for(Hose& h : worldHoses)
    {
        if(h.live && h.seen != frame)
        {
            hide(h);
        }
    }
}

// A hose's rings built (once a frame) and drawn.
void drawHose(Hose& h, bool cloned, const glm::mat4& mirror)
{
    if(!h.live || !h.cord.visible())
    {
        return;
    }
    if(h.builtFrame != host_framecount)
    {
        h.builtFrame = host_framecount;
        QVR_PROFILE("ad hoses build");
        const double t0 = Sys_DoubleTime();
        const hands::State& s = hands::current();
        const bool built = h.cord.build(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]), scratch.rings, h.sides);
        h.hoseRings = built ? gfx::uploadTube(scratch.rings) : gfx::TubeBatch{};
        h.hoseMirror = built && cloned ? (reflect(scratch.rings, mirror, scratch.mirrored), gfx::uploadTube(scratch.mirrored)) : gfx::TubeBatch{};
        buildCost = buildCost * 0.9 + (Sys_DoubleTime() - t0) * 0.1;
    }
    QVR_GPU_PROFILE("ad hoses draw");
    const glm::vec3 albedo = h.cord.albedo();
    const glm::vec3 rust = h.cord.flat() ? h.cord.rust() : glm::vec3{0.f}; // (a flail's chain: rusted links, faceted)
    if(h.hoseRings.count)
    {
        gfx::drawTube(h.hoseRings, h.sides, albedo, keyLight, rust, h.cord.flat());
    }
    if(h.hoseMirror.count)
    {
        gfx::drawTube(h.hoseMirror, h.sides, albedo, keyLight, rust, h.cord.flat());
    }
}

[[nodiscard]] bool hoseCutEdict(int num);

// The tanks drawn this frame near the eyes (lying about, on an armed monster's back) remembered: where, which way up.
// (Not one cut free of its gun: no gun taken flies it to the back.)
void updateSeen(const glm::vec3& eye)
{
    const float m2u = units::metresToUnits();
    const double now = vr_gametime;
    for(int n = 1; n < cl.num_entities; n++)
    {
        const entity_t& e = cl_entities[n];
        if(!e.model || e.model->type != mod_alias || strncmp(e.model->name, "progs/vr_fueltank", 17) != 0 ||
            e.msgtime != cl.mtime[0])
        {
            continue;
        }
        const int variant = adguns::tankVariantOf(e.model->name);
        view::ViewEntity ve;
        if(variant < 0 || !drawnEntity(n, ve) ||
            glm::distance(eye, glm::vec3{e.origin[0], e.origin[1], e.origin[2]}) > 6.f * m2u || hoseCutEdict(n))
        {
            continue;
        }
        SeenTank* slot = nullptr;
        for(SeenTank& t : seenTanks)
        {
            slot = t.num == n ? &t : slot;
        }
        for(SeenTank& t : seenTanks)
        {
            slot = !slot && t.seen < now - 1.0 ? &t : slot; // (a free one: none drawn within the last second)
        }
        if(!slot)
        {
            continue;
        }
        slot->num = n;
        slot->variant = variant;
        slot->bottom = view::modelPoint(ve, glm::vec3{0.f, 0.f, -tankHeight * 0.5f * m2u});
        const glm::vec3 top = view::modelPoint(ve, glm::vec3{0.f, 0.f, tankHeight * 0.5f * m2u});
        slot->axis = glm::distance(top, slot->bottom) > 1e-3f ? glm::normalize(top - slot->bottom) : glm::vec3{0.f, 0.f, 1.f};
        slot->seen = now;
    }
}

// Gun `variant` just taken into a hand at `gunAt`: the tank drawn there lately (the nearest of its kind within 1.6 m,
// drawn within the last 0.3 s) flies to the back.
void startFlight(int variant, const glm::vec3& gunAt)
{
    const float m2u = units::metresToUnits();
    const double now = vr_gametime;
    SeenTank* best = nullptr;
    float bestD = 1.6f * m2u;
    for(SeenTank& t : seenTanks)
    {
        const float d = glm::distance(t.bottom, gunAt);
        if(t.variant == variant && t.seen >= now - 0.3 && d < bestD)
        {
            best = &t;
            bestD = d;
        }
    }
    if(!best || vr_adgun_tank_pickup_time.value <= 0.f)
    {
        return;
    }
    flight.on = true;
    flight.own = !back.live;
    flight.variant = variant;
    flight.from = best->bottom;
    flight.fromAxis = best->axis;
    flight.start = now;
    flight.flights++;
    flight.firstGap = -1.f;
    best->seen = -1.0; // (taken: not another hand's too)
}

// The flight now, from where the tank was to `target` (the back tank's bottom, upright): its place and its up; false
// once it has arrived.
bool flyNow(const glm::vec3& target, glm::vec3& at, glm::vec3& axis)
{
    const float dur = vr_adgun_tank_pickup_time.value;
    const float t = dur > 0.f ? static_cast<float>(vr_gametime - flight.start) / dur : 1.f;
    if(!flight.on || t >= 1.f || t < 0.f)
    {
        flight.on = false;
        return false;
    }
    const float m2u = units::metresToUnits();
    const float e = t * t * (3.f - 2.f * t);
    const glm::vec3 up{0.f, 0.f, 1.f};
    at = glm::mix(flight.from, target, e) + up * (sinf(t * 3.14159265f) * 0.1f * m2u);
    const glm::vec3 a = glm::mix(flight.fromAxis, up, e);
    axis = glm::dot(a, a) > 1e-6f ? glm::normalize(a) : up;
    if(flight.firstGap < 0.f)
    {
        flight.firstGap = glm::distance(at, flight.from);
    }
    return true;
}

// The hose cut's stubs (Vittorio, 2026-10-24; QC vr_ad_enemyguns.qc "Cutting the hose"): a gun lying about whose hose a
// blade cut and the tank cut free of it (.vr_hosecut, the time of the cut) each keep a short free end of the hose, at the
// gun's inlet and the tank's outlet, swinging with it (Verlet, as the world hoses: falling, lying on the world). The
// nearest stubMax within worldReach of the eyes (stubs, above).

// Whether server edict `num` is a cut piece (its stub drawn; a cut tank is no gun's tank: the pickup's flight skips it).
[[nodiscard]] bool hoseCutEdict(int num)
{
    const int cut = progs::fields().vr_hosecut;
    if(!sv.active || cut < 0 || num <= svs.maxclients || num >= sv.qcvm.num_edicts)
    {
        return false;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    edict_t* e = EDICT_NUM(num);
    const bool isCut = !e->free && progs::fieldFloat(e, cut) > 0.f;
    PR_PopQCVM(old);
    return isCut;
}

// Stub `h` of entity `num` (hose `variant`'s colour) from `end`, leaving it along `dir`.
void stepStub(Hose& h, int num, int variant, const glm::vec3& end, const glm::vec3& dir)
{
    const float m2u = units::metresToUnits();
    const float rest = stubLength * m2u / static_cast<float>(stubNodes - 1);
    const glm::vec3 way = glm::normalize(dir + glm::vec3{1e-6f});
    const bool fresh = !h.live || h.gun != num || h.p.size() != static_cast<za::SizeT>(stubNodes) ||
                       glm::distance(end, h.lastOutlet) > 2.f * m2u;
    if(fresh)
    {
        // (laid out the way it leaves the body; then it falls)
        h.p.clear();
        h.prev.clear();
        for(int k = 0; k < stubNodes; k++)
        {
            const glm::vec3 q = end + way * (rest * static_cast<float>(k));
            h.p.pushBack(q);
            h.prev.pushBack(q);
        }
        h.touching.clear();
        h.touching.resize(static_cast<za::SizeT>(stubNodes), static_cast<char>(0));
        h.lastDt = 0.f;
        h.time = vr_gametime;
        h.still = 0;
    }
    const bool moved = fresh || glm::distance(end, h.lastOutlet) > 0.05f || glm::distance(way, h.lastInlet) > 0.01f;
    h.still = moved ? 0 : h.still + 1;
    h.live = true;
    h.lying = false;
    h.variant = variant;
    h.gun = num;
    h.tank = 0;
    h.length = stubLength * m2u;
    h.lastOutlet = end;
    h.lastInlet = way; // (a stub's: the way its end leaves the body)
    const float dt = fresh ? 1.f / 90.f : static_cast<float>(za::clamp(vr_gametime - h.time, 1.0 / 500.0, 1.0 / 20.0));
    if((!fresh && vr_gametime == h.time) || h.still > 90)
    {
        return; // (paused, or lying still: as it lies)
    }
    za::Vector<char>& pinned = scratch.pinned;
    pinned.clear();
    pinned.resize(static_cast<za::SizeT>(stubNodes), static_cast<char>(0));
    pinned[0] = pinned[1] = 1;
    h.p[0] = end;
    h.p[1] = end + way * rest;
    h.from.clear();
    for(const glm::vec3& q : h.p)
    {
        h.from.pushBack(q);
    }
    verlet::fall(h.p.data(), h.prev.data(), pinned.data(), static_cast<za::SizeT>(stubNodes), dt, h.lastDt, 2.5f, 9.81f * m2u);
    za::Vector<float>& rests = scratch.rests;
    rests.clear();
    rests.resize(static_cast<za::SizeT>(stubNodes - 1), rest);
    verlet::holdLengths(h.p.data(), pinned.data(), static_cast<za::SizeT>(stubNodes), rests.data(), 20, 1.f);
    verlet::collide(h.p.data(), h.prev.data(), h.from.data(), pinned.data(), static_cast<za::SizeT>(stubNodes), hoseRadius * m2u,
        0.2f, nullptr, h.touching.data());
    h.lastDt = dt;
    h.time = vr_gametime;
    coil::Style style;
    style.turns = 0;
    style.wireRadius = hoseRadius;
    style.albedo = hoseColour(variant);
    h.cord.shape(h.p.data(), h.p.size(), style);
}

// The cut pieces near the eyes found on the server (a listen server's) and their stubs stepped.
void setupStubs(const glm::vec3& eye)
{
    stubsDrawn = 0;
    const int cut = progs::fields().vr_hosecut;
    int nums[stubMax];
    float dists[stubMax];
    int count = 0;
    if(sv.active && cl.worldmodel && cut >= 0)
    {
        qcvm_t* old = nullptr;
        PR_PushQCVM(&sv.qcvm, &old);
        for(int n = svs.maxclients + 1; n < qcvm->num_edicts && n < cl.num_entities; n++)
        {
            edict_t* e = EDICT_NUM(n);
            if(e->free || !e->v.modelindex || progs::fieldFloat(e, cut) <= 0.f)
            {
                continue;
            }
            const float d = glm::distance(eye, glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]});
            if(d > worldReach)
            {
                continue;
            }
            int at = count < stubMax ? count++ : -1;
            if(at < 0)
            {
                int farthest = 0;
                for(int k = 1; k < stubMax; k++)
                {
                    farthest = dists[k] > dists[farthest] ? k : farthest;
                }
                at = dists[farthest] > d ? farthest : -1;
            }
            if(at >= 0)
            {
                nums[at] = n;
                dists[at] = d;
            }
        }
        PR_PopQCVM(old);
    }
    const int frame = host_framecount;
    for(int k = 0; k < count; k++)
    {
        view::ViewEntity ve;
        if(!drawnEntity(nums[k], ve))
        {
            continue;
        }
        int variant = adguns::tankVariantOf(ve.ent.model->name);
        glm::vec3 end, dir;
        if(variant >= 0)
        {
            const glm::vec3 om = adguns::tankOutlet();
            end = view::modelPoint(ve, om);
            dir = view::modelPoint(ve, om - glm::vec3{0.f, 0.f, 1.f}) - end;
        }
        else
        {
            variant = adguns::variantOf(ve.ent.model->name);
            glm::vec3 im, idm;
            if(!fuelled(variant) || !adguns::hoseInlet(variant, im, idm))
            {
                continue;
            }
            end = view::modelPoint(ve, im);
            dir = view::modelPoint(ve, im + idm) - end;
        }
        Hose* h = nullptr;
        for(Hose& x : stubs)
        {
            h = !h && x.live && x.gun == nums[k] ? &x : h;
        }
        for(Hose& x : stubs)
        {
            h = !h && !x.live && x.seen != frame ? &x : h;
        }
        if(!h)
        {
            continue;
        }
        h->seen = frame;
        stepStub(*h, nums[k], variant, end, dir);
        stubsDrawn++;
    }
    for(Hose& h : stubs)
    {
        if(h.live && h.seen != frame)
        {
            hide(h);
        }
    }
}

} // namespace

int crossHose(int tank, const glm::vec3& p0, const glm::vec3& p1, float radius, glm::vec3& at, float& share)
{
    float best = radius + hoseRadius * units::metresToUnits();
    int found = -2;
    for(const Hose& h : worldHoses)
    {
        if(!h.live || !h.lying || h.tank != tank || h.p.size() < 2)
        {
            continue;
        }
        found = found < 0 ? -1 : found;
        const za::SizeT n = h.p.size();
        for(za::SizeT k = 0; k + 1 < n; k++)
        {
            float s, t;
            closestSegments(p0, p1, h.p[k], h.p[k + 1], s, t);
            const glm::vec3 q = h.p[k] + (h.p[k + 1] - h.p[k]) * t;
            const float d = glm::distance(p0 + (p1 - p0) * s, q);
            if(d < best)
            {
                best = d;
                found = 1;
                at = q;
                share = (static_cast<float>(k) + t) / static_cast<float>(n - 1);
            }
        }
    }
    return found;
}

void relinkWorn()
{
    wornCount = 0;
    const int link = progs::fields().vr_fuellink;
    if(!sv.active || !cl.worldmodel || link < 0 || vr_adgun_tank_worn.value == 0.f)
    {
        return;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    for(int n = svs.maxclients + 1; n < qcvm->num_edicts && wornCount < wornMax; n++)
    {
        edict_t* e = EDICT_NUM(n);
        if(e->free || !e->v.modelindex || strcmp(PR_GetString(e->v.classname), "vr_fueltank_worn") != 0)
        {
            continue;
        }
        const int wearer = e->v.owner ? NUM_FOR_EDICT(PROG_TO_EDICT(e->v.owner)) : 0;
        const int ofs = progs::fieldInt(e, link);
        const int gun = ofs ? NUM_FOR_EDICT(PROG_TO_EDICT(ofs)) : 0;
        if(wearer <= 0 || wearer >= cl.num_entities || n >= cl.num_entities)
        {
            continue;
        }
        // Its place on its wearer as the server has it, carried onto the wearer as drawn (its movement smoothed, its
        // turn): on the drawn body, as its weapon is on the drawn hands (vr_hitreact.cpp relinkArms).
        entity_t& ce = cl_entities[n];
        const entity_t& cm = cl_entities[wearer];
        const edict_t* m = EDICT_NUM(wearer);
        if(ce.model && cm.model && ce.msgtime == cl.mtime[0] && cm.msgtime == cl.mtime[0])
        {
            const float turn = (cm.angles[1] - m->v.angles[1]) * (3.14159265f / 180.f);
            const float c = cosf(turn), s = sinf(turn);
            const float dx = e->v.origin[0] - m->v.origin[0], dy = e->v.origin[1] - m->v.origin[1];
            ce.origin[0] = cm.origin[0] + dx * c - dy * s;
            ce.origin[1] = cm.origin[1] + dx * s + dy * c;
            ce.origin[2] = cm.origin[2] + (e->v.origin[2] - m->v.origin[2]);
            ce.angles[0] = e->v.angles[0];
            ce.angles[1] = e->v.angles[1] + (cm.angles[1] - m->v.angles[1]);
            ce.angles[2] = e->v.angles[2];
        }
        worn[wornCount++] = Worn{n, gun, wearer};
    }
    PR_PopQCVM(old);
}

void init()
{
    Cmd_AddCommand("vr_adgun_hose_info", info_f);
}

void setupView(const hands::State& s)
{
    QVR_PROFILE("ad hoses");
    const double t0 = Sys_DoubleTime();
    const bool living = cl.stats[STAT_HEALTH] > 0 && !cl.intermission && s.valid;
    for(int h = 0; h < 2; h++)
    {
        pilotFlames(h, living ? view::heldWeapon(h) : nullptr);
    }
    setupHoses = 0;
    if(s.valid)
    {
        setupWorld(s);
        setupStubs(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]));
        updateSeen(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]));
    }
    // What is carried: the hands' guns, the holsters'.
    const bool alive = living && vr_adgun_hose.value != 0.f;
    const bool holstered = vr_adgun_hose_holstered.value != 0.f;
    int want[carriedCount];
    const view::ViewEntity* ve[carriedCount];
    bool any = false;
    for(int i = 0; i < carriedCount; i++)
    {
        ve[i] = !alive ? nullptr : i < 2 ? view::heldWeapon(i) : holstered ? view::holsterGun(i - 2) : nullptr;
        const int v = ve[i] && ve[i]->ent.model ? adguns::variantOf(ve[i]->ent.model->name) : -1;
        want[i] = fuelled(v) && !tankBurst(i) ? v : -1;
        any = any || want[i] >= 0;
    }
    // The tank's colour: the gun last taken into a hand (the main hand's, both at once), kept while that kind is carried.
    for(int h = 1; h >= 0; h--)
    {
        const int hand = h == 1 ? 1 - HAND_MAIN : HAND_MAIN; // (the off hand first: the main hand's wins a tie)
        if(want[hand] >= 0 && lastHeld[hand] != want[hand])
        {
            back.variant = want[hand];
        }
    }
    // A gun newly in a hand, not from a holster nor the other hand: taken from where it lay (or from an armed monster):
    // its tank flies to the back.
    for(int hand = 0; hand < 2; hand++)
    {
        const int v = want[hand];
        if(v < 0 || lastHeld[hand] == v)
        {
            continue;
        }
        bool moved = lastWant[1 - hand] == v + 1 && want[1 - hand] != v;
        for(int i = 2; i < carriedCount; i++)
        {
            moved = moved || (lastWant[i] == v + 1 && want[i] != v);
        }
        if(!moved)
        {
            startFlight(v, glm::vec3{ve[hand]->ent.origin[0], ve[hand]->ent.origin[1], ve[hand]->ent.origin[2]});
        }
    }
    for(int i = 0; i < carriedCount; i++)
    {
        lastWant[i] = want[i] + 1;
    }
    lastHeld[0] = want[0];
    lastHeld[1] = want[1];
    bool kept = false;
    int first = -1;
    for(int i = 0; i < carriedCount; i++)
    {
        kept = kept || (want[i] >= 0 && want[i] == back.variant);
        first = first < 0 ? want[i] : first;
    }
    if(!any)
    {
        for(Hose& h : carried)
        {
            if(h.live)
            {
                hide(h);
            }
        }
        back.live = false;
        back.variant = -1;
        flight.on = false;
        const double cost = Sys_DoubleTime() - t0;
        setupCost = setupCost * 0.9 + cost * 0.1;
        return;
    }
    back.variant = kept ? back.variant : first;
    back.live = true;
    const float m2u = units::metresToUnits();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    bodyFwd = glm::normalize(glm::vec3{fwd.x, fwd.y, 0.f});
    bodyRight = glm::normalize(glm::vec3{right.x, right.y, 0.f});
    // The tank's middle on the back (carried by the chest), the torso's middle line before it.
    const glm::vec3 centre = body::chestAnchor(s, glm::vec3{vr_adgun_tank_x.value, vr_adgun_tank_y.value, vr_adgun_tank_z.value});
    const glm::vec3 torso = centre + bodyFwd * (0.15f * m2u);
    back.tank = centre - glm::vec3{0.f, 0.f, tankHeight * 0.5f * m2u};
    back.axis = glm::vec3{0.f, 0.f, 1.f};
    glm::vec3 flyAt, flyAxis;
    if(flyNow(back.tank, flyAt, flyAxis))
    {
        // (taken just now: flying there from where it lay; the back tank itself, its hoses following, or one more)
        (flight.own ? back.tank : flight.tank) = flyAt;
        (flight.own ? back.axis : flight.axis) = flyAxis;
    }
    gatherArms();
    const bool armsCollide = vr_adgun_hose_arms.value != 0.f;
    for(int i = 0; i < carriedCount; i++)
    {
        if(want[i] < 0)
        {
            if(carried[i].live)
            {
                hide(carried[i]);
            }
            continue;
        }
        setupHoses++;
        stepCarried(i, want[i], *ve[i], torso, armsCollide);
    }
    const double cost = Sys_DoubleTime() - t0;
    setupCost = setupCost * 0.9 + cost * 0.1;
}

void drawOpaque()
{
    glm::mat4 mirror;
    const bool cloned = bodyclone::mirror(mirror);
    for(Hose& h : carried)
    {
        drawHose(h, cloned, mirror);
    }
    for(Hose& h : worldHoses)
    {
        drawHose(h, false, mirror);
    }
    for(Hose& h : stubs)
    {
        drawHose(h, false, mirror);
    }
    if(!back.live)
    {
        return;
    }
    if(back.builtFrame != host_framecount)
    {
        back.builtFrame = host_framecount;
        buildTank(back.tank, back.axis, scratch.rings);
        back.rings = gfx::uploadTube(scratch.rings);
        back.mirror = cloned ? (reflect(scratch.rings, mirror, scratch.mirrored), gfx::uploadTube(scratch.mirrored)) : gfx::TubeBatch{};
        if(flight.on && !flight.own)
        {
            buildTank(flight.tank, flight.axis, scratch.rings);
            flight.rings = gfx::uploadTube(scratch.rings);
        }
    }
    QVR_GPU_PROFILE("ad hoses draw");
    gfx::drawTube(back.rings, 14, tankColour(back.variant), keyLight, hazardBand);
    if(back.mirror.count)
    {
        gfx::drawTube(back.mirror, 14, tankColour(back.variant), keyLight, hazardBand);
    }
    if(flight.on && !flight.own && flight.rings.count)
    {
        gfx::drawTube(flight.rings, 14, tankColour(flight.variant), keyLight, hazardBand); // (one more, flying to join it)
    }
}

} // namespace qvr::adtank

extern "C" void VR_RelinkWornTanks(void)
{
    qvr::adtank::relinkWorn();
}
