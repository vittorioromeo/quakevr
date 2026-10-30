// vr_particles.cpp -- see vr_particles.hpp. The presets and the per-type behaviours are the old
// engine's (r_part.cpp: R_RunParticle2Effect and friends, CL_RunParticles), with the same
// numbers; the drawing is camera-facing quads of 1.5 x scale units, turned by their angle about
// the view direction, as the old geometry shader built them.

#include "vr_random.hpp"
#include "vr_geom.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_particles.hpp"
#include "vr_tracy.h"
#include "vr_units.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hue.hpp"
#include "vr_jobs.hpp"
#include "vr_lighting.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_text3d.hpp"
#include "vr_flashlight.hpp"
#include "vr_weaponfx.hpp"
#include "vr_lootbeams.hpp"
#include "vr_profile.hpp"
#include "vr_profile_systems.hpp"
#include "vr_retro.hpp"
#include "vr_water.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Limits.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Bitset.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Log.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"
#include "vr_particles_internal.hpp"

#include <string.h>

namespace qvr::retrolight
{
[[nodiscard]] bool on(); // retro lighting (vr_retrolight.cpp)
} // namespace qvr::retrolight

namespace qvr::particles
{
using namespace detail;


NearLimits nearLimits()
{
    const float cm = units::metresToUnits() * 0.01f;
    const float cull = za::max(0.f, vr_particle_near_cull.value) * cm;
    return {cull, za::max(cull, za::max(0.f, vr_particle_near_fade.value) * cm)};
}

namespace detail
{

za::Vector<Particle> pool; // reserved to maxParticles at startup (init): it never reallocates in play
double lastRun = -1.0;

za::FastNonCryptoRng rng{static_cast<za::U64>(za::Clock::nowNanoseconds())}; // a new sequence every run (vr_particle_seed: a fixed one)


// ---- Presets --------------------------------------------------------------------------------

void explosion(const glm::vec3& org)
{
    make(256, [&](Particle& p, int i) {
        p.cell = CellCircle;
        setColor(p, ramp1[0], 255);
        p.die = cl.time + 1.5;
        p.ramp = static_cast<float>(rndi(0, 4));
        p.scale = vrand::range(rng, 0.6f, 1.2f);
        p.acc = gravity(0.5f);
        p.type = i & 1 ? Explode : Explode2;
        p.org = org + glm::vec3{vrand::range(rng, -16, 16), vrand::range(rng, -16, 16), vrand::range(rng, -16, 16)};
        p.vel = {vrand::range(rng, -256, 256), vrand::range(rng, -256, 256), vrand::range(rng, -256, 256)};
    });
    make(64, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, ramp1[0], 255);
        p.die = cl.time + 1.5;
        p.scale = vrand::range(rng, 1.9f, 2.9f) * 0.55f;
        p.acc = gravity(0.5f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{vrand::range(rng, -16, 16), vrand::range(rng, -16, 16), vrand::range(rng, -16, 16)};
        p.vel = {vrand::range(rng, -256, 256), vrand::range(rng, -256, 256), vrand::range(rng, -256, 256)};
    });
    make(48, [&](Particle& p, int) {
        p.cell = CellRock;
        setColor(p, 167 + rndi(0, 8), 255);
        p.die = cl.time + 1.5;
        p.scale = vrand::range(rng, 0.9f, 1.9f);
        p.acc = gravity(0.5f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{vrand::range(rng, -16, 16), vrand::range(rng, -16, 16), vrand::range(rng, -16, 16)};
        p.vel = {vrand::range(rng, -256, 256), vrand::range(rng, -256, 256), vrand::range(rng, -256, 256)};
    });
    make(za::clamp(vr_explosion_particles.value, 0.f, 16.f), [&](Particle& p, int) {
        p.cell = CellExplosion;
        // The fire texture is neutral: tint it as flame, rather than the sparks' sulphur-yellow palette entry.
        p.color = glm::vec4{1.f, 0.48f, 0.12f, 1.f};
        p.die = cl.time + 1.5;
        p.scale = vrand::range(rng, 0.5f, 2.1f) * 2.f;
        p.acc = gravity(0.05f);
        p.type = TxExplode;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{vrand::range(rng, -11, 11), vrand::range(rng, -11, 11), vrand::range(rng, -11, 11)};
        p.vel = {vrand::range(rng, -8, 8), vrand::range(rng, -8, 8), vrand::range(rng, -8, 8)};
    }, false); // Exact large-fireball count; the general particle multiplier controls the other particles.
    make(3, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), 225);
        p.die = cl.time + 3.5;
        p.scale = vrand::range(rng, 1.2f, 1.5f);
        p.type = TxSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {vrand::range(rng, -24, 24), vrand::range(rng, -24, 24), vrand::range(rng, -24, 24)};
    });
}

void bulletPuff(const glm::vec3& org, const glm::vec3& dir, int color, int count)
{
    // Quake's black (a bullet, a nail): grey chips of the wall, not black ones.
    const auto debris = [color]() { return color < 8 ? rndi(6, 13) : (color & ~7) + rndi(0, 8); };
    make(count * 0.7f, [&](Particle& p, int) {
        p.cell = CellRock;
        setColor(p, debris(), 255);
        p.die = cl.time + 0.7 * rndi(0, 5);
        p.scale = vrand::range(rng, 0.5f, 0.9f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(0.26f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = (dir + 0.3f) * glm::vec3{vrand::range(rng, -75, 75), vrand::range(rng, -75, 75), vrand::range(rng, -75, 75)};
    });
    make(1, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), 45);
        p.die = cl.time + 1.25 * rndi(0, 5);
        p.scale = vrand::range(rng, 0.3f, 0.5f);
        p.type = TxSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{rndi(-2, 2), rndi(-2, 2), rndi(-2, 2)};
        p.vel = {vrand::range(rng, -12, 12), vrand::range(rng, -12, 12), vrand::range(rng, -12, 12)};
    });
    make(count * 0.7f, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, debris(), 255);
        p.die = cl.time + 0.75 * rndi(0, 5);
        p.scale = vrand::range(rng, 0.05f, 0.3f);
        p.type = Static;
        p.acc = gravity(0.08f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {vrand::range(rng, -24, 24), vrand::range(rng, -24, 24), vrand::range(rng, -24, 24) + vrand::range(rng, 10, 40)};
    });
    make(count * 0.4f, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, 109 + rndi(0, 8), 255);
        p.die = cl.time + 1.25 * rndi(0, 5);
        p.scale = vrand::range(rng, 1.95f, 2.87f) * 0.35f;
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(1.f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {vrand::range(rng, -48, 48), vrand::range(rng, -48, 48), vrand::range(rng, 60, 360)};
    });
}



// The particles made since `first` in the force grab's hue (vr_forcegrab_hue; by default the
// player's, vr_hue.hpp) and saturation (vr_forcegrab_saturation), each as bright as its palette
// colour was.
void inForceGrabHue(za::SizeT first)
{
    const glm::vec3 c = hue::color(vr_forcegrab_hue, vr_forcegrab_saturation, 0.75f, 1.f);
    for(za::SizeT i = first; i < pool.size(); i++)
    {
        Particle& p = pool[i];
        p.color = glm::vec4{c * za::max(p.color.r, p.color.g, p.color.b), p.color.a};
    }
}

// ---- Simulation -----------------------------------------------------------------------------

// Where plinking drops went back into their liquid this run (their rings, made after it).
struct Plink
{
    float x, y, surface, scale;
    glm::vec4 color;
    int liquid;
};
za::Vector<Plink> plinks;

void run()
{
    if(vr_particle_freeze.value != 0.f || cl.time == lastRun)
    {
        return;
    }
    const float dt = lastRun < 0.0 ? 0.f : static_cast<float>(CLAMP(0.0, cl.time - lastRun, 0.1));
    lastRun = cl.time;
    if(dt <= 0.f)
    {
        return;
    }

    const float time2 = dt * 10.f;
    const float time3 = dt * 15.f;
    const float dvel = 4.f * dt;
    const auto fade = [dt](Particle& p, float perSecond255) { p.color.a += perSecond255 / 255.f * dt; };
    const auto spin = [dt](Particle& p, float rate) { p.angle += rate * dt * (p.spinBack ? -1.f : 1.f); };

    for(Particle& p : pool)
    {
        p.vel += p.acc * dt;
        p.org += p.vel * dt;

        switch(p.type)
        {
            case Explode:
                p.ramp += time2;
                if(p.ramp >= 8.f)
                {
                    p.die = -1.0;
                }
                else
                {
                    p.color = glm::vec4{paletteColor(ramp1[static_cast<int>(p.ramp)]), p.color.a};
                }
                p.vel += p.vel * dvel;
                break;
            case Explode2:
                p.ramp += time3;
                if(p.ramp >= 8.f)
                {
                    p.die = -1.0;
                }
                else
                {
                    p.color = glm::vec4{paletteColor(ramp2[static_cast<int>(p.ramp)]), p.color.a};
                }
                p.vel -= p.vel * dt;
                break;
            case Blob: p.vel += p.vel * dvel; break;
            case TxExplode:
                fade(p, -345.f);
                p.scale += 135.f * dt;
                spin(p, 0.75f);
                break;
            case TxSmoke:
                fade(p, -70.f);
                p.scale += 47.f * dt;
                break;
            case TxBigSmoke:
                fade(p, -35.f);
                p.scale += 37.f * dt;
                break;
            case Lightning:
                fade(p, -84.f);
                p.scale -= 32.f * dt;
                break;
            case Rock: spin(p, 25.f); break;
            case GunSmoke:
                fade(p, -110.f);
                p.scale += 69.f * dt;
                p.org.z += 18.f * dt;
                break;
            case GunPickup:
                fade(p, -80.f);
                p.scale -= 0.1f * dt;
                break;
            case Drip: fade(p, -170.f); break;
            case Custom:
                // Fading in first (`ramp` seconds of it left: the shore's spray, as `fade` rises to its peak), then out
                // to nothing by its end.
                if(p.ramp > 0.f)
                {
                    p.ramp -= dt;
                    if(p.ramp <= 0.f)
                    {
                        p.color.a += p.fade * (dt + p.ramp);
                        p.fade = -p.color.a / za::max(static_cast<float>(p.die - cl.time), 0.05f);
                        p.ramp = 0.f;
                    }
                }
                p.color.a += p.fade * dt;
                p.scale += p.grow * dt;
                p.vel *= za::max(0.f, 1.f - p.drag * dt);
                p.angle += p.spin * dt;
                break;
            default: break;
        }
        // A drop falling to its liquid: the surface where it is, raised or sunk by the waves and ripples (close to it).
        if(p.liquid && !p.flat && p.vel.z < 0.f && p.org.z < p.floor + 48.f)
        {
            p.rise = water::surfaceRise({p.org.x, p.org.y, p.floor + 0.5f}, p.liquid, geom::vec(r_refdef.vieworg));
        }
        // A drop going back into its liquid: a little ring where it went in (added after the loop).
        if(p.plink && p.org.z < p.floor + p.rise && p.vel.z < 0.f && p.die >= cl.time && plinks.size() < 48)
        {
            plinks.pushBack({p.org.x, p.org.y, p.floor + 0.5f, p.scale, p.color, p.liquid});
        }
    }

    za::vectorEraseIf(pool, [](const Particle& p) {
        return p.die < cl.time || p.color.a <= 0.f || p.scale <= 0.f || (p.org.z < p.floor + p.rise && p.vel.z < 0.f);
    });

    for(const Plink& k : plinks)
    {
        if(pool.size() >= maxParticles)
        {
            break;
        }
        Particle p;
        p.cell = CellRing;
        p.flat = true;
        p.angle = rndAngle();
        p.color = glm::vec4{glm::vec3{k.color}, 0.2f};
        p.die = cl.time + 0.5;
        p.scale = 0.5f + 0.6f * k.scale;
        p.type = Custom;
        p.fade = -0.2f / 0.5f;
        p.grow = 14.f / 0.525f; // its crest (at 0.525 of the scale) spreading at 14 units/s
        p.org = {k.x, k.y, k.surface + 0.35f};
        p.floor = k.surface;
        p.liquid = k.liquid;
        pool.pushBack(p);
    }
    plinks.clear();
}

