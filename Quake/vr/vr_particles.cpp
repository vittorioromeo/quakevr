// vr_particles.cpp -- see vr_particles.hpp. The presets and the per-type behaviours are the old
// engine's (r_part.cpp: R_RunParticle2Effect and friends, CL_RunParticles), with the same
// numbers; the drawing is camera-facing quads of 1.5 x scale units, turned by their angle about
// the view direction, as the old geometry shader built them.

#include "vr_particles.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_text3d.hpp"
#include "vr_flashlight.hpp"
#include "vr_profile.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace qvr::particles
{
namespace
{

// Colour ramps (palette indices).
constexpr int ramp1[8] = {111, 112, 107, 105, 103, 101, 99, 97}; // gold, brown, peach
constexpr int ramp2[8] = {111, 110, 109, 108, 107, 106, 104, 102}; // gold, brown

enum Type : std::uint8_t
{
    Static,
    Explode,
    Explode2,
    Blob,
    TxExplode,
    TxSmoke,
    TxBigSmoke,
    Lightning,
    Rock,
    GunSmoke,
    GunPickup,
    Drip,   // fades out quickly (a gib's blood trail)
    Custom, // fades, grows, slows and spins at its own rates (fade, grow, drag, spin)
};

// Atlas cells.
enum Cell : std::uint8_t
{
    CellCircle,
    CellExplosion,
    CellSmoke,
    CellBlood,
    CellBloodMist,
    CellLightning,
    CellSpark,
    CellRock,
    CellGunSmoke,
    CellGlow, // a soft round glow (generated)
    CellRing, // a soft ring (generated): ripples on a liquid
    CellCount
};

struct Particle
{
    glm::vec3 org;
    glm::vec3 vel{0.f};
    glm::vec3 acc{0.f};
    glm::vec4 color{1.f};
    float angle{0.f};
    float scale{1.f};
    float ramp{0.f};
    double die{0.0};
    float fade{0.f}; // Custom: alpha per second,
    float grow{0.f}; // scale per second,
    float drag{0.f}; // the part of its speed lost per second,
    float spin{0.f}; // radians per second
    float floor{-1e9f}; // gone once it falls below this height (a drop back into its liquid)
    Type type{Static};
    Cell cell{CellCircle};
    bool spinBack{false};
    bool additive{false}; // glows: added to the scene (else alpha blended)
    bool flat{false};     // lying flat (a ripple on a liquid), not facing the view
};

constexpr std::size_t maxParticles = 32768;
std::vector<Particle> pool;
double lastRun = -1.0;

std::mt19937 rng{std::random_device{}()};

[[nodiscard]] float rnd(float lo, float hi)
{
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

[[nodiscard]] int rndi(int lo, int hiExclusive)
{
    return std::uniform_int_distribution<int>{lo, hiExclusive - 1}(rng);
}

[[nodiscard]] float rndAngle()
{
    return rnd(0.f, 2.f * 3.14159265f);
}

[[nodiscard]] glm::vec3 paletteColor(int index)
{
    const auto* c = reinterpret_cast<const unsigned char*>(&d_8to24table[index & 255]);
    return {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f};
}

[[nodiscard]] glm::vec3 gravity(float mult)
{
    return {0.f, 0.f, -sv_gravity.value * mult};
}

// Makes `count` particles (times vr_particle_mult), each set up by `f(p)`.
template <typename F>
void make(float count, F&& f)
{
    const int n = static_cast<int>(count * std::max(0.f, vr_particle_mult.value));
    for(int i = 0; i < n && pool.size() < maxParticles; i++)
    {
        Particle p;
        p.angle = rndAngle();
        f(p, i);
        pool.push_back(p);
    }
}

void setColor(Particle& p, int index, float alpha255)
{
    p.color = glm::vec4{paletteColor(index), alpha255 / 255.f};
}

// How many of a trail's particles for `length` units of it, one per `spacing` on average (rounded
// at random: a short segment each frame still gets its share).
[[nodiscard]] float perLength(float length, float spacing)
{
    return std::floor(length / spacing + rnd(0.f, 1.f));
}

[[nodiscard]] glm::vec3 inBox(float half)
{
    return {rnd(-half, half), rnd(-half, half), rnd(-half, half)};
}

// A random direction.
[[nodiscard]] glm::vec3 onSphere()
{
    const float z = rnd(-1.f, 1.f);
    const float a = rndAngle();
    const float r = std::sqrt(std::max(0.f, 1.f - z * z));
    return {r * std::cos(a), r * std::sin(a), z};
}

// ---- Presets --------------------------------------------------------------------------------

void explosion(const glm::vec3& org)
{
    make(256, [&](Particle& p, int i) {
        p.cell = CellCircle;
        setColor(p, ramp1[0], 255);
        p.die = cl.time + 1.5;
        p.ramp = static_cast<float>(rndi(0, 4));
        p.scale = rnd(0.6f, 1.2f);
        p.acc = gravity(0.5f);
        p.type = i & 1 ? Explode : Explode2;
        p.org = org + glm::vec3{rnd(-16, 16), rnd(-16, 16), rnd(-16, 16)};
        p.vel = {rnd(-256, 256), rnd(-256, 256), rnd(-256, 256)};
    });
    make(64, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, ramp1[0], 255);
        p.die = cl.time + 1.5;
        p.scale = rnd(1.9f, 2.9f) * 0.55f;
        p.acc = gravity(0.5f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{rnd(-16, 16), rnd(-16, 16), rnd(-16, 16)};
        p.vel = {rnd(-256, 256), rnd(-256, 256), rnd(-256, 256)};
    });
    make(48, [&](Particle& p, int) {
        p.cell = CellRock;
        setColor(p, 167 + rndi(0, 8), 255);
        p.die = cl.time + 1.5;
        p.scale = rnd(0.9f, 1.9f);
        p.acc = gravity(0.5f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{rnd(-16, 16), rnd(-16, 16), rnd(-16, 16)};
        p.vel = {rnd(-256, 256), rnd(-256, 256), rnd(-256, 256)};
    });
    make(1, [&](Particle& p, int) {
        p.cell = CellExplosion;
        setColor(p, ramp1[0], 255);
        p.die = cl.time + 1.5;
        p.scale = rnd(0.5f, 2.1f) * 2.f;
        p.acc = gravity(0.05f);
        p.type = TxExplode;
        p.spinBack = rndi(0, 2);
        p.org = org + glm::vec3{rnd(-11, 11), rnd(-11, 11), rnd(-11, 11)};
        p.vel = {rnd(-8, 8), rnd(-8, 8), rnd(-8, 8)};
    });
    make(3, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), 225);
        p.die = cl.time + 3.5;
        p.scale = rnd(1.2f, 1.5f);
        p.type = TxSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {rnd(-24, 24), rnd(-24, 24), rnd(-24, 24)};
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
        p.scale = rnd(0.5f, 0.9f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(0.26f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = (dir + 0.3f) * glm::vec3{rnd(-75, 75), rnd(-75, 75), rnd(-75, 75)};
    });
    make(1, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), 45);
        p.die = cl.time + 1.25 * rndi(0, 5);
        p.scale = rnd(0.3f, 0.5f);
        p.type = TxSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{rndi(-2, 2), rndi(-2, 2), rndi(-2, 2)};
        p.vel = {rnd(-12, 12), rnd(-12, 12), rnd(-12, 12)};
    });
    make(count * 0.7f, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, debris(), 255);
        p.die = cl.time + 0.75 * rndi(0, 5);
        p.scale = rnd(0.05f, 0.3f);
        p.type = Static;
        p.acc = gravity(0.08f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {rnd(-24, 24), rnd(-24, 24), rnd(-24, 24) + rnd(10, 40)};
    });
    make(count * 0.4f, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, 109 + rndi(0, 8), 255);
        p.die = cl.time + 1.25 * rndi(0, 5);
        p.scale = rnd(1.95f, 2.87f) * 0.35f;
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(1.f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {rnd(-48, 48), rnd(-48, 48), rnd(60, 360)};
    });
}

