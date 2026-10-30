// vr_particles_blood.cpp -- part of vr_particles.cpp (see vr_particles.hpp, vr_particles_internal.hpp): the blood,
// smoke, steam and spark emitters, the gore's drips, specks and droplets, and the chainsaw, contact, battery and torch
// smoke.

#include "vr_particles_internal.hpp"

namespace qvr::particles
{
using namespace detail;


namespace detail
{

// Blood let in a liquid (water::bloodInLiquid, vr_water_blood: one point test a burst, not a particle): its spray
// slowed to a stop in a few tenths of a second, sinking slowly, growing bigger and fainter and lasting a few seconds:
// a slowly blooming cloud. (Blood crossing a surface on its way, a ragdoll half in: not yet; BACKLOG.md.)
void soak(Particle& p, float lifeMult)
{
    const float life = vrand::range(rng, 2.f, 4.f) * lifeMult;
    p.type = Custom;
    p.vel *= 0.25f;
    p.drag = 5.f;
    p.acc = gravity(0.03f); // (5 units a second down, its push spent)
    p.die = cl.time + life;
    p.color.a *= 0.7f;
    p.fade = -p.color.a / life;
    p.scale *= 1.6f;
    p.grow = p.scale * 2.f / life; // (three times as big as it fades)
    p.spin = vrand::range(rng, -0.3f, 0.3f);
    p.streak = 0.f;
    p.plink = false;
}

void blood(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const bool wet = water::bloodInLiquid(org);
    mist(org, dir, count, wet);
    constexpr int colors[] = {247, 248, 249, 250, 251};
    make(count * 2.f, [&](Particle& p, int) {
        p.cell = CellBlood;
        setColor(p, colors[rndi(0, 5)], 100);
        p.die = cl.time + 0.7 * rndi(0, 3);
        p.scale = vrand::range(rng, 0.35f, 0.6f) * 6.5f;
        p.type = Static;
        p.acc = gravity(0.29f);
        p.org = org + glm::vec3{vrand::range(rng, -2, 2), vrand::range(rng, -2, 2), vrand::range(rng, -2, 2)};
        p.vel = (dir + 0.3f) * glm::vec3{vrand::range(rng, -10, 10), vrand::range(rng, -10, 10), vrand::range(rng, -10, 10)};
        p.vel.z += vrand::range(rng, 0, 40);
        if(wet)
        {
            soak(p);
        }
    });
    make(count * (wet ? 6.f : 24.f), [&](Particle& p, int) { // (in a liquid: fewer, each a little cloud)
        p.cell = CellCircle;
        setColor(p, colors[rndi(0, 5)], 175);
        p.die = cl.time + 0.4 * rndi(0, 3);
        p.scale = vrand::range(rng, 0.12f, 0.2f);
        p.type = Static;
        p.acc = gravity(0.45f);
        p.org = org + glm::vec3{vrand::range(rng, -2, 2), vrand::range(rng, -2, 2), vrand::range(rng, -2, 2)};
        p.vel = (dir + 0.3f) * glm::vec3{vrand::range(rng, -3, 3), vrand::range(rng, -3, 3), vrand::range(rng, -3, 3)} * 13.f;
        p.vel.z += vrand::range(rng, 20, 60);
        if(wet)
        {
            soak(p, 0.6f);
            p.scale *= 3.f;
            p.grow *= 3.f;
        }
    });
    make(1, [&](Particle& p, int) {
        p.cell = CellBloodMist;
        setColor(p, 225, 38);
        p.die = cl.time + 2.0;
        p.scale = vrand::range(rng, 1.1f, 2.4f) * 15.f;
        p.acc = gravity(-0.03f);
        p.type = TxSmoke;
        p.org = org + glm::vec3{vrand::range(rng, -8, 8), vrand::range(rng, -8, 8), vrand::range(rng, -8, 8)};
        p.vel = glm::vec3{-4.f};
        if(wet)
        {
            soak(p, 1.5f);
            p.color.a = 0.25f; // (its cloud: fainter than the drops' would make it)
            p.fade = -p.color.a / static_cast<float>(p.die - cl.time);
        }
    });
}

// Behind a flying gib, a few steps apart: a faint smear left in the air and drops falling from it.
void bloodTrail(const glm::vec3& org, const glm::vec3& dir, int count)
{
    constexpr int colors[] = {247, 248, 249, 250, 251};
    const bool wet = water::bloodInLiquid(org); // (a gib sinking: a trail of little clouds, hanging)
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellBlood;
        setColor(p, colors[rndi(0, 5)], vrand::range(rng, 80, 120));
        p.die = cl.time + vrand::range(rng, 0.4f, 0.8f);
        p.scale = vrand::range(rng, 0.35f, 0.6f) * 7.f;
        p.type = Drip;
        p.acc = gravity(0.12f);
        p.org = org + glm::vec3{vrand::range(rng, -1.5f, 1.5f), vrand::range(rng, -1.5f, 1.5f), vrand::range(rng, -1.5f, 1.5f)};
        p.vel = dir * vrand::range(rng, 0.f, 12.f) + glm::vec3{vrand::range(rng, -4, 4), vrand::range(rng, -4, 4), vrand::range(rng, -4, 4)};
        if(wet)
        {
            soak(p);
        }
    });
    if(wet)
    {
        return; // (no falling drops)
    }
    make(count * 2.f, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, colors[rndi(0, 5)], vrand::range(rng, 190, 240));
        p.die = cl.time + vrand::range(rng, 0.5f, 1.f);
        p.scale = vrand::range(rng, 0.25f, 0.45f);
        p.type = Static;
        p.acc = gravity(0.8f);
        p.org = org + glm::vec3{vrand::range(rng, -4, 4), vrand::range(rng, -4, 4), vrand::range(rng, -4, 4)};
        p.vel = dir * vrand::range(rng, 0.f, 25.f) + glm::vec3{vrand::range(rng, -10, 10), vrand::range(rng, -10, 10), vrand::range(rng, -6, 14)};
    });
}

