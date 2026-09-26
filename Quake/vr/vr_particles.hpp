// vr_particles.hpp -- Quake VR's own particles (the old engine's r_part.cpp, ported): textured,
// coloured, spinning and fading sprites for the QC's particle2 presets (QVR_PARTICLE_PRESET_*:
// bullet puffs, blood, explosions, lightning, smoke, sparks, gun smoke, teleports, pickup and
// force grab sparkles, lava spikes, liquid splashes). They also stand in for Quake's own effects (r_part.c): wall
// hits, explosions, the trails of rockets, lava balls, grenades, gibs and the scrag's, hell
// knight's and vore's projectiles (fire and smoke; glowing ribbons with sparkles in the colours of
// their lights), the tarbaby's explosion, lava splashes and teleports. The textures are
// quakevr/textures/particle_*.tga (and a generated disc and glow), premultiplied and mipmapped;
// glows are added to the scene, the rest alpha blended, in one draw through vr_gfx in the scene's
// translucent pass (VR_DrawSceneTranslucent).
//
// vr_particles 0 falls back to Quake's own particle effects.

#pragma once

#include <glm/glm.hpp>

namespace qvr::particles
{

// The QC's particle2 presets (QC/vr_defs.qc QVR_PARTICLE_PRESET_*), and the engine's own.
enum class Preset : int
{
    BulletPuff,
    Blood,
    Explosion,
    Lightning,
    Smoke,
    Sparks,
    GunSmoke,
    Teleport,
    GunPickup,
    GunForceGrab,
    LavaSpike,
    BigSmoke,
    ForceGrabTrail, // vr_fgfx.cpp: behind an object flying to the hand
    BloodTrail,     // vr_decals.cpp: behind a flying gib (`dir`: the way it goes)
    // Something hitting water, slime or lava (sent by the server's water splashes, vr_physics.cpp, and
    // QC's watersplash builtin): `org` on the surface, `dir` the way it went in, `count` how hard
    // (4 a bullet, 6-15 a hand or a thrown thing, 20-50 a body). Drops thrown up and out in a crown,
    // foam and rings spreading on the surface, in the liquid's colour (lava glows and throws
    // embers); vr_water_splash scales them (0 off).
    Splash
};

// Spawns a preset's particles (count scaled by vr_particle_mult); false if they are off, for the
// caller to fall back on Quake's effects.
bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count);

// Whether they are on and drawn (the VR protocol, vr_particles): false for Quake's effects.
[[nodiscard]] bool enabled();

// Removes them all (a new map, a disconnect).
void clear();

// Spent casings (vr_shells.cpp): a faint puff of smoke and a few tiny sparks where one is thrown
// out going `dir` (`smoke` how much: a hot breech smokes more), and the thin wisp one trails from
// `from` to `to` as it flies (`strength` 0..1, fading as it cools). Nothing with vr_particles 0.
void shellEject(const glm::vec3& org, const glm::vec3& dir, float smoke, int sparks);
void shellTrail(const glm::vec3& from, const glm::vec3& to, float strength);

// A lava nail's streak (vr_emissive.cpp) from `from` to `to`: a hot, short-lived glowing core and a
// few embers falling off it. Nothing with vr_particles 0.
void lavaNailTrail(const glm::vec3& from, const glm::vec3& to);

// Live particles (vr_memstats).
[[nodiscard]] int liveCount();

} // namespace qvr::particles
