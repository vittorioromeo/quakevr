// vr_physics.hpp -- server-side Quake VR physics shared with other VR sources.

#pragma once

#include "vr_engine.hpp"

namespace qvr::physics
{

// QC's FL_FORCEGRABBABLE (QC/defs.qc): boxes, gibs, thrown weapons.
inline constexpr int FL_FORCEGRABBABLE = 1 << 15;

// Rigid bodies and held objects (vr_rigid.cpp).

// Whether `p` is inside `ent`'s drawn box (turned with it), grown by `margin`: what a hand must be
// in to take hold of an object (Quake's boxes do not turn, and items' are widened).
[[nodiscard]] bool pointInModelBox(edict_t* ent, const glm::vec3& p, float margin);

// The middle of `ent`'s drawn model (turned with it), in the world: QC's modelcentre.
[[nodiscard]] glm::vec3 modelCentre(edict_t* ent);

// QC's carryangles: a held object's angles (into `out`), turning with the hand at `handAngles`.
// At the grip (`grab`) its turn relative to the hand is kept, and its angles are unchanged.
void carryAngles(edict_t* ent, const float* handAngles, bool grab, float* out);

// Forgets rigid bodies' and held objects' state (a new server).
void resetRigidBodies();

// Water splashes and sounds (vr_physics.cpp).

// Before a toss, bounce or missile's move (vr_rigid.cpp): if it goes into a liquid on the way, its
// splash, at once (it may hit the bottom in the same move).
void predictWaterEntry(edict_t* ent);

// Where the segment `from` -> `to` first goes into a liquid (water, slime, lava) from the open, or
// out of one into it: the surface's point in `at`; false if it crosses none (QC's liquidentry).
[[nodiscard]] bool liquidEntry(const glm::vec3& from, const glm::vec3& to, glm::vec3& at);

// The sound a splash makes (QC's QVR_SPLASH_*).
enum class SplashSound : int
{
    None,  // its maker plays its own (the player going in)
    Shot,  // a small plip
    Thing  // small or big by `strength`
};

// A splash on a liquid's surface at `at`, something going `dir` into it `strength` hard (particles
// Preset::Splash's count: 4 a shot, 6-15 a hand or a thrown thing, 20-50 a body), for every
// client, and its sound (vr_water_sounds) (QC's watersplash).
void waterSplash(const glm::vec3& at, const glm::vec3& dir, float strength, SplashSound sound);

} // namespace qvr::physics
