#pragma once

// vr_particles_internal.hpp -- what vr_particles.cpp (see vr_particles.hpp) shares with its parts: the emitters
// (vr_particles_blood.cpp: blood, smoke, sparks; vr_particles_trails.cpp: trails, liquids, fire, electric, embers,
// shells) and the reports (vr_particles_diag.cpp). Internal: included by those alone. What was the file's
// anonymous namespace is particles::detail: its types and small helpers here, its variables declared here and
// defined in vr_particles.cpp where they stood, its functions declared here and defined in the file that owns them.

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
#include <string.h>

namespace qvr::particles::detail
{

// Colour ramps (palette indices).
inline constexpr int ramp1[8] = {111, 112, 107, 105, 103, 101, 99, 97}; // gold, brown, peach
inline constexpr int ramp2[8] = {111, 110, 109, 108, 107, 106, 104, 102}; // gold, brown

enum Type : za::U8
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
enum Cell : za::U8
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
    CellDrop,  // a drop of liquid (generated): a clear ball with a bright rim and a glint, drawn streaked
    CellSpray, // a cloud of fine droplets in a haze (generated)
    CellFoam,  // a patch of bubbly foam (generated), lying on a liquid
    CellFire, // generated wispy flame tongue; emission sources keep their low-poly flame models
    CellCount
};

// Whether the place's light falls on a particle (vr_particle_light): Lit ones take the lightmap where they are and the
// dynamic lights round them (blood, smoke, dust, chips, water); Emissive ones are light themselves and stay as bright
// in the dark (fire, explosions' fireballs and their ramps, sparks, glows, lightning, lava, tracers). Auto: worked out
// from what it is (classify) the first frame it is drawn; a preset sets it where its look says otherwise.
enum class Lighting : za::U8
{
    Auto,
    Lit,
    Emissive,
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
    float streak{0.f};  // stretched along its motion as seen, by this many seconds of it (drops; 0: a square quad)
    int liquid{0};      // a splash's (CONTENTS_*; 0 none): its floor (a drop's, and a flat one's: the surface) rises and
    float rise{0.f};    // sinks with the waves and ripples on it (water::surfaceRise), by `rise` now
    Type type{Static};
    Cell cell{CellCircle};
    bool spinBack{false};
    bool additive{false}; // glows: added to the scene (else alpha blended)
    bool flat{false};     // lying flat (a ripple on a liquid), not facing the view
    bool plink{false};    // a drop that leaves a little ring where it falls back into its liquid (at `floor`)
    // The cosine and sine of `angle` as of `csAngle` (buildInstances, buildLying): worked out again only when it has
    // turned since (most particles never turn).
    glm::vec2 cs{1.f, 0.f};
    float csAngle{ZA_FLOAT_NAN};
    // Lit (vr_particle_light): the lightmap's light where it was last traced (lightCache.pos; again once it has moved
    // 8 units), read again every few frames for the light styles' flicker; 0..255, 128 Quake's full light.
    lightcache_t lightCache{};
    za::U8 lightmap[3]{128, 128, 128};
    bool lightKnown{false};
    Lighting lighting{Lighting::Auto};
};

inline constexpr za::SizeT maxParticles = 32768;

extern za::Vector<Particle> pool; // reserved to maxParticles at startup (init): it never reallocates in play
extern double lastRun;
extern za::FastNonCryptoRng rng;

[[nodiscard]] inline int rndi(int lo, int hiExclusive)
{
    return rng.getI(lo, hiExclusive - 1);
}

[[nodiscard]] inline float rndAngle()
{
    return vrand::range(rng, 0.f, 2.f * 3.14159265f);
}

[[nodiscard]] inline glm::vec3 paletteColor(int index)
{
    const auto* c = reinterpret_cast<const unsigned char*>(&d_8to24table[index & 255]);
    return {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f};
}

[[nodiscard]] inline glm::vec3 gravity(float mult)
{
    return {0.f, 0.f, -sv_gravity.value * mult};
}

// Makes `count` particles (times vr_particle_mult), each set up by `f(p)`.
template <typename F>
inline void make(float count, F&& f, bool multiply = true)
{
    const int n = static_cast<int>(count * (multiply ? za::max(0.f, vr_particle_mult.value) : 1.f));
    for(int i = 0; i < n && pool.size() < maxParticles; i++)
    {
        Particle p;
        p.angle = rndAngle();
        f(p, i);
        pool.pushBack(p);
    }
}

// Lit particles (vr_particle_light): the place's light on those that are not light themselves (Lighting).
[[nodiscard]] inline bool lightOn()
{
    return vr_particle_light.value != 0.f;
}

inline void setColor(Particle& p, int index, float alpha255)
{
    p.color = glm::vec4{paletteColor(index), alpha255 / 255.f};
}

// How many of a trail's particles for `length` units of it, one per `spacing` on average (rounded
// at random: a short segment each frame still gets its share).
[[nodiscard]] inline float perLength(float length, float spacing)
{
    return za::floor(length / spacing + vrand::range(rng, 0.f, 1.f));
}

[[nodiscard]] inline glm::vec3 inBox(float half)
{
    return {vrand::range(rng, -half, half), vrand::range(rng, -half, half), vrand::range(rng, -half, half)};
}

// A random direction.
[[nodiscard]] inline glm::vec3 onSphere()
{
    const float z = vrand::range(rng, -1.f, 1.f);
    const float a = rndAngle();
    const float r = za::sqrt(za::max(0.f, 1.f - z * z));
    return {r * za::cos(a), r * za::sin(a), z};
}