// ---- Drawing --------------------------------------------------------------------------------

gfx::Texture atlas = 0;
bool atlasFailed = false;
glm::vec4 cellUv[CellCount]{};
bool cellTrim[CellCount]{};

constexpr int cellSize = 132; // 128 plus a border
constexpr int atlasColumns = 4;
constexpr int atlasRows = 4;
static_assert(CellCount <= atlasColumns * atlasRows, "the atlas has no room for another cell: add a row");

// A soft disc as the old engine's generated circle (sharpness 8): a quarter of the quad across,
// which is how big the old off-centre disc was.
[[nodiscard]] za::Vector<za::U8> buildDisc(int size)
{
    constexpr float sharpness = 8.f;
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    const float k = 16.f / (size * 0.25f); // the old 16-texel radius, over a quarter of the size
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) * k;
            const float dy = (y - c) * k;
            const float r = za::min(255.f, dx * dx + dy * dy);
            dst[static_cast<za::SizeT>((y * size + x) * 4 + 3)] =
                static_cast<za::U8>(za::min(255.f, sharpness * (255.f - r)));
        }
    }
    return dst;
}

// A soft glow: white, its alpha falling off as a Gaussian to nothing at the edge.
[[nodiscard]] za::Vector<za::U8> buildGlow(int size)
{
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r2 = dx * dx + dy * dy;
            const float a = za::max(0.f, za::exp(-r2 * 4.5f) - za::exp(-4.5f)) / (1.f - za::exp(-4.5f));
            dst[static_cast<za::SizeT>((y * size + x) * 4 + 3)] = static_cast<za::U8>(a * 255.f + 0.5f);
        }
    }
    return dst;
}

// A ripple: a soft white ring (a Gaussian band at 70% of the radius), fainter inside than out, as
// a wave's front is the steep side; broken a little along its length (foam on the crest), and a faint second crest
// inside it.
[[nodiscard]] za::Vector<za::U8> buildRing(int size)
{
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r = za::sqrt(dx * dx + dy * dy);
            const float t = za::atan2(dy, dx);
            const float band = (r - 0.7f) / (r < 0.7f ? 0.1f : 0.05f);
            const float inner = (r - 0.5f) / 0.05f;
            const float broken = 0.72f + 0.16f * za::sin(7.f * t + 1.3f) + 0.12f * za::sin(17.f * t + 0.4f);
            const float a = r >= 0.98f ? 0.f : za::min(1.f, za::exp(-band * band) * broken + 0.18f * za::exp(-inner * inner));
            dst[static_cast<za::SizeT>((y * size + x) * 4 + 3)] = static_cast<za::U8>(a * 255.f + 0.5f);
        }
    }
    return dst;
}

// A drop of liquid, filling the cell (it is drawn stretched along its motion: a streak): see-through in the middle,
// its rim brighter and more opaque (the light bent round its edge), a glint up on one side, a faint halo round it.
[[nodiscard]] za::Vector<za::U8> buildDrop(int size)
{
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 0);
    const float c = (size - 1) * 0.5f;
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float rn = za::sqrt(dx * dx + dy * dy) / 0.78f; // 1: the drop's edge
            const float body = za::clamp((1.f - rn) / 0.2f, 0.f, 1.f);
            const float gx = dx + 0.3f, gy = dy + 0.3f;
            const float glint = za::exp(-(gx * gx + gy * gy) / (0.2f * 0.2f));
            const float halo = rn > 1.f ? 0.22f * za::exp(-((rn - 1.f) / 0.12f) * ((rn - 1.f) / 0.12f)) : 0.f;
            const float a = za::min(1.f, za::max(body * (0.72f + 0.28f * rn * rn * rn), glint) + halo);
            const float v = za::min(1.f, 0.7f + 0.25f * rn * rn + 0.6f * glint);
            za::U8* px = &dst[static_cast<za::SizeT>((y * size + x) * 4)];
            px[0] = px[1] = px[2] = static_cast<za::U8>(v * 255.f + 0.5f);
            px[3] = static_cast<za::U8>(a * 255.f + 0.5f);
        }
    }
    return dst;
}

// A fixed pseudo-random sequence for the generated cells (the same atlas every run).
struct CellRandom
{
    za::U32 state;
    float next()
    {
        return vrand::lcg01(state);
    }
};

// Spray: a cloud of fine droplets, thicker in the middle, in a faint haze.
[[nodiscard]] za::Vector<za::U8> buildSpray(int size)
{
    za::Vector<float> alpha(static_cast<za::SizeT>(size * size), 0.f);
    const float c = (size - 1) * 0.5f;
    CellRandom rand{0x5eed1u};
    for(int i = 0; i < 220; i++)
    {
        // Gaussian (Box-Muller), kept inside the cell.
        const float u = za::max(rand.next(), 1e-4f), t = rand.next() * 6.2831853f;
        const float r = za::min(za::sqrt(-2.f * za::log(u)) * 0.33f, 0.88f);
        const float px = c + za::cos(t) * r * c, py = c + za::sin(t) * r * c;
        const float radius = (0.7f + 1.6f * rand.next() * rand.next()) * size / 128.f;
        const float strength = 0.55f + 0.45f * rand.next();
        for(int y = za::max(0, static_cast<int>(py - radius - 2.f)); y <= za::min(size - 1, static_cast<int>(py + radius + 2.f)); y++)
        {
            for(int x = za::max(0, static_cast<int>(px - radius - 2.f)); x <= za::min(size - 1, static_cast<int>(px + radius + 2.f)); x++)
            {
                const float d = za::sqrt((x - px) * (x - px) + (y - py) * (y - py));
                const float a = strength * za::clamp(radius + 0.7f - d, 0.f, 1.f);
                float& dstA = alpha[static_cast<za::SizeT>(y * size + x)];
                dstA = za::max(dstA, a);
            }
        }
    }
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 255);
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r2 = dx * dx + dy * dy;
            const float haze = 0.3f * za::max(0.f, za::exp(-r2 * 3.5f) - za::exp(-3.5f));
            const float a = za::min(1.f, alpha[static_cast<za::SizeT>(y * size + x)] + haze) * (r2 >= 1.f ? 0.f : 1.f);
            dst[static_cast<za::SizeT>((y * size + x) * 4 + 3)] = static_cast<za::U8>(a * 255.f + 0.5f);
        }
    }
    return dst;
}

// Foam: a patch of bubbles (their rims bright, a thin film between them), its edge ragged and soft.
[[nodiscard]] za::Vector<za::U8> buildFoam(int size)
{
    struct Bubble
    {
        float x, y, r;
    };
    za::Vector<Bubble> bubbles;
    const float c = (size - 1) * 0.5f;
    CellRandom rand{0xf0a3u};
    for(int i = 0; i < 90; i++)
    {
        const float r = za::sqrt(rand.next()) * 0.85f, t = rand.next() * 6.2831853f;
        bubbles.pushBack({c + za::cos(t) * r * c, c + za::sin(t) * r * c, (2.f + 6.f * rand.next() * rand.next()) * size / 128.f});
    }
    za::Vector<za::U8> dst(static_cast<za::SizeT>(size * size * 4), 255);
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r = za::sqrt(dx * dx + dy * dy);
            const float t = za::atan2(dy, dx);
            const float edge = r + 0.05f * za::sin(3.f * t + 0.7f) + 0.04f * za::sin(7.f * t + 2.1f) + 0.03f * za::sin(13.f * t + 4.f);
            const float patch = za::clamp((0.92f - edge) / 0.45f, 0.f, 1.f);
            float b = 0.14f; // the film
            for(const Bubble& bu : bubbles)
            {
                const float d = za::sqrt((x - bu.x) * (x - bu.x) + (y - bu.y) * (y - bu.y));
                const float rim = (d - bu.r) / (0.9f * size / 128.f);
                b = za::max(b, d < bu.r ? 0.12f + 0.7f * za::exp(-rim * rim) : za::exp(-rim * rim));
            }
            dst[static_cast<za::SizeT>((y * size + x) * 4 + 3)] = static_cast<za::U8>(za::min(1.f, b * patch) * 255.f + 0.5f);
        }
    }
    return dst;
}

