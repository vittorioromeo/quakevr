// vr_particles_trails.cpp -- part of vr_particles.cpp (see vr_particles.hpp, vr_particles_internal.hpp): trails,
// impacts and splashes (rockets, sparkles, blobs, lava, teleports), liquids (mist, splash), fire, AD's streams, tank
// fire, electricity, embers and shells.

#include "vr_particles_internal.hpp"

namespace qvr::particles
{
using namespace detail;


namespace detail
{

// A coloured explosion (TE_EXPLOSION2: the old R_ParticleExplosion2): coloured blobs, sparks,
// rocks, the fireball and smoke.
void explosion2(const glm::vec3& org, int colorStart, int colorLength)
{
    int colorMod = 0;
    make(256, [&](Particle& p, int) {
        p.cell = CellCircle;
        setColor(p, colorStart + (colorMod++ % za::max(colorLength, 1)), 255);
        p.die = cl.time + 1.5;
        p.scale = vrand::range(rng, 1.6f, 3.5f);
        p.acc = gravity(0.5f);
        p.type = Blob;
        p.org = org + glm::vec3{vrand::range(rng, -16, 16), vrand::range(rng, -16, 16), vrand::range(rng, -16, 16)};
        p.vel = {vrand::range(rng, -256, 256), vrand::range(rng, -256, 256), vrand::range(rng, -256, 256)};
    });
    explosion(org); // the rest as the plain explosion's (minus its ramp circles, close enough)
}

// ---- Quake's own effects (R_RocketTrail, R_BlobExplosion, R_LavaSplash, R_TeleportSplash) -----

// The colours of the scrag's, hell knight's and vore's projectiles, as their lights (vr_emissive.cpp).

[[nodiscard]] glm::vec3 fireColor()
{
    return glm::mix(glm::vec3{1.f, 0.42f, 0.1f}, glm::vec3{1.f, 0.8f, 0.35f}, vrand::range(rng, 0.f, 1.f));
}

// Flames licking off something burning as it flies (rockets, lava balls, hell knight flames), at
// `spacing` units apart along `from` -> `to`.
void flames(const glm::vec3& from, const glm::vec3& to, float spacing, float scale, const glm::vec3& tint)
{
    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), spacing), [&](Particle& p, int) {
        p.cell = CellExplosion;
        p.additive = true;
        p.color = glm::vec4{fireColor() * tint, vrand::range(rng, 0.45f, 0.7f)};
        p.die = cl.time + 0.45;
        p.scale = vrand::range(rng, 0.8f, 1.2f) * scale;
        p.type = Custom;
        p.fade = -vrand::range(rng, 1.8f, 2.6f);
        p.grow = 2.5f * scale;
        p.spin = vrand::range(rng, -3.f, 3.f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(1.f);
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
        p.die = cl.time + vrand::range(rng, 0.5f, 0.9f);
        p.scale = vrand::range(rng, 0.22f, 0.4f);
        p.type = Custom;
        p.fade = -1.5f;
        p.grow = -0.2f;
        p.drag = 1.2f;
        p.spin = vrand::range(rng, -6.f, 6.f);
        p.acc = gravity(grav);
        p.org = from + d * vrand::range(rng, 0.f, 1.f);
        p.vel = onSphere() * vrand::range(rng, 0.3f, 1.f) * speed;
    });
}

// Smoke left behind, `spacing` units apart: `grey` its colour, `alpha` how thick.
void smokeTrail(const glm::vec3& from, const glm::vec3& to, float spacing, float grey, float alpha, float scale)
{
    const glm::vec3 d = to - from;
    make(perLength(glm::length(d), spacing), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = grey * vrand::range(rng, 0.85f, 1.15f);
        p.color = glm::vec4{g, g, g * 0.97f, alpha * vrand::range(rng, 0.8f, 1.2f)};
        p.die = cl.time + vrand::range(rng, 1.8f, 2.6f);
        p.scale = vrand::range(rng, 0.8f, 1.2f) * scale;
        p.type = Custom;
        p.fade = -alpha / 2.2f;
        p.grow = 3.5f * scale;
        p.drag = 0.8f;
        p.spin = vrand::range(rng, -0.6f, 0.6f);
        p.acc = gravity(-0.02f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(1.5f);
        p.vel = inBox(6.f);
    });
}

// Wood dust (Preset::WoodDust): soft clouds of sawdust that spread, slow and settle, and splinters in the wood's browns
// thrown out (along `dir` too) and falling.
void woodDust(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const glm::vec3 push = glm::length(dir) > 0.01f ? glm::normalize(dir) * 50.f : glm::vec3{0.f}; // (sent as a way: QVR_SVC_PARTICLE2)
    make(count, [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.85f, 1.1f);
        p.color = glm::vec4{0.78f * g, 0.67f * g, 0.5f * g, vrand::range(rng, 0.45f, 0.7f)};
        p.die = cl.time + vrand::range(rng, 1.8f, 3.0f);
        p.scale = vrand::range(rng, 2.f, 3.5f) * 1.6f;
        p.type = Custom;
        p.fade = -0.24f;
        p.grow = 7.f;
        p.drag = 2.5f;
        p.spin = vrand::range(rng, -0.8f, 0.8f);
        p.acc = gravity(0.03f);
        p.org = org + inBox(8.f);
        p.vel = inBox(50.f) + push;
    });
    make(static_cast<float>(count) * 0.6f, [&](Particle& p, int) {
        p.cell = CellSpark;
        setColor(p, rndi(20, 30), 255);
        p.lighting = Lighting::Lit; // wood chips (the spark's shape, not hot)
        p.die = cl.time + vrand::range(rng, 1.f, 1.8f);
        p.scale = vrand::range(rng, 1.8f, 3.2f);
        p.type = Rock;
        p.spinBack = rndi(0, 2);
        p.acc = gravity(1.f);
        p.org = org + inBox(8.f);
        p.vel = glm::vec3{vrand::range(rng, -110, 110), vrand::range(rng, -110, 110), vrand::range(rng, 40, 200)} + push;
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
        p.scale = vrand::range(rng, 0.9f, 1.1f) * glowScale;
        p.type = Custom;
        p.fade = -0.5f;
        p.grow = -glowScale * 1.8f;
        p.org = from + d * vrand::range(rng, 0.f, 1.f);
    });
    make(perLength(length, 4.f), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(color, glm::vec3{1.f}, vrand::range(rng, 0.f, 0.4f)), 1.f};
        p.die = cl.time + vrand::range(rng, 0.5f, 0.8f);
        p.scale = vrand::range(rng, 0.25f, 0.45f);
        p.type = Custom;
        p.fade = -1.4f;
        p.grow = -0.25f;
        p.drag = 1.5f;
        p.spin = vrand::range(rng, -8.f, 8.f);
        p.acc = gravity(0.03f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(1.5f);
        p.vel = onSphere() * vrand::range(rng, 12.f, 36.f);
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
                bloodTrail(from + d * vrand::range(rng, 0.f, 1.f), dir, 1);
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
        p.color = glm::vec4{glm::mix(color, glm::vec3{1.f}, vrand::range(rng, 0.f, 0.35f)), 1.f};
        p.die = cl.time + vrand::range(rng, 0.4f, 0.8f);
        p.scale = vrand::range(rng, 0.3f, 0.55f);
        p.type = Custom;
        p.fade = -1.4f;
        p.drag = 2.f;
        p.spin = vrand::range(rng, -8.f, 8.f);
        p.acc = gravity(0.2f);
        p.org = org + inBox(2.f);
        p.vel = onSphere() * vrand::range(rng, 40.f, 120.f);
    });
}

