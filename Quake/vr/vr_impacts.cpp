// vr_impacts.cpp -- see vr_impacts.hpp.

#include "vr_impacts.hpp"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_engine.hpp"
#include "vr_particles.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.hpp"
#include "vr_random.hpp"
#include "vr_trace.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

#include <stdlib.h>
#include <string.h>


namespace qvr::impacts
{
namespace
{

// The hit's own decal and sound (cl_tent.c: VR_DecalTempEntity, the tink and ricochets) for now: the columns the
// material ladders' marks and sounds move into (design_review.md proposal 4).
enum class Decal : int
{
    Own
};
enum class Sound : int
{
    Own
};

// What a hit of a kind on a material throws up (counts at vr_gunfx_intensity 1).
struct Row
{
    int dust;   // puffs of dust, lit by the place (dustColours)
    int sparks; // hot sparks glancing off
    int chips;  // chips of wood, lit by the place
    int haze;   // puffs of the thunderbolt's ozone haze
    Decal decal;
    Sound sound;
};

constexpr int materials = static_cast<int>(Material::Count);
constexpr int kinds = static_cast<int>(HitKind::Count);

// [material][kind]: Bullet, Nail, SuperNail, Bolt.
constexpr Row table[materials][kinds] = {
    // None: nothing (sky, liquids, nothing near)
    {{0, 0, 0, 0, Decal::Own, Sound::Own}, {0, 0, 0, 0, Decal::Own, Sound::Own}, {0, 0, 0, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 0, Decal::Own, Sound::Own}},
    // Stone
    {{4, 0, 0, 0, Decal::Own, Sound::Own}, {2, 0, 0, 0, Decal::Own, Sound::Own}, {4, 0, 0, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 1, Decal::Own, Sound::Own}},
    // Dirt
    {{6, 0, 0, 0, Decal::Own, Sound::Own}, {4, 0, 0, 0, Decal::Own, Sound::Own}, {5, 0, 0, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 1, Decal::Own, Sound::Own}},
    // Metal
    {{1, 7, 0, 0, Decal::Own, Sound::Own}, {0, 5, 0, 0, Decal::Own, Sound::Own}, {1, 8, 0, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 1, Decal::Own, Sound::Own}},
    // Wood
    {{2, 0, 8, 0, Decal::Own, Sound::Own}, {1, 0, 4, 0, Decal::Own, Sound::Own}, {2, 0, 7, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 1, Decal::Own, Sound::Own}},
    // Flesh: nothing new (the gore's mist comes with the blood: particles.cpp mist, vr_gore_mist)
    {{0, 0, 0, 0, Decal::Own, Sound::Own}, {0, 0, 0, 0, Decal::Own, Sound::Own}, {0, 0, 0, 0, Decal::Own, Sound::Own},
        {0, 0, 0, 0, Decal::Own, Sound::Own}},
};

constexpr glm::vec3 dustColours[materials] = {
    {0.f, 0.f, 0.f}, {0.66f, 0.62f, 0.55f}, {0.55f, 0.46f, 0.35f}, {0.5f, 0.5f, 0.5f}, {0.7f, 0.6f, 0.45f}, {0.f, 0.f, 0.f}};
constexpr const char* materialNames[] = {"none", "stone", "dirt", "metal", "wood", "flesh"};
static_assert(za::getArraySize(materialNames) == materials);
constexpr const char* kindNames[] = {"bullet", "nail", "supernail", "bolt"};
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

// vr_impact_test <kind 0..3> [count] [material]: a hit of that kind on what you look at (within 1024 units), as if
// made of `material` (a name of materialNames) if given.
void test_f()
{
    const int kind = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 0, kinds - 1) : 0;
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

} // namespace

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
