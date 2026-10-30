// vr_impacts.cpp -- see vr_impacts.hpp.

#include "vr_impacts.hpp"
#include "vr_impact_shared.h"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_engine.hpp"
#include "vr_particles.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_random.hpp"
#include "vr_trace.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

#include <array>
#include <stdlib.h>
#include <string.h>


namespace qvr::impacts
{

static_assert(static_cast<int>(Material::Count) == VR_MAT_COUNT && static_cast<int>(Material::Armour) == VR_MAT_ARMOUR &&
              static_cast<int>(Material::Grenade) == VR_MAT_GRENADE && static_cast<int>(Material::Wood) == VR_MAT_WOOD);
static_assert(static_cast<int>(HitKind::Count) == VR_HIT_COUNT && static_cast<int>(HitKind::Bolt) == VR_HIT_BOLT &&
              static_cast<int>(HitKind::Pellet) == VR_HIT_PELLET && static_cast<int>(HitKind::Bash) == VR_HIT_BASH);

namespace
{

// A hit's decal: a wall hit's own mark (cl_tent.c VR_DecalTempEntity, carved as the bullet or nail made it), or none
// (a thing hit: its wound is the QC's, vr_wounds.qc).
enum class Decal : int
{
    None,
    TempEntity
};

// A hit's sound sets (impactsound): one sample picked with one of QC's random() numbers (as the QC's own picks did:
// VR_Metal_Sound, VR_Pick4). The physics sounds' sets are vr/phys/<material>_<weight>1..4.
enum class Sound : int
{
    None,
    MetalShot,  // a shot's tink, now and then a ricochet
    MetalBlade, // a blade's clang
    MetalBlunt, // a blunt blow's or a thrown thing's heavy metal knock (a 40 kg box's)
    SoftLayer,  // a blunt blow's second layer on a wall or the floor: the hand's own knock
    WoodLayer,  // on wood
    FleshLayer, // on a body
    MetalLight, // armour's light metal knock over the flesh
    Count
};

struct SoundSet
{
    const char* samples[4];
    float below[4]; // sample i when the number is below below[i] (the last: any)
};

constexpr SoundSet soundSets[] = {
    {{nullptr, nullptr, nullptr, nullptr}, {0.f, 0.f, 0.f, 0.f}},
    {{"weapons/tink1.wav", "weapons/ric1.wav", "weapons/ric2.wav", "weapons/ric3.wav"}, {0.7f, 0.8f, 0.9f, 2.f}},
    {{"player/axhit2.wav", nullptr, nullptr, nullptr}, {2.f, 0.f, 0.f, 0.f}},
    {{"vr/phys/metal_h1.wav", "vr/phys/metal_h2.wav", "vr/phys/metal_h3.wav", "vr/phys/metal_h4.wav"}, {0.25f, 0.5f, 0.75f, 2.f}},
    {{"vr/phys/soft_m1.wav", "vr/phys/soft_m2.wav", "vr/phys/soft_m3.wav", "vr/phys/soft_m4.wav"}, {0.25f, 0.5f, 0.75f, 2.f}},
    {{"vr/phys/wood_m1.wav", "vr/phys/wood_m2.wav", "vr/phys/wood_m3.wav", "vr/phys/wood_m4.wav"}, {0.25f, 0.5f, 0.75f, 2.f}},
    {{"vr/phys/flesh_m1.wav", "vr/phys/flesh_m2.wav", "vr/phys/flesh_m3.wav", "vr/phys/flesh_m4.wav"}, {0.25f, 0.5f, 0.75f, 2.f}},
    {{"vr/phys/metal_l1.wav", "vr/phys/metal_l2.wav", "vr/phys/metal_l3.wav", "vr/phys/metal_l4.wav"}, {0.25f, 0.5f, 0.75f, 2.f}},
};
static_assert(za::getArraySize(soundSets) == static_cast<int>(Sound::Count));

constexpr int presetBlood = static_cast<int>(particles::Preset::Blood);
constexpr int presetSparks = static_cast<int>(particles::Preset::Sparks);
constexpr int presetWoodDust = static_cast<int>(particles::Preset::WoodDust);

// What the server sends for a hit on a thing (QC's impact(): one particle2 message). Its direction:
// (dir x dirScale + dirAdd + spray) x leafScale, the spray [crandom() x, crandom() y, random() z + lift] drawn from
// QC's random() (in that order) when it has one; its origin: org + dir x orgAlong; its count: amount x countScale,
// clamped to countMin..countMax (as the QC's max(min, min(max, damage x 0.5)) did).
struct Fx
{
    int preset{0}; // a particle2 preset (QC QVR_PARTICLE_PRESET_*: particles::Preset), 0: nothing sent
    float countScale{1.f};
    float countMin{-1e9f};
    float countMax{1e9f};
    float dirScale{1.f};
    float leafScale{1.f};
    glm::vec3 dirAdd{0.f};
    glm::vec3 spray{0.f};
    float lift{0.f};
    float orgAlong{0.f};
};

// One material hit one way. A world surface's hit (the client's, cl_tent.c) adds dust, sparks, chips or haze (counts
// at vr_gunfx_intensity 1) to Quake's own puff; a thing's (the QC's) sends `thing`; the sounds are impactsound's.
struct Row
{
    int dust{0};   // puffs of dust, lit by the place (dustColours)
    int sparks{0}; // hot sparks glancing off
    int chips{0};  // chips of wood, lit by the place
    int haze{0};   // puffs of the thunderbolt's ozone haze
    Decal decal{Decal::None};
    Fx thing;
    Sound sound{Sound::None}; // the hit's own (VR_IMPACT_SOUND)
    Sound layer{Sound::None}; // a blunt blow's second layer (VR_IMPACT_LAYER)
    Sound over{Sound::None};  // over the layer (VR_IMPACT_OVER)
};

constexpr int materials = static_cast<int>(Material::Count);
constexpr int kinds = static_cast<int>(HitKind::Count);
constexpr int clientKinds = static_cast<int>(HitKind::Bolt) + 1; // (the world's hits: bullet, nail, super nail, bolt)

using Table = std::array<std::array<Row, kinds>, materials>;

// The things' effects, as the five QC ladders sent them (TraceAttack, spawn_touchblood, VR_HitBlood, VR_HitNonFlesh,
// VR_HitBloodSplash; and the chainsaw's, a thrown thing's). Where they disagreed, each ladder's own row keeps its
// output (ROUND23.md, "One impact-effect table": the disagreement list).
constexpr Fx sparks(float dirScale)
{
    return {presetSparks, 0.5f, 3.f, 10.f, dirScale};
}
constexpr Fx splinters(float dirScale)
{
    return {presetWoodDust, 0.5f, 2.f, 8.f, dirScale};
}
constexpr Fx blood(float dirScale)
{
    return {presetBlood, 1.f, -1e9f, 1e9f, dirScale, 0.2f}; // (SpawnBlood's own 0.2 last)
}
constexpr Fx grenadeSparks()
{
    return {presetSparks, 0.f, 8.f, 8.f, 0.f, 1.f, {0.f, 0.f, 40.f}}; // (VR_GrenShot_Sparks)
}
constexpr Fx sprayed(Fx f, glm::vec3 spray, float lift)
{
    f.dirScale = 0.f;
    f.spray = spray;
    f.lift = lift;
    return f;
}
constexpr Fx along(Fx f, float orgAlong)
{
    f.orgAlong = orgAlong;
    return f;
}

constexpr Table makeTable()
{
    Table t{};
    auto row = [&t](Material m, HitKind k) -> Row& { return t[static_cast<int>(m)][static_cast<int>(k)]; };
    using M = Material;
    using K = HitKind;
    // The world's surfaces (the client's wall hits): dust, sparks, chips, haze; the hit's own decal.
    const int world[5][4][4] = {
        {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, // None: nothing (sky, liquids, nothing near)
        {{4, 0, 0, 0}, {2, 0, 0, 0}, {4, 0, 0, 0}, {0, 0, 0, 1}}, // Stone
        {{6, 0, 0, 0}, {4, 0, 0, 0}, {5, 0, 0, 0}, {0, 0, 0, 1}}, // Dirt
        {{1, 7, 0, 0}, {0, 5, 0, 0}, {1, 8, 0, 0}, {0, 0, 0, 1}}, // Metal
        {{2, 0, 8, 0}, {1, 0, 4, 0}, {2, 0, 7, 0}, {0, 0, 0, 1}}, // Wood
    };
    for(int m = 0; m < 5; m++)
    {
        for(int k = 0; k < clientKinds; k++)
        {
            Row& r = t[m][k];
            r.dust = world[m][k][0];
            r.sparks = world[m][k][1];
            r.chips = world[m][k][2];
            r.haze = world[m][k][3];
            r.decal = k == static_cast<int>(K::Bolt) ? Decal::None : Decal::TempEntity;
        }
    }
    // (Flesh's wall hits: nothing new, the gore's mist comes with the blood: particles.cpp mist, vr_gore_mist.)
    for(const M fleshy : {M::Flesh, M::Armour})
    {
        row(fleshy, K::Bullet).thing = blood(0.2f);              // TraceAttack: SpawnBlood(org, vel x 0.2)
        row(fleshy, K::Nail).thing = along(blood(1.f), 0.01f);   // spawn_touchblood: at origin + vel x 0.01
        row(fleshy, K::SuperNail).thing = row(fleshy, K::Nail).thing;
        row(fleshy, K::Blow).thing = blood(1.f);                 // VR_HitBlood, SpawnBlood
        row(fleshy, K::Splash).thing = sprayed(blood(1.f), {240.f, 240.f, 120.f}, 100.f); // SpawnBloodSplash
        row(fleshy, K::Saw).thing = blood(60.f);                 // the chainsaw: its (crandom right + up) x 60
        // (Pellet: nothing, VR_HitNonFlesh leaves flesh's blood to its caller.)
        row(fleshy, K::Blow).layer = Sound::FleshLayer;
    }
    row(M::Armour, K::Blow).over = Sound::MetalLight;
    row(M::Metal, K::Bullet).thing = sparks(0.3f);
    row(M::Metal, K::Nail).thing = along(sparks(1.f), 0.01f);
    row(M::Metal, K::SuperNail).thing = row(M::Metal, K::Nail).thing;
    row(M::Metal, K::Pellet).thing = sparks(1.f);
    row(M::Metal, K::Blow).thing = sparks(0.2f);
    row(M::Metal, K::Splash).thing = sprayed(sparks(1.f), {60.f, 60.f, 40.f}, 30.f);
    row(M::Metal, K::Saw).thing = {presetSparks, 0.f, 4.f, 4.f, 0.f, 1.f, {0.f, 0.f, 40.f}};
    for(const K shot : {K::Bullet, K::Nail, K::SuperNail, K::Pellet})
    {
        row(M::Metal, shot).sound = Sound::MetalShot;
    }
    // (A shot's sound plays with its hit, VR_ImpactFx, once a frame; a blow's are the melee's: VR_Melee_HitSoundOn.)
    row(M::Metal, K::Bash).sound = Sound::MetalBlunt;
    row(M::Metal, K::Blade).sound = Sound::MetalBlade;
    row(M::Wood, K::Bullet).thing = splinters(0.2f);
    row(M::Wood, K::Nail).thing = along(splinters(1.f), 0.01f);
    row(M::Wood, K::SuperNail).thing = row(M::Wood, K::Nail).thing;
    row(M::Wood, K::Pellet).thing = splinters(0.7f);
    row(M::Wood, K::Blow).thing = splinters(0.2f);
    row(M::Wood, K::Splash).thing = sprayed(splinters(1.f), {48.f, 48.f, 24.f}, 20.f);
    row(M::Wood, K::Saw).thing = splinters(12.f);
    row(M::Wood, K::Bash).thing = splinters(1.f);
    row(M::Wood, K::Blow).layer = Sound::WoodLayer;
    row(M::Stone, K::Blow).layer = Sound::SoftLayer;
    for(const K k : {K::Bullet, K::Nail, K::SuperNail, K::Pellet, K::Blow, K::Splash})
    {
        row(M::Grenade, k).thing = grenadeSparks(); // (a nail's at its origin: no orgAlong)
        // (Torch: nothing; its hit is VR_WallTorch_KnockOff's embers.)
    }
    // A blunt blow's layer on a torch or a grenade: as on a body (VR_Blunt_HitLayer took them for one).
    row(M::Torch, K::Blow).layer = Sound::FleshLayer;
    row(M::Grenade, K::Blow).layer = Sound::FleshLayer;
    return t;
}

constexpr Table table = makeTable();

constexpr glm::vec3 dustColours[materials] = {{0.f, 0.f, 0.f}, {0.66f, 0.62f, 0.55f}, {0.55f, 0.46f, 0.35f},
    {0.5f, 0.5f, 0.5f}, {0.7f, 0.6f, 0.45f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}};
constexpr const char* materialNames[] = {"none", "stone", "dirt", "metal", "wood", "flesh", "armour", "torch", "grenade"};
static_assert(za::getArraySize(materialNames) == materials);
constexpr const char* kindNames[] = {
    "bullet", "nail", "supernail", "bolt", "pellet", "blow", "blade", "splash", "saw", "bash"};
static_assert(za::getArraySize(kindNames) == kinds);

constexpr int maxLookups = 16;       // surface lookups a frame at most (a shotgun's pellets: 6 to 14)
constexpr float boltHazeEvery = 0.06f; // seconds between a beam's haze puffs

za::FastNonCryptoRng rng{0xC2B2AE3D27D4EB4Full}; // (visual only)
lightcache_t lightCache;            // (the hit's place's light)

struct Frame
{
    double time{-1.0};
    int lookups{0};
};
Frame frame;

struct BeamHaze
{
    int ent{-1};
    double last{-1.0};
};
BeamHaze beams[8];

[[nodiscard]] Material ofTexture(const char* name)
{
    switch(debris::materialOf(name))
    {
        case debris::Material::None: return Material::None;
        case debris::Material::Natural: return Material::Dirt;
        case debris::Material::Metal: return Material::Metal;
        case debris::Material::Wood: return Material::Wood;
        default: return Material::Stone; // fieldstone, masonry, brick, the rest
    }
}

struct Surface
{
    glm::vec3 at{0.f};
    glm::vec3 normal{0.f, 0.f, 1.f};
    Material material{Material::None};
    const char* texture{""};
};

// The world's surface nearest `org` within `reach` (along `prefer` first, then down, then round), its material.
[[nodiscard]] bool surfaceNear(const glm::vec3& org, float reach, const glm::vec3& prefer, Surface& out)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    const glm::vec3 dirs[] = {prefer, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}};
    float best = 2.f;
    for(const glm::vec3& d : dirs)
    {
        if(glm::dot(d, d) < 0.5f)
        {
            continue;
        }
        const trace_t tr = worldtrace::world(org, org + glm::normalize(d) * reach, false);
        if(tr.fraction < best && tr.fraction < 1.f && !tr.startsolid && !tr.allsolid)
        {
            best = tr.fraction;
            out.at = worldtrace::endPos(tr);
            out.normal = worldtrace::normal(tr);
        }
    }
    if(best > 1.f)
    {
        return false;
    }
    const msurface_t* s = physsound::surfaceOnSegment(cl.worldmodel, out.at + out.normal * 2.f, out.at - out.normal * 6.f);
    if(!s || !s->texinfo || s->texinfo->texnum < 0 || s->texinfo->texnum >= cl.worldmodel->numtextures ||
        !cl.worldmodel->textures[s->texinfo->texnum])
    {
        return false;
    }
    out.texture = cl.worldmodel->textures[s->texinfo->texnum]->name;
    out.material = (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) ? Material::None : ofTexture(out.texture);
    return true;
}

// A direction off a surface of normal `n`: mostly out, `spread` sideways.
[[nodiscard]] glm::vec3 outOf(const glm::vec3& n, float spread)
{
    const glm::vec3 r{vrand::range(rng, -1.f, 1.f), vrand::range(rng, -1.f, 1.f), vrand::range(rng, -1.f, 1.f)};
    return glm::normalize(n + (r - n * glm::dot(r, n)) * spread + glm::vec3{0.f, 0.f, 1e-4f});
}

[[nodiscard]] int scaled(int n, float k)
{
    return n <= 0 || k <= 0.f ? 0 : za::max(1, static_cast<int>(static_cast<float>(n) * k + 0.5f));
}

struct Made
{
    int dust{0}, sparks{0}, chips{0}, haze{0};
};

int put(const particles::GunParticle& g)
{
    return particles::gunParticle(g) ? 1 : 0;
}

Made emit(const Row& row, Material mat, const Surface& s, float k, float shade)
{
    Made m;
    const glm::vec3 dust = dustColours[static_cast<int>(mat)];
    for(int i = 0, n = scaled(row.dust, k); i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        const glm::vec3 d = outOf(s.normal, 0.8f);
        g.org = s.at + s.normal * 1.5f + d * vrand::range(rng, 0.f, 2.f);
        g.vel = d * vrand::range(rng, 30.f, 90.f);
        const float v = vrand::range(rng, 0.9f, 1.1f);
        g.color = glm::vec4{dust * (v * shade), vrand::range(rng, 0.55f, 0.75f)};
        g.scale = vrand::range(rng, 3.f, 5.f);
        g.grow = vrand::range(rng, 18.f, 28.f);
        g.life = vrand::range(rng, 0.6f, 1.f);
        g.drag = 4.f;
        g.gravity = 0.04f;
        m.dust += put(g);
    }
    for(int i = 0, n = scaled(row.sparks, k); i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Spark;
        const glm::vec3 d = outOf(s.normal, 1.4f);
        g.org = s.at + s.normal * 0.5f;
        g.vel = d * vrand::range(rng, 180.f, 420.f);
        g.color = glm::vec4{1.f, vrand::range(rng, 0.7f, 0.9f), vrand::range(rng, 0.35f, 0.6f), 1.f};
        g.scale = vrand::range(rng, 0.7f, 1.1f);
        g.life = vrand::range(rng, 0.08f, 0.22f);
        g.drag = 2.f;
        g.gravity = 1.f;
        g.streak = 0.03f;
        m.sparks += put(g);
    }
    for(int i = 0, n = scaled(row.chips, k); i < n; i++)
    {
        particles::GunParticle g;
        g.kind = particles::GunParticle::Chip;
        const glm::vec3 d = outOf(s.normal, 1.f);
        g.org = s.at + s.normal * 1.f;
        g.vel = d * vrand::range(rng, 80.f, 170.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 20.f, 60.f)};
        const float v = vrand::range(rng, 0.8f, 1.1f);
        g.color = glm::vec4{glm::vec3{0.78f, 0.58f, 0.34f} * (v * shade), 1.f};
        g.scale = vrand::range(rng, 1.6f, 2.6f);
        g.life = vrand::range(rng, 0.5f, 0.8f);
        g.drag = 1.f;
        g.gravity = 1.f;
        m.chips += put(g);
    }
    for(int i = 0, n = scaled(row.haze, k); i < n; i++)
    {
        // Ozone: a faint bluish haze, faintly luminous (the arc's light in it), lingering and rising slowly.
        particles::GunParticle g;
        g.kind = particles::GunParticle::Smoke;
        g.org = s.at + s.normal * vrand::range(rng, 2.f, 6.f) + glm::vec3{vrand::range(rng, -3.f, 3.f), vrand::range(rng, -3.f, 3.f), 0.f};
        g.vel = s.normal * vrand::range(rng, 6.f, 14.f) + glm::vec3{0.f, 0.f, vrand::range(rng, 4.f, 9.f)};
        const float v = vrand::range(rng, 0.9f, 1.1f);
        g.color = glm::vec4{0.5f * v, 0.58f * v, 0.82f * v, vrand::range(rng, 0.12f, 0.2f)};
        g.scale = vrand::range(rng, 3.f, 5.f);
        g.grow = vrand::range(rng, 12.f, 20.f);
        g.life = vrand::range(rng, 1.f, 1.6f);
        g.drag = 2.f;
        g.gravity = -0.03f;
        m.haze += put(g);
    }
    return m;
}