// A tarbaby blowing up: violet and red blobs flung out, a violet flash, sparks and smoke.
void blobExplosion(const glm::vec3& org)
{
    make(160, [&](Particle& p, int i) {
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{i & 1 ? glm::vec3{0.65f, 0.25f, 1.f} : glm::vec3{1.f, 0.2f, 0.35f}, 0.9f};
        p.die = cl.time + vrand::range(rng, 1.f, 1.4f);
        p.scale = vrand::range(rng, 1.4f, 2.4f);
        p.type = Custom;
        p.fade = -0.8f;
        p.grow = -0.6f;
        p.drag = 1.6f;
        p.acc = gravity(0.3f);
        p.org = org + inBox(16.f);
        p.vel = inBox(256.f);
    });
    make(za::clamp(vr_explosion_particles.value, 0.f, 16.f), [&](Particle& p, int) {
        p.cell = CellExplosion; // the neutral fireball texture also takes the tarbaby's violet tint
        p.additive = true;
        p.color = glm::vec4{0.6f, 0.3f, 1.f, 1.f};
        p.die = cl.time + 1.5;
        p.scale = vrand::range(rng, 1.2f, 2.f) * 2.f;
        p.type = TxExplode;
        p.spinBack = rndi(0, 2);
        p.org = org + inBox(8.f);
        p.vel = inBox(8.f);
    }, false);
    make(48, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(glm::vec3{0.7f, 0.35f, 1.f}, glm::vec3{1.f}, vrand::range(rng, 0.f, 0.4f)), 1.f};
        p.die = cl.time + 1.2;
        p.scale = vrand::range(rng, 0.9f, 1.4f);
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
        p.scale = vrand::range(rng, 1.4f, 1.9f);
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
        dir = {vrand::range(rng, -128.f, 128.f), vrand::range(rng, -128.f, 128.f), 256.f};
        return org + glm::vec3{dir.x, dir.y, vrand::range(rng, 0.f, 63.f)};
    };
    make(260, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{1.f, vrand::range(rng, 0.25f, 0.55f), 0.05f, 0.9f};
        p.die = cl.time + vrand::range(rng, 1.6f, 2.4f);
        p.scale = vrand::range(rng, 0.9f, 1.6f);
        p.type = Custom;
        p.fade = -0.4f;
        p.grow = -0.3f;
        p.acc = gravity(0.12f);
        p.vel = glm::normalize(dir) * vrand::range(rng, 60.f, 140.f);
    });
    make(200, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{fireColor(), 1.f};
        p.die = cl.time + vrand::range(rng, 1.f, 2.f);
        p.scale = vrand::range(rng, 0.35f, 0.6f);
        p.type = Custom;
        p.fade = -0.6f;
        p.drag = 0.5f;
        p.spin = vrand::range(rng, -6.f, 6.f);
        p.acc = gravity(0.2f);
        p.vel = glm::normalize(dir) * vrand::range(rng, 90.f, 220.f) + inBox(30.f);
    });
    make(24, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellExplosion;
        p.additive = true;
        p.color = glm::vec4{fireColor(), 0.8f};
        p.die = cl.time + 1.2;
        p.scale = vrand::range(rng, 3.f, 5.f);
        p.type = Custom;
        p.fade = -0.9f;
        p.grow = 6.f;
        p.spin = vrand::range(rng, -1.f, 1.f);
        p.vel = {0.f, 0.f, vrand::range(rng, 30.f, 70.f)};
    });
    make(10, [&](Particle& p, int) {
        glm::vec3 dir;
        p.org = spot(dir);
        p.cell = CellSmoke;
        p.color = glm::vec4{0.2f, 0.18f, 0.16f, 0.7f};
        p.die = cl.time + 4.0;
        p.scale = vrand::range(rng, 4.f, 6.f);
        p.type = TxBigSmoke;
        p.acc = gravity(-0.05f);
        p.vel = {vrand::range(rng, -10.f, 10.f), vrand::range(rng, -10.f, 10.f), vrand::range(rng, 20.f, 40.f)};
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
        const glm::vec3 at{vrand::range(rng, -16.f, 16.f), vrand::range(rng, -16.f, 16.f), vrand::range(rng, -24.f, 32.f)};
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{glm::mix(pale, glm::vec3{1.f}, vrand::range(rng, 0.f, 1.f)), 1.f};
        p.die = cl.time + vrand::range(rng, 0.6f, 1.2f);
        p.scale = vrand::range(rng, 0.3f, 0.55f);
        p.type = Custom;
        p.fade = -1.1f;
        p.drag = 1.8f;
        p.spin = vrand::range(rng, -8.f, 8.f);
        p.acc = gravity(0.05f);
        p.org = org + at;
        p.vel = glm::normalize(at + glm::vec3{0.f, 0.f, 1e-3f}) * vrand::range(rng, 50.f, 120.f);
    });
    make(60, [&](Particle& p, int) {
        const float a = rndAngle();
        const float r = vrand::range(rng, 4.f, 16.f);
        p.cell = CellGlow;
        p.additive = true;
        p.color = glm::vec4{pale, 0.5f};
        p.die = cl.time + vrand::range(rng, 0.8f, 1.3f);
        p.scale = vrand::range(rng, 1.f, 1.6f);
        p.type = Custom;
        p.fade = -0.5f;
        p.grow = -0.6f;
        p.org = org + glm::vec3{za::cos(a) * r, za::sin(a) * r, vrand::range(rng, -24.f, 24.f)};
        p.vel = {0.f, 0.f, vrand::range(rng, 30.f, 80.f)};
    });
}

// ---- Splashes ---------------------------------------------------------------------------------