bool ensureAtlas()
{
    if(atlas || atlasFailed)
    {
        return atlas != 0;
    }

    const int width = cellSize * atlasColumns;
    const int height = cellSize * atlasRows;
    za::Vector<za::U8> pixels(static_cast<za::SizeT>(width * height * 4), 0);

    const auto put = [&](Cell cell, const za::U8* rgba, int w, int h) {
        const int cx = (cell % atlasColumns) * cellSize + 2;
        const int cy = (cell / atlasColumns) * cellSize + 2;
        for(int y = 0; y < h; y++)
        {
            ZA_MEMCPY(&pixels[static_cast<za::SizeT>(((cy + y) * width + cx) * 4)], rgba + static_cast<za::SizeT>(y * w * 4),
                static_cast<za::SizeT>(w * 4));
        }
        // Half a texel in, so that filtering stays inside the image.
        cellUv[cell] = {(cx + 0.5f) / width, (cy + 0.5f) / height, (cx + w - 0.5f) / width, (cy + h - 0.5f) / height};
    };

    put(CellCircle, buildDisc(64).data(), 64, 64);
    put(CellGlow, buildGlow(64).data(), 64, 64);
    put(CellRing, buildRing(96).data(), 96, 96);
    put(CellDrop, buildDrop(48).data(), 48, 48);
    put(CellSpray, buildSpray(128).data(), 128, 128);
    put(CellFoam, buildFoam(128).data(), 128, 128);

    struct File
    {
        Cell cell;
        const char* name;
    };
    constexpr File files[] = {{CellExplosion, "textures/particle_explosion"}, {CellSmoke, "textures/particle_smoke"},
        {CellBlood, "textures/particle_blood"}, {CellBloodMist, "textures/particle_blood_mist"},
        {CellLightning, "textures/particle_lightning"}, {CellSpark, "textures/particle_spark"},
        {CellRock, "textures/particle_rock"}, {CellGunSmoke, "textures/particle_gun_smoke"},
        {CellFire, "textures/particle_fire"}};
    for(const File& f : files)
    {
        const int mark = Hunk_LowMark();
        int w = 0, h = 0;
        enum srcformat fmt;
        const byte* data = Image_LoadImage(f.name, &w, &h, &fmt);
        if(!data || w > cellSize - 4 || h > cellSize - 4)
        {
            Hunk_FreeToLowMark(mark);
            Con_Warning("VR: particle texture %s missing or too big; using Quake's particles\n", f.name);
            atlasFailed = true;
            return false;
        }
        put(f.cell, data, w, h);
        Hunk_FreeToLowMark(mark);
    }

    // Premultiplied (drawn with the premultiplied blend: glows, alpha 0, add) and mipmapped.
    for(za::SizeT i = 0; i < pixels.size(); i += 4)
    {
        for(za::SizeT c = 0; c < 3; c++)
        {
            pixels[i + c] = static_cast<za::U8>((pixels[i + c] * pixels[i + 3] + 127) / 255);
        }
    }
    // Include nearby cells: retro sampling can reach outside this image. No alpha threshold:
    // even the faintest authored wisp contributes to the bounds. The shader only trims
    // level 0..2 reads whose entire sampling footprint stays within this guard.
    glm::vec4 support[CellCount];
    for(int cell = 0; cell < CellCount; cell++)
    {
        const glm::vec4 uv = cellUv[cell];
        glm::vec2 lo{1.f}, hi{0.f};
        const int x0 = za::max(0, static_cast<int>(uv.x * width - 16.f));
        const int y0 = za::max(0, static_cast<int>(uv.y * height - 16.f));
        const int x1 = za::min(width - 1, static_cast<int>(uv.z * width + 16.f));
        const int y1 = za::min(height - 1, static_cast<int>(uv.w * height + 16.f));
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                const auto i = static_cast<za::SizeT>((y * width + x) * 4);
                if(pixels[i] || pixels[i + 1] || pixels[i + 2] || pixels[i + 3])
                {
                    const glm::vec2 center{(x + 0.5f) / width, (y + 0.5f) / height};
                    lo = glm::min(lo, center);
                    hi = glm::max(hi, center);
                }
            }
        }
        support[cell] = glm::vec4{lo, hi};
        // Reject marginal savings before selecting a larger shader. Even with the
        // maximum accepted footprint, a candidate must remove at least 10% of area.
        const glm::vec2 margin{15.f / width, 15.f / height};
        const glm::vec2 size = glm::max(glm::vec2{0.f}, glm::min(glm::vec2{uv.z, uv.w}, hi + margin) -
            glm::max(glm::vec2{uv.x, uv.y}, lo - margin));
        cellTrim[cell] = size.x * size.y < 0.9f * (uv.z - uv.x) * (uv.w - uv.y);
    }
    gfx::particleSupportBounds({support, CellCount}, width, height);
    atlas = gfx::createTexture(width, height, pixels.data(), true, "particle atlas");
    atlasFailed = atlas == 0;
    return atlas != 0;
}


} // namespace detail

bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count)
{
    QVR_PROFILE("particle spawn");
    // The splash's ripples on the liquid (vr_water_ripples, vr_water.cpp), with Quake VR's particles or without.
    glm::vec3 surface{0.f};
    int under = 0;
    if(preset == Preset::Splash)
    {
        if(VR_NoLiquidEffects(cl.worldmodel, &org.x)) { return true; }
        water::addRipple(org, static_cast<float>(count));
    }
    else if(preset == Preset::Explosion && (under = underwaterExplosion(org, surface)) > 0)
    {
        water::addRipple(surface, static_cast<float>(under));
    }
    else if(preset == Preset::Blood || preset == Preset::BloodTrail)
    {
        // Blood in a liquid, or falling onto one: a cloud on its surface (vr_water_blood), with the particles or without.
        (void)water::addBlood(org, preset == Preset::Blood ? za::clamp(static_cast<float>(count) / 20.f, 0.15f, 1.f)
                                                           : za::min(0.1f * static_cast<float>(count), 0.3f));
    }

    if(!vr_particles.value || !ensureAtlas())
    {
        return false;
    }

    switch(preset)
    {
        case Preset::BulletPuff: bulletPuff(org, dir, 0, count); break;
        case Preset::Blood: blood(org, dir, count); break;
        case Preset::Explosion:
            explosion(org);
            if(under > 0)
            {
                splash(surface, glm::vec3{0.f, 0.f, -1.f}, under);
            }
            break;
        case Preset::Lightning: lightning(org, count); break;
        case Preset::Smoke: smoke(org, count, false); break;
        case Preset::Sparks: sparks(org, count, 102, 112, 0.45f, 0.0); break;
        case Preset::GunSmoke: gunSmoke(org, count); break;
        case Preset::Teleport: sparks(org, count, 208, 220, 0.65f, 0.6); break;
        // Subtle and slow: faint, small, drifting up.
        case Preset::GunPickup: sparkles(org, count, 12, 16, 70, 120, 1.6, 0.22f, -0.03f, 6.f, 2.f, 1.f, 6.f); break;
        case Preset::GunForceGrab: // QVR r18: in the player's hue
        {
            const za::SizeT first = pool.size();
            sparkles(org, count, 106, 111, 90, 140, 1.2, 0.25f, -0.04f, 5.f, 3.f, 1.f, 8.f);
            inForceGrabHue(first);
            break;
        }
        case Preset::LavaSpike: sparkles(org, count, 247, 254, 180, 225, 0.5, 0.17f, 0.17f, 0.3f, 2.f, 0.f, 0.f); break;
        case Preset::BigSmoke: smoke(org, count, true); break;
        case Preset::ForceGrabTrail: // QVR r18: in the player's hue
        {
            const za::SizeT first = pool.size();
            sparkles(org, count, 208, 214, 170, 230, 0.4, 0.28f, 0.f, 1.5f, 4.f, -2.f, 2.f);
            inForceGrabHue(first);
            break;
        }
        case Preset::BloodTrail: bloodTrail(org, dir, count); break;
        case Preset::Splash: splash(org, dir, count); break;
        case Preset::WoodDust: woodDust(org, dir, count); break;
        case Preset::ChainsawSmoke: chainsawSmoke(org, glm::length(dir) > 0.01f ? glm::normalize(dir) : dir, count); break;
        case Preset::ContactSparks: contactSparks(org, count); break;
        case Preset::BatterySmoke: batterySmoke(org, count); break;
        case Preset::AdFlame: adStreamFire(org, dir, count, false); break;
        case Preset::AdPoison: adStreamFire(org, dir, count, true); break;
        case Preset::PoisonPuff: poisonPuff(org, dir, count); break;
        case Preset::ElectricSparks: electricSparks(org, dir, count); break;
        case Preset::AdFlameHit: // a flame spent on a wall: a tongue or two licking up off it (its scorch: vr_decals.cpp)
        {
            const glm::vec3 back = glm::dot(dir, dir) > 1e-4f ? -glm::normalize(dir) : glm::vec3{0.f, 0.f, 1.f};
            adStreamFire(org + back * 3.f, back * 0.15f + glm::vec3{0.f, 0.f, 0.35f}, count, false);
            break;
        }
        case Preset::Steam: steam(org, count); break;
        case Preset::TankFire: tankFire(org, dir, count); break;
        case Preset::TankSmoke: tankSmoke(org, dir, count); break;
        case Preset::TankCloud: tankCloud(org, dir, count); break;
        default: blood(org, dir, count); break;
    }
    return true;
}

bool enabled()
{
    return (cl.protocolflags & PRFL_QUAKEVR) && vr_particles.value && ensureAtlas();
}


void init()
{
    static_assert(sizeof(Particle) <= 152, "the pool's reservation (4.75 MB) grows with it");
    pool.reserve(maxParticles);
}

void prepare()
{
    if(vr_particles.value)
    {
        (void)ensureAtlas();
    }
}

void clear()
{
    pool.clear();
    lastRun = -1.0;
    // Tests: the same particles every run (vr_particle_seed; 0: random).
    if(vr_particle_seed.value != 0.f)
    {
        rng = za::FastNonCryptoRng{static_cast<za::U32>(vr_particle_seed.value)};
    }
}


// ---- Shoreline foam particles (vr_water_foam_particles, vr_lava_foam_*, vr_slime_foam_*) ---------------------------
// Small puffs of foam (CellFoam, facing the view) along the shore, where the shader's foam band (LiquidFoam) meets the
// rock: they straddle the line where the liquid's plane cuts the wall, so it is not one straight edge. Each grows a
// little, drifts along the shore and out into the liquid, and fades out over its life. From the world's shore
// (water::shoreSegments: the liquid tops' rims, found once as the map's liquid mesh is made), only the segments within
// kShoreReach of the eye (shoreNear: listed again when the eye has moved kShoreRelist units) whose faces were in a view's
// PVS lately; fewer further off, none past kShoreReach. At most about kShoreMaxLive of them alive (the rate scaled down
// past it). Each liquid's rate, life, opacity, size and colour its own (water::foamLook).
ShoreNear shoreNear;