void blood(const glm::vec3& org, const glm::vec3& dir, int count)
{
    constexpr int colors[] = {247, 248, 249, 250, 251};
    make(count * 2.f, [&](Particle& p, int) {
        p.cell = CellBlood;
        setColor(p, colors[rndi(0, 5)], 100);
        p.die = cl.time + 0.7 * rndi(0, 3);
        p.scale = rnd(0.35f, 0.6f) * 6.5f;
        p.type = Static;
        p.acc = gravity(0.29f);
        p.org = org + glm::vec3{rnd(-2, 2), rnd(-2, 2), rnd(-2, 2)};
        p.vel = (dir + 0.3f) * glm::vec3{rnd(-10, 10), rnd(-10, 10), rnd(-10, 10)};
        p.vel.z += rnd(0, 40);
    });
    make(count * 24.f, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, colors[rndi(0, 5)], 175);
        p.die = cl.time + 0.4 * rndi(0, 3);
        p.scale = rnd(0.12f, 0.2f);
        p.type = Static;
        p.acc = gravity(0.45f);
        p.org = org + glm::vec3{rnd(-2, 2), rnd(-2, 2), rnd(-2, 2)};
        p.vel = (dir + 0.3f) * glm::vec3{rnd(-3, 3), rnd(-3, 3), rnd(-3, 3)} * 13.f;
        p.vel.z += rnd(20, 60);
    });
    make(1, [&](Particle& p, int) {
        p.cell = CellBloodMist;
        setColor(p, 225, 38);
        p.die = cl.time + 2.0;
        p.scale = rnd(1.1f, 2.4f) * 15.f;
        p.acc = gravity(-0.03f);
        p.type = TxSmoke;
        p.org = org + glm::vec3{rnd(-8, 8), rnd(-8, 8), rnd(-8, 8)};
        p.vel = glm::vec3{-4.f};
    });
}

// Behind a flying gib, a few steps apart: a faint smear left in the air and drops falling from it.
void bloodTrail(const glm::vec3& org, const glm::vec3& dir, int count)
{
    constexpr int colors[] = {247, 248, 249, 250, 251};
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellBlood;
        setColor(p, colors[rndi(0, 5)], rnd(80, 120));
        p.die = cl.time + rnd(0.4f, 0.8f);
        p.scale = rnd(0.35f, 0.6f) * 7.f;
        p.type = Drip;
        p.acc = gravity(0.12f);
        p.org = org + glm::vec3{rnd(-1.5f, 1.5f), rnd(-1.5f, 1.5f), rnd(-1.5f, 1.5f)};
        p.vel = dir * rnd(0.f, 12.f) + glm::vec3{rnd(-4, 4), rnd(-4, 4), rnd(-4, 4)};
    });
    make(count * 2.f, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, colors[rndi(0, 5)], rnd(190, 240));
        p.die = cl.time + rnd(0.5f, 1.f);
        p.scale = rnd(0.25f, 0.45f);
        p.type = Static;
        p.acc = gravity(0.8f);
        p.org = org + glm::vec3{rnd(-4, 4), rnd(-4, 4), rnd(-4, 4)};
        p.vel = dir * rnd(0.f, 25.f) + glm::vec3{rnd(-10, 10), rnd(-10, 10), rnd(-6, 14)};
    });
}

void lightning(const glm::vec3& org, int count)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellLightning;
        setColor(p, 254, rnd(180, 220));
        p.die = cl.time + 1.2 * rndi(0, 3);
        p.scale = rnd(0.35f, 0.6f) * 6.2f;
        p.type = Lightning;
        p.org = org + glm::vec3{rnd(-3, 3), rnd(-3, 3), rnd(-3, 3)};
        p.vel = {rnd(-185, 185), rnd(-185, 185), rnd(-185, 185)};
    });
}

void smoke(const glm::vec3& org, int count, bool big)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), big ? 255 : 125);
        p.die = cl.time + 2.5 * rndi(0, 5);
        p.scale = big ? rnd(1.2f, 1.7f) * 2.8f : rnd(1.2f, 1.5f) * 0.8f;
        p.type = big ? TxBigSmoke : TxSmoke;
        p.acc = gravity(-0.09f);
        const int spread = big ? 5 : 4;
        p.org = org + glm::vec3{rndi(-spread, spread), rndi(-spread, spread), rndi(-spread, spread)};
        p.vel = {rnd(-24, 24), rnd(-24, 24), rnd(-24, 24)};
    });
}

// Sparks thrown up and falling (sparks, teleport): colours [lo, hi), scale factor, life.
void sparks(const glm::vec3& org, int count, int lo, int hi, float scale, double life)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, rndi(lo, hi), 255);
        p.die = cl.time + (life > 0.0 ? life : 2.0 * rndi(0, 5));
        p.scale = rnd(1.55f, 2.87f) * scale;
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(1.f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {rnd(-48, 48), rnd(-48, 48), rnd(60, 360)};
    });
}