// The client's map's contents at `p`: a liquid's (currents are water), or 0 for anything else.
[[nodiscard]] int liquidAt(const glm::vec3& p)
{
    return worldtrace::liquidKind(worldtrace::liquidContents(p, true));
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
// ones would glow in a dark pool. 1 with lit particles (vr_particle_light): they take it as they are drawn.
lightcache_t shadeLight{}; // shadeAt's light lookup (the client's frame)

[[nodiscard]] float shadeAt(const glm::vec3& org)
{
    lightcache_t& cache = shadeLight;
    if(!cl.worldmodel || lightOn()) // (lit particles: lit where they are as they move, by the dynamic lights too)
    {
        return 1.f;
    }
    vec3_t v{org.x, org.y, org.z + 1.f};
    const float light = static_cast<float>(R_LightPoint(v, 0.f, &cache)); // 128: Quake's full light
    return za::clamp(za::max(light, 20.f) / 120.f, 0.18f, 1.3f);
}

// The gore's blood mist (vr_gore_mist*; ROUND21.md, "Gore: bloody hands, washing, dying bodies, blood mist"): with every
// blood effect (a hit, a cut, a gib bursting: whatever bleeds), a few large, faint clouds of blood hanging in the air,
// spreading and drifting along the blow as they fade. A bleed's several blood effects in one place in one frame (a gib
// bursting sends eight) make one bleed's clouds. None close to the eyes (vr_gore_mist_near): the player's own bleeding
// is not in his face.
glm::vec3 lastMistOrg{0.f}; // the last clouds' place and time
double lastMistTime = -1.0;

void mist(const glm::vec3& org, const glm::vec3& dir, int count, bool wet)
{
    const float amount = za::clamp(vr_gore_mist.value, 0.f, 10.f);
    const float alpha = za::clamp(vr_gore_mist_alpha.value, 0.f, 1.f);
    if(amount <= 0.f || alpha <= 0.f)
    {
        return;
    }
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    if(glm::distance(org, eye) < vr_gore_mist_near.value)
    {
        return;
    }
    if(cl.time == lastMistTime && glm::distance(org, lastMistOrg) < 24.f)
    {
        return;
    }
    lastMistTime = cl.time;
    lastMistOrg = org;

    const float size = za::max(0.05f, vr_gore_mist_size.value);
    const float life = za::max(0.2f, vr_gore_mist_life.value);
    const float speed = za::max(0.f, vr_gore_mist_speed.value);
    const float dark = za::clamp(vr_gore_mist_dark.value, 0.f, 1.f);
    const glm::vec3 colour = glm::vec3{0.62f, 0.05f, 0.035f} * (1.f - 0.8f * dark) * shadeAt(org);
    const float len = glm::length(dir);
    const glm::vec3 way = len > 1e-3f ? dir / len : glm::vec3{0.f};
    // (In a liquid: slower, longer, sinking slowly, spreading less fast: soak's cloud.)
    const float drag = wet ? 3.f : 1.2f; // the part of their speed lost a second: they hang in the air
    make(amount * za::clamp(2.f + static_cast<float>(count) / 10.f, 2.f, 6.f) + vrand::range(rng, 0.f, 1.f), [&](Particle& p, int) {
        p.cell = CellBloodMist;
        p.color = glm::vec4{colour, alpha * vrand::range(rng, 0.7f, 1.f)};
        p.type = Custom;
        const float lives = life * vrand::range(rng, 0.8f, 1.2f) * (wet ? 2.f : 1.f);
        p.fade = -p.color.a / lives;
        p.die = cl.time + lives;
        p.scale = vrand::range(rng, 1.6f, 2.6f) * 15.f * size * (wet ? 0.6f : 1.f);
        p.grow = vrand::range(rng, 22.f, 34.f) * size * za::max(0.f, vr_gore_mist_grow.value) * (wet ? 0.35f : 1.f);
        p.drag = drag;
        p.spin = vrand::range(rng, -0.4f, 0.4f);
        p.acc = glm::vec3{0.f, 0.f, (wet ? -4.f : vr_gore_mist_rise.value) * drag}; // (rising at that speed, once its push is spent)
        p.org = org + inBox(8.f * size);
        p.vel = (way * vrand::range(rng, 12.f, 36.f) + onSphere() * vrand::range(rng, 4.f, 14.f)) * speed * (wet ? 0.3f : 1.f);
    });
}

// Something hitting a liquid's surface at `org` going `dir`, `count` hard (see Preset::Splash). Drops thrown up in a
// crown and a jet, streaked along their motion, a fine spray round them, clouds of spray and a mist, foam spreading
// on the surface and rings riding the ripples' crest (the geometric ripples are vr_water.cpp's). vr_water_splash: how
// many (0 off), vr_water_splash_size: how big, vr_water_splash_ring_speed and _ring_size: the rings.
void splash(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float amount = za::clamp(vr_water_splash.value, 0.f, 3.f);
    if(amount <= 0.f)
    {
        return;
    }
    const int liquid = liquidUnder(org);
    const bool lava = liquid == CONTENTS_LAVA;
    const bool slime = liquid == CONTENTS_SLIME;
    const float s = za::clamp(count / 10.f, 0.1f, 8.f); // 1: a hand slapping the water hard
    const float size = za::min(za::sqrt(s), 2.2f);       // heights and spreads grow slower
    const float big = za::clamp(vr_water_splash_size.value, 0.25f, 4.f);
    const float shade = lava ? 1.f : shadeAt(org);
    const glm::vec3 tint = lava ? glm::vec3{1.f, 0.42f, 0.1f} : slime ? glm::vec3{0.42f, 0.78f, 0.2f} : glm::vec3{0.8f, 0.9f, 1.f};
    const glm::vec3 drops = tint * shade;
    // Lava's drops, alpha blended (added to the bright lava, they vanish): hot yellow to orange.
    const auto hotLava = [] { return glm::mix(glm::vec3{1.f, 0.38f, 0.06f}, glm::vec3{1.f, 0.88f, 0.5f}, vrand::range(rng, 0.f, 1.f)); };
    const glm::vec3 foam = glm::mix(tint, glm::vec3{1.f}, lava ? 0.1f : 0.45f) * shade;
    const float surface = org.z;

    // The crown leans with a thing going in at an angle (a shot), away from where it came from.
    glm::vec3 lean{dir.x, dir.y, 0.f};
    const float leanLength = glm::length(lean);
    lean = leanLength > 1e-3f ? lean / leanLength * za::min(leanLength, 1.f) : glm::vec3{0.f};

    // A drop: streaked along its motion, falling back in (gone at the surface).
    const auto drop = [&](Particle& p, float alpha) {
        p.cell = CellDrop;
        p.color = glm::vec4{lava ? hotLava() : drops * vrand::range(rng, 0.85f, 1.2f), lava ? 1.f : alpha};
        p.type = Custom;
        p.acc = gravity(1.f);
        p.floor = surface - 0.5f;
        p.liquid = liquid;
        p.streak = lava ? 0.01f : 0.016f;
    };
    // One lying on the surface, on its waves and ripples.
    const auto lying = [&](Particle& p) {
        p.flat = true;
        p.floor = surface;
        p.liquid = liquid;
    };

    // The crown: drops thrown up and out (a third leave a little ring where they fall back).
    make((10.f + 22.f * s) * amount, [&](Particle& p, int i) {
        const float a = rndAngle();
        const glm::vec3 out{za::cos(a), za::sin(a), 0.f};
        drop(p, vrand::range(rng, 0.6f, 0.9f));
        p.die = cl.time + 2.5;
        p.scale = vrand::range(rng, 0.7f, 1.4f) * (0.85f + 0.2f * size) * big; // 2-5 cm across
        p.fade = lava ? -0.5f : -0.12f;
        p.plink = !lava && i % 4 == 0;
        p.org = org + out * vrand::range(rng, 0.5f, 3.f) * size + glm::vec3{0.f, 0.f, 0.5f};
        p.vel = out * vrand::range(rng, 15.f, 55.f) * size + lean * vrand::range(rng, 10.f, 45.f) * size +
                glm::vec3{0.f, 0.f, vrand::range(rng, 70.f, 180.f) * za::max(size, 0.75f)};
    });

    // Fine spray: many small drops flung wider and lower, gone sooner.
    make((8.f + 30.f * s) * amount, [&](Particle& p, int) {
        const float a = rndAngle();
        const glm::vec3 out{za::cos(a), za::sin(a), 0.f};
        drop(p, vrand::range(rng, 0.45f, 0.75f));
        p.die = cl.time + vrand::range(rng, 0.8f, 1.4f);
        p.scale = vrand::range(rng, 0.35f, 0.7f) * (0.9f + 0.2f * size) * big;
        p.fade = -0.5f;
        p.org = org + out * vrand::range(rng, 0.5f, 2.5f) * size + glm::vec3{0.f, 0.f, 0.5f};
        p.vel = out * vrand::range(rng, 30.f, 95.f) * size + lean * vrand::range(rng, 10.f, 50.f) * size + glm::vec3{0.f, 0.f, vrand::range(rng, 40.f, 150.f) * size};
    });

    // The jet: a thin column of big drops thrown straight up (a bullet's is most of its splash), in a plume of spray.
    make((3.f + 4.f * s) * amount, [&](Particle& p, int i) {
        drop(p, 0.85f);
        p.die = cl.time + 3.0;
        p.scale = vrand::range(rng, 0.9f, 1.7f) * (0.85f + 0.35f * size) * big;
        p.fade = lava ? -0.4f : -0.1f;
        p.streak *= 1.3f;
        p.plink = !lava && i % 2 == 0;
        p.org = org + inBox(0.5f + 0.5f * size) + glm::vec3{0.f, 0.f, 1.f};
        p.vel = inBox(5.f + 6.f * size) + lean * vrand::range(rng, 0.f, 30.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 130.f, 260.f) * za::clamp(size, 0.7f, 1.5f)};
    });
    make((0.6f + 0.4f * s) * amount, [&](Particle& p, int) {
        p.cell = lava ? CellExplosion : CellSpray;
        p.additive = lava;
        p.color = glm::vec4{foam, lava ? 0.4f : 0.55f};
        p.die = cl.time + 0.9;
        p.scale = vrand::range(rng, 1.6f, 2.4f) * (0.7f + 0.4f * size) * big;
        p.type = Custom;
        p.fade = -0.7f;
        p.grow = 2.5f * size * big;
        p.drag = 1.5f;
        p.spin = vrand::range(rng, -0.8f, 0.8f);
        p.acc = gravity(0.35f);
        p.org = org + glm::vec3{0.f, 0.f, 2.f * size};
        p.vel = inBox(6.f) + lean * vrand::range(rng, 0.f, 20.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 60.f, 110.f) * za::clamp(size, 0.7f, 1.5f)};
    });

    // Spray: clouds of fine droplets thrown up and out round the crown, spreading and falling.
    make((2.f + 3.f * s) * amount, [&](Particle& p, int) {
        const float a = rndAngle();
        const glm::vec3 out{za::cos(a), za::sin(a), 0.f};
        p.cell = lava ? CellExplosion : CellSpray;
        p.additive = lava;
        p.color = glm::vec4{foam * vrand::range(rng, 0.9f, 1.1f), lava ? 0.4f : vrand::range(rng, 0.4f, 0.6f)};
        p.die = cl.time + 1.2;
        p.scale = vrand::range(rng, 1.8f, 3.2f) * (0.6f + 0.45f * size) * big;
        p.type = Custom;
        p.fade = lava ? -0.6f : -0.55f;
        p.grow = 2.8f * size * big;
        p.drag = 1.8f;
        p.spin = vrand::range(rng, -1.f, 1.f);
        p.acc = gravity(0.4f);
        p.org = org + out * vrand::range(rng, 0.5f, 3.f) * size + glm::vec3{0.f, 0.f, vrand::range(rng, 1.f, 3.f) * size};
        p.vel = out * vrand::range(rng, 20.f, 55.f) * size + lean * vrand::range(rng, 5.f, 30.f) * size + glm::vec3{0.f, 0.f, vrand::range(rng, 25.f, 70.f) * size};
    });

    // Mist over a hard splash: faint, wide and slow.
    if(s >= 0.8f && !lava)
    {
        make((0.5f + 0.4f * s) * amount, [&](Particle& p, int) {
            p.cell = CellSmoke;
            p.color = glm::vec4{foam, vrand::range(rng, 0.1f, 0.16f)};
            p.die = cl.time + 2.2;
            p.scale = vrand::range(rng, 3.f, 4.5f) * size * big;
            p.type = Custom;
            p.fade = -0.075f;
            p.grow = 3.f * big;
            p.drag = 1.2f;
            p.spin = vrand::range(rng, -0.4f, 0.4f);
            p.acc = gravity(-0.02f);
            p.org = org + inBox(2.f * size) + glm::vec3{0.f, 0.f, 3.f * size};
            p.vel = inBox(12.f * size) + glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 25.f)};
        });
    }

    // Foam: patches of bubbles lying on the surface, spreading and thinning out (lava: a dark crust).
    make((1.f + 0.8f * s) * amount, [&](Particle& p, int) {
        const float a = rndAngle();
        const glm::vec3 out{za::cos(a), za::sin(a), 0.f};
        const float life = vrand::range(rng, 2.f, 3.f);
        p.cell = CellFoam;
        lying(p);
        p.color = glm::vec4{lava ? glm::vec3{0.2f, 0.06f, 0.02f} : foam, lava ? 0.6f : vrand::range(rng, 0.5f, 0.7f)};
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 2.2f, 3.5f) * (0.6f + 0.45f * size) * big;
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = vrand::range(rng, 1.f, 1.6f) * size * big;
        p.drag = 2.f;
        p.spin = vrand::range(rng, -0.3f, 0.3f);
        p.org = org + out * vrand::range(rng, 0.f, 3.f) * size + glm::vec3{0.f, 0.f, 0.3f};
        p.vel = out * vrand::range(rng, 4.f, 14.f) * size;
    });

    // Rings: riding the ripples' crest out (vr_water_ripple_speed times vr_water_splash_ring_speed), one more for a
    // harder hit, each after it slower and fainter. The crest is at 0.525 of a ring's scale (buildRing).
    const float ringSpeed = water::rippleSpeed(liquid) * za::clamp(vr_water_splash_ring_speed.value, 0.f, 4.f);
    const float ringSize = za::clamp(vr_water_splash_ring_size.value, 0.f, 4.f);
    const int rings = ringSize <= 0.f ? 0 : s < 0.25f ? 1 : s < 2.f ? 2 : 3;
    for(int i = 0; i < rings; i++)
    {
        make(1.f, [&](Particle& p, int) {
            const float fi = static_cast<float>(i);
            const float life = (1.3f + 0.35f * fi) * (0.85f + 0.25f * size) * ringSize;
            p.cell = CellRing;
            lying(p);
            p.color = glm::vec4{lava ? glm::vec3{0.22f, 0.06f, 0.02f} : foam, (lava ? 0.6f : 0.55f) / (1.f + 0.5f * fi)};
            p.die = cl.time + life;
            p.scale = (1.5f + fi) * (0.7f + 0.4f * size) * ringSize * big / 0.525f;
            p.type = Custom;
            p.fade = -p.color.a / life;
            p.grow = ringSpeed * (1.f - 0.15f * fi) * (0.85f + 0.15f * size) / 0.525f;
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
            p.die = cl.time + vrand::range(rng, 0.6f, 1.2f);
            p.scale = vrand::range(rng, 0.25f, 0.45f);
            p.type = Custom;
            p.fade = -1.f;
            p.drag = 0.8f;
            p.spin = vrand::range(rng, -6.f, 6.f);
            p.acc = gravity(0.35f);
            p.org = org + inBox(2.f) + glm::vec3{0.f, 0.f, 1.f};
            p.vel = onSphere() * vrand::range(rng, 20.f, 60.f) * size + glm::vec3{0.f, 0.f, vrand::range(rng, 40.f, 120.f) * size};
        });
        make(1.f + s * 0.5f, [&](Particle& p, int) {
            p.cell = CellSmoke;
            p.color = glm::vec4{0.2f, 0.17f, 0.15f, 0.5f};
            p.die = cl.time + 2.5;
            p.scale = vrand::range(rng, 1.2f, 1.8f) * size;
            p.type = TxSmoke;
            p.acc = gravity(-0.05f);
            p.org = org + inBox(2.f) + glm::vec3{0.f, 0.f, 2.f};
            p.vel = inBox(8.f) + glm::vec3{0.f, 0.f, 20.f};
        });
    }
}