[[nodiscard]] float pointSegmentDistance(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
{
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 0.f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::length(p - (a + ab * t));
}

void shoreFoam()
{
    if(vr_particle_freeze.value != 0.f || vr_water_foam.value <= 0.f || !vr_particles.value || !cl.worldmodel ||
        cl.time == shoreNear.last)
    {
        return;
    }
    const float dt = shoreNear.last < 0.0 ? 0.f : static_cast<float>(CLAMP(0.0, cl.time - shoreNear.last, 0.1));
    shoreNear.last = cl.time;
    shoreNear.spawned = 0;
    shoreNear.expectedLive = 0.f;
    unsigned generation = 0;
    const za::Vector<water::ShoreSegment>& shore = water::shoreSegments(generation);
    if(dt <= 0.f || shore.empty())
    {
        return;
    }
    const double start = Sys_DoubleTime();
    water::FoamLook looks[3];
    float rate[3];
    bool any = false;
    const float mult = za::max(0.f, vr_particle_mult.value);
    for(int i = 0; i < 3; i++)
    {
        looks[i] = water::foamLook(i);
        rate[i] = looks[i].on && looks[i].particleOpacity > 0.f ? looks[i].particles * kShoreRate * mult : 0.f;
        any |= rate[i] > 0.f;
    }
    if(!any || !ensureAtlas())
    {
        return;
    }
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    if(generation != shoreNear.generation || glm::length(eye - shoreNear.at) > kShoreRelist)
    {
        shoreNear.generation = generation;
        shoreNear.at = eye;
        shoreNear.list.clear();
        const float reach = kShoreReach + kShoreRelist;
        for(za::SizeT i = 0; i < shore.size(); i++)
        {
            const water::ShoreSegment& s = shore[i];
            if(za::fabs(s.z - eye.z) < reach && pointSegmentDistance(glm::vec2(eye), s.a, s.b) < reach)
            {
                shoreNear.list.pushBack(static_cast<za::U32>(i));
            }
        }
    }
    // how many would be alive at the rates asked (the segments seen lately, as if all near): the cap's scale
    float live = 0.f;
    for(const za::U32 i : shoreNear.list)
    {
        const water::ShoreSegment& s = shore[i];
        if(rate[s.liquid] > 0.f && water::shoreSeen(s))
        {
            live += glm::length(s.b - s.a) * rate[s.liquid] * looks[s.liquid].particleLife;
        }
    }
    shoreNear.expectedLive = live;
    const float scale = live > kShoreMaxLive ? kShoreMaxLive / live : 1.f;
    for(const za::U32 i : shoreNear.list)
    {
        const water::ShoreSegment& s = shore[i];
        if(rate[s.liquid] <= 0.f || !water::shoreSeen(s))
        {
            continue;
        }
        const glm::vec2 d = s.b - s.a;
        const float len = glm::length(d);
        const int n = static_cast<int>(len * rate[s.liquid] * scale * dt + vrand::range(rng, 0.f, 1.f));
        for(int k = 0; k < n && pool.size() < maxParticles; k++)
        {
            const glm::vec2 at = s.a + d * vrand::range(rng, 0.f, 1.f);
            const float away = glm::length(glm::vec3{at.x, at.y, s.z} - eye);
            if(away > kShoreReach || (away > kShoreNear && vrand::range(rng, kShoreNear, kShoreReach) < away))
            {
                continue;
            }
            const water::FoamLook& l = looks[s.liquid];
            const bool lava = s.liquid == 1;
            const float size = l.particleSize;
            const float life = l.particleLife * vrand::range(rng, 0.7f, 1.3f);
            const glm::vec2 along = d / len;
            // on the shore's line (the particle shore setting out from it), a little either side: over the hard edge
            const glm::vec3 org{at + s.in * (l.particleShore + vrand::range(rng, -0.5f, 1.5f) * size), s.z};
            Particle p;
            p.angle = rndAngle();
            p.type = Custom;
            p.liquid = water::foamContents(s.liquid); // (its light: lava's glows)
            p.die = cl.time + life;
            // Rising puffs of bubbly foam (the shore spray, vr_water_foam_particle_spray 1, retired in round 23).
            const float alpha = l.particleOpacity * vrand::range(rng, 0.7f, 1.f);
            p.cell = CellFoam;
            p.color = glm::vec4{l.color * (lava ? 1.f : shadeAt(org)) * vrand::range(rng, 0.9f, 1.1f), alpha};
            const float big = vrand::range(rng, 3.f, 5.f) * size; // 3 to 4.5 units across, growing to 7 to 11
            p.scale = 0.6f * big;
            p.grow = 0.9f * big / life;
            p.fade = -alpha / life;
            p.spin = vrand::range(rng, -0.6f, 0.6f);
            p.drag = 0.8f;
            // a little above the surface (the bottom in the liquid), drifting along the shore and out
            p.org = org + glm::vec3{0.f, 0.f, vrand::range(rng, 0.1f, 0.5f) * 0.75f * p.scale};
            const float drift = vrand::range(rng, -4.f, 4.f);
            p.vel = glm::vec3{along * drift + s.in * vrand::range(rng, 0.5f, 2.5f), vrand::range(rng, 0.f, 1.5f)};
            pool.pushBack(p);
            shoreNear.spawned++;
        }
    }
    shoreNear.costMs = (Sys_DoubleTime() - start) * 1000.0;
}


// Live large blast fireballs only (the diagnostics exclude the small trail/emitter flames).
int largeExplosionCount()
{
    int n = 0;
    for(const Particle& p : pool) { n += p.cell == CellExplosion && p.type == TxExplode ? 1 : 0; }
    return n;
}

// Live particles (vr_memstats).
int liveCount()
{
    return static_cast<int>(pool.size());
}

int capacity()
{
    return static_cast<int>(maxParticles);
}

// ---- Soft particles (vr_soft_particles) -----------------------------------------------------
// Where a particle meets a wall or a floor it fades out over the last `fade` units in front of the opaque scene (the
// scene's distances, vr_water.hpp), instead of cutting a hard line through it. Small ones (drops, sparks, chips) over a
// unit or two; puffs of smoke, fire, blood and glows over a third of their radius (at most 16 units), their quad moved
// towards the eye by as much (`pull`, along the eye's rays: the same on screen, in both eyes; at most 12 units and
// their radius): one born against a wall stays whole in its middle, one behind a wall does not show through it.
// Ripples lying on a liquid stay hard (on an opaque liquid they would fade away). Times vr_soft_particles_scale.
struct Softness
{
    float fade{0.f};
    float pull{0.f};
};

[[nodiscard]] bool softOn()
{
    return vr_soft_particles.value != 0.f && vr_soft_particles_scale.value > 0.f &&
           (GL_NeedsSceneEffects() || GL_NeedsPostprocess()); // else the window's depth: nothing to read
}

[[nodiscard]] Softness softness(Cell cell, float radius)
{
    float fade = 0.f;
    bool puff = false;
    switch(cell)
    {
        case CellRing: return {}; // (lying on a liquid)
        case CellFoam: fade = za::clamp(radius * 0.5f, 1.f, 4.f); break; // (lying: none; standing, the shore's: soft on the rock)
        case CellCircle:
        case CellDrop:
        case CellSpark: fade = 1.5f; break;
        case CellRock: fade = 1.f; break;
        default:
            puff = true;
            fade = za::clamp(radius * 0.35f, 1.5f, 16.f);
            break;
    }
    fade *= za::min(vr_soft_particles_scale.value, 4.f);
    return {fade, puff ? za::min(fade, radius, 12.f) : 0.f};
}

// ---- Lying on a liquid (a splash's rings and foam) ---------------------------------------------
// On the surface as it is drawn, raised and sunk by the geometric waves and the ripples (water::surfaceRise): a grid
// of pieces up to 10 units across (at most 12 a side), each corner at the surface's height there, `p.org.z - p.floor`
// above it. Else a flat quad would sink into the crests (hidden by an opaque liquid) and float over the troughs.
za::Vector<gfx::Vertex> lyingVertices;
za::SizeT lyingCount = 0; // this view's (lyingVertices only grows: no vertex constructed again for each view)

namespace
{

// A particle's grid over the liquid, laid one at a time (the main thread).
struct LyingScratch
{
    za::Vector<gfx::Vertex> grid;
    auto members() { return qvr::mem::list(grid); }
};
mem::Scratch<LyingScratch> lyingScratch{"particles"};

} // namespace

void lieOnLiquid(const Particle& p, const glm::vec3& r, const glm::vec3& u, const glm::vec4& uv, const glm::vec4& color, const glm::vec3& eye)
{
    const int n = za::clamp(static_cast<int>(za::ceil(1.5f * p.scale / 10.f)), 1, 12);
    // (at least a unit: the grid's triangles are a little off the bilinear surface worked out here)
    const float lift = za::max(p.org.z - p.floor, 1.f);
    za::Vector<gfx::Vertex>& grid = lyingScratch.grid;
    grid.resize(static_cast<za::SizeT>((n + 1) * (n + 1)));
    for(int j = 0; j <= n; j++)
    {
        for(int i = 0; i <= n; i++)
        {
            const float fu = static_cast<float>(i) / n, fr = static_cast<float>(j) / n;
            glm::vec3 pos = p.org + u * (2.f * fu - 1.f) + r * (2.f * fr - 1.f);
            pos.z = p.floor + lift + water::surfaceRise({pos.x, pos.y, p.floor}, p.liquid, eye);
            grid[static_cast<za::SizeT>(j * (n + 1) + i)] = {pos, {uv.x + (uv.z - uv.x) * fu, uv.y + (uv.w - uv.y) * fr}, color, 0.f};
        }
    }
    const za::SizeT end = lyingCount + static_cast<za::SizeT>(n * n * 6);
    if(lyingVertices.size() < end)
    {
        lyingVertices.resize(end);
    }
    gfx::Vertex* out = lyingVertices.data() + lyingCount;
    for(int j = 0; j < n; j++)
    {
        for(int i = 0; i < n; i++)
        {
            const gfx::Vertex& a = grid[static_cast<za::SizeT>(j * (n + 1) + i)];
            const gfx::Vertex& b = grid[static_cast<za::SizeT>(j * (n + 1) + i + 1)];
            const gfx::Vertex& c = grid[static_cast<za::SizeT>((j + 1) * (n + 1) + i)];
            const gfx::Vertex& d = grid[static_cast<za::SizeT>((j + 1) * (n + 1) + i + 1)];
            out[0] = a;
            out[1] = b;
            out[2] = d;
            out[3] = a;
            out[4] = d;
            out[5] = c;
            out += 6;
        }
    }
    lyingCount = end;
}

// ---- Lit particles (vr_particle_light) -------------------------------------------------------
// Those that are not light themselves (Lighting::Lit) take the place's light, as the alias models do
// (R_SetupAliasLighting): the lightmap under them (R_LightPoint) through its contrast (lighting::lightCurve), then, as
// the world adds them, the dynamic lights round them (this frame's: muzzle flashes, explosions, the flashlight's cone,
// torches), times their colour: 1 at Quake's full light, up to 2 (overbright, as the world). Not the dynamic lights'
// shadows. Worked out once a frame on the CPU, for every view (both eyes, the spectator, half
// resolution alike), with retro lighting's levels (the models') when it is on.
//
// The cost is bounded: a particle looks the lightmap up again only when it has moved 8 units across (16 up or down)
// since, and then from its cell's kept trace (lightTraces: a burst's particles and a trail's share them); a cell not
// traced yet is, at most traceBudget a frame (the others keep their last light a frame longer). On a surface lit by a
// changing light style, a quarter of them each frame read their lightmap texels again for the flicker (a cached
// R_LightPoint: no trace). About 25 to 40 ns a lit particle a frame (vr_particle_light_report: 0.15 to 0.25 ms with 5000
// to 6500 of them in particles_dense).

// What a particle is, for its light (Lighting::Auto): the explicit list of what is light itself. Glows added to the
// scene are; so are the explosions' and the tarbaby's colour ramps and lightning, and everything in or of lava (its
// steam aside).
[[nodiscard]] Lighting classify(const Particle& p)
{
    if(p.additive)
    {
        return Lighting::Emissive;
    }
    switch(p.type)
    {
        case Explode:
        case Explode2:
        case Blob:
        case Lightning: return Lighting::Emissive; // (fire's colour ramps, electricity)
        case Static:
        case TxExplode:
        case TxSmoke:
        case TxBigSmoke:
        case Rock:
        case GunSmoke:
        case GunPickup:
        case Drip:
        case Custom: break; // (by its look)
    }
    if(p.liquid == CONTENTS_LAVA && p.cell != CellSmoke)
    {
        return Lighting::Emissive; // a lava splash's drops, rings and foam
    }
    switch(p.cell)
    {
        case CellExplosion: // fireballs (alpha blended in explosion(): still fire)
        case CellLightning:
        case CellSpark: // hot sparks and embers, glints (wood chips are set Lit: woodDust)
        case CellGlow:
        case CellFire: return Lighting::Emissive;
        case CellCircle: // blood, dust, chips, drips (the fire ramps are Explode and Blob above)
        case CellSmoke:  // smoke, dust, steam
        case CellBlood:
        case CellBloodMist:
        case CellRock:
        case CellGunSmoke:
        case CellRing: // water: ripples, drops, spray, foam
        case CellDrop:
        case CellSpray:
        case CellFoam: return Lighting::Lit;
        case CellCount: break;
    }
    return Lighting::Lit;
}

// One lightmap trace for each cell (16 units: a luxel), kept and shared by every particle that comes into it
// (a burst's hundreds, a trail's); the light styles' flicker read again from its surface where it changes.
struct LightTrace
{
    const qmodel_t* world{nullptr}; // (the map's: none from another)
    int key[3]{};
    lightcache_t cache{};
    za::U8 rgb[3]{};
};
constexpr za::SizeT lightTraceSlots = 4096; // (a power of two)
constexpr int traceBudget = 512;            // traces a frame at most
constexpr float retraceDistance = 8.f;      // units moved across since its last trace (the luxels are 16)
constexpr float retraceRise = 16.f;         // units up or down (the trace finds the floor under it: the same light)
constexpr float lightCellSize = 16.f;       // a kept trace's cell, across and up (a luxel)
LightTrace lightTraces[lightTraceSlots];

// This frame's dynamic lights that may reach particles (indices into r_lightbuffer.lights), and each particle's light
// (by pool index; 1 for emissive ones).
struct FrameLight
{
    glm::vec3 pos;
    float radius;
    glm::vec3 color;
    glm::vec4 spot; // gpulight_t's (VR_SpotCone; w 0: a point light)
};
FrameLight frameLights[MAX_DLIGHTS];
int frameLightCount = 0;
za::Vector<glm::vec3> lightOf;
za::SizeT lightCount = 0; // the particles lightOf is for (the pool may have grown since)

LightStats lightStats;
jobs::Site lightSite{"particle light"}; // (its parallelFor: vr_jobs_sites)
// The dynamic lights' work (the last frame's lit particles times this frame's lights, and 4 for each light that
// reached one) from which lightParticles splits it between threads: combined (6800 lit, 21 lights, 91000 reaches)
// splits, 0.3 ms off the main thread; explosions_storm (1400 lit, 13 lights, 800 reaches), particles_dense (5700,
// 5, 100) and combat_48 (2400, 6, 200) stay on the caller (a split's start costs more than it saves there).
constexpr za::SizeT splitLightWork = 65536;
double lightMsSum = 0.0; // lightParticles' time over the frames since the last report
int lightFrames = 0;
long long lightTraceSum = 0; // ... and the traces it made
long long lightLitSum = 0;   // ... and the particles it lit

[[nodiscard]] za::U8 lightByte(float v)
{
    return static_cast<za::U8>(za::clamp(v, 0.f, 255.f) + 0.5f);
}

// Whether a surface's light changes (a light style other than 0, Quake's steady one; 255 ends the list).
[[nodiscard]] bool flickers(const msurface_t& surf)
{
    for(int k = 0; k < MAXLIGHTMAPS && surf.styles[k] != 255; k++)
    {
        if(surf.styles[k] != 0)
        {
            return true;
        }
    }
    return false;
}

// The lightmap's light at the particle (p.lightmap: 0..255, 128 Quake's full).
void lightmapOf(Particle& p, za::SizeT index, LightStats& st)
{
    if(!cl.worldmodel->lightdata)
    {
        p.lightmap[0] = p.lightmap[1] = p.lightmap[2] = 128; // a map with no light: drawn fullbright
        p.lightKnown = true;
        return;
    }
    const glm::vec3 d = glm::abs(p.org - geom::vec(p.lightCache.pos));
    if(p.lightKnown && za::max(d.x, d.y) < retraceDistance && d.z < retraceRise)
    {
        // The light styles' flicker: the cached place's texels read again (no trace), a quarter of them each frame,
        // on a surface lit by a style that changes (not 0, Quake's steady light).
        if(p.lightCache.surfidx > 0 && ((index + static_cast<za::SizeT>(host_framecount)) & 3) == 0 &&
            flickers(cl.worldmodel->surfaces[p.lightCache.surfidx - 1]))
        {
            vec3_t at{p.lightCache.pos[0], p.lightCache.pos[1], p.lightCache.pos[2]};
            (void)R_LightPoint(at, 2.f, &p.lightCache);
            for(int c = 0; c < 3; c++)
            {
                p.lightmap[c] = lightByte(lightcolor[c]);
            }
            st.refreshed++;
        }
        return;
    }
    const int key[3] = {static_cast<int>(za::floor(p.org.x / lightCellSize)), static_cast<int>(za::floor(p.org.y / lightCellSize)),
        static_cast<int>(za::floor(p.org.z / lightCellSize))};
    const za::U32 hash = (static_cast<za::U32>(key[0]) * 73856093u) ^ (static_cast<za::U32>(key[1]) * 19349663u) ^
                         (static_cast<za::U32>(key[2]) * 83492791u);
    LightTrace& t = lightTraces[hash & (lightTraceSlots - 1)];
    if(t.world == cl.worldmodel && t.key[0] == key[0] && t.key[1] == key[1] && t.key[2] == key[2])
    {
        p.lightCache = t.cache;
        if(p.lightCache.surfidx > 0 && flickers(cl.worldmodel->surfaces[p.lightCache.surfidx - 1]))
        {
            vec3_t at{p.lightCache.pos[0], p.lightCache.pos[1], p.lightCache.pos[2]};
            (void)R_LightPoint(at, 2.f, &p.lightCache); // (its texels now: no trace)
            for(int c = 0; c < 3; c++)
            {
                p.lightmap[c] = lightByte(lightcolor[c]);
            }
        }
        else
        {
            ZA_MEMCPY(p.lightmap, t.rgb, sizeof(p.lightmap));
        }
        p.lightKnown = true;
        st.shared++;
        return;
    }
    if(st.traces >= traceBudget)
    {
        st.stale++; // its last light a frame longer (a new one where no cell was traced: Quake's full light until then)
        return;
    }
    vec3_t at{p.org.x, p.org.y, p.org.z};
    p.lightCache.surfidx = 0; // (a trace from here, whatever was cached)
    (void)R_LightPoint(at, 2.f, &p.lightCache); // (from 2 units up: one lying on the floor finds the floor)
    for(int c = 0; c < 3; c++)
    {
        p.lightmap[c] = lightByte(lightcolor[c]);
    }
    p.lightKnown = true;
    st.traces++;
    t.world = cl.worldmodel;
    ZA_MEMCPY(t.key, key, sizeof(t.key));
    t.cache = p.lightCache;
    ZA_MEMCPY(t.rgb, p.lightmap, sizeof(t.rgb));
}

// DarkPlaces' falloff (vr_dlight_falloff; DarkPlacesAtten, vr_glsl.h).
[[nodiscard]] float darkplacesAtten(float dist, float radius)
{
    const float d = dist / za::max(radius, 1.f);
    return za::clamp((1.f - d) * 2.f / (1.f + d * d), 0.f, 1.f);
}

// Retro lighting's levels (the models' own light's: vr_retrolight_model_steps a unit, a unit the lightmap's whole
// range, twice Quake's full light) on a light `l` (1: Quake's full), the hue kept. No dither (one light a quad).
[[nodiscard]] glm::vec3 retroLevels(const glm::vec3& l)
{
    const float steps = za::clamp(vr_retrolight_model_steps.value, 0.f, 256.f);
    const float m = za::max(l.x, l.y, l.z) * 0.5f;
    if(steps <= 0.f || m <= 1e-5f)
    {
        return l;
    }
    // Even in brightness: the levels in the square root of the light (m^0.5, then back by squaring); else in the light
    // itself. (A square root and a product: two powf calls a particle were a quarter millisecond a frame in a fight.)
    if(vr_retrolight_spacing.value != 0.f)
    {
        const float q = za::max(za::floor(za::sqrt(m) * steps + 0.5f), 0.f) / steps;
        return l * (q * q / m);
    }
    const float q = za::floor(m * steps + 0.5f);
    return l * (za::max(q, 0.f) / steps / m);
}

// What litLight needs of the frame.
struct LitFrame
{
    const float* curve; // lighting::lightCurve of each lightmap value
    const FrameLight* lights;
    int lightCount;
    bool darkplaces, retro;
};

// A lit particle's light: its lightmap's (after the contrast, as the world's) and the dynamic lights added after it.
[[nodiscard]] glm::vec3 litLight(const Particle& p, const LitFrame& lf, za::SizeT& reached)
{
    glm::vec3 c{lf.curve[p.lightmap[0]], lf.curve[p.lightmap[1]], lf.curve[p.lightmap[2]]};
    for(int k = 0; k < lf.lightCount; k++)
    {
        const FrameLight& l = lf.lights[k];
        const glm::vec3 to = p.org - l.pos;
        const float d2 = glm::dot(to, to);
        if(d2 >= l.radius * l.radius)
        {
            continue;
        }
        reached++;
        const float dist = za::sqrt(d2);
        float add = lf.darkplaces ? darkplacesAtten(dist, l.radius) * 128.f : l.radius - dist; // (128: Quake's full)
        if(l.spot.w != 0.f && dist > 1e-3f) // VR_SpotCone's
        {
            const float t = za::clamp(l.spot.w - glm::dot(glm::vec3{l.spot}, to) / dist, 0.f, 1.f);
            add *= 1.f - t * t * (3.f - 2.f * t);
        }
        if(add > 0.f)
        {
            c += add * l.color;
        }
    }
    const glm::vec3 f = glm::clamp(c * (1.f / 128.f), glm::vec3{0.f}, glm::vec3{2.f});
    return lf.retro ? retroLevels(f) : f;
}

// Every particle's light this frame (lightOf), and the dynamic lights that reach any.
void lightParticles()
{
    const za::I64 start = za::Clock::nowNanoseconds();
    LightStats st;
    if(lightOf.size() < pool.size())
    {
        lightOf.resize(pool.size());
    }
    const bool on = lightOn() && cl.worldmodel;
    frameLightCount = 0;
    if(on)
    {
        for(int i = 0; i < r_framedata.numlights && i < MAX_DLIGHTS; i++)
        {
            const gpulight_t& l = r_lightbuffer.lights[i];
            if(l.shadow[3] != 0.f || l.gatelo[3] != 0.f || l.radius <= 0.f)
            {
                continue; // a map light's shadow entry, a light through a portal, none
            }
            const glm::vec3 axis{l.spot[0], l.spot[1], l.spot[2]};
            frameLights[frameLightCount++] = {{l.pos[0], l.pos[1], l.pos[2]}, l.radius, {l.color[0], l.color[1], l.color[2]},
                glm::dot(axis, axis) > 0.f ? glm::vec4{axis, l.spot[3]} : glm::vec4{0.f}};
        }
    }
    st.lights = frameLightCount;
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    // lighting::lightCurve on every lightmap value once (a power each: not three for every particle).
    float curve[256];
    for(int v = 0; v < 256; v++)
    {
        float k[3] = {static_cast<float>(v), 0.f, 0.f};
        lighting::lightCurve(k);
        curve[v] = k[0];
    }
    const bool retro = retrolight::on() && vr_retrolight_models.value != 0.f;
    const LitFrame lf{curve, frameLights, frameLightCount, darkplaces, retro};
    // The lightmap's light in order on this thread: the traces share a budget and a cache, and R_LightPoint its globals
    // (and the model's data: never read on the pool's threads). With many particles and lights, the dynamic lights
    // after it on the game's threads (each writing only its own lightOf: the same results however it is split);
    // else in the same pass (splitLightWork).
    const za::SizeT work = static_cast<za::SizeT>(lightStats.lit) * static_cast<za::SizeT>(frameLightCount) + 4 * lightStats.reached;
    const bool split = jobs::workers() > 0 && work >= splitLightWork;
    {
        // (profiling builds: on this thread, every particle's lightmap light; the dynamic lights too unless split)
        QVR_TRACY_SCOPE(zoneLightmap, "particle light: lightmap pass");
        for(za::SizeT i = 0; i < pool.size(); i++)
        {
            Particle& p = pool[i];
            if(p.lighting == Lighting::Auto)
            {
                p.lighting = classify(p);
            }
            if(!on || p.lighting == Lighting::Emissive)
            {
                st.emissive += p.lighting == Lighting::Emissive ? 1 : 0;
                lightOf[i] = glm::vec3{1.f};
                continue;
            }
            lightmapOf(p, i, st);
            if(!split)
            {
                lightOf[i] = litLight(p, lf, st.reached);
            }
            st.lit++;
        }
    }
    if(split)
    {
        za::Atomic<za::SizeT> reached{0};
        jobs::parallelFor(lightSite, pool.size(), 1024, [&](za::SizeT begin, za::SizeT end) {
            za::SizeT n = 0;
            for(za::SizeT i = begin; i < end; i++)
            {
                const Particle& p = pool[i];
                if(on && p.lighting != Lighting::Emissive)
                {
                    lightOf[i] = litLight(p, lf, n);
                }
            }
            reached.fetchAddRelaxed(n);
        });
        st.reached = reached.loadRelaxed(); // (after the call: every chunk ran)
    }
    lightCount = pool.size();
    st.ms = static_cast<double>(za::Clock::nowNanoseconds() - start) * 1e-6;
    lightMsSum += st.ms;
    lightFrames++;
    lightTraceSum += st.traces;
    lightLitSum += st.lit;
    lightStats = st;
}


// vr_debug_particle_waste's counts since the last report, drained from the GPU's 32-bit counters once a frame (a near
// burst at 2784 pixels an eye overflows them in a few frames).
za::U64 wasteTotals[gfx::particleWasteSlots * 3]{};
bool wasteCounted = false;

void drainWaste()
{
    za::U32 counts[gfx::particleWasteSlots * 3]{};
    if(gfx::readParticleWaste({counts, gfx::particleWasteSlots * 3}))
    {
        for(int i = 0; i < gfx::particleWasteSlots * 3; i++) wasteTotals[i] += counts[i];
        wasteCounted = true;
    }
}


// The frame's particles for the GPU (gfx::ParticleInstance, one record each, made and uploaded once a frame and drawn
// in both eyes: the quads are made in the vertex shader from each eye's camera), and a splash's rings and foam lying on
// the waves (lyingVertices, made on the CPU for each view: only those in its frustum, R_CullBox on a box round each,
// 64 units more than its quad). Each eye skips the draw (and the scene's distances) when the box round all of them is
// out of its view.
za::Vector<gfx::ParticleInstance> instances;
za::SizeT instanceCount = 0;   // this frame's (instances only grows: no record constructed again each frame)
za::Vector<za::U32> lying; // this frame's lying ones (indices into the pool)
gfx::ParticleBatch batch;         // this frame's upload
glm::vec3 boundsMin{0.f}, boundsMax{0.f}; // round all of this frame's quads (as far as each can reach)
int builtFrame = -1;             // host_framecount of the last build

void buildInstances()
{
    lying.clear();
    if(instances.size() < pool.size())
    {
        instances.resize(pool.size());
    }
    glm::vec3 lo{ZA_FLOAT_MAX}, hi{-ZA_FLOAT_MAX};
    gfx::ParticleInstance* out = instances.data();
    bool trim = false;
    for(za::SizeT i = 0; i < pool.size(); i++)
    {
        Particle& p = pool[i];
        if(p.flat && p.liquid)
        {
            lying.pushBack(static_cast<za::U32>(i)); // a splash's rings and foam (per view)
            continue;
        }
        const float reach = 1.5f * p.scale + (p.streak > 0.f ? 8.f : 0.f);
        lo = glm::min(lo, p.org - reach);
        hi = glm::max(hi, p.org + reach);
        // The quad's right and up, turned by the particle's angle about the view direction.
        if(p.angle != p.csAngle)
        {
            p.cs = {za::cos(p.angle), za::sin(p.angle)};
            p.csAngle = p.angle;
        }
        // Premultiplied: a glow's alpha 0 adds it.
        const float a = za::min(p.color.a, 1.f);
        const Softness sn = softness(p.cell, 0.75f * p.scale);
        gfx::ParticleInstance& q = *out++;
        q.org = p.org;
        q.half = 0.75f * p.scale;
        q.color = {glm::vec3{p.color} * lightOf[i] * a, p.additive ? 0.f : a}; // (lit: lightParticles)
        q.vel = p.vel;
        q.streak = p.streak;
        q.cos = p.cs.x;
        q.sin = p.cs.y;
        q.soft = sn.fade;
        q.pull = sn.pull;
        q.uv = cellUv[p.cell];
        q.flat = p.flat ? 1.f : 0.f;
        q.cell = cellTrim[p.cell] ? static_cast<float>(p.cell) : -1.f;
        q.kind = static_cast<float>(p.cell);
        q.pad = 0.f; // (flags.w: the shader skips retro textures; none does since the shore's spray, round 23)
        trim |= cellTrim[p.cell];
    }
    instanceCount = static_cast<za::SizeT>(out - instances.data());
    boundsMin = lo;
    boundsMax = hi;
    batch = gfx::uploadParticles({instances.data(), instanceCount}, trim);
}

// This view's lying ones (lieOnLiquid), those in its frustum.
void buildLying()
{
    lyingCount = 0;
    if(lying.empty())
    {
        return;
    }
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    // Near the eye as the others (the particle vertex shader): faded from vr_particle_near_fade to _near_cull, none nearer.
    const auto [cull, fadeFrom] = nearLimits();
    for(const za::U32 i : lying)
    {
        Particle& p = pool[i];
        const float eyeDistance = glm::distance(p.org, eye);
        if(eyeDistance < cull)
        {
            continue;
        }
        float nearFade = 1.f;
        if(eyeDistance < fadeFrom)
        {
            const float f = za::clamp((eyeDistance - cull) / za::max(fadeFrom - cull, 1e-3f), 0.f, 1.f);
            nearFade = f * f * (3.f - 2.f * f);
        }
        const float reach = 1.5f * p.scale + 64.f;
        vec3_t mins = {p.org.x - reach, p.org.y - reach, p.org.z - reach};
        vec3_t maxs = {p.org.x + reach, p.org.y + reach, p.org.z + reach};
        if(R_CullBox(mins, maxs))
        {
            continue;
        }
        if(p.angle != p.csAngle)
        {
            p.cs = {za::cos(p.angle), za::sin(p.angle)};
            p.csAngle = p.angle;
        }
        const float c = p.cs.x;
        const float s = p.cs.y;
        // Lying on the horizontal plane.
        const glm::vec3 pr{1.f, 0.f, 0.f};
        const glm::vec3 pu{0.f, 1.f, 0.f};
        glm::vec3 r = (pr * c + pu * s) * (0.75f * p.scale);
        glm::vec3 u = (pu * c - pr * s) * (0.75f * p.scale);
        if(p.streak > 0.f)
        {
            const glm::vec3 ray = glm::normalize(p.org - eye);
            const glm::vec3 across = p.vel - ray * glm::dot(p.vel, ray);
            const float speed = glm::length(across);
            if(speed > 1.f)
            {
                const glm::vec3 along = across / speed;
                r = glm::cross(along, ray) * (0.75f * p.scale);
                u = along * (0.75f * p.scale + za::min(p.streak * speed, 8.f));
            }
        }
        const float a = za::min(p.color.a, 1.f);
        const glm::vec4 color = glm::vec4{glm::vec3{p.color} * lightOf[i] * a, p.additive ? 0.f : a} * nearFade;
        lieOnLiquid(p, r, u, cellUv[p.cell], color, eye);
    }
}

} // namespace qvr::particles