void gunSmoke(const glm::vec3& org, int count)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellGunSmoke;
        p.angle = rnd(-10.f, 10.f) * 0.0174533f + 1.5707963f;
        setColor(p, rndi(10, 16), rnd(85, 125));
        p.die = cl.time + 3.5;
        p.scale = rnd(0.9f, 1.5f) * 0.1f;
        p.type = GunSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{0.f, 0.f, 3.f};
        p.vel = {rnd(-3, 3), rnd(-3, 3), rnd(-3, 3)};
    });
}

// Rising, fading sparkles (pickups, force grab, lava spikes).
void sparkles(const glm::vec3& org, int count, int lo, int hi, float alphaLo, float alphaHi, double life, float scale,
    float grav, float spread, float speed, float upLo, float upHi)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, rndi(lo, hi), rnd(alphaLo, alphaHi));
        p.die = cl.time + life;
        p.scale = rnd(1.55f, 2.87f) * scale;
        p.type = GunPickup;
        p.acc = gravity(grav);
        p.org = org + glm::vec3{rnd(-spread, spread), rnd(-spread, spread), rnd(-spread, spread)};
        p.vel = {rnd(-speed, speed), rnd(-speed, speed), upHi > upLo ? rnd(upLo, upHi) : rnd(-speed, speed)};
    });
}

// ---- Simulation -----------------------------------------------------------------------------

void run()
{
    if(cl.time == lastRun)
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
                p.color.a += p.fade * dt;
                p.scale += p.grow * dt;
                p.vel *= std::max(0.f, 1.f - p.drag * dt);
                p.angle += p.spin * dt;
                break;
            default: break;
        }
    }

    std::erase_if(pool, [](const Particle& p) {
        return p.die < cl.time || p.color.a <= 0.f || p.scale <= 0.f || (p.org.z < p.floor && p.vel.z < 0.f);
    });
}

// ---- Drawing --------------------------------------------------------------------------------

gfx::Texture atlas = 0;
bool atlasFailed = false;
glm::vec4 cellUv[CellCount]{};

constexpr int cellSize = 132; // 128 plus a border
constexpr int atlasColumns = 4;
constexpr int atlasRows = 3;

// A soft disc as the old engine's generated circle (sharpness 8): a quarter of the quad across,
// which is how big the old off-centre disc was.
[[nodiscard]] std::vector<std::uint8_t> buildDisc(int size)
{
    constexpr float sharpness = 8.f;
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    const float k = 16.f / (size * 0.25f); // the old 16-texel radius, over a quarter of the size
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) * k;
            const float dy = (y - c) * k;
            const float r = std::min(255.f, dx * dx + dy * dy);
            dst[static_cast<std::size_t>((y * size + x) * 4 + 3)] =
                static_cast<std::uint8_t>(std::min(255.f, sharpness * (255.f - r)));
        }
    }
    return dst;
}

// A soft glow: white, its alpha falling off as a Gaussian to nothing at the edge.
[[nodiscard]] std::vector<std::uint8_t> buildGlow(int size)
{
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r2 = dx * dx + dy * dy;
            const float a = std::max(0.f, std::exp(-r2 * 4.5f) - std::exp(-4.5f)) / (1.f - std::exp(-4.5f));
            dst[static_cast<std::size_t>((y * size + x) * 4 + 3)] = static_cast<std::uint8_t>(a * 255.f + 0.5f);
        }
    }
    return dst;
}

// A ripple: a soft white ring (a Gaussian band at 70% of the radius), fainter inside than out, as
// a wave's front is the steep side.
[[nodiscard]] std::vector<std::uint8_t> buildRing(int size)
{
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(size * size * 4), 255);
    const float c = (size - 1) * 0.5f;
    for(int y = 0; y < size; y++)
    {
        for(int x = 0; x < size; x++)
        {
            const float dx = (x - c) / c, dy = (y - c) / c;
            const float r = std::sqrt(dx * dx + dy * dy);
            const float band = (r - 0.7f) / (r < 0.7f ? 0.16f : 0.08f);
            const float a = r >= 0.98f ? 0.f : std::exp(-band * band);
            dst[static_cast<std::size_t>((y * size + x) * 4 + 3)] = static_cast<std::uint8_t>(a * 255.f + 0.5f);
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
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4), 0);

    const auto put = [&](Cell cell, const std::uint8_t* rgba, int w, int h) {
        const int cx = (cell % atlasColumns) * cellSize + 2;
        const int cy = (cell / atlasColumns) * cellSize + 2;
        for(int y = 0; y < h; y++)
        {
            std::memcpy(&pixels[static_cast<std::size_t>(((cy + y) * width + cx) * 4)], rgba + static_cast<std::size_t>(y * w * 4),
                static_cast<std::size_t>(w * 4));
        }
        // Half a texel in, so that filtering stays inside the image.
        cellUv[cell] = {(cx + 0.5f) / width, (cy + 0.5f) / height, (cx + w - 0.5f) / width, (cy + h - 0.5f) / height};
    };

    put(CellCircle, buildDisc(64).data(), 64, 64);
    put(CellGlow, buildGlow(64).data(), 64, 64);
    put(CellRing, buildRing(96).data(), 96, 96);

    struct File
    {
        Cell cell;
        const char* name;
    };
    constexpr File files[] = {{CellExplosion, "textures/particle_explosion"}, {CellSmoke, "textures/particle_smoke"},
        {CellBlood, "textures/particle_blood"}, {CellBloodMist, "textures/particle_blood_mist"},
        {CellLightning, "textures/particle_lightning"}, {CellSpark, "textures/particle_spark"},
        {CellRock, "textures/particle_rock"}, {CellGunSmoke, "textures/particle_gun_smoke"}};
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
    for(std::size_t i = 0; i < pixels.size(); i += 4)
    {
        for(std::size_t c = 0; c < 3; c++)
        {
            pixels[i + c] = static_cast<std::uint8_t>((pixels[i + c] * pixels[i + 3] + 127) / 255);
        }
    }
    atlas = gfx::createTexture(width, height, pixels.data(), true);
    atlasFailed = atlas == 0;
    return atlas != 0;
}