// An explosion under a liquid's surface (a rocket into a pool): the liquid thrown up above it, less the deeper it went
// off (its splash's strength, 0: none), and where.
[[nodiscard]] int underwaterExplosion(const glm::vec3& org, glm::vec3& surface)
{
    return surfaceAbove(org, 96.f, surface) ? za::max(0, static_cast<int>(45.f - 0.3f * (surface.z - org.z))) : 0;
}

void adStreamTongues(const glm::vec3& org, const glm::vec3& vel, int count, bool poison);

// `n` rounded up or down at random (a fraction's share of the time up): a few a second from a fraction an emission.
[[nodiscard]] float someOf(float n)
{
    return za::floor(n + vrand::range(rng, 0.f, 1.f));
}

// AD support: a flamethrower's stream (Preset::AdFlame, AdPoison): our torches' fire tongues (CellFire, as fireSource)
// at random angles over AD's flame sprite, going its way (`dir` its velocity / 100) and slowing, rising, fading.
// vr_adgun_fx_rate: each think's (0.05 s) tongues made this many times over, spread back along the way the flame came
// (a denser stream, no gaps between the flames); then the stream's extras: embers and smoke off the fire, a green mist
// lingering after the poison (vr_adgun_fx_embers, vr_adgun_fx_mist).
void adStreamFire(const glm::vec3& org, const glm::vec3& dir, int count, bool poison)
{
    const glm::vec3 vel = dir * 100.f;
    const int times = za::clamp(static_cast<int>(vr_adgun_fx_rate.value + 0.5f), 1, 4);
    for(int t = 0; t < times; t++)
    {
        adStreamTongues(org - vel * (0.05f * static_cast<float>(t) / static_cast<float>(times)), vel, count, poison);
    }
    const float embers = za::clamp(vr_adgun_fx_embers.value, 0.f, 3.f);
    if(!poison && embers > 0.f)
    {
        // Embers thrown off the stream, arcing down and dimming; now and then a wisp of dark smoke from its tail.
        make(someOf(0.35f * embers * static_cast<float>(count)), [&](Particle& p, int) {
            p.cell = CellSpark;
            p.type = Custom;
            p.additive = true;
            p.color = glm::vec4{glm::mix(glm::vec3{1.f, 0.45f, 0.1f}, glm::vec3{1.f, 0.85f, 0.4f}, vrand::range(rng, 0.f, 0.6f)), 1.f};
            const float life = vrand::range(rng, 0.5f, 1.1f);
            p.die = cl.time + life;
            p.fade = -0.9f / life;
            p.scale = vrand::range(rng, 0.25f, 0.45f);
            p.grow = -0.15f;
            p.drag = 1.2f;
            p.org = org + inBox(4.f);
            p.vel = vel * vrand::range(rng, 0.3f, 0.7f) + inBox(60.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 20.f, 70.f)};
            p.acc = gravity(0.35f);
        });
        make(someOf(0.15f * embers * static_cast<float>(count)), [&](Particle& p, int) {
            p.cell = CellSmoke;
            p.type = Custom;
            const float g = vrand::range(rng, 0.12f, 0.2f);
            p.color = glm::vec4{g, g * 0.95f, g * 0.9f, vrand::range(rng, 0.18f, 0.28f)};
            const float life = vrand::range(rng, 1.f, 1.6f);
            p.die = cl.time + life;
            p.fade = -p.color.a / life;
            p.scale = vrand::range(rng, 2.f, 3.f);
            p.grow = 3.f;
            p.spin = vrand::range(rng, -0.8f, 0.8f);
            p.drag = 1.5f;
            p.org = org + vel * 0.08f + inBox(6.f);
            p.vel = vel * 0.25f + inBox(8.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 15.f, 30.f)};
            p.acc = glm::vec3{0.f, 0.f, 12.f};
        });
    }
    const float mist = za::clamp(vr_adgun_fx_mist.value, 0.f, 3.f);
    if(poison && mist > 0.f)
    {
        // A faint green mist left hanging where the poison went, slowly spreading and settling.
        make(someOf(0.2f * mist * static_cast<float>(count)), [&](Particle& p, int) {
            p.cell = CellSmoke;
            p.type = Custom;
            p.additive = true;
            const float g = vrand::range(rng, 0.8f, 1.1f);
            p.color = glm::vec4{0.25f * g, 0.6f * g, 0.1f * g, vrand::range(rng, 0.08f, 0.13f)};
            const float life = vrand::range(rng, 2.2f, 3.4f);
            p.die = cl.time + life;
            p.fade = -p.color.a / life;
            p.scale = vrand::range(rng, 3.5f, 5.f);
            p.grow = 1.5f;
            p.spin = vrand::range(rng, -0.3f, 0.3f);
            p.drag = 3.f;
            p.org = org + inBox(8.f);
            p.vel = vel * 0.4f + inBox(10.f);
            p.acc = glm::vec3{0.f, 0.f, -2.f};
        });
    }
}