// R_DrawSpriteModels: sprites (explosions, bubbles) left for VR_DrawSceneTranslucent, soft.
extern "C" int VR_SoftSprites(void)
{
    return qvr::particles::softOn();
}

// R_DrawSpriteModelsSoft: a sprite's fade distance (as a puff of that radius). Not moved towards the eye: it writes
// depth, as in the opaque pass, and would hide the particles in front of where it is (the explosion's smoke); an
// explosion is set 8 units out from what the rocket hit.
extern "C" float VR_SoftSpriteFade(float radius)
{
    return qvr::particles::softness(qvr::particles::CellExplosion, radius).fade;
}

namespace qvr::particles
{

// The near fade's and the size cap's limits as the particle vertex shader applies them (vr_gfx_gl.cpp): none within
// `cull2` (squared) of the eye, none more than `capHalf` pixels from middle to edge (0: no cap).
struct DrawnLimits
{
    float cull2, capHalf;
};
[[nodiscard]] DrawnLimits drawnLimits(float viewHeight)
{
    const float cull = nearLimits().cull;
    return {cull * cull, za::clamp(vr_particle_max_screen.value, 0.f, 4.f) * 0.5f * viewHeight};
}
// A particle's half size on the screen as drawn, `w` from the eye along the view.
[[nodiscard]] float drawnHalf(const gfx::ParticleInstance& q, const glm::vec3& eye, float w, float pixelScale,
    const DrawnLimits& lim)
{
    const glm::vec3 d = q.org - eye;
    if(glm::dot(d, d) < lim.cull2)
    {
        return 0.f;
    }
    const float half = q.half * pixelScale / w;
    return lim.capHalf > 0.f ? za::min(half, lim.capHalf) : half;
}

// How much of the view this frame's large particles cover (split: which are large), in views: roughly, by each one's
// middle (not its streak), each at most the whole view.
[[nodiscard]] float largeCover(const gfx::ParticleSplit& split, const int viewport[4])
{
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const glm::vec3 forward = glm::cross(up, right);
    const float screen = static_cast<float>(viewport[2]) * static_cast<float>(viewport[3]);
    if(screen <= 0.f)
    {
        return 0.f;
    }
    float area = 0.f;
    const DrawnLimits lim = drawnLimits(static_cast<float>(viewport[3]));
    for(za::SizeT i = 0; i < instanceCount; i++)
    {
        const gfx::ParticleInstance& q = instances[i];
        const float w = glm::dot(q.org - eye, forward);
        if(w <= 1.f)
        {
            continue;
        }
        const float half = drawnHalf(q, eye, w, split.pixelScale, lim);
        if(half > 0.f && half >= split.largePixels)
        {
            area += za::min(4.f * half * half, screen);
        }
    }
    return area / screen;
}

// Whether this frame's particles are drawn at half size (vr_particle_halfres): decided in its first view, for every
// view (the eyes alike), when they would cover enough of it to pay for blending them in (and clearing their
// target): from a view and a half of them, until under one.
bool halfRes = false;
int halfResFrame = -1;

[[nodiscard]] bool halfResThisFrame(const gfx::ParticleSplit& split, const int viewport[4])
{
    if(halfResFrame != host_framecount)
    {
        halfResFrame = host_framecount;
        const float cover = vr_particle_halfres_force.value != 0.f ? 2.f : largeCover(split, viewport);
        halfRes = cover >= (halfRes ? 1.f : 1.5f);
    }
    return halfRes;
}

// How many times over this frame's particles cover the view: each one's square (its middle's, not its streak) where
// it is on the screen, at `pixelScale` pixels a unit at distance 1, in views (not what the scene hides of them).
[[nodiscard]] float screenCover(float pixelScale, const int viewport[4])
{
    const float width = static_cast<float>(viewport[2]), height = static_cast<float>(viewport[3]);
    if(width <= 0.f || height <= 0.f)
    {
        return 0.f;
    }
    const glm::mat4 mvp = gfx::sceneViewProjection();
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const DrawnLimits lim = drawnLimits(height);
    float area = 0.f;
    for(za::SizeT i = 0; i < instanceCount; i++)
    {
        const gfx::ParticleInstance& q = instances[i];
        const glm::vec4 clip = mvp * glm::vec4{q.org, 1.f};
        if(clip.w <= 1.f)
        {
            continue;
        }
        const float half = drawnHalf(q, eye, clip.w, pixelScale, lim);
        const float x = (clip.x / clip.w * 0.5f + 0.5f) * width, y = (clip.y / clip.w * 0.5f + 0.5f) * height;
        const float across = za::min(x + half, width) - za::max(x - half, 0.f);
        const float down = za::min(y + half, height) - za::max(y - half, 0.f);
        if(across > 0.f && down > 0.f)
        {
            area += across * down;
        }
    }
    return area / (width * height);
}

// Whether this frame's particles are composited in reverse order, skipping what is hidden (vr_particle_saturate):
// decided in its first view, for every view, when all of them would cover enough of it to pay for the target's clear,
// the opaque marks and the blend into the scene: from vr_particle_saturate_cover views of them, until under two thirds
// of that (0: always).
bool reverseOrder = false;
int reverseOrderFrame = -1;

[[nodiscard]] bool reverseOrderThisFrame(float pixelScale, const int viewport[4])
{
    if(vr_particle_saturate.value == 0.f)
    {
        return false;
    }
    if(reverseOrderFrame != host_framecount)
    {
        reverseOrderFrame = host_framecount;
        const float from = za::max(0.f, vr_particle_saturate_cover.value);
        const float cover = from > 0.f ? screenCover(pixelScale, viewport) : 0.f;
        reverseOrder = cover >= (reverseOrder ? from * (2.f / 3.f) : from);
    }
    return reverseOrder;
}

// vr_frame_log: this frame's particles in its first view (`framelog:` lines): live, drawn, how many within half a metre
// of the eye, how many times over they cover the view (and the large ones), and how they are drawn.
int loggedFrame = -1;

void logFrame(const gfx::ParticleSplit& split, const int viewport[4], bool half)
{
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float nearUnits = units::metresToUnits() * 0.5f;
    int nearCount = 0;
    for(za::SizeT i = 0; i < instanceCount; i++)
    {
        const glm::vec3 d = instances[i].org - eye;
        nearCount += glm::dot(d, d) < nearUnits * nearUnits ? 1 : 0;
    }
    profile::systems::frameLogLine(va("framelog: %d particles %d live, %d drawn, %d within 0.5 m, cover %.2f views (large %.2f), half %d, "
               "reverse %d, view %dx%d, half targets made %d\n",
        host_framecount, liveCount(), static_cast<int>(instanceCount), nearCount,
        screenCover(split.pixelScale, viewport), largeCover(split, viewport), half ? 1 : 0,
        reverseOrderThisFrame(split.pixelScale, viewport) ? 1 : 0, viewport[2], viewport[3], gfx::halfTargetsMade()));
}

} // namespace qvr::particles

