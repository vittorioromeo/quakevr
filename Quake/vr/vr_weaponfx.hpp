// vr_weaponfx.hpp -- programmatic weapon effects (docs/vr-port/ROUND21.md, "Weapon effects: recoil, muzzle flash,
// tracers"), for weapons whose models don't have them, each weapon's own settings in Weapon Offsets > Effects
// (vr_wofs_fx_*), the global ones on Weapons > Weapon Effects:
// - Recoil: each shot kicks the drawn weapon (and the hand on it) back and tips its muzzle up, easing back to rest in
//   the weapon's Recoil Time. Looks only, as a parried blow's knock and the tired arms' shake (vr_view.cpp puts the
//   muzzle back on the steady weapon): the aim and the shots stay where the controller is.
// - Muzzle flash: a moment of the shotgun's own flash (progs/vr_muzzleflash.mdl, Misc/quakevr/make_muzzleflash.py) at
//   the weapon's muzzle anchor, turned as the gun is, every frame; gone as soon as the gun leaves the hand that fired it
//   (dropped, holstered, thrown, passed to the other hand). The grunts' guns flash at their muzzles too (soldier.mdl's
//   gun: a rigid piece of its vertices, its front ring and its rear).
// - Tracers: a hitscan pellet or round (the shotguns, the burst rifle, the grunts') may draw a streak of light flying
//   from the muzzle to what it hit (vr_tracer_*: speed, length, thickness, colour, the chance a pellet shows one; each
//   weapon's multipliers and colour), depth tested in the scene's translucent pass.
// The QC says when (weaponfired(), tracer(): QVR_SVC_FIRED, QVR_SVC_TRACER); where from comes from what the client draws.
// Tests: vr_debug_weaponfx 1 prints each shot's effects, 2 also the recoil every frame; vr_weaponfx_test [hand] fires
// the held weapon's effects (no shot: Debug > Tests).

#pragma once

#include "vr_view.hpp"

namespace qvr::weaponfx
{

// The grunts' muzzle flashes drawn at once, at most.
inline constexpr int maxEnemyFlashes = 8;

// QVR_SVC_FIRED and QVR_SVC_TRACER.
void parseFired();
void parseTracer();

// The drawn hand `hand`'s recoil this frame (world units and degrees, as painknock::offset), `handAngles` the hand's
// drawn angles (which way back is). Zero without one.
void recoilOffset(int hand, const glm::vec3& handAngles, glm::vec3& pos, glm::vec3& angles);

// Once a frame, from the view's setup, the weapons placed: the muzzle flashes put on the drawn weapons' muzzles (hidden
// when none), the grunts' on theirs; the tracers that ended forgotten.
void frame(const view::ViewEntity (&weapons)[2], view::ViewEntity (&flashes)[2],
    view::ViewEntity (&enemyFlashes)[maxEnemyFlashes]);

// The scene's translucent pass (each eye): the tracers.
void drawTranslucent();

// A map's load (VR_NewMap): the flash's model loaded, not at the first shot.
void prepare();

// Everything forgotten (a new map, a disconnect).
void clear();

// vr_weaponfx_test.
void registerCommands();

} // namespace qvr::weaponfx
