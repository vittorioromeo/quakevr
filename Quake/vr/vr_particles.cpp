// vr_particles.cpp -- see vr_particles.hpp. The presets and the per-type behaviours are the old
// engine's (r_part.cpp: R_RunParticle2Effect and friends, CL_RunParticles), with the same
// numbers; the drawing is camera-facing quads of 1.5 x scale units, turned by their angle about
// the view direction, as the old geometry shader built them.

#include "vr_particles.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_text3d.hpp"
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
    Drip, // fades out quickly (a gib's blood trail)
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
    Type type{Static};
    Cell cell{CellCircle};
    bool spinBack{false};
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
    make(count * 0.7f, [&](Particle& p, int) {
        p.cell = CellRock;
        setColor(p, (color & ~7) + rndi(0, 8), 255);
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
        setColor(p, (color & ~7) + rndi(0, 8), 255);
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
            default: break;
        }
    }

    std::erase_if(pool, [](const Particle& p) { return p.die < cl.time || p.color.a <= 0.f || p.scale <= 0.f; });
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

    atlas = gfx::createTexture(width, height, pixels.data());
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
        case Preset::Explosion: explosion(org); break;
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

} // namespace qvr::particles

// R_RenderScene, after the translucent pass: the particles, depth-tested against the scene.
extern "C" void VR_DrawSceneTranslucent()
{
    QVR_GPU_PROFILE("vr particles");
    using namespace qvr;
    using namespace qvr::particles;

    text3d::drawTranslucent(); // the floating texts, the wrist log (vr_text3d.cpp)

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
        const glm::vec3 r = (right * c + up * s) * (0.75f * p.scale);
        const glm::vec3 u = (up * c - right * s) * (0.75f * p.scale);
        const glm::vec4& uv = cellUv[p.cell];
        const glm::vec4 color{p.color.r, p.color.g, p.color.b, std::min(p.color.a, 1.f)};

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
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Alpha, .depthTest = true, .depthWrite = false}, atlas);
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
    // Impacts: the bullet puff in the effect's colour.
    if(!vr_particles.value || !ensureAtlas())
    {
        return 0;
    }
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