// adStreamFire's tongues: `count` of them at `org`, the flame going at `vel`.
void adStreamTongues(const glm::vec3& org, const glm::vec3& vel, int count, bool poison)
{
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellFire;
        p.type = Custom;
        p.additive = true;
        const glm::vec3 c = poison ? glm::mix(glm::vec3{0.2f, 0.85f, 0.12f}, glm::vec3{0.55f, 1.f, 0.25f}, vrand::range(rng, 0.f, 1.f))
                                   : fireColor();
        const float life = vrand::range(rng, 0.25f, 0.45f);
        p.color = glm::vec4{c, poison ? 0.5f : 0.65f};
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 6.f, 10.f);
        p.grow = 16.f;
        p.spin = vrand::range(rng, -2.5f, 2.5f);
        p.drag = 2.f;
        p.org = org + inBox(3.f);
        p.vel = vel * vrand::range(rng, 0.5f, 0.9f) + inBox(12.f) + glm::vec3{0.f, 0.f, 10.f};
        p.acc = glm::vec3{0.f, 0.f, poison ? 8.f : 45.f};
    });
}

// AD support: puffs of green smoke over a poisoned body (Preset::PoisonPuff): rising from anywhere in its box (`half`
// its half size / 16), swelling and thinning.
void poisonPuff(const glm::vec3& org, const glm::vec3& half, int count)
{
    const glm::vec3 box = glm::abs(half) * 16.f;
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.type = Custom;
        // (Faintly glowing, as AD's poison: additive, so that it shows in the dark too.)
        p.additive = true;
        const float g = vrand::range(rng, 0.8f, 1.1f);
        p.color = glm::vec4{0.32f * g, 0.75f * g, 0.12f * g, vrand::range(rng, 0.3f, 0.45f)};
        const float life = vrand::range(rng, 1.f, 1.6f);
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 2.4f, 3.6f);
        p.grow = 2.5f;
        p.spin = vrand::range(rng, -0.6f, 0.6f);
        p.drag = 0.8f;
        p.org = org + glm::vec3{vrand::range(rng, -box.x, box.x), vrand::range(rng, -box.y, box.y), vrand::range(rng, -box.z, box.z)} * 0.8f;
        p.vel = inBox(6.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 6.f, 14.f)};
        p.acc = glm::vec3{0.f, 0.f, 4.f};
    });
}

// A random point on the ground's plane within `radius` of the middle (evenly over the disc).
[[nodiscard]] glm::vec3 inDisc(float radius)
{
    const float a = rndAngle();
    const float r = radius * za::sqrt(vrand::range(rng, 0.f, 1.f));
    return {r * za::cos(a), r * za::sin(a), 0.f};
}

// AD support: a fuel tank's burst field (Preset::TankFire, TankSmoke, TankCloud; QC vr_ad_enemyguns.qc
// VR_TankField_Look): `org` a spot on the ground in it, `dir` (spread round it in 8 units, as the message holds 1/16ths
// to 8; size; alpha: the field fading).
// The fire: tongues of the torches' fire (CellFire) rising straight up off the ground, slowing at the top, and a few
// embers going up with them.
void tankFire(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float spread = za::clamp(dir.x, 0.f, 7.9f) * 8.f;
    const float size = za::clamp(dir.y, 0.2f, 3.f);
    const float alpha = za::clamp(dir.z, 0.f, 1.f);
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellFire;
        p.type = Custom;
        // Some glowing (added to the scene: a dark place's fire), most drawn over it in deep oranges, so that they read
        // on a bright floor too (added light vanishes on one: vrtesthall's).
        p.additive = vrand::range(rng, 0.f, 1.f) < 0.4f;
        const float life = vrand::range(rng, 0.35f, 0.7f);
        const glm::vec3 c = p.additive ? fireColor()
                                       : glm::mix(glm::vec3{0.85f, 0.2f, 0.02f}, glm::vec3{1.f, 0.55f, 0.1f}, vrand::range(rng, 0.f, 1.f));
        p.color = glm::vec4{c, (p.additive ? vrand::range(rng, 0.55f, 0.75f) : vrand::range(rng, 0.65f, 0.85f)) * alpha};
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 5.f, 8.f) * size;
        p.grow = vrand::range(rng, 2.f, 6.f) * size;
        p.spin = vrand::range(rng, -0.6f, 0.6f);
        p.drag = 1.2f;
        p.org = org + inDisc(spread) + glm::vec3{0.f, 0.f, vrand::range(rng, 2.f, 8.f) * size};
        p.vel = glm::vec3{vrand::range(rng, -3.f, 3.f), vrand::range(rng, -3.f, 3.f), vrand::range(rng, 55.f, 95.f) * size};
        p.acc = glm::vec3{0.f, 0.f, 30.f};
    });
    make(someOf(0.25f * static_cast<float>(count)), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.type = Custom;
        p.additive = true;
        p.color = glm::vec4{glm::mix(glm::vec3{1.f, 0.45f, 0.1f}, glm::vec3{1.f, 0.85f, 0.4f}, vrand::range(rng, 0.f, 0.6f)), alpha};
        const float life = vrand::range(rng, 0.7f, 1.4f);
        p.die = cl.time + life;
        p.fade = -alpha / life;
        p.scale = vrand::range(rng, 0.2f, 0.4f);
        p.drag = 0.6f;
        p.org = org + inDisc(spread) + glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 24.f)};
        p.vel = glm::vec3{vrand::range(rng, -12.f, 12.f), vrand::range(rng, -12.f, 12.f), vrand::range(rng, 60.f, 130.f)};
        p.acc = gravity(0.08f);
    });
}

// ... its smoke: dark puffs off the flames' tops, rising, swelling and thinning out.
void tankSmoke(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float spread = za::clamp(dir.x, 0.f, 7.9f) * 8.f;
    const float size = za::clamp(dir.y, 0.2f, 3.f);
    const float alpha = za::clamp(dir.z, 0.f, 1.f);
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.type = Custom;
        const float g = vrand::range(rng, 0.08f, 0.16f);
        p.color = glm::vec4{g, g * 0.94f, g * 0.88f, vrand::range(rng, 0.32f, 0.48f) * alpha};
        const float life = vrand::range(rng, 2.5f, 3.8f);
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 4.f, 7.f) * size;
        p.grow = vrand::range(rng, 2.5f, 4.f) * size;
        p.spin = vrand::range(rng, -0.5f, 0.5f);
        p.drag = 0.5f;
        p.org = org + inDisc(spread) + glm::vec3{0.f, 0.f, vrand::range(rng, 28.f, 44.f) * size};
        p.vel = glm::vec3{vrand::range(rng, -6.f, 6.f), vrand::range(rng, -6.f, 6.f), vrand::range(rng, 22.f, 40.f)};
        p.acc = glm::vec3{0.f, 0.f, 5.f};
    });
}

