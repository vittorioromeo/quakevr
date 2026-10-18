// vr_adtank.cpp -- see vr_adtank.hpp.

#include "vr_adtank.hpp"

#include "vr_adguns.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_bodyclone.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
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

// A hand's hose and tank.
struct Hose
{
    int variant{-1};
    bool live{false};
    za::Vector<glm::vec3> p, prev;
    float lastDt{0.f};
    double time{0.0};
    glm::vec3 lastTank{0.f};
    glm::vec3 tank{0.f};   // the tank's bottom middle (world)
    glm::vec3 outlet{0.f}; // the hose's first end, under the tank
    glm::vec3 clip{0.f};   // the clip's
    glm::vec3 inlet{0.f};  // the gun's
    coil::Cord cord;
    // The draw's (built once a frame, uploaded once, drawn in both eyes; the mirror image's too).
    gfx::TubeBatch hoseRings, tankRings, hoseMirror, tankMirror;
    int sides{0};
    int builtFrame{-1};
};
Hose hoses[2];

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
unsigned pilotSeed = 0x9E3779B9u;
[[nodiscard]] float pilotRandom()
{
    pilotSeed = pilotSeed * 1664525u + 1013904223u;
    return static_cast<float>(pilotSeed >> 8) / static_cast<float>(1u << 24);
}