// A coloured explosion (TE_EXPLOSION2: the old R_ParticleExplosion2): coloured blobs, sparks,
// rocks, the fireball and smoke.
void explosion2(const glm::vec3& org, int colorStart, int colorLength)
{
    int colorMod = 0;
    make(256, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, colorStart + (colorMod++ % std::max(colorLength, 1)), 255);
        p.die = cl.time + 1.5;
        p.scale = rnd(1.6f, 3.5f);
        p.acc = gravity(0.5f);
        p.type = Blob;
        p.org = org + glm::vec3{rnd(-16, 16), rnd(-16, 16), rnd(-16, 16)};
        p.vel = {rnd(-256, 256), rnd(-256, 256), rnd(-256, 256)};
    });
    explosion(org); // the rest as the plain explosion's (minus its ramp circles, close enough)
}

// ---- Quake's own effects (R_RocketTrail, R_BlobExplosion, R_LavaSplash, R_TeleportSplash) -----

// The colours of the scrag's, hell knight's and vore's projectiles, as their lights (vr_emissive.cpp).
const glm::vec3 scragGreen{0.45f, 1.f, 0.2f};
const glm::vec3 knightOrange{1.f, 0.52f, 0.16f};
const glm::vec3 vorePurple{0.95f, 0.4f, 0.9f};

[[nodiscard]] glm::vec3 fireColor()
{
    return glm::mix(glm::vec3{1.f, 0.42f, 0.1f}, glm::vec3{1.f, 0.8f, 0.35f}, rnd(0.f, 1.f));
}

// Flames licking off something burning as it flies (rockets, lava balls, hell knight flames), at
// `spacing` units apart along `from` -> `to`.
void flames(const glm::vec3& from, const glm::vec3& to, float spacing, float scale, const glm::vec3& tint)
{
    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), spacing), [&](Particle& p, int) {
        p.cell = CellExplosion;
        p.additive = true;
        p.color = glm::vec4{fireColor() * tint, rnd(0.45f, 0.7f)};
        p.die = cl.time + 0.45;
        p.scale = rnd(0.8f, 1.2f) * scale;
        p.type = Custom;
        p.fade = -rnd(1.8f, 2.6f);
        p.grow = 2.5f * scale;
        p.spin = rnd(-3.f, 3.f);
        p.org = from + d * rnd(0.f, 1.f) + inBox(1.f);
        p.vel = inBox(8.f) + glm::vec3{0.f, 0.f, 12.f};
    });
}

// Embers: small sparks thrown off, falling a little, `spacing` units apart.
void embers(const glm::vec3& from, const glm::vec3& to, float spacing, const glm::vec3& color, float speed, float grav)
{
    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), spacing), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{color, 1.f};
        p.die = cl.time + rnd(0.5f, 0.9f);
        p.scale = rnd(0.22f, 0.4f);
        p.type = Custom;
        p.fade = -1.5f;
        p.grow = -0.2f;
        p.drag = 1.2f;
        p.spin = rnd(-6.f, 6.f);
        p.acc = gravity(grav);
        p.org = from + d * rnd(0.f, 1.f);
        p.vel = onSphere() * rnd(0.3f, 1.f) * speed;
    });
}

// Smoke left behind, `spacing` units apart: `grey` its colour, `alpha` how thick.
void smokeTrail(const glm::vec3& from, const glm::vec3& to, float spacing, float grey, float alpha, float scale)
{
    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), spacing), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = grey * rnd(0.85f, 1.15f);
        p.color = glm::vec4{g, g, g * 0.97f, alpha * rnd(0.8f, 1.2f)};
        p.die = cl.time + rnd(1.8f, 2.6f);
        p.scale = rnd(0.8f, 1.2f) * scale;
        p.type = Custom;
        p.fade = -alpha / 2.2f;
        p.grow = 3.5f * scale;
        p.drag = 0.8f;
        p.spin = rnd(-0.6f, 0.6f);
        p.acc = gravity(-0.02f);
        p.org = from + d * rnd(0.f, 1.f) + inBox(1.5f);
        p.vel = inBox(6.f);
    });
}

// A magic projectile's trail: a soft glow of its colour and sparkles drifting off it.
void sparkleTrail(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color, float glowScale)
{
    const glm::vec3 d = to - from;
    const float length = glm::length(d);
    make(perLength(length, 1.2f), [&](Particle& p, int) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{color, 0.22f};
        p.die = cl.time + 0.45;
        p.scale = rnd(0.9f, 1.1f) * glowScale;
        p.type = Custom;
        p.fade = -0.5f;
        p.grow = -glowScale * 1.8f;
        p.org = from + d * rnd(0.f, 1.f);
    });
    make(perLength(length, 4.f), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(color, glm::vec3{1.f}, rnd(0.f, 0.4f)), 1.f};
        p.die = cl.time + rnd(0.5f, 0.8f);
        p.scale = rnd(0.25f, 0.45f);
        p.type = Custom;
        p.fade = -1.4f;
        p.grow = -0.25f;
        p.drag = 1.5f;
        p.spin = rnd(-8.f, 8.f);
        p.acc = gravity(0.03f);
        p.org = from + d * rnd(0.f, 1.f) + inBox(1.5f);
        p.vel = onSphere() * rnd(12.f, 36.f);
    });
}

// R_RocketTrail's trails, `type` as Quake's (128 and up: denser, the same here), for a model `size`
// times a rocket's.
void rocketTrail(const glm::vec3& from, const glm::vec3& to, int type, float size)
{
    switch(type & 127)
    {
        case 0: // a rocket, a lava ball: fire, embers and dark smoke
            flames(from, to, 2.2f / size, 1.8f * size, glm::vec3{1.f});
            embers(from, to, 8.f / size, glm::vec3{1.f, 0.6f, 0.2f}, 40.f * size, 0.15f);
            smokeTrail(from, to, 4.f, 0.28f, 0.4f, 1.2f * size);
            break;
        case 1: // a grenade: light grey smoke, and its fuse
            smokeTrail(from, to, 3.f, 0.55f, 0.4f, 0.8f);
            embers(from, to, 14.f, glm::vec3{1.f, 0.55f, 0.15f}, 15.f, 0.1f);
            break;
        case 2: // blood
        case 4: // a little blood
        {
            const glm::vec3 d = to - from;
            const float length = glm::length(d);
            const glm::vec3 dir = length > 0.f ? d / length : glm::vec3{0.f};
            const float n = perLength(length, (type & 127) == 2 ? 6.f : 12.f);
            for(int i = 0; i < static_cast<int>(n); i++)
            {
                bloodTrail(from + d * rnd(0.f, 1.f), dir, 1);
            }
            break;
        }
        case 3: sparkleTrail(from, to, scragGreen, 4.f); break; // a scrag's spit
        case 5:                                                 // a hell knight's flame
            flames(from, to, 2.f, 1.2f, glm::vec3{1.f, 0.85f, 0.7f});
            sparkleTrail(from, to, knightOrange, 3.f);
            break;
        case 6: sparkleTrail(from, to, vorePurple, 5.f); break; // a vore's ball
        default: break;
    }
}