// R_RenderScene, after the translucent pass: the sprites the opaque pass left (soft), the particles, depth-tested
// against the scene.
extern "C" void VR_DrawSceneTranslucent()
{
    QVR_GPU_PROFILE("vr particles");
    using namespace qvr;
    using namespace qvr::particles;

    // The particles first (on the CPU): the simulation and the records for the GPU once a frame, then this view's
    // pieces lying on the waves (those in its frustum). The shore's foam first (once a frame: its own clock).
    if((cl.protocolflags & PRFL_QUAKEVR) && builtFrame != host_framecount)
    {
        QVR_PROFILE("particle shore foam");
        shoreFoam();
    }
    const bool particles = (cl.protocolflags & PRFL_QUAKEVR) && !pool.empty() && atlas;
    const bool soft = softOn();
    bool inView = false;
    if(particles)
    {
        if(builtFrame != host_framecount)
        {
            builtFrame = host_framecount;
            if(vr_debug_particle_waste.value != 0.f)
            {
                drainWaste(); // the last frame's (vr_particle_waste_report)
            }
            {
                QVR_PROFILE("particle sim");
                run();
            }
            {
                QVR_PROFILE("particle light");
                lightParticles(); // (after the first view's R_PushDlights: this frame's dynamic lights)
            }
            QVR_PROFILE("particle verts");
            buildInstances();
        }
        {
            QVR_PROFILE("particle lying");
            buildLying();
        }
        if(instanceCount > 0)
        {
            vec3_t mins = {boundsMin.x, boundsMin.y, boundsMin.z};
            vec3_t maxs = {boundsMax.x, boundsMax.y, boundsMax.z};
            inView = !R_CullBox(mins, maxs);
        }
    }
    else
    {
        lyingCount = 0;
    }
    const bool drawn = inView || lyingCount > 0;
    const int retroSet = retro::categorySet(retro::Category::Particles);

    // Retro keeps the full-resolution look by default. Its separate opt-in half-resolution
    // mode retains the world grid/palette but trades some edge detail for lower overdraw.
    int viewport[4];
    R_SceneViewport(viewport);
    // (Quake's projection: its rows swapped about, as R_DrawParticles reads it: 1 / tan(fov y / 2) at [2][1].)
    const gfx::ParticleSplit split{za::abs(r_matproj[2 * 4 + 1]) * 0.5f * static_cast<float>(viewport[3]),
        retroSet > 0 ? za::max(0.f, vr_particle_retro_halfres_pixels.value) :
                       za::max(0.f, vr_particle_halfres_pixels.value),
        static_cast<float>(viewport[2]), static_cast<float>(viewport[3])};
    const bool allowHalf = retroSet > 0 ? vr_particle_retro_halfres.value != 0.f : vr_particle_halfres.value != 0.f;
    // (Also in the frames composited in reverse order, the densest: at 2784 an eye 1.4 against 5.0 ms at full size,
    // ROUND22.md "Half resolution in the densest frames"; vr_particle_halfres_dense 0, full size there, retired.)
    const bool half = inView && allowHalf &&
                      (GL_NeedsSceneEffects() || GL_NeedsPostprocess()) && halfResThisFrame(split, viewport);
    if(inView && loggedFrame != host_framecount && profile::systems::frameLogOn())
    {
        loggedFrame = host_framecount;
        logFrame(split, viewport, half);
    }

    // The opaque scene's distances, for the soft ones (the liquids' when they made them this view): only when a soft
    // sprite or particle is drawn, or particles at half size (hidden behind them).
    gfx::Texture distances = 0;
    if((drawn || R_SoftSpritesPending()) && (soft || half))
    {
        QVR_GPU_PROFILE("vr scene distances");
        distances = water::opaqueSceneDistances();
    }
    if(R_SoftSpritesPending())
    {
        QVR_GPU_PROFILE("sprites");
        R_DrawSpriteModelsSoft(distances); // first: they stood for opaque (and write depth)
        if(distances && drawn)
        {
            // The particles fade against them too (moved towards the eye, they would be drawn over them).
            water::sceneDepthChanged();
            distances = water::opaqueSceneDistances();
        }
    }

    text3d::drawTranslucent(); // the floating texts, the wrist log (vr_text3d.cpp)
    flashlight::drawTranslucent(); // the flashlight's visible beam (vr_flashlight.cpp)
    weaponfx::drawTranslucent(); // the bullet tracers (vr_weaponfx.cpp)
    lootbeams::drawTranslucent(); // the pillars of light over loot lying about (vr_lootbeams.cpp)
    lines::drawInScene();          // the lightning's arcs (vr_shock.cpp), depth-tested

    if(!drawn)
    {
        return;
    }

    QVR_PROFILE("particle upload"); // (the draw calls; the records were uploaded once this frame)
    const gfx::State state{.shade = gfx::Shade::Texture, .blend = gfx::Blend::Premultiplied, .depthTest = true, .depthWrite = false,
        .sceneDistances = soft ? distances : 0, .retro = retroSet}; // retro textures (vr_retro.hpp)
    if(lyingCount > 0)
    {
        gfx::draw({lyingVertices.data(), lyingCount}, gfx::sceneViewProjection(), state, atlas); // first: under the rest
        lyingCount = 0;
    }
    if(inView)
    {
        // Moved towards the eye only when the scene's distances are read (else not soft at all). At half size: all of
        // them (vr_particle_halfres_pixels 0), or the large ones, blended in under the small ones.
        const bool pull = soft && distances;
        bool halfDrawn = false;
        if(half && distances)
        {
            QVR_GPU_PROFILE("half size");
            int width = 0, height = 0;
            water::opaqueSceneDistancesSize(width, height);
            halfDrawn = gfx::drawParticlesHalf(batch, pull, atlas, split, distances, width, height, viewport, soft, retroSet, R_SetupGL);
        }
        if(!halfDrawn || split.largePixels > 0.f)
        {
            gfx::drawParticles(batch, pull, state, atlas, halfDrawn ? gfx::ParticlePass::Small : gfx::ParticlePass::All, split,
                reverseOrderThisFrame(split.pixelScale, viewport));
        }
    }
}

