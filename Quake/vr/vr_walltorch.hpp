// vr_walltorch.hpp -- wall torches taken off their walls: the engine's side (QC vr_walltorch.qc; docs/vr-port/ROUND21.md,
// "Wall torches you can take").
//
// A taken torch is the wall torch's own entity, drawn with our stick (progs/vrtorch.mdl, make_walltorch.py); the
// server sends how big its fire is as the stick's frame (0 out, 1..16 sixteenths). The client draws the fire over the
// stick's head: id's wall torch's own flame (progs/flame.mdl without its stick, made from that file as it loads:
// progs/vrtorch_fire.mdl), upright whichever way the stick points, leaning away from the way it moves, shrinking as it
// dies. Its light is the wall torch's (vr_emissive.cpp, VR_TorchLights), at the flame, dimming with it. The crackle
// of the wall it hung on (a static sound) is silenced once it has gone, and the torch's own crackle follows it.

#pragma once

#include "vr_engine.hpp"

namespace qvr::walltorch
{

// Client: where client entity `ent`'s fire is this frame (the wall torch's light's place: its flame's middle) and how
// much of it is left (0..1), if it is a lit taken torch. For VR_TorchLights, after VR_WallTorchFlames.
[[nodiscard]] bool fire(int ent, glm::vec3& at, float& level);

// The game directory changed: the models' slots are reused.
void onGameDirChanged();

// Server, after a saved game is loaded (VR_OnLoadGame): the map's wall torches that the save doesn't have (a save made
// while they were static entities, before this change, or with vr_walltorch 0) are spawned again.
void restoreAfterLoad();

} // namespace qvr::walltorch