void lightning(const glm::vec3& org, int count)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellLightning;
        setColor(p, 254, vrand::range(rng, 180, 220));
        p.die = cl.time + 1.2 * rndi(0, 3);
        p.scale = vrand::range(rng, 0.35f, 0.6f) * 6.2f;
        p.type = Lightning;
        p.org = org + glm::vec3{vrand::range(rng, -3, 3), vrand::range(rng, -3, 3), vrand::range(rng, -3, 3)};
        p.vel = {vrand::range(rng, -185, 185), vrand::range(rng, -185, 185), vrand::range(rng, -185, 185)};
    });
}

void smoke(const glm::vec3& org, int count, bool big)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(0, 8), big ? 255 : 125);
        p.die = cl.time + 2.5 * rndi(0, 5);
        p.scale = big ? vrand::range(rng, 1.2f, 1.7f) * 2.8f : vrand::range(rng, 1.2f, 1.5f) * 0.8f;
        p.type = big ? TxBigSmoke : TxSmoke;
        p.acc = gravity(-0.09f);
        const int spread = big ? 5 : 4;
        p.org = org + glm::vec3{rndi(-spread, spread), rndi(-spread, spread), rndi(-spread, spread)};
        p.vel = {vrand::range(rng, -24, 24), vrand::range(rng, -24, 24), vrand::range(rng, -24, 24)};
    });
}

// Fire put out by water (Preset::Steam): thin white puffs rising quickly off the surface and gone in a second or so.
void steam(const glm::vec3& org, int count)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSmoke;
        setColor(p, rndi(11, 15), vrand::range(rng, 70, 110));
        p.die = cl.time + vrand::range(rng, 0.7f, 1.3f);
        p.scale = vrand::range(rng, 0.9f, 1.4f) * 0.9f;
        p.type = TxSmoke;
        p.acc = gravity(-0.3f);
        p.org = org + glm::vec3{vrand::range(rng, -3, 3), vrand::range(rng, -3, 3), vrand::range(rng, 0, 2)};
        p.vel = {vrand::range(rng, -14, 14), vrand::range(rng, -14, 14), vrand::range(rng, 20, 45)};
    });
}

