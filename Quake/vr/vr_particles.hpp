// vr_particles.hpp -- Quake VR's own particles (the old engine's r_part.cpp, ported): textured,
// coloured, spinning and fading sprites for the QC's particle2 presets (QVR_PARTICLE_PRESET_*:
// bullet puffs, blood, explosions, lightning, smoke, sparks, gun smoke, teleports, pickup and
// force grab sparkles, lava spikes). They also stand in for Quake's own effects (r_part.c): wall
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
    BloodTrail      // vr_decals.cpp: behind a flying gib (`dir`: the way it goes)
};

// Spawns a preset's particles (count scaled by vr_particle_mult); false if they are off, for the
// caller to fall back on Quake's effects.
bool spawn(const glm::vec3& org, const glm::vec3& dir, Preset preset, int count);

// Whether they are on and drawn (the VR protocol, vr_particles): false for Quake's effects.
[[nodiscard]] bool enabled();

// Removes them all (a new map, a disconnect).
void clear();

} // namespace qvr::particles