// A glowing burst where a scrag's spit or a hell knight's spike hits a wall.
void magicImpact(const glm::vec3& org, const glm::vec3& color, int count)
{
    make(1, [&](Particle& p, int) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{color, 0.7f};
        p.die = cl.time + 0.3;
        p.scale = 9.f;
        p.type = Custom;
        p.fade = -2.8f;
        p.grow = 14.f;
        p.org = org;
    });
    make(count, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(color, glm::vec3{1.f}, rnd(0.f, 0.35f)), 1.f};
        p.die = cl.time + rnd(0.4f, 0.8f);
        p.scale = rnd(0.3f, 0.55f);
        p.type = Custom;
        p.fade = -1.4f;
        p.drag = 2.f;
        p.spin = rnd(-8.f, 8.f);
        p.acc = gravity(0.2f);
        p.org = org + inBox(2.f);
        p.vel = onSphere() * rnd(40.f, 120.f);
    });
}

// A tarbaby blowing up: violet and red blobs flung out, a violet flash, sparks and smoke.
void blobExplosion(const glm::vec3& org)
{
    make(160, [&](Particle& p, int i) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{i & 1 ? glm::vec3{0.65f, 0.25f, 1.f} : glm::vec3{1.f, 0.2f, 0.35f}, 0.9f};
        p.die = cl.time + rnd(1.f, 1.4f);
        p.scale = rnd(1.4f, 2.4f);
        p.type = Custom;
        p.fade = -0.8f;
        p.grow = -0.6f;
        p.drag = 1.6f;
        p.acc = gravity(0.3f);
        p.org = org + inBox(16.f);
        p.vel = inBox(256.f);
    });
    make(3, [&](Particle& p, int) {
        p.cell = CellSmoke; // grey: tinted violet (the fireball's texture is orange)
        p.additive = true;
        p.color = glm::vec4{0.6f, 0.3f, 1.f, 1.f};
        p.die = cl.time + 1.5;
        p.scale = rnd(1.2f, 2.f) * 2.f;
        p.type = TxExplode;
        p.spinBack = rndi(0, 2);
        p.org = org + inBox(8.f);
        p.vel = inBox(8.f);
    });
    make(48, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(glm::vec3{0.7f, 0.35f, 1.f}, glm::vec3{1.f}, rnd(0.f, 0.4f)), 1.f};
        p.die = cl.time + 1.2;
        p.scale = rnd(0.9f, 1.4f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(0.5f);
        p.org = org + inBox(12.f);
        p.vel = inBox(220.f);
    });
    make(4, [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.color = glm::vec4{0.22f, 0.16f, 0.26f, 0.8f};
        p.die = cl.time + 3.5;
        p.scale = rnd(1.4f, 1.9f);
        p.type = TxSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + inBox(6.f);
        p.vel = inBox(24.f);
    });
}

// Lava thrown up over a 256-unit square (Chthon rising): glowing droplets and embers, bursts of
// fire and smoke.
void lavaSplash(const glm::vec3& org)
{
    const auto spot = [&](glm::vec3& dir) {
        dir = {rnd(-128.f, 128.f), rnd(-128.f, 128.f), 256.f};
        return org + glm::vec3{dir.x, dir.y, rnd(0.f, 63.f)};
    };
    make(260, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{1.f, rnd(0.25f, 0.55f), 0.05f, 0.9f};
        p.die = cl.time + rnd(1.6f, 2.4f);
        p.scale = rnd(0.9f, 1.6f);
        p.type = Custom;
        p.fade = -0.4f;
        p.grow = -0.3f;
        p.acc = gravity(0.12f);
        p.vel = glm::normalize(dir) * rnd(60.f, 140.f);
    });
    make(200, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{fireColor(), 1.f};
        p.die = cl.time + rnd(1.f, 2.f);
        p.scale = rnd(0.35f, 0.6f);
        p.type = Custom;
        p.fade = -0.6f;
        p.drag = 0.5f;
        p.spin = rnd(-6.f, 6.f);
        p.acc = gravity(0.2f);
        p.vel = glm::normalize(dir) * rnd(90.f, 220.f) + inBox(30.f);
    });
    make(24, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellExplosion;
        p.additive = true;
        p.color = glm::vec4{fireColor(), 0.8f};
        p.die = cl.time + 1.2;
        p.scale = rnd(3.f, 5.f);
        p.type = Custom;
        p.fade = -0.9f;
        p.grow = 6.f;
        p.spin = rnd(-1.f, 1.f);
        p.vel = {0.f, 0.f, rnd(30.f, 70.f)};
    });
    make(10, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellSmoke;
        p.color = glm::vec4{0.2f, 0.18f, 0.16f, 0.7f};
        p.die = cl.time + 4.0;
        p.scale = rnd(4.f, 6.f);
        p.type = TxBigSmoke;
        p.acc = gravity(-0.05f);
        p.vel = {rnd(-10.f, 10.f), rnd(-10.f, 10.f), rnd(20.f, 40.f)};
    });
}

// A teleport: a flash, sparkles bursting out of the player's shape and motes rising.
void teleportSplash(const glm::vec3& org)
{
    const glm::vec3 pale{0.7f, 0.82f, 1.f};
    make(1, [&](Particle& p, int) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{pale, 0.8f};
        p.die = cl.time + 0.4;
        p.scale = 30.f;
        p.type = Custom;
        p.fade = -2.4f;
        p.grow = 30.f;
        p.org = org + glm::vec3{0.f, 0.f, 4.f};
    });
    make(180, [&](Particle& p, int) {
        const glm::vec3 at{rnd(-16.f, 16.f), rnd(-16.f, 16.f), rnd(-24.f, 32.f)};
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(pale, glm::vec3{1.f}, rnd(0.f, 1.f)), 1.f};
        p.die = cl.time + rnd(0.6f, 1.2f);
        p.scale = rnd(0.3f, 0.55f);
        p.type = Custom;
        p.fade = -1.1f;
        p.drag = 1.8f;
        p.spin = rnd(-8.f, 8.f);
        p.acc = gravity(0.05f);
        p.org = org + at;
        p.vel = glm::normalize(at + glm::vec3{0.f, 0.f, 1e-3f}) * rnd(50.f, 120.f);
    });
    make(60, [&](Particle& p, int) {
        const float a = rndAngle();
        const float r = rnd(4.f, 16.f);
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{pale, 0.5f};
        p.die = cl.time + rnd(0.8f, 1.3f);
        p.scale = rnd(1.f, 1.6f);
        p.type = Custom;
        p.fade = -0.5f;
        p.grow = -0.6f;
        p.org = org + glm::vec3{std::cos(a) * r, std::sin(a) * r, rnd(-24.f, 24.f)};
        p.vel = {0.f, 0.f, rnd(30.f, 80.f)};
    });
}