[[nodiscard]] bool on(const cvar_t& v)
{
    return v.value != 0.f;
}

[[nodiscard]] bool lookupAllowed()
{
    if(frame.time != cl.time)
    {
        frame.time = cl.time;
        frame.lookups = 0;
    }
    return frame.lookups++ < maxLookups;
}

// (vr_impact_test's material, -1: the surface's own)
int forcedMaterial = -1;

void at(HitKind kind, const glm::vec3& org, const glm::vec3& prefer, float reach)
{
    if(!on(vr_gunfx) || !particles::enabled() || !lookupAllowed())
    {
        return;
    }
    QVR_PROFILE("impacts");
    Surface s;
    if(!surfaceNear(org, reach, prefer, s))
    {
        return;
    }
    if(forcedMaterial >= 0)
    {
        s.material = static_cast<Material>(forcedMaterial);
    }
    const Row& row = table[static_cast<int>(s.material)][static_cast<int>(kind)];
    const float k = za::clamp(vr_gunfx_intensity.value, 0.f, 3.f);
    vec3_t lp{s.at.x + s.normal.x * 2.f, s.at.y + s.normal.y * 2.f, s.at.z + s.normal.z * 2.f};
    const float light = za::clamp(static_cast<float>(R_LightPoint(lp, 0.f, &lightCache)) / 128.f, 0.f, 1.6f);
    // (dust and chips drawn as their colour, shaded once by the place's light: dim in the dark, never black)
    const Made m = emit(row, s.material, s, k, 0.35f + 0.5f * za::min(light, 1.3f));
    if(vr_debug_gunfx.value)
    {
        Con_Printf("gunfx impact %s on %s (%s) dust %d sparks %d chips %d haze %d\n", kindNames[static_cast<int>(kind)],
            materialNames[static_cast<int>(s.material)], s.texture, m.dust, m.sparks, m.chips, m.haze);
    }
}

