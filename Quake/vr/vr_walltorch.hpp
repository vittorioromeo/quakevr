// vr_walltorch.hpp -- wall torches taken off their walls: the engine's side (QC vr_walltorch.qc; docs/vr-port/ROUND21.md,
// "Wall torches you can take").
//
// A wall torch (on its wall and taken) is drawn with our stick (progs/vrtorch.mdl, make_walltorch.py, its skin id's
// torch's, copied in from progs/flame.mdl as it loads); the server sends how big its fire is as the stick's frame (0 out,
// 1..16 sixteenths; 17: on its wall, a full fire). The client draws the fire over the
// stick's head: id's wall torch's own flame (progs/flame.mdl without its stick, made from that file as it loads:
// progs/vrtorch_fire.mdl), upright whichever way the stick points, leaning away from the way it moves, shrinking as it
// dies. Its light is the wall torch's (vr_emissive.cpp, VR_TorchLights), at the flame, dimming with it. The crackle
// of the wall it hung on (a static sound) is silenced once it has gone, and the torch's own crackle follows it.
// The flame's foot is the head's highest point (ROUND21.md, "Torch flames: upright from the head's top, swings, smoke,
// burning you"): in the pit standing up, on the head lying level, upside down three flames round the stick where the
// head meets it, rising along it (bigger, brighter, smoking more, dripping fire). Swung, it leans and trails back,
// flattening when fast. Every lit torch smokes (on its wall too, id's static ones with vr_walltorch 0).

#pragma once

#include "vr_engine.hpp"

namespace qvr::walltorch
{

// Client: where client entity `ent`'s fire is this frame (the wall torch's light's place: its flame's middle) and how
// much of it is left (0..1), if it is a lit taken torch. For VR_TorchLights, after VR_WallTorchFlames.
[[nodiscard]] bool fire(int ent, glm::vec3& at, float& level);

// Client, as an alias model is drawn (VR_AliasPreTransform): a torch's flame's own scale this frame (flattened and
// stretched back as it is swung), about its origin in its axes. False: not one of them.
[[nodiscard]] bool stretch(const entity_t* e, glm::vec3& k);

// Server (single player: the client's last frame, QC's torchflametouch): what of you the flame of the torch you hold
// (client entity `ent`) touches (selfcollide::FlameTouchPart bits, the deepest contact's bit (but the holding hand's)
// times 256 added; 0: nothing, not held, or neither vr_burn_self nor
// vr_burn_drop on), and where (on the body, the way out of it). Its flame's foot is the head's highest point; upside
// down it burns along the stick, so it reaches the hand holding it.
[[nodiscard]] unsigned flameOnYou(int ent, glm::vec3& at, glm::vec3& out);

// Client: whether `e` is a wall torch on its wall drawn as our stick (its light is the wall torch's, as id's is).
[[nodiscard]] bool onWall(const entity_t& e);

// The game directory changed: the models' slots are reused.
void onGameDirChanged();

// Server, after a saved game is loaded (VR_OnLoadGame): the map's wall torches that the save doesn't have (a save made
// while they were static entities, before this change, or with vr_walltorch 0) are spawned again.
void restoreAfterLoad();

} // namespace qvr::walltorch