// Sparks thrown up and falling (sparks, teleport): colours [lo, hi), scale factor, life.
void sparks(const glm::vec3& org, int count, int lo, int hi, float scale, double life)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, rndi(lo, hi), 255);
        p.die = cl.time + (life > 0.0 ? life : 2.0 * rndi(0, 5));
        p.scale = vrand::range(rng, 1.55f, 2.87f) * scale;
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(1.f);
        p.org = org + glm::vec3{rndi(-4, 4), rndi(-4, 4), rndi(-4, 4)};
        p.vel = {vrand::range(rng, -48, 48), vrand::range(rng, -48, 48), vrand::range(rng, 60, 360)};
    });
}

void gunSmoke(const glm::vec3& org, int count)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellGunSmoke;
        p.angle = vrand::range(rng, -10.f, 10.f) * 0.0174533f + 1.5707963f;
        setColor(p, rndi(10, 16), vrand::range(rng, 85, 125));
        p.die = cl.time + 3.5;
        p.scale = vrand::range(rng, 0.9f, 1.5f) * 0.1f;
        p.type = GunSmoke;
        p.acc = gravity(-0.09f);
        p.org = org + glm::vec3{0.f, 0.f, 3.f};
        p.vel = {vrand::range(rng, -3, 3), vrand::range(rng, -3, 3), vrand::range(rng, -3, 3)};
    });
}

// Rising, fading sparkles (pickups, force grab, lava spikes).
void sparkles(const glm::vec3& org, int count, int lo, int hi, float alphaLo, float alphaHi, double life, float scale,
    float grav, float spread, float speed, float upLo, float upHi)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, rndi(lo, hi), vrand::range(rng, alphaLo, alphaHi));
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 1.55f, 2.87f) * scale;
        p.type = GunPickup;
        p.acc = gravity(grav);
        p.org = org + glm::vec3{vrand::range(rng, -spread, spread), vrand::range(rng, -spread, spread), vrand::range(rng, -spread, spread)};
        p.vel = {vrand::range(rng, -speed, speed), vrand::range(rng, -speed, speed), upHi > upLo ? vrand::range(rng, upLo, upHi) : vrand::range(rng, -speed, speed)};
    });
}
} // namespace detail

void bloodMist(const glm::vec3& org, const glm::vec3& dir, int count)
{
    if(enabled())
    {
        lastMistTime = -1.0; // (asked for: never merged with the last)
        mist(org, dir, count);
    }
}

void bloodDrip(const glm::vec3& org, float fall, float floorZ, float size, const glm::vec3& color)
{
    if(!vr_particles.value || !ensureAtlas())
    {
        return;
    }
    // The drop, and two fainter behind it: a short streak as it falls (they fall together).
    for(int i = 0; i < 3 && pool.size() < maxParticles; i++)
    {
        Particle p;
        p.cell = CellCircle;
        p.color = glm::vec4{color, i == 0 ? 0.95f : 0.55f - 0.2f * i};
        p.scale = size * (1.f - 0.2f * i);
        p.type = Static;
        p.acc = gravity(1.f);
        p.org = org + glm::vec3{0.f, 0.f, size * 1.6f * i};
        p.die = cl.time + fall + 0.05;
        p.floor = floorZ + 0.3f;
        pool.pushBack(p);
    }
}