// `ve` (the pyro's gun, held in hand `i`) burning its pilot light this frame: small flames off its pilot pipe's tip,
// drifting forward and up, now and then a spark. None while paused or under water.
void pilotFlames(int i, const view::ViewEntity* ve)
{
    Pilot& p = pilots[i];
    glm::vec3 tipModel;
    if(!ve || !ve->ent.model || !adguns::pilotLight(adguns::variantOf(ve->ent.model->name), tipModel) || !cl.worldmodel)
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
            particles::hotTrail(tip, fwd * 30.f + wobble * 30.f + glm::vec3{0.f, 0.f, 12.f}, yellow, 0.8f, 0.3f, true);
        }
        else if(r < 0.32f)
        {
            particles::hotTrail(tip + fwd * 0.2f, fwd * 4.f + wobble * 2.f, blue, 0.45f, 0.12f, false);
        }
        else
        {
            particles::hotTrail(tip + fwd * 0.5f, fwd * 9.f + wobble * 4.f + glm::vec3{0.f, 0.f, 5.f}, yellow, 0.5f, 0.2f, false);
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

// The torso as an upright elliptic cylinder round `axis` (its middle line, world), from `bottom` to `top` (world z): a
// point inside pushed out to its surface (sideways semi-axis `side`, forwards `depth`, metres in units).
void pushOutOfTorso(glm::vec3& p, const glm::vec3& axis, float bottom, float top, float side, float depth)
{
    if(p.z < bottom || p.z > top)
    {
        return;
    }
    const glm::vec3 d{p.x - axis.x, p.y - axis.y, 0.f};
    const float x = glm::dot(d, bodyRight) / side, y = glm::dot(d, bodyFwd) / depth;
    const float k = x * x + y * y;
    if(k >= 1.f)
    {
        return;
    }
    const float s = k > 1e-8f ? 1.f / sqrtf(k) : 0.f;
    const glm::vec3 out = k > 1e-8f ? (bodyRight * (x * s * side) + bodyFwd * (y * s * depth)) : bodyRight * side;
    p.x = axis.x + out.x;
    p.y = axis.y + out.y;
}

void hide(Hose& h)
{
    h.live = false;
    h.variant = -1;
    h.cord.hide();
    h.p.clear();
    h.prev.clear();
}

// The tank's rings: an upright bottle (an outlet's neck under it, domes, a valve on top), its band in the hazard colour
// (the rings' rust).
void buildTank(const Hose& h, za::Vector<gfx::TubeRing>& out)
{
    out.clear();
    const float m2u = units::metresToUnits();
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec3 light = coil::lightAt(h.tank + up * (tankHeight * 0.5f * m2u));
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
        ring.mid = glm::vec4{h.tank + up * (a[0] * m2u), za::max(a[1] * r * m2u, 1e-4f)};
        ring.across = glm::vec4{bodyRight, 0.f};
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

// vr_adgun_hose_info: each hand's hose (its gun, its ends, its length and sag, how far into the torso its worst point
// lies), and the cost.
void info_f()
{
    Con_Printf("adgun hose: on %g, route %s, tank at %g %g %g, %.3f ms a frame stepping %d hose(s), %.3f ms building their "
               "rings, %.1f units a metre\n",
        vr_adgun_hose.value, vr_adgun_hose_route.value != 0.f ? "shoulder" : "hip", vr_adgun_tank_x.value, vr_adgun_tank_y.value,
        vr_adgun_tank_z.value, setupCost * 1000.0, setupHoses, buildCost * 1000.0, units::metresToUnits());
    Con_Printf("adgun pilot light: %d and %d particles made (main, off hand)\n", pilots[HAND_MAIN].made, pilots[1 - HAND_MAIN].made);
    for(int i = 0; i < 2; i++)
    {
        const Hose& h = hoses[i];
        const char* hand = i == HAND_MAIN ? "main" : "off";
        if(!h.live || h.p.size() != static_cast<za::SizeT>(nodes))
        {
            Con_Printf("adgun hose %s: none\n", hand);
            continue;
        }
        float length = 0.f, lowest = 1e9f;
        for(za::SizeT k = 0; k + 1 < h.p.size(); k++)
        {
            length += glm::distance(h.p[k], h.p[k + 1]);
            lowest = za::min(lowest, h.p[k].z);
        }
        const float m2u = units::metresToUnits();
        Con_Printf("adgun hose %s: variant %d, tank %.1f %.1f %.1f, outlet %.2f, clip %.2f, inlet %.2f (units off), length %.2f m, "
                   "lowest %.2f m under the inlet, %d rings\n",
            hand, h.variant, h.tank.x, h.tank.y, h.tank.z, glm::distance(h.p[0], h.outlet), glm::distance(h.p[clipNode], h.clip),
            glm::distance(h.p.back(), h.inlet), length / m2u, (h.inlet.z - lowest) / m2u, h.cord.rings());
        Con_Printf("adgun hose %s: drawn %d hose rings, %d tank rings; in the body's mirror image %d and %d\n", hand,
            static_cast<int>(h.hoseRings.count), static_cast<int>(h.tankRings.count), static_cast<int>(h.hoseMirror.count),
            static_cast<int>(h.tankMirror.count));
    }
}

} // namespace

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
    const bool alive = living && vr_adgun_hose.value != 0.f;
    int want[2]{-1, -1};
    const view::ViewEntity* ve[2]{nullptr, nullptr};
    for(int h = 0; h < 2; h++)
    {
        ve[h] = alive ? view::heldWeapon(h) : nullptr;
        const int v = ve[h] && ve[h]->ent.model ? adguns::variantOf(ve[h]->ent.model->name) : -1;
        want[h] = fuelled(v) ? v : -1;
    }
    if(want[0] < 0 && want[1] < 0)
    {
        for(Hose& h : hoses)
        {
            if(h.live)
            {
                hide(h);
            }
        }
        setupHoses = 0;
        return;
    }
    const float m2u = units::metresToUnits();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    bodyFwd = glm::normalize(glm::vec3{fwd.x, fwd.y, 0.f});
    bodyRight = glm::normalize(glm::vec3{right.x, right.y, 0.f});
    const glm::vec3 worldUp{0.f, 0.f, 1.f};
    const bool both = want[0] >= 0 && want[1] >= 0;
    // The tank's middle on the back (carried by the chest), the torso's middle line before it.
    const glm::vec3 centre = body::chestAnchor(s, glm::vec3{vr_adgun_tank_x.value, vr_adgun_tank_y.value, vr_adgun_tank_z.value});
    const glm::vec3 torso = centre + bodyFwd * (0.15f * m2u);
    const bool shoulder = vr_adgun_hose_route.value != 0.f;
    setupHoses = 0;
    for(int i = 0; i < 2; i++)
    {
        Hose& h = hoses[i];
        if(want[i] < 0)
        {
            if(h.live)
            {
                hide(h);
            }
            continue;
        }
        setupHoses++;
        const float side = i == HAND_MAIN ? 1.f : -1.f; // the body's right for the main hand
        h.tank = centre + bodyRight * (both ? side * 0.075f * m2u : 0.f) - worldUp * (tankHeight * 0.5f * m2u);
        h.outlet = h.tank - worldUp * (0.03f * m2u);
        h.clip = shoulder ? torso + bodyRight * (side * 0.14f * m2u) + worldUp * (0.24f * m2u)
                          : torso + bodyRight * (side * 0.22f * m2u) - worldUp * (0.3f * m2u);
        glm::vec3 inletModel, inletDirModel;
        (void)adguns::hoseInlet(want[i], inletModel, inletDirModel);
        h.inlet = view::modelPoint(*ve[i], inletModel);
        const glm::vec3 inletDir = view::modelPoint(*ve[i], inletModel + inletDirModel) - h.inlet;
        // The two pieces' lengths: the tank's to the clip a little slack, the clip's to the gun as far as the gun is
        // (stretching: a hose pays out through its clip) with slack, at least 30 cm.
        const float lenA = glm::distance(h.outlet, h.clip) * 1.15f + 0.04f * m2u;
        const float lenB = za::max(glm::distance(h.clip, h.inlet) * 1.15f, 0.3f * m2u);
        const auto pieceRest = [&](int k) { return k < clipNode ? lenA / static_cast<float>(clipNode) : lenB / static_cast<float>(nodes - 1 - clipNode); };

        const bool fresh = !h.live || h.variant != want[i] || h.p.size() != static_cast<za::SizeT>(nodes) ||
                           glm::distance(h.tank, h.lastTank) > 1.f * m2u;
        if(fresh)
        {
            // Laid along the way: the tank to the clip, the clip to the gun, sagging a little.
            h.p.clear();
            h.prev.clear();
            for(int k = 0; k < nodes; k++)
            {
                const float t = k <= clipNode ? static_cast<float>(k) / static_cast<float>(clipNode)
                                              : static_cast<float>(k - clipNode) / static_cast<float>(nodes - 1 - clipNode);
                const glm::vec3 a = k <= clipNode ? h.outlet : h.clip, b = k <= clipNode ? h.clip : h.inlet;
                const glm::vec3 q = glm::mix(a, b, t) - worldUp * (sinf(t * 3.14159265f) * 0.05f * m2u);
                h.p.pushBack(q);
                h.prev.pushBack(q);
            }
            h.lastDt = 0.f;
            h.time = vr_gametime;
            h.lastTank = h.tank;
        }
        h.live = true;
        h.variant = want[i];
        const float dt = fresh ? 1.f / 90.f : static_cast<float>(za::clamp(vr_gametime - h.time, 1.0 / 500.0, 1.0 / 20.0));
        if(!fresh && vr_gametime == h.time)
        {
            continue; // (paused: no step)
        }

        // The body's movement carries the hose along whole (it is all on the body); the gun's own swings it.
        const glm::vec3 carried = h.tank - h.lastTank;
        for(int k = 1; k < nodes - 1; k++)
        {
            h.p[static_cast<za::SizeT>(k)] += carried;
            h.prev[static_cast<za::SizeT>(k)] += carried;
        }
        za::Vector<char>& pinned = scratch.pinned;
        pinned.clear();
        pinned.resize(static_cast<za::SizeT>(nodes), static_cast<char>(0));
        pinned[0] = 1;
        pinned[static_cast<za::SizeT>(clipNode)] = 1;
        pinned[static_cast<za::SizeT>(nodes - 1)] = 1;
        h.p[0] = h.outlet;
        h.p[static_cast<za::SizeT>(clipNode)] = h.clip;
        h.p.back() = h.inlet;
        // Leaving the gun along its inlet's way (a stiff stub at each end).
        h.p[static_cast<za::SizeT>(nodes - 2)] = h.inlet + glm::normalize(inletDir + glm::vec3{1e-6f}) * pieceRest(nodes - 2);
        pinned[static_cast<za::SizeT>(nodes - 2)] = 1;
        h.p[1] = h.outlet - worldUp * pieceRest(0);
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
        }
        h.lastDt = dt;
        h.time = vr_gametime;
        h.lastTank = h.tank;
        coil::Style style;
        style.turns = 0;
        style.wireRadius = hoseRadius;
        style.albedo = hoseColour(h.variant);
        h.cord.shape(h.p.data(), h.p.size(), style);
    }
    const double cost = Sys_DoubleTime() - t0;
    setupCost = setupCost * 0.9 + cost * 0.1;
}

