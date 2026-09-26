// vr_particles.hpp -- Quake VR's own particles (the old engine's r_part.cpp, ported): textured,
// coloured, spinning and fading sprites for the QC's particle2 presets (QVR_PARTICLE_PRESET_*:
// bullet puffs, blood, explosions, lightning, smoke, sparks, gun smoke, teleports, pickup and
// force grab sparkles, lava spikes, liquid splashes). They also stand in for Quake's own effects (r_part.c): wall
// hits, explosions, the trails of rockets, lava balls, grenades, gibs and the scrag's, hell
// knight's and vore's projectiles (fire and smoke; glowing ribbons with sparkles in the colours of
// their lights), the tarbaby's explosion, lava splashes and teleports. The textures are
// quakevr/textures/particle_*.tga (and a generated disc and glow), premultiplied and mipmapped;
// glows are added to the scene, the rest alpha blended, in one draw through vr_gfx in the scene's
// translucent pass (VR_DrawSceneTranslucent). With vr_soft_particles they fade out close in front of the opaque scene
// (its distances: vr_water.hpp), as do the sprites (explosions, bubbles), left to that pass by r_sprite.c.
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
    // (4 a bullet, 6-15 a hand or a thrown thing, 20-50 a body). Drops streaked along their motion
    // thrown up in a crown and a jet, fine spray, clouds of spray and mist, foam and rings lying on
    // the surface (on its waves), in the liquid's colour (lava glows and throws embers);
    // vr_water_splash scales how many (0 off), vr_water_splash_size how big, _ring_speed and
    // _ring_size the rings. Also the ripples on the liquid (water::addRipple), with or without
    // Quake VR's particles (as an explosion under a surface does).
    Splash
};

// Spawns a preset's particles (count scaled by vr_particle_mult); false if they are off, for the
// caller to fall back on Quake's effects.
bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count);

// Whether they are on and drawn (the VR protocol, vr_particles): false for Quake's effects.
[[nodiscard]] bool enabled();

// Removes them all (a new map, a disconnect).
void clear();

// The gore's drops (vr_gore.cpp): one falling straight down from `org` for `fall` seconds (Quake's
// gravity) until it reaches `floorZ`, `size` about as big as a particle's scale, in `color` (lit
// already); and `count` tiny specks thrown up off `normal` where one lands.
void bloodDrip(const glm::vec3& org, float fall, float floorZ, float size, const glm::vec3& color);
void bloodSpecks(const glm::vec3& org, const glm::vec3& normal, int count, const glm::vec3& color);

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