// vr_impact_test <kind 0..3: the world's hits> [count] [material]: a hit of that kind on what you look at (within 1024 units), as if
// made of `material` (a name of materialNames) if given.
void test_f()
{
    const int kind = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 0, clientKinds - 1) : 0;
    const int count = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 1, 16) : 1;
    forcedMaterial = -1;
    for(int i = 0; i < materials && Cmd_Argc() > 3; i++)
    {
        if(!q_strcasecmp(Cmd_Argv(3), materialNames[i]))
        {
            forcedMaterial = i;
        }
    }
    vec3_t f, r, u;
    AngleVectors(r_refdef.viewangles, f, r, u);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const glm::vec3 fwd{f[0], f[1], f[2]};
    const trace_t tr = worldtrace::world(eye, eye + fwd * 1024.f, false);
    if(tr.fraction >= 1.f)
    {
        Con_Printf("vr_impact_test: nothing within 1024 units\n");
        return;
    }
    const glm::vec3 hitAt = worldtrace::endPos(tr) - fwd * 4.f; // (Quake's puff point: 4 units back off the wall)
    for(int i = 0; i < count; i++)
    {
        frame.lookups = 0;
        at(static_cast<HitKind>(kind), hitAt, fwd, 8.f);
    }
    forcedMaterial = -1;
    Con_Printf("vr_impact_test: %s x%d at %.1f %.1f %.1f\n", kindNames[kind], count, hitAt.x, hitAt.y, hitAt.z);
}

