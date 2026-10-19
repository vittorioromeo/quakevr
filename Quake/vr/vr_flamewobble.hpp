// vr_flamewobble.hpp -- flames stirred by a hand passing them (Vittorio, 2026-10-19: "passing a hand quickly through or
// near a torch or another flame makes the flame lean in the direction the hand moved, as if moving the air").
//
// Each frame the tracked hands' speeds in the room (the play space's own motion left out: walking past a torch does not
// stir it) are read. A hand faster than vr_flame_wobble_speed (m/s) pushes the flames near it: a lean (sideways units
// a unit up) on a damped spring, so the flame swings the hand's way, overshoots a little back and settles in about a
// second. The push grows with the hand's speed and its nearness (none past a reach that grows with the flame's size),
// times vr_flame_wobble_strength; only the flames near a fast hand are looked at, and only the ones swinging are kept.
//
// Leaned: the map's flame models (progs/flame2.mdl: braziers, the large and small flames, burning bodies' flames:
// sheared over their base in VR_AliasPreTransform), the wall torches' fires (vr_walltorch.cpp: their flame tilted as by
// the torch's own motion), the pyro's pilot light (its particles carried along); and the fire and smoke particles
// near the fast hand are carried with the air it moves (particles::gust). id's whole torch model (progs/flame.mdl,
// vr_walltorch 0: stick and flame in one) and candles are not leaned (their stick would lean too).

#pragma once

#include "vr_engine.hpp"

#include <glm/glm.hpp>

#include <cstdint>

namespace qvr::flamewobble
{

// Once a frame, as the view is set up (the hands placed): the hands' speeds, the pushes, the springs stepped.
void frame();

// A flame drawn by its own code (a wall torch's fire, the pilot light): its key (unique among these, stable from frame to
// frame), its foot and height (world units, up the world's z), the hand carrying it (its own motion is not a push; -1
// none). Registered for the next frame's pushes; returns its lean now (sideways units a unit up, world x y; zero when
// still or with vr_flame_wobble 0).
[[nodiscard]] glm::vec2 source(std::uint32_t key, const glm::vec3& foot, float height, int carrier);

// VR_AliasPreTransform: a map flame model swinging, sheared over its base (world space). Nothing for the others.
void leanModel(const entity_t* e, float matrix[16]);

// A new map, a disconnect.
void clear();

// vr_flame_wobble_info (Debug > Info): the hands' speeds, the swinging flames, the nearest flames to you.
void registerCommands();

} // namespace qvr::flamewobble