void bloodSpecks(const glm::vec3& org, const glm::vec3& normal, int count, const glm::vec3& color)
{
    if(!vr_particles.value || !ensureAtlas())
    {
        return;
    }
    for(int i = 0; i < count && pool.size() < maxParticles; i++)
    {
        Particle p;
        p.cell = CellCircle;
        p.color = glm::vec4{color, 0.85f};
        p.scale = vrand::range(rng, 0.12f, 0.22f);
        p.type = Static;
        p.acc = gravity(1.f);
        p.org = org + normal * 0.3f;
        const float a = rndAngle();
        p.vel = glm::vec3{za::cos(a), za::sin(a), 0.f} * vrand::range(rng, 15.f, 35.f) + normal * vrand::range(rng, 25.f, 50.f);
        p.die = cl.time + vrand::range(rng, 0.15f, 0.3f);
        p.floor = org.z - 0.5f;
        pool.pushBack(p);
    }
}

void bloodDroplet(const glm::vec3& org, const glm::vec3& vel, float size, float life, const glm::vec3& color, float floorZ, bool wet)
{
    if(!vr_particles.value || pool.size() >= maxParticles || !ensureAtlas())
    {
        return;
    }
    Particle p;
    p.cell = CellCircle;
    p.color = glm::vec4{color, 0.9f};
    p.scale = size;
    p.type = Custom;
    p.fade = -0.9f / za::max(life, 0.02f); // (gone as it dies)
    p.grow = -0.4f * size / za::max(life, 0.02f);
    p.drag = 0.6f;
    p.streak = 0.012f; // (drawn out a little along its flight)
    p.lighting = Lighting::Emissive; // (its colour lit already: the stream's, which it leaves)
    p.acc = gravity(1.f);
    p.org = org;
    p.vel = vel;
    p.die = cl.time + life;
    p.floor = floorZ;
    if(wet)
    {
        soak(p);
        p.floor = -1e9f;
    }
    pool.pushBack(p);
}

void chainsawSmoke(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float alpha = za::clamp(vr_chainsaw_smoke_alpha.value, 0.f, 1.f);
    if(!vr_particles.value || alpha <= 0.f || count <= 0 || !ensureAtlas())
    {
        return;
    }
    // A two-stroke's thin blue-grey wisp: small as it leaves, spreading, slowing and rising as it fades.
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.5f, 0.6f);
        p.color = glm::vec4{g * 0.94f, g * 0.97f, g, alpha * vrand::range(rng, 0.75f, 1.15f)};
        const float life = vrand::range(rng, 1.6f, 2.4f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.3f, 0.45f);
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = vrand::range(rng, 1.2f, 2.f);
        p.drag = 2.f;
        p.spin = vrand::range(rng, -0.8f, 0.8f);
        p.acc = gravity(-0.035f);
        p.org = org + inBox(0.3f);
        p.vel = dir * vrand::range(rng, 6.f, 12.f) + inBox(2.5f);
    });
}

void contactSparks(const glm::vec3& org, int count)
{
    if(!vr_particles.value || count <= 0 || !ensureAtlas())
    {
        return;
    }
    // An electric arc's spatter: tiny blue-white points, glowing, streaked by their speed, gone in a fraction of a second.
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSpark;
        const float w = vrand::range(rng, 0.8f, 1.f);
        p.color = glm::vec4{w * vrand::range(rng, 0.7f, 0.9f), w * vrand::range(rng, 0.85f, 1.f), w, 1.f};
        const float life = vrand::range(rng, 0.12f, 0.4f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.2f, 0.4f);
        p.type = Custom;
        p.fade = -1.f / life;
        p.grow = -0.3f;
        p.drag = 1.5f;
        p.additive = true;
        p.streak = 0.025f;
        p.acc = gravity(0.6f);
        p.org = org + inBox(0.3f);
        p.vel = glm::vec3{vrand::range(rng, -70.f, 70.f), vrand::range(rng, -70.f, 70.f), vrand::range(rng, -20.f, 100.f)};
    });
}