// ---- Splashes ---------------------------------------------------------------------------------

// The client's map's contents at `p`: a liquid's (currents are water), or 0 for anything else.
[[nodiscard]] int liquidAt(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return 0;
    }
    vec3_t v{p.x, p.y, p.z};
    const int c = Mod_PointInLeaf(v, cl.worldmodel)->contents;
    if(c == CONTENTS_WATER || c == CONTENTS_SLIME || c == CONTENTS_LAVA)
    {
        return c;
    }
    return c <= CONTENTS_CURRENT_0 && c >= CONTENTS_CURRENT_DOWN ? CONTENTS_WATER : 0;
}

// The liquid a splash on its surface is in (just under it): CONTENTS_WATER, _SLIME or _LAVA; water
// if none is found.
[[nodiscard]] int liquidUnder(const glm::vec3& org)
{
    for(const float dz : {-2.f, -6.f, -12.f})
    {
        if(const int c = liquidAt(org + glm::vec3{0.f, 0.f, dz}))
        {
            return c;
        }
    }
    return CONTENTS_WATER;
}

// The surface above `p` in a liquid, at most `range` units up (false: none, or a ceiling first).
[[nodiscard]] bool surfaceAbove(const glm::vec3& p, float range, glm::vec3& out)
{
    if(!liquidAt(p))
    {
        return false;
    }
    glm::vec3 wet = p;
    for(float d = 4.f; d <= range; d += 4.f)
    {
        const glm::vec3 q = p + glm::vec3{0.f, 0.f, d};
        if(liquidAt(q))
        {
            wet = q;
            continue;
        }
        vec3_t v{q.x, q.y, q.z};
        if(Mod_PointInLeaf(v, cl.worldmodel)->contents != CONTENTS_EMPTY)
        {
            return false;
        }
        glm::vec3 dry = q;
        for(int i = 0; i < 8; i++)
        {
            const glm::vec3 mid = (dry + wet) * 0.5f;
            (liquidAt(mid) ? wet : dry) = mid;
        }
        out = (dry + wet) * 0.5f;
        return true;
    }
    return false;
}

// How lit the place is (the lightmap under it): the drops take no light of their own, and white
// ones would glow in a dark pool.
[[nodiscard]] float shadeAt(const glm::vec3& org)
{
    static lightcache_t cache{};
    if(!cl.worldmodel)
    {
        return 1.f;
    }
    vec3_t v{org.x, org.y, org.z + 1.f};
    const float light = static_cast<float>(R_LightPoint(v, 0.f, &cache)); // 128: Quake's full light
    return std::clamp(std::max(light, 20.f) / 120.f, 0.18f, 1.3f);
}