// The row of a material and a kind the QC asked for (out of range: the empty row).
[[nodiscard]] const Row& rowOf(float material, float kind)
{
    const int m = static_cast<int>(material);
    const int k = static_cast<int>(kind);
    return m >= 0 && m < materials && k >= 0 && k < kinds ? table[m][k] : table[0][static_cast<int>(HitKind::Bolt)];
}

// QC's crandom(): 2 x (random() - 0.5).
[[nodiscard]] float crandom()
{
    return 2.f * (PR_RandomNumber() - 0.5f);
}

} // namespace

void PF_impact()
{
    const Fx& f = rowOf(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1)).thing;
    G_FLOAT(OFS_RETURN) = 0.f;
    if(f.preset == 0)
    {
        return;
    }
    const float* o = G_VECTOR(OFS_PARM2);
    const float* d = G_VECTOR(OFS_PARM3);
    const float amount = G_FLOAT(OFS_PARM4);
    const glm::vec3 dir{d[0], d[1], d[2]};
    glm::vec3 v = dir * f.dirScale + f.dirAdd;
    if(f.spray != glm::vec3{0.f})
    {
        // (as QC's [crandom() * x, crandom() * y, random() * z + lift]: x drawn first)
        const float x = crandom() * f.spray.x;
        const float y = crandom() * f.spray.y;
        const float z = PR_RandomNumber() * f.spray.z + f.lift;
        v += glm::vec3{x, y, z};
    }
    v = v * f.leafScale;
    const glm::vec3 org = glm::vec3{o[0], o[1], o[2]} + dir * f.orgAlong;
    const float count = za::max(f.countMin, za::min(f.countMax, amount * f.countScale));
    const float orgF[3] = {org.x, org.y, org.z};
    const float dirF[3] = {v.x, v.y, v.z};
    progs::sendParticle2(orgF, dirF, f.preset, static_cast<int>(count));
    G_FLOAT(OFS_RETURN) = static_cast<float>(f.preset);
}