void batterySmoke(const glm::vec3& org, int count)
{
    const float alpha = za::clamp(vr_reload_battery_smoke_alpha.value, 0.f, 1.f);
    if(!vr_particles.value || alpha <= 0.f || count <= 0 || !ensureAtlas())
    {
        return;
    }
    // Burnt insulation's thin acrid wisp: small as it leaves the contact, spreading and rising as it fades.
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.3f, 0.42f);
        p.color = glm::vec4{g, g * 0.98f, g * 0.94f, alpha * vrand::range(rng, 0.7f, 1.1f)};
        const float life = vrand::range(rng, 1.4f, 2.2f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.2f, 0.35f);
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = vrand::range(rng, 1.f, 1.7f);
        p.drag = 2.f;
        p.spin = vrand::range(rng, -0.8f, 0.8f);
        p.acc = gravity(-0.04f);
        p.org = org + inBox(0.25f);
        p.vel = inBox(1.5f) + glm::vec3{0.f, 0.f, vrand::range(rng, 4.f, 9.f)};
    });
}

void torchSmoke(const glm::vec3& org, const glm::vec3& drift, int count, float alpha)
{
    alpha = za::clamp(alpha, 0.f, 1.f);
    if(!vr_particles.value || alpha <= 0.f || count <= 0 || !ensureAtlas())
    {
        return;
    }
    // Pitch-black wisps off the flame's tips: small and dense as they leave, spreading, slowing and thinning as they rise.
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.16f, 0.26f);
        p.color = glm::vec4{g * 1.05f, g, g * 0.95f, alpha * vrand::range(rng, 0.7f, 1.2f)};
        const float life = vrand::range(rng, 2.4f, 3.6f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 1.6f, 2.6f);
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = vrand::range(rng, 4.f, 6.5f);
        p.drag = 1.4f;
        p.spin = vrand::range(rng, -0.7f, 0.7f);
        p.acc = gravity(-0.012f) + inBox(3.f);
        p.org = org + inBox(1.2f);
        p.vel = drift + glm::vec3{0.f, 0.f, vrand::range(rng, 10.f, 18.f)} + inBox(2.5f);
    });
}

void smoulderSmoke(const glm::vec3& org, const glm::vec3& out, int count, float alpha)
{
    alpha = za::clamp(alpha, 0.f, 1.f);
    if(!vr_particles.value || alpha <= 0.f || count <= 0 || !ensureAtlas())
    {
        return;
    }
    // Thin grey wisps off charred skin: small as they leave it, lifting off and rising, spreading and thinning (lighter
    // and fainter than a torch's pitch smoke).
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.48f, 0.62f);
        p.color = glm::vec4{g * 1.03f, g, g * 0.96f, alpha * vrand::range(rng, 0.6f, 1.1f)};
        const float life = vrand::range(rng, 1.6f, 2.6f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 1.2f, 2.f);
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = vrand::range(rng, 3.f, 4.5f);
        p.drag = 1.2f;
        p.spin = vrand::range(rng, -0.6f, 0.6f);
        p.acc = gravity(-0.01f) + inBox(2.5f);
        p.org = org + out * 1.5f + inBox(0.6f);
        p.vel = out * vrand::range(rng, 2.f, 5.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 7.f, 13.f)} + inBox(2.f);
    });
}

void torchDrip(const glm::vec3& org, const glm::vec3& vel)
{
    if(!enabled())
    {
        return;
    }
    // Burning pitch: a hot drop falling off the head, dimming as it falls.
    make(1.f, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, vrand::range(rng, 0.42f, 0.62f), vrand::range(rng, 0.1f, 0.2f), 1.f};
        p.die = cl.time + vrand::range(rng, 0.6f, 1.1f);
        p.scale = vrand::range(rng, 0.2f, 0.32f);
        p.type = Custom;
        p.fade = -0.9f;
        p.grow = -0.12f;
        p.drag = 0.3f;
        p.spin = vrand::range(rng, -6.f, 6.f);
        p.acc = gravity(1.f);
        p.org = org + inBox(0.6f);
        p.vel = vel + inBox(4.f);
    });
}
} // namespace qvr::particles