namespace
{

// The grenades' trails as last logged (VR_GrenadeTrail, developer 1: a line as one starts or stops), by entity.
struct GrenadeTrailLog
{
    za::Bitset<MAX_EDICTS> seen, smoking;
};
GrenadeTrailLog grenadeTrails;

} // namespace

// A grenade's smoke trail: not a hand grenade with its pin in (vr_grenade.qc: skin 1 of progs/grenade.mdl, of the
// multi-grenade's progs/mervup.mdl or of the proximity grenade's progs/proxbomb.mdl, muted by make_grenade_skins.py until
// it is armed).
extern "C" int VR_GrenadeTrail(int ent)
{
    if(ent <= 0 || ent >= cl.num_entities)
    {
        return 1;
    }
    const entity_t& e = cl_entities[ent];
    constexpr int unarmedSkin = 1;
    const bool smokes = !(e.model && e.skinnum == unarmedSkin && qvr::modelmeta::isQuakeGrenade(e.model));
    // developer 1: each grenade's trail as it starts or stops.
    const auto n = static_cast<size_t>(ent);
    if(developer.value && (!grenadeTrails.seen[n] || grenadeTrails.smoking[n] != smokes))
    {
        grenadeTrails.seen.set(static_cast<za::SizeT>(n));
        grenadeTrails.smoking.setBit(static_cast<za::SizeT>(n), smokes);
        Con_DPrintf("grenade %d (%s, skin %d): smoke trail %s\n", ent, e.model ? e.model->name : "?", e.skinnum,
            smokes ? "on" : "off");
    }
    return smokes;
}