void PF_impactsound()
{
    const Row& r = rowOf(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
    const int slot = static_cast<int>(G_FLOAT(OFS_PARM2));
    const Sound which = slot == VR_IMPACT_LAYER ? r.layer : (slot == VR_IMPACT_OVER ? r.over : r.sound);
    const SoundSet& set = soundSets[static_cast<int>(which)];
    const char* pick = "";
    if(which != Sound::None)
    {
        const float n = PR_RandomNumber();
        for(int i = 0; i < 4 && set.samples[i]; i++)
        {
            pick = set.samples[i];
            if(n < set.below[i])
            {
                break;
            }
        }
    }
    G_INT(OFS_RETURN) = PR_SetEngineString(pick);
}

const char* materialName(Material m)
{
    return materialNames[za::clamp(static_cast<int>(m), 0, materials - 1)];
}

void hit(HitKind kind, const glm::vec3& org)
{
    if(on(vr_gunfx_impacts))
    {
        at(kind, org, glm::vec3{0.f}, 8.f);
    }
}

void beam(const qmodel_s* model, int ent, const glm::vec3& start, const glm::vec3& end)
{
    if(!model || !on(vr_gunfx_ozone) || strcmp(model->name, "progs/bolt2.mdl") != 0) // (the thunderbolt's beam)
    {
        return;
    }
    BeamHaze* b = &beams[0];
    for(BeamHaze& o : beams)
    {
        if(o.ent == ent)
        {
            b = &o;
            break;
        }
        if(o.last < b->last)
        {
            b = &o;
        }
    }
    if(b->ent == ent && cl.time - b->last < boltHazeEvery && cl.time >= b->last)
    {
        return;
    }
    b->ent = ent;
    b->last = cl.time;
    const glm::vec3 d = end - start;
    const float len = glm::length(d);
    at(HitKind::Bolt, end, len > 1e-3f ? d / len : glm::vec3{0.f}, 10.f);
}

void clear()
{
    frame = Frame{};
    for(BeamHaze& b : beams)
    {
        b = BeamHaze{};
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_impact_test", test_f);
}

} // namespace qvr::impacts

// cl_tent.c: Quake's wall hits (vr_api.h's QVR_DECAL_TE_*: a bullet, a nail, a super nail).
extern "C" void VR_ImpactTempEntity(int what, const float* pos)
{
    using namespace qvr::impacts;
    const glm::vec3 p{pos[0], pos[1], pos[2]};
    switch(what)
    {
        case QVR_DECAL_TE_GUNSHOT: hit(HitKind::Bullet, p); break;
        case QVR_DECAL_TE_SPIKE: hit(HitKind::Nail, p); break;
        case QVR_DECAL_TE_SUPERSPIKE: hit(HitKind::SuperNail, p); break;
        default: break;
    }
}

// PF_sound: a QC sound, printed under vr_debug_impacts ("impfx snd <ent> <channel> <sample> <volume 0-255> <attn>").
extern "C" void VR_DebugImpactSound(int ent, int channel, const char* sample, int volume, float attenuation)
{
    if(qvr::vr_debug_impacts.value)
    {
        Con_Printf("impfx snd %d %d %s %d %.2f\n", ent, channel, sample, volume, attenuation);
    }
}

// cl_tent.c CL_ParseBeam: a beam's ends.
extern "C" void VR_ImpactBeam(const qmodel_s* model, int ent, const float* start, const float* end)
{
    qvr::impacts::beam(model, ent, {start[0], start[1], start[2]}, {end[0], end[1], end[2]});
}