// ... the poison sprayer's cloud: big puffs of green smoke (a smoke bomb's) low over the ground, billowing out and
// churning slowly, thinning out; a faint glow in some, as AD's poison (additive: it shows in the dark too).
void tankCloud(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const float spread = za::clamp(dir.x, 0.f, 7.9f) * 8.f;
    const float size = za::clamp(dir.y, 0.2f, 3.f);
    const float alpha = za::clamp(dir.z, 0.f, 1.f);
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.type = Custom;
        const float g = vrand::range(rng, 0.8f, 1.15f);
        p.color = glm::vec4{0.15f * g, 0.4f * g, 0.08f * g, vrand::range(rng, 0.35f, 0.5f) * alpha};
        const float life = vrand::range(rng, 2.4f, 3.6f);
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 10.f, 15.f) * size;
        p.grow = vrand::range(rng, 2.f, 4.f) * size;
        p.spin = vrand::range(rng, -0.35f, 0.35f);
        p.drag = 0.8f;
        p.org = org + inDisc(spread) + glm::vec3{0.f, 0.f, vrand::range(rng, 8.f, 48.f) * size};
        p.vel = glm::vec3{vrand::range(rng, -8.f, 8.f), vrand::range(rng, -8.f, 8.f), vrand::range(rng, 2.f, 9.f)};
        p.acc = glm::vec3{0.f, 0.f, 1.5f};
    });
    make(someOf(0.35f * static_cast<float>(count)), [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.type = Custom;
        p.additive = true;
        const float g = vrand::range(rng, 0.8f, 1.1f);
        p.color = glm::vec4{0.25f * g, 0.62f * g, 0.1f * g, vrand::range(rng, 0.06f, 0.1f) * alpha};
        const float life = vrand::range(rng, 2.f, 3.f);
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 5.f, 7.f) * size;
        p.grow = 1.5f * size;
        p.spin = vrand::range(rng, -0.3f, 0.3f);
        p.drag = 0.8f;
        p.org = org + inDisc(spread) + glm::vec3{0.f, 0.f, vrand::range(rng, 6.f, 26.f) * size};
        p.vel = glm::vec3{vrand::range(rng, -6.f, 6.f), vrand::range(rng, -6.f, 6.f), vrand::range(rng, 1.f, 5.f)};
        p.acc = glm::vec3{0.f, 0.f, 1.f};
    });
}

// Short blue-white crackling arcs (CellLightning), `count` of them about `org`, `reach` units out.
void electricArcs(const glm::vec3& org, float count, float reach, float size)
{
    make(count, [&](Particle& p, int) {
        p.cell = CellLightning;
        p.type = Custom;
        p.additive = true;
        const float w = vrand::range(rng, 0.8f, 1.f);
        p.color = glm::vec4{w * vrand::range(rng, 0.55f, 0.75f), w * vrand::range(rng, 0.75f, 0.9f), w, vrand::range(rng, 0.7f, 1.f)};
        const float life = vrand::range(rng, 0.05f, 0.14f);
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.scale = vrand::range(rng, 0.7f, 1.3f) * size;
        p.spin = vrand::range(rng, -20.f, 20.f);
        p.org = org + inBox(reach);
    });
}

// AD support: an electric gun's muzzle or its bolt's hit (Preset::ElectricSparks): crackling arcs and `count` quick
// blue-white sparks thrown along `dir`.
void electricSparks(const glm::vec3& org, const glm::vec3& dir, int count)
{
    const glm::vec3 d = glm::length(dir) > 0.01f ? glm::normalize(dir) : glm::vec3{0.f};
    electricArcs(org + d * 2.f, za::max(1.f, count * 0.4f), 2.f, 2.2f);
    make(static_cast<float>(count), [&](Particle& p, int) {
        p.cell = CellSpark;
        const float w = vrand::range(rng, 0.8f, 1.f);
        p.color = glm::vec4{w * vrand::range(rng, 0.6f, 0.85f), w * vrand::range(rng, 0.8f, 1.f), w, 1.f};
        const float life = vrand::range(rng, 0.12f, 0.35f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.2f, 0.4f);
        p.type = Custom;
        p.fade = -1.f / life;
        p.grow = -0.3f;
        p.drag = 1.5f;
        p.additive = true;
        p.streak = 0.025f;
        p.acc = gravity(0.5f);
        p.org = org + inBox(0.5f);
        p.vel = d * vrand::range(rng, 40.f, 140.f) + onSphere() * vrand::range(rng, 30.f, 80.f);
    });
}

// AD support: AD's plasma rifles' bolt (progs/proj_plasma.mdl, the soldiers', eliminators' and centurions', theirs and
// the player's: its model's grenade smoke flag): a faint smoke and crackling arcs and sparks along it.
void electricTrail(const glm::vec3& from, const glm::vec3& to)
{
    const float length = glm::length(to - from);
    smokeTrail(from, to, 9.f, 0.6f, 0.12f, 0.6f);
    const glm::vec3 d = to - from;
    const float arcs = perLength(length, 9.f);
    for(int i = 0; i < static_cast<int>(arcs); i++)
    {
        electricArcs(from + d * vrand::range(rng, 0.f, 1.f), 1.f, 1.5f, 1.4f);
    }
    make(perLength(length, 12.f), [&](Particle& p, int) {
        p.cell = CellSpark;
        const float w = vrand::range(rng, 0.8f, 1.f);
        p.color = glm::vec4{w * vrand::range(rng, 0.6f, 0.85f), w * vrand::range(rng, 0.8f, 1.f), w, 1.f};
        const float life = vrand::range(rng, 0.15f, 0.3f);
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.18f, 0.32f);
        p.type = Custom;
        p.fade = -1.f / life;
        p.drag = 2.f;
        p.additive = true;
        p.streak = 0.02f;
        p.acc = gravity(0.3f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(1.f);
        p.vel = onSphere() * vrand::range(rng, 25.f, 70.f);
    });
}

[[nodiscard]] bool adElectricBolt(const qmodel_t* m)
{
    return m && !strcmp(m->name, "progs/proj_plasma.mdl"); // AD support
}
} // namespace detail

// A fire model's emission burst: its world-axis upward velocity does not invert with a held torch.
int fireSource(const glm::vec3& at, float sourceScale)
{
    if(!vr_particles.value || !r_particles.value || !ensureAtlas()) { return 0; }
    const za::SizeT before = pool.size();
    const auto range = [](float a, float b, float low, float high) {
        a = za::clamp(a, low, high); b = za::clamp(b, low, high);
        return vrand::range(rng, za::min(a, b), za::max(a, b));
    };
    make(za::clamp(vr_fire_particles_count.value, 0.f, 16.f), [&](Particle& p, int) {
        p.cell = CellFire;
        p.type = Custom;
        p.additive = true;
        p.angle = -1.5707963f + vrand::range(rng, -0.25f, 0.25f); // clockwise: the shader rotates +angle counterclockwise
        const float s = sourceScale * za::clamp(vr_fire_particles_size.value, 0.1f, 4.f);
        p.scale = vrand::range(rng, 2.f, 3.f) * s;
        p.org = at + glm::vec3{vrand::range(rng, -1.f, 1.f), vrand::range(rng, -1.f, 1.f), vrand::range(rng, -0.5f, 0.5f)} * sourceScale;
        const float life = range(vr_fire_particles_life_min.value, vr_fire_particles_life_max.value, 0.05f, 5.f);
        const float speed = range(vr_fire_particles_speed_min.value, vr_fire_particles_speed_max.value, 0.f, 10.f);
        const float up = range(vr_fire_particles_up_min.value, vr_fire_particles_up_max.value, 0.f, 10.f);
        const float angle = rndAngle();
        p.vel = glm::vec3{za::cos(angle) * speed, za::sin(angle) * speed, up} * units::metresToUnits();
        p.color = glm::vec4{fireColor(), za::clamp(vr_fire_particles_alpha.value, 0.f, 1.f)};
        p.die = cl.time + life;
        p.fade = -p.color.a / life;
        p.grow = s * 1.5f;
        p.spin = vrand::range(rng, -0.3f, 0.3f);
        p.acc = glm::vec3{0.f, 0.f, units::metresToUnits() * 0.3f};
    }, false);
    return static_cast<int>(pool.size() - before);
}