// vr_particles_trails.cpp's trail colours
inline const glm::vec3 scragGreen{0.45f, 1.f, 0.2f};
inline const glm::vec3 knightOrange{1.f, 0.52f, 0.16f};
inline const glm::vec3 vorePurple{0.95f, 0.4f, 0.9f};
extern glm::vec3 lastMistOrg; // the last clouds' place and time
extern double lastMistTime;

// vr_particles.cpp
void explosion(const glm::vec3& org);
bool ensureAtlas();

// vr_particles_blood.cpp, vr_particles_trails.cpp
void soak(Particle& p, float lifeMult = 1.f); // (vr_particles_blood.cpp)
void blood(const glm::vec3& org, const glm::vec3& dir, int count);
void bloodTrail(const glm::vec3& org, const glm::vec3& dir, int count);
void lightning(const glm::vec3& org, int count);
void smoke(const glm::vec3& org, int count, bool big);
void steam(const glm::vec3& org, int count);
void sparks(const glm::vec3& org, int count, int lo, int hi, float scale, double life);
void gunSmoke(const glm::vec3& org, int count);
void sparkles(const glm::vec3& org, int count, int lo, int hi, float alphaLo, float alphaHi, double life, float scale, float grav, float spread, float speed, float upLo, float upHi);
void explosion2(const glm::vec3& org, int colorStart, int colorLength);
[[nodiscard]] glm::vec3 fireColor();
void flames(const glm::vec3& from, const glm::vec3& to, float spacing, float scale, const glm::vec3& tint);
void embers(const glm::vec3& from, const glm::vec3& to, float spacing, const glm::vec3& color, float speed, float grav);
void smokeTrail(const glm::vec3& from, const glm::vec3& to, float spacing, float grey, float alpha, float scale);
void woodDust(const glm::vec3& org, const glm::vec3& dir, int count);
void sparkleTrail(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color, float glowScale);
void rocketTrail(const glm::vec3& from, const glm::vec3& to, int type, float size);
void magicImpact(const glm::vec3& org, const glm::vec3& color, int count);
void blobExplosion(const glm::vec3& org);
void lavaSplash(const glm::vec3& org);
void teleportSplash(const glm::vec3& org);
 // The client's map's contents at `p`: a liquid's (currents are water), or 0 for anything else. [[nodiscard]] int liquidAt(const glm::vec3& p);
[[nodiscard]] int liquidUnder(const glm::vec3& org);
[[nodiscard]] bool surfaceAbove(const glm::vec3& p, float range, glm::vec3& out);
[[nodiscard]] float shadeAt(const glm::vec3& org);
void mist(const glm::vec3& org, const glm::vec3& dir, int count, bool wet = false); // the gore's blood mist
void splash(const glm::vec3& org, const glm::vec3& dir, int count);
[[nodiscard]] int underwaterExplosion(const glm::vec3& org, glm::vec3& surface);
[[nodiscard]] float someOf(float n);
void adStreamFire(const glm::vec3& org, const glm::vec3& dir, int count, bool poison);
void adStreamTongues(const glm::vec3& org, const glm::vec3& vel, int count, bool poison);
void poisonPuff(const glm::vec3& org, const glm::vec3& half, int count);
[[nodiscard]] glm::vec3 inDisc(float radius);
void tankFire(const glm::vec3& org, const glm::vec3& dir, int count);
void tankSmoke(const glm::vec3& org, const glm::vec3& dir, int count);
void tankCloud(const glm::vec3& org, const glm::vec3& dir, int count);
void electricArcs(const glm::vec3& org, float count, float reach, float size);
void electricSparks(const glm::vec3& org, const glm::vec3& dir, int count);
void electricTrail(const glm::vec3& from, const glm::vec3& to);
[[nodiscard]] bool adElectricBolt(const qmodel_t* m);

} // namespace qvr::particles::detail

namespace qvr::particles
{

// vr_particles.cpp's state that the reports (vr_particles_diag.cpp) read
inline constexpr float kShoreReach = 640.f;   // units: none further (the shader's foam fades out from 600)
inline constexpr float kShoreNear = 320.f;    // ... fewer past this
inline constexpr float kShoreRelist = 64.f;   // the near list made again once the eye has moved this far
inline constexpr float kShoreRate = 0.15f;    // particles a second for each unit of shore at 1 (15 for 100 units)
inline constexpr float kShoreMaxLive = 600.f; // about this many alive at most

struct ShoreNear
{
    za::Vector<za::U32> list; // indices into water::shoreSegments
    glm::vec3 at{1e9f};       // where the eye was as it was made
    unsigned generation = ~0u;
    double last = -1.0;       // cl.time of the last spawn (once a frame)
    int spawned = 0;          // the last frame's (vr_water_foam_info)
    float expectedLive = 0.f; // ... how many would be alive at the rate asked, before the cap
    double costMs = 0.0;      // ... and its cost
};
extern ShoreNear shoreNear;

// The last frame's numbers (vr_particle_light_report).
struct LightStats
{
    int lit{0}, emissive{0}, traces{0}, shared{0}, refreshed{0}, stale{0}, lights{0};
    za::SizeT reached{0}; // a dynamic light that reaches a lit particle, counted once for each such pair
    double ms{0.0}; // lightParticles' time
};
extern LightStats lightStats;
extern za::Vector<glm::vec3> lightOf;
extern za::SizeT lightCount;
extern double lightMsSum;
extern int lightFrames;
extern long long lightTraceSum;
extern long long lightLitSum;
extern za::U64 wasteTotals[gfx::particleWasteSlots * 3];
extern bool wasteCounted;
void drainWaste();

// vr_particles_diag.cpp
void mistTest_f();
void bloodSprayTest_f();
void shoreFoamInfo_f();
void lightReport_f();
void wasteReport_f();

} // namespace qvr::particles