// Something hitting a liquid's surface at `org` going `dir`, `count` hard (see Preset::Splash).
void splash(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float amount = std::clamp(vr_water_splash.value, 0.f, 3.f);
    if(amount <= 0.f)
    {
        return;
    }
    const int liquid = liquidUnder(org);
    const bool lava = liquid == CONTENTS_LAVA;
    const bool slime = liquid == CONTENTS_SLIME;
    const float s = std::clamp(count / 10.f, 0.1f, 8.f); // 1: a hand slapping the water hard
    const float size = std::min(std::sqrt(s), 1.7f);       // heights and spreads grow slower
    const float shade = lava ? 1.f : shadeAt(org);
    const glm::vec3 tint = lava ? glm::vec3{1.f, 0.42f, 0.1f} : slime ? glm::vec3{0.42f, 0.78f, 0.2f} : glm::vec3{0.8f, 0.9f, 1.f};
    const glm::vec3 drops = tint * shade;
    // Lava's drops, alpha blended (added to the bright lava, they vanish): hot yellow to orange.
    const auto hotLava = [] { return glm::mix(glm::vec3{1.f, 0.38f, 0.06f}, glm::vec3{1.f, 0.88f, 0.5f}, rnd(0.f, 1.f)); };
    const glm::vec3 foam = glm::mix(tint, glm::vec3{1.f}, lava ? 0.1f : 0.35f) * shade;
    const float surface = org.z;

    // The crown leans with a thing going in at an angle (a shot), away from where it came from.
    glm::vec3 lean{dir.x, dir.y, 0.f};
    const float leanLength = glm::length(lean);
    lean = leanLength > 1e-3f ? lean / leanLength * std::min(leanLength, 1.f) : glm::vec3{0.f};

    // The crown: drops thrown up and out, falling back in (gone at the surface).
    make((6.f + 14.f * s) * amount, [&](Particle& p, int) {
        const float a = rndAngle();
        const glm::vec3 out{std::cos(a), std::sin(a), 0.f};
        p.cell = CellCircle;
        p.color = glm::vec4{lava ? hotLava() : drops * rnd(0.8f, 1.15f), lava ? 1.f : rnd(0.55f, 0.85f)};
        p.die = cl.time + 2.5;
        p.scale = rnd(0.45f, 1.1f) * (0.8f + 0.25f * size); // 1-3 cm
        p.type = Custom;
        p.fade = lava ? -0.5f : -0.15f;
        p.acc = gravity(1.f);
        p.floor = surface - 0.5f;
        p.org = org + out * rnd(0.3f, 2.f) * size + glm::vec3{0.f, 0.f, 0.5f};
        p.vel = out * rnd(12.f, 45.f) * size + lean * rnd(10.f, 40.f) * size +
                glm::vec3{0.f, 0.f, rnd(60.f, 160.f) * std::max(size, 0.75f)};
    });

    // The jet: a thin column of drops thrown straight up (a bullet's is most of its splash).
    make((2.f + 2.5f * s) * amount, [&](Particle& p, int) {
        p.cell = CellCircle;
        p.color = glm::vec4{lava ? hotLava() : drops, lava ? 1.f : 0.8f};
        p.die = cl.time + 3.0;
        p.scale = rnd(0.55f, 1.2f) * (0.8f + 0.4f * size);
        p.type = Custom;
        p.fade = lava ? -0.4f : -0.1f;
        p.acc = gravity(1.f);
        p.floor = surface - 0.5f;
        p.org = org + inBox(0.5f + 0.5f * size) + glm::vec3{0.f, 0.f, 1.f};
        p.vel = inBox(5.f + 5.f * size) + lean * rnd(0.f, 25.f) + glm::vec3{0.f, 0.f, rnd(120.f, 230.f) * std::clamp(size, 0.7f, 1.25f)};
    });

    // Foam and spray: soft puffs at the surface, spreading and fading.
    make((1.f + 1.2f * s) * amount, [&](Particle& p, int) {
        const float a = rndAngle();
        p.cell = lava ? CellExplosion : CellSmoke;
        p.additive = lava;
        p.color = glm::vec4{foam * rnd(0.9f, 1.1f), lava ? 0.45f : rnd(0.28f, 0.42f)};
        p.die = cl.time + 1.6;
        p.scale = rnd(1.f, 1.6f) * (0.6f + 0.6f * size);
        p.type = Custom;
        p.fade = lava ? -0.5f : -0.3f;
        p.grow = 1.6f * size;
        p.drag = 2.5f;
        p.spin = rnd(-1.f, 1.f);
        p.org = org + glm::vec3{std::cos(a), std::sin(a), 0.f} * rnd(0.f, 3.f) * size + glm::vec3{0.f, 0.f, rnd(0.5f, 3.f) * size};
        p.vel = glm::vec3{std::cos(a), std::sin(a), 0.f} * rnd(10.f, 30.f) * size + glm::vec3{0.f, 0.f, rnd(4.f, 20.f)};
    });

    // Ripples: rings spreading on the surface (one more for a harder hit, slower and fainter).
    const int rings = s < 0.25f ? 1 : s < 2.f ? 2 : 3;
    for(int i = 0; i < rings; i++)
    {
        make(1.f, [&](Particle& p, int) {
            p.cell = CellRing;
            p.flat = true;
            p.color = glm::vec4{lava ? glm::vec3{0.22f, 0.06f, 0.02f} : foam, (lava ? 0.55f : 0.45f) / (1.f + 0.5f * static_cast<float>(i))};
            p.die = cl.time + 1.6 + 0.4 * i;
            p.scale = (1.2f + 1.2f * static_cast<float>(i)) * (0.7f + 0.5f * size);
            p.type = Custom;
            p.fade = -p.color.a / (1.4f + 0.4f * static_cast<float>(i));
            p.grow = (7.f - 1.8f * static_cast<float>(i)) * (0.8f + 0.5f * size);
            p.org = org + glm::vec3{0.f, 0.f, 0.35f};
        });
    }

    // Lava: embers thrown up and a little dark smoke.
    if(lava)
    {
        make((4.f + 8.f * s) * amount, [&](Particle& p, int) {
            p.cell = CellSpark;
            p.additive = true;
            p.color = glm::vec4{fireColor(), 1.f};
            p.die = cl.time + rnd(0.6f, 1.2f);
            p.scale = rnd(0.25f, 0.45f);
            p.type = Custom;
            p.fade = -1.f;
            p.drag = 0.8f;
            p.spin = rnd(-6.f, 6.f);
            p.acc = gravity(0.35f);
            p.org = org + inBox(2.f) + glm::vec3{0.f, 0.f, 1.f};
            p.vel = onSphere() * rnd(20.f, 60.f) * size + glm::vec3{0.f, 0.f, rnd(40.f, 120.f) * size};
        });
        make(1.f + s * 0.5f, [&](Particle& p, int) {
            p.cell = CellSmoke;
            p.color = glm::vec4{0.2f, 0.17f, 0.15f, 0.5f};
            p.die = cl.time + 2.5;
            p.scale = rnd(1.2f, 1.8f) * size;
            p.type = TxSmoke;
            p.acc = gravity(-0.05f);
            p.org = org + inBox(2.f) + glm::vec3{0.f, 0.f, 2.f};
            p.vel = inBox(8.f) + glm::vec3{0.f, 0.f, 20.f};
        });
    }
}

// An explosion under a liquid's surface (a rocket into a pool): the liquid thrown up above it, less
// the deeper it went off.
void underwaterExplosion(const glm::vec3& org)
{
    glm::vec3 surface;
    if(surfaceAbove(org, 96.f, surface))
    {
        splash(surface, glm::vec3{0.f, 0.f, -1.f}, static_cast<int>(45.f - 0.3f * (surface.z - org.z)));
    }
}

} // namespace

bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count)
{
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
            underwaterExplosion(org);
            break;
        case Preset::Lightning: lightning(org, count); break;
        case Preset::Smoke: smoke(org, count, false); break;
        case Preset::Sparks: sparks(org, count, 102, 112, 0.45f, 0.0); break;
        case Preset::GunSmoke: gunSmoke(org, count); break;
        case Preset::Teleport: sparks(org, count, 208, 220, 0.65f, 0.6); break;
        // Subtle and slow: faint, small, drifting up.
        case Preset::GunPickup: sparkles(org, count, 12, 16, 70, 120, 1.6, 0.22f, -0.03f, 6.f, 2.f, 1.f, 6.f); break;
        case Preset::GunForceGrab: sparkles(org, count, 106, 111, 90, 140, 1.2, 0.25f, -0.04f, 5.f, 3.f, 1.f, 8.f); break;
        case Preset::LavaSpike: sparkles(org, count, 247, 254, 180, 225, 0.5, 0.17f, 0.17f, 0.3f, 2.f, 0.f, 0.f); break;
        case Preset::BigSmoke: smoke(org, count, true); break;
        case Preset::ForceGrabTrail: sparkles(org, count, 208, 214, 170, 230, 0.4, 0.28f, 0.f, 1.5f, 4.f, -2.f, 2.f); break;
        case Preset::BloodTrail: bloodTrail(org, dir, count); break;
        case Preset::Splash: splash(org, dir, count); break;
        default: blood(org, dir, count); break;
    }
    return true;
}

bool enabled()
{
    return (cl.protocolflags & PRFL_QUAKEVR) && vr_particles.value && ensureAtlas();
}

void clear()
{
    pool.clear();
    lastRun = -1.0;
}