void gust(const glm::vec3& at, float radius, const glm::vec3& vel, float dt)
{
    constexpr float rate = 10.f; // a second: how fast the air takes a particle at the hand along
    if(radius <= 0.f || dt <= 0.f || pool.empty())
    {
        return;
    }
    const float r2 = radius * radius;
    for(Particle& p : pool)
    {
        if(p.cell != CellFire && p.cell != CellSmoke)
        {
            continue;
        }
        const glm::vec3 d = p.org - at;
        const float d2 = glm::dot(d, d);
        if(d2 >= r2)
        {
            continue;
        }
        const float w = 1.f - za::sqrt(d2) / radius;
        p.vel += (vel - p.vel) * za::min(1.f, rate * w * w * dt);
    }
}

// Incandescent physical chunks leave short flame tongues and slower, lingering smoke.
void explosionDebrisTrail(const glm::vec3& from, const glm::vec3& to, float size, float heat)
{
    if(!vr_particles.value || !r_particles.value || !ensureAtlas()) { return; }
    const float length = glm::length(to - from);
    const float density = za::clamp(vr_explosion_debris_trail.value, 0.f, 3.f);
    if(length <= 0.f || density <= 0.f) { return; }
    const float count = perLength(length, za::max(1.f, size) / density);
    make(count, [&](Particle& p, int) {
        p.cell = CellExplosion;
        p.org = glm::mix(from, to, vrand::range(rng, 0.f, 1.f));
        p.color = glm::vec4{1.f, 0.3f + 0.2f * heat, 0.04f, 0.65f * heat};
        p.additive = true;
        p.type = Custom;
        p.scale = size * vrand::range(rng, 0.6f, 1.f);
        p.die = cl.time + vrand::range(rng, 0.15f, 0.3f);
        p.fade = -3.f;
        p.grow = size * 2.f;
        p.vel = inBox(4.f) + glm::vec3{0.f, 0.f, 12.f};
        p.spin = vrand::range(rng, -2.f, 2.f);
    });
    make(count * 0.5f, [&](Particle& p, int) {
        p.cell = CellSmoke;
        p.org = glm::mix(from, to, vrand::range(rng, 0.f, 1.f));
        p.color = glm::vec4{0.25f, 0.22f, 0.2f, 0.32f};
        p.type = Custom;
        p.scale = size * vrand::range(rng, 0.8f, 1.4f);
        p.die = cl.time + vrand::range(rng, 0.7f, 1.2f);
        p.fade = -0.3f;
        p.grow = size * 4.f;
        p.vel = inBox(5.f) + glm::vec3{0.f, 0.f, 14.f};
        p.spin = vrand::range(rng, -0.5f, 0.5f);
    });
}

void shellEject(const glm::vec3& org, const glm::vec3& dir, float smoke, int sparks)
{
    if(!vr_particles.value || !ensureAtlas())
    {
        return;
    }

    // A faint grey puff out of the port, drifting after the shell and spreading.
    make(za::ceil(2.f * smoke), [&](Particle& p, int) {
        p.cell = CellSmoke;
        const float g = vrand::range(rng, 0.5f, 0.62f);
        p.color = glm::vec4{g, g, g * 0.97f, vrand::range(rng, 0.14f, 0.22f) * za::min(smoke, 1.5f)};
        p.die = cl.time + 1.8;
        p.scale = vrand::range(rng, 0.35f, 0.55f);
        p.type = Custom;
        p.fade = -p.color.a / 1.6f;
        p.grow = vrand::range(rng, 1.4f, 2.2f);
        p.drag = 2.5f;
        p.spin = vrand::range(rng, -1.f, 1.f);
        p.acc = gravity(-0.03f);
        p.org = org + inBox(0.4f);
        p.vel = dir * vrand::range(rng, 4.f, 14.f) + inBox(3.f);
    });
    // A few tiny sparks of burning powder, gone in a blink.
    make(static_cast<float>(sparks), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, vrand::range(rng, 0.55f, 0.8f), vrand::range(rng, 0.2f, 0.35f), 1.f};
        p.die = cl.time + vrand::range(rng, 0.12f, 0.35f);
        p.scale = vrand::range(rng, 0.1f, 0.18f);
        p.type = Custom;
        p.fade = -3.f;
        p.drag = 3.f;
        p.spin = vrand::range(rng, -8.f, 8.f);
        p.acc = gravity(0.3f);
        p.org = org;
        p.vel = dir * vrand::range(rng, 30.f, 80.f) + onSphere() * vrand::range(rng, 5.f, 25.f);
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
        p.color = glm::vec4{1.f, vrand::range(rng, 0.38f, 0.5f), 0.12f, 0.55f};
        p.die = cl.time + 0.16;
        p.scale = vrand::range(rng, 1.4f, 1.9f);
        p.type = Custom;
        p.fade = -3.4f;
        p.grow = -6.f;
        p.org = from + d * vrand::range(rng, 0.f, 1.f);
    });
    // Embers shed on the way, falling and dimming.
    make(perLength(length, 22.f), [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, vrand::range(rng, 0.35f, 0.6f), vrand::range(rng, 0.08f, 0.18f), 1.f};
        p.die = cl.time + vrand::range(rng, 0.35f, 0.7f);
        p.scale = vrand::range(rng, 0.18f, 0.32f);
        p.type = Custom;
        p.fade = -1.6f;
        p.grow = -0.2f;
        p.drag = 1.6f;
        p.spin = vrand::range(rng, -8.f, 8.f);
        p.acc = gravity(0.25f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(0.8f);
        p.vel = onSphere() * vrand::range(rng, 8.f, 28.f);
    });
}

void counterEmber(const glm::vec3& org, const glm::vec3& vel, float bright)
{
    if(bright <= 0.f || !enabled())
    {
        return;
    }

    // Gold, a little hotter than the lava nails' embers and bigger (they must read on a blade at arm's length), rising
    // off the surface and dimming as they go.
    make(1.f, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        p.color = glm::vec4{1.f, vrand::range(rng, 0.68f, 0.86f), vrand::range(rng, 0.22f, 0.36f), za::min(1.f, bright)};
        p.die = cl.time + vrand::range(rng, 0.4f, 0.75f);
        p.scale = vrand::range(rng, 0.34f, 0.56f);
        p.type = Custom;
        p.fade = -1.5f;
        p.grow = -0.35f;
        p.drag = 1.8f;
        p.spin = vrand::range(rng, -6.f, 6.f);
        p.acc = gravity(-0.02f);
        p.org = org;
        p.vel = vel;
    });
}

void hotEmber(const glm::vec3& org, const glm::vec3& vel, const glm::vec3& color, float bright)
{
    if(bright <= 0.f || !enabled())
    {
        return;
    }

    // Small and hot: the glow's colour, yellower in the middle of some, rising off the metal and dimming as they go.
    make(1.f, [&](Particle& p, int) {
        p.cell = CellSpark;
        p.additive = true;
        const float yellow = vrand::range(rng, 0.f, 0.45f);
        p.color = glm::vec4{glm::mix(color, glm::vec3{1.f, 0.85f, 0.45f}, yellow), za::min(1.f, bright)};
        p.die = cl.time + vrand::range(rng, 0.35f, 0.8f);
        p.scale = vrand::range(rng, 0.3f, 0.55f);
        p.type = Custom;
        p.fade = -1.4f;
        p.grow = -0.3f;
        p.drag = 1.6f;
        p.spin = vrand::range(rng, -6.f, 6.f);
        p.acc = gravity(-0.025f);
        p.org = org;
        p.vel = vel;
    }, false);
}