// Quake's own effects, when Quake VR's particles are on (as the old engine drew them).
extern "C" int VR_RunParticleEffect(const float* org, const float* dir, int color, int count)
{
    QVR_PROFILE("particle spawn");
    using namespace qvr;
    using namespace qvr::particles;
    if(!(cl.protocolflags & PRFL_QUAKEVR))
    {
        return 0;
    }
    const glm::vec3 o{org[0], org[1], org[2]};
    const glm::vec3 d{dir[0], dir[1], dir[2]};
    if(count == 1024)
    {
        return spawn(o, d, Preset::Explosion, 1); // Quake's "explosion" count
    }
    if(color >= 64 && color < 80)
    {
        return spawn(o, d, Preset::Blood, za::max(1, count / 8));
    }
    if(!vr_particles.value || !ensureAtlas())
    {
        return 0;
    }
    // A scrag's spit and a hell knight's spike on a wall (TE_WIZSPIKE, TE_KNIGHTSPIKE).
    if((color == 20 && count == 30) || (color == 226 && count == 20))
    {
        magicImpact(o, color == 20 ? scragGreen : knightOrange, count);
        return 1;
    }
    // Impacts: the bullet puff in the effect's colour.
    bulletPuff(o, d, color, count);
    return 1;
}

extern "C" int VR_ParticleExplosion(const float* org)
{
    QVR_PROFILE("particle spawn");
    using namespace qvr;
    using namespace qvr::particles;
    return (cl.protocolflags & PRFL_QUAKEVR) && spawn({org[0], org[1], org[2]}, glm::vec3{0.f}, Preset::Explosion, 1);
}

extern "C" int VR_ParticleExplosion2(const float* org, int colorStart, int colorLength)
{
    using namespace qvr;
    using namespace qvr::particles;
    if(!(cl.protocolflags & PRFL_QUAKEVR) || !vr_particles.value || !ensureAtlas())
    {
        return 0;
    }
    explosion2({org[0], org[1], org[2]}, colorStart, colorLength);
    return 1;
}

// Quake's trails (CL_RocketTrail, R_RocketTrail's types) and splashes (r_part.c), when Quake VR's
// particles are on. A trail is as big as the model leaving it (a lava ball's bigger than a rocket's).
extern "C" int VR_EntityTrail(int ent, int type)
{
    QVR_PROFILE("particle spawn");
    using namespace qvr::particles;
    if(!enabled() || ent <= 0 || ent >= cl.num_entities)
    {
        return 0;
    }
    const entity_t& e = cl_entities[ent];
    float size = 1.f;
    if(e.model)
    {
        const glm::vec3 extent{e.model->maxs[0] - e.model->mins[0], e.model->maxs[1] - e.model->mins[1],
            e.model->maxs[2] - e.model->mins[2]};
        size = za::clamp(glm::length(extent) * 0.5f / 9.f, 0.7f, 2.5f);
    }
    if((type & 127) == 1 && qvr::vr_adgun_bolt_fx.value && adElectricBolt(e.model)) // AD support: its plasma rifles' bolt: electric, not grenade smoke
    {
        electricTrail({e.trailorg[0], e.trailorg[1], e.trailorg[2]}, {e.origin[0], e.origin[1], e.origin[2]});
        return 1;
    }
    rocketTrail({e.trailorg[0], e.trailorg[1], e.trailorg[2]}, {e.origin[0], e.origin[1], e.origin[2]}, type, size);
    return 1;
}

extern "C" int VR_BlobExplosion(const float* org)
{
    using namespace qvr::particles;
    if(!enabled())
    {
        return 0;
    }
    blobExplosion({org[0], org[1], org[2]});
    return 1;
}

extern "C" int VR_LavaSplash(const float* org)
{
    using namespace qvr::particles;
    if(!enabled())
    {
        return 0;
    }
    lavaSplash({org[0], org[1], org[2]});
    return 1;
}

extern "C" int VR_TeleportSplash(const float* org)
{
    using namespace qvr::particles;
    if(!enabled())
    {
        return 0;
    }
    teleportSplash({org[0], org[1], org[2]});
    return 1;
}