void shellEject(const glm::vec3& org, const glm::vec3& dir, float smoke, int sparks)
{
    if(!vr_particles.value || !ensureAtlas())
    {
        return;
    }

    // A faint grey puff out of the port, drifting after the shell and spreading.
    make(std::ceil(2.f * smoke), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = rnd(0.5f, 0.62f);
        p.color = glm::vec4{g, g, g * 0.97f, rnd(0.14f, 0.22f) * std::min(smoke, 1.5f)};
        p.die = cl.time + 1.8;
        p.scale = rnd(0.35f, 0.55f);
        p.type = Custom;
        p.fade = -p.color.a / 1.6f;
        p.grow = rnd(1.4f, 2.2f);
        p.drag = 2.5f;
        p.spin = rnd(-1.f, 1.f);
        p.acc = gravity(-0.03f);
        p.org = org + inBox(0.4f);
        p.vel = dir * rnd(4.f, 14.f) + inBox(3.f);
    });
    // A few tiny sparks of burning powder, gone in a blink.
    make(static_cast<float>(sparks), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, rnd(0.55f, 0.8f), rnd(0.2f, 0.35f), 1.f};
        p.die = cl.time + rnd(0.12f, 0.35f);
        p.scale = rnd(0.1f, 0.18f);
        p.type = Custom;
        p.fade = -3.f;
        p.drag = 3.f;
        p.spin = rnd(-8.f, 8.f);
        p.acc = gravity(0.3f);
        p.org = org;
        p.vel = dir * rnd(30.f, 80.f) + onSphere() * rnd(5.f, 25.f);
    });
}

void lavaNailTrail(const glm::vec3& from, const glm::vec3& to)
{
    if(!enabled())
    {
        return;
    }

    // A hot core: short-lived glows along the path, bright enough for the bloom, so that the
    // nail draws a brief streak of molten light.
    const glm::vec3 d = to - from;
    const float length = glm::length(d);
    make(perLength(length, 5.f), [&](Particle& p, int) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{1.f, rnd(0.38f, 0.5f), 0.12f, 0.55f};
        p.die = cl.time + 0.16;
        p.scale = rnd(1.4f, 1.9f);
        p.type = Custom;
        p.fade = -3.4f;
        p.grow = -6.f;
        p.org = from + d * rnd(0.f, 1.f);
    });
    // Embers shed on the way, falling and dimming.
    make(perLength(length, 22.f), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, rnd(0.35f, 0.6f), rnd(0.08f, 0.18f), 1.f};
        p.die = cl.time + rnd(0.35f, 0.7f);
        p.scale = rnd(0.18f, 0.32f);
        p.type = Custom;
        p.fade = -1.6f;
        p.grow = -0.2f;
        p.drag = 1.6f;
        p.spin = rnd(-8.f, 8.f);
        p.acc = gravity(0.25f);
        p.org = from + d * rnd(0.f, 1.f) + inBox(0.8f);
        p.vel = onSphere() * rnd(8.f, 28.f);
    });
}

void shellTrail(const glm::vec3& from, const glm::vec3& to, float strength)
{
    if(strength <= 0.f || !vr_particles.value || !ensureAtlas())
    {
        return;
    }

    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), 1.6f), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = rnd(0.52f, 0.62f);
        p.color = glm::vec4{g, g, g * 0.97f, rnd(0.05f, 0.09f) * strength};
        p.die = cl.time + 1.4;
        p.scale = rnd(0.22f, 0.36f);
        p.type = Custom;
        p.fade = -p.color.a / 1.2f;
        p.grow = 1.1f;
        p.drag = 1.5f;
        p.spin = rnd(-0.8f, 0.8f);
        p.acc = gravity(-0.03f);
        p.org = from + d * rnd(0.f, 1.f) + inBox(0.2f);
        p.vel = inBox(1.5f);
    });
}

// Live particles (vr_memstats).
int liveCount()
{
    return static_cast<int>(pool.size());
}

} // namespace qvr::particles

// R_RenderScene, after the translucent pass: the particles, depth-tested against the scene.
extern "C" void VR_DrawSceneTranslucent()
{
    QVR_GPU_PROFILE("vr particles");
    using namespace qvr;
    using namespace qvr::particles;

    text3d::drawTranslucent(); // the floating texts, the wrist log (vr_text3d.cpp)
    flashlight::drawTranslucent(); // the flashlight's visible beam (vr_flashlight.cpp)

    if(!(cl.protocolflags & PRFL_QUAKEVR) || pool.empty() || !atlas)
    {
        return;
    }

    run();

    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);

    // Resized rather than cleared (no constructing what is written over): every vertex is set below.
    static std::vector<gfx::Vertex> vertices;
    vertices.resize(pool.size() * 6);
    gfx::Vertex* out = vertices.data();
    for(const Particle& p : pool)
    {
        // The quad's right and up, turned by the particle's angle about the view direction.
        const float c = std::cos(p.angle);
        const float s = std::sin(p.angle);
        // Flat ones (ripples) lie on the horizontal plane.
        const glm::vec3 pr = p.flat ? glm::vec3{1.f, 0.f, 0.f} : right;
        const glm::vec3 pu = p.flat ? glm::vec3{0.f, 1.f, 0.f} : up;
        const glm::vec3 r = (pr * c + pu * s) * (0.75f * p.scale);
        const glm::vec3 u = (pu * c - pr * s) * (0.75f * p.scale);
        const glm::vec4& uv = cellUv[p.cell];
        // Premultiplied: a glow's alpha 0 adds it.
        const float a = std::min(p.color.a, 1.f);
        const glm::vec4 color{glm::vec3{p.color} * a, p.additive ? 0.f : a};

        const gfx::Vertex downLeft{p.org - u - r, {uv.x, uv.y}, color};
        const gfx::Vertex upRight{p.org + u + r, {uv.z, uv.w}, color};
        out[0] = downLeft;
        out[1] = {p.org + u - r, {uv.z, uv.y}, color}; // up left
        out[2] = upRight;
        out[3] = downLeft;
        out[4] = upRight;
        out[5] = {p.org - u + r, {uv.x, uv.w}, color}; // down right
        out += 6;
    }

    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Premultiplied, .depthTest = true, .depthWrite = false}, atlas);
}

// Quake's own effects, when Quake VR's particles are on (as the old engine drew them).
extern "C" int VR_RunParticleEffect(const float* org, const float* dir, int color, int count)
{
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
        return spawn(o, d, Preset::Blood, std::max(1, count / 8));
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
        size = std::clamp(glm::length(extent) * 0.5f / 9.f, 0.7f, 2.5f);
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