void drawOpaque()
{
    glm::mat4 mirror;
    const bool cloned = bodyclone::mirror(mirror);
    for(Hose& h : hoses)
    {
        if(!h.live || !h.cord.visible())
        {
            continue;
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
            buildTank(h, scratch.rings);
            h.tankRings = gfx::uploadTube(scratch.rings);
            h.tankMirror = cloned ? (reflect(scratch.rings, mirror, scratch.mirrored), gfx::uploadTube(scratch.mirrored)) : gfx::TubeBatch{};
            buildCost = buildCost * 0.9 + (Sys_DoubleTime() - t0) * 0.1;
        }
        QVR_GPU_PROFILE("ad hoses draw");
        const glm::vec3 albedo = h.cord.albedo();
        if(h.hoseRings.count)
        {
            gfx::drawTube(h.hoseRings, h.sides, albedo, keyLight);
        }
        if(h.hoseMirror.count)
        {
            gfx::drawTube(h.hoseMirror, h.sides, albedo, keyLight);
        }
        gfx::drawTube(h.tankRings, 14, tankColour(h.variant), keyLight, hazardBand);
        if(h.tankMirror.count)
        {
            gfx::drawTube(h.tankMirror, 14, tankColour(h.variant), keyLight, hazardBand);
        }
    }
}

} // namespace qvr::adtank
