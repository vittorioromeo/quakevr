// vr_walltorch.hpp -- wall torches taken off their walls: the engine's side (QC vr_walltorch.qc; docs/vr-port/ROUND21.md,
// "Wall torches you can take").
//
// A wall torch (on its wall and taken) is drawn with our stick (progs/vrtorch.mdl, make_walltorch.py, its skin id's
// torch's, copied in from progs/flame.mdl as it loads); the server sends how big its fire is as the stick's frame (0 out,
// 1..16 sixteenths; 17: on its wall, a full fire). The client draws the fire over the
// stick's head: id's wall torch's own flame (progs/flame.mdl without its stick, made from that file as it loads:
// progs/vrtorch_fire.mdl), attached to the torch's head and rising against gravity. Past horizontal the height
// smoothly shrinks to a short visible inverted flame (vr_walltorch_inv_size); particles rise in world space at every
// orientation. Swing lean and flattening remain. Existing fuel, extinguishing, relighting, crackle, light, smoke and
// burning-contact behavior remain; the old three-flame ring is gone.

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
// vr_burn_drop on), and where (on the body, the way out of it). Uses the single flame's transformed axis and height.
[[nodiscard]] unsigned flameOnYou(int ent, glm::vec3& at, glm::vec3& out);

// Client: whether `e` is a wall torch on its wall drawn as our stick (its light is the wall torch's, as id's is).
[[nodiscard]] bool onWall(const entity_t& e);

// The game directory changed: the models' slots are reused.
void onGameDirChanged();
// The map's load (VR_NewMap's prewarm): the flame's model made now if the map has torches (drawn from the first frame:
// it was made then, a hitch of up to 15 ms just after the load).
void prepare();
// Diagnostic of the actual flame transform at 0, 90 and 180 degrees.
void tiltTest();

// Server, after a saved game is loaded (VR_OnLoadGame): the map's wall torches that the save doesn't have (a save made
// while they were static entities, before this change, or with vr_walltorch 0) are spawned again.
void restoreAfterLoad();

} // namespace qvr::walltorch