void hotTrail(const glm::vec3& org, const glm::vec3& vel, const glm::vec3& color, float bright, float life, bool spark)
{
    if(bright <= 0.f || life <= 0.f || !enabled())
    {
        return;
    }

    make(1.f, [&](Particle& p, int) {
        p.additive = true;
        p.type = Custom;
        p.org = org;
        p.vel = vel;
        const float l = life * vrand::range(rng, 0.6f, 1.f);
        p.die = cl.time + l;
        if(spark)
        {
            // A spark thrown off the hot metal: bright, tiny, falling a little.
            p.cell = CellSpark;
            p.color = glm::vec4{glm::mix(color, glm::vec3{1.f, 0.9f, 0.6f}, vrand::range(rng, 0.3f, 0.7f)), za::min(1.f, bright)};
            p.scale = vrand::range(rng, 0.25f, 0.45f);
            p.fade = -za::min(1.f, bright) / l;
            p.grow = -0.15f;
            p.drag = 2.5f;
            p.acc = gravity(0.06f);
            p.spin = vrand::range(rng, -8.f, 8.f);
        }
        else
        {
            // A small tongue of flame hanging in the air where the blade passed, rising as it fades.
            p.cell = CellFire;
            p.angle = -1.5707963f + vrand::range(rng, -0.4f, 0.4f); // upright (the shader turns +angle counterclockwise)
            p.color = glm::vec4{glm::mix(color, glm::vec3{1.f, 0.6f, 0.15f}, 0.35f), 0.75f * za::min(1.f, bright)};
            p.scale = vrand::range(rng, 1.f, 1.8f);
            p.fade = -p.color.a / l;
            p.grow = 0.6f;
            p.drag = 3.f;
            p.acc = gravity(-0.03f);
            p.spin = vrand::range(rng, -0.4f, 0.4f);
        }
    }, false);
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
        const float g = vrand::range(rng, 0.52f, 0.62f);
        p.color = glm::vec4{g, g, g * 0.97f, vrand::range(rng, 0.05f, 0.09f) * strength};
        p.die = cl.time + 1.4;
        p.scale = vrand::range(rng, 0.22f, 0.36f);
        p.type = Custom;
        p.fade = -p.color.a / 1.2f;
        p.grow = 1.1f;
        p.drag = 1.5f;
        p.spin = vrand::range(rng, -0.8f, 0.8f);
        p.acc = gravity(-0.03f);
        p.org = from + d * vrand::range(rng, 0.f, 1.f) + inBox(0.2f);
        p.vel = inBox(1.5f);
    });
}

// QVR round 21: a casing dropping into a liquid. The splash preset's parts at a casing's size: a few small drops
// thrown up a few centimetres (half of them leaving a little ring where they fall back), one ring riding the ripple's
// crest, a wisp of foam; in lava an ember or two. vr_water_splash scales how many, _size how big, _ring_size the ring.
void shellSplash(const glm::vec3& org, const glm::vec3& dir, float strength)
{
    if(VR_NoLiquidEffects(cl.worldmodel, &org.x)) { return; }
    const float amount = za::clamp(vr_water_splash.value, 0.f, 3.f);
    if(amount <= 0.f || !vr_particles.value || !ensureAtlas())
    {
        return;
    }
    const int liquid = liquidUnder(org);
    const bool lava = liquid == CONTENTS_LAVA;
    const bool slime = liquid == CONTENTS_SLIME;
    const float s = za::clamp(strength, 0.f, 1.f);
    const float big = za::clamp(vr_water_splash_size.value, 0.25f, 4.f);
    const float shade = lava ? 1.f : shadeAt(org);
    const glm::vec3 tint = lava ? glm::vec3{1.f, 0.42f, 0.1f} : slime ? glm::vec3{0.42f, 0.78f, 0.2f} : glm::vec3{0.8f, 0.9f, 1.f};
    const glm::vec3 foam = glm::mix(tint, glm::vec3{1.f}, lava ? 0.1f : 0.45f) * shade;
    const glm::vec3 lean = glm::vec3{dir.x, dir.y, 0.f} * 0.5f;

    // The drops: a small crown and a jet, 3 to 12 cm up.
    make((4.f + 4.f * s) * amount, [&](Particle& p, int i) {
        const float a = rndAngle();
        const glm::vec3 out{za::cos(a), za::sin(a), 0.f};
        p.cell = CellDrop;
        p.type = Custom;
        p.color = glm::vec4{lava ? glm::mix(glm::vec3{1.f, 0.38f, 0.06f}, glm::vec3{1.f, 0.88f, 0.5f}, vrand::range(rng, 0.f, 1.f))
                                 : tint * shade * vrand::range(rng, 0.85f, 1.2f),
            lava ? 1.f : vrand::range(rng, 0.6f, 0.85f)};
        p.acc = gravity(1.f);
        p.floor = org.z - 0.5f;
        p.liquid = liquid;
        p.streak = 0.012f;
        p.plink = !lava && i % 2 == 0;
        p.die = cl.time + 1.2;
        p.fade = lava ? -0.6f : -0.2f;
        p.scale = vrand::range(rng, 0.25f, 0.45f) * big;
        const bool jet = i < 2;
        p.org = org + out * vrand::range(rng, 0.2f, 0.8f) + glm::vec3{0.f, 0.f, 0.3f};
        p.vel = jet ? inBox(3.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 45.f, 70.f) * (0.8f + 0.3f * s)}
                    : out * vrand::range(rng, 8.f, 22.f) + lean * vrand::range(rng, 5.f, 15.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 28.f, 50.f) * (0.8f + 0.3f * s)};
    });

    // One ring spreading from it, small and faint.
    const float ringSize = za::clamp(vr_water_splash_ring_size.value, 0.f, 4.f);
    if(ringSize > 0.f)
    {
        const float ringSpeed = water::rippleSpeed(liquid) * za::clamp(vr_water_splash_ring_speed.value, 0.f, 4.f);
        make(1.f, [&](Particle& p, int) {
            const float life = 0.9f * ringSize;
            p.cell = CellRing;
            p.flat = true;
            p.floor = org.z;
            p.liquid = liquid;
            p.color = glm::vec4{lava ? glm::vec3{0.22f, 0.06f, 0.02f} : foam, lava ? 0.45f : 0.35f};
            p.die = cl.time + life;
            p.scale = 0.9f * ringSize * big / 0.525f;
            p.type = Custom;
            p.fade = -p.color.a / life;
            p.grow = ringSpeed * 0.6f / 0.525f;
            p.org = org + glm::vec3{0.f, 0.f, 0.35f};
        });
    }

    // A wisp of foam where it went in (lava: a dark crust), and an ember.
    make(1.f, [&](Particle& p, int) {
        const float life = vrand::range(rng, 1.f, 1.6f);
        p.cell = CellFoam;
        p.flat = true;
        p.floor = org.z;
        p.liquid = liquid;
        p.color = glm::vec4{lava ? glm::vec3{0.2f, 0.06f, 0.02f} : foam, lava ? 0.5f : vrand::range(rng, 0.3f, 0.4f)};
        p.die = cl.time + life;
        p.scale = vrand::range(rng, 0.7f, 1.f) * big;
        p.type = Custom;
        p.fade = -p.color.a / life;
        p.grow = 0.4f * big;
        p.drag = 2.f;
        p.spin = vrand::range(rng, -0.3f, 0.3f);
        p.org = org + glm::vec3{0.f, 0.f, 0.3f};
    });
    if(lava)
    {
        make(1.f + 2.f * s, [&](Particle& p, int) {
            p.cell = CellSpark;
            p.additive = true;
            p.color = glm::vec4{fireColor(), 1.f};
            p.die = cl.time + vrand::range(rng, 0.4f, 0.8f);
            p.scale = vrand::range(rng, 0.2f, 0.35f);
            p.type = Custom;
            p.fade = -1.5f;
            p.drag = 0.8f;
            p.spin = vrand::range(rng, -6.f, 6.f);
            p.acc = gravity(0.35f);
            p.org = org + glm::vec3{0.f, 0.f, 0.5f};
            p.vel = onSphere() * vrand::range(rng, 8.f, 20.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 20.f, 45.f)};
        });
    }
}
} // namespace qvr::particles
