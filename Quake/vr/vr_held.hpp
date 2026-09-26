// vr_held.hpp -- objects the local player carries (vr_carry.qc: ammo and health boxes, backpacks,
// gibs), drawn in the hand on the client.
//
// The server places a held object from the hand positions the client sent, and the client draws
// it where the server put it, interpolated: behind or ahead of the hand drawn this frame whenever
// the player moves or turns (and by the network's delay in multiplayer). So the local player's
// own held objects are drawn in the hand instead: the server tells which entity each hand holds
// (STAT_QVR_CARRYMAIN, STAT_QVR_CARRYOFF), the client takes its place in the hand when it first
// sees it held, and from then on draws it there, turning and moving with the hand at once. Let go,
// it eases back to where the server has it. Other players see the server's position.
//
// Also the drawn box of a model (with the networked scale and offset and the weapon scaling) and
// the angle conventions, shared with the rigid bodies (vr_rigid.cpp).

#pragma once

#include "vr_engine.hpp"

namespace qvr::held
{

// Model axes (x forward, y left, z up) of an entity's angles as the renderer turns it: an alias
// model's pitch is inverted (R_EntityMatrix tilts the model's forward up for a positive pitch, view
// angles tilt it down), a brush model's not (R_DrawBrushModels inverts it once more). View angles
// (the hands) turn as a brush model.
[[nodiscard]] glm::mat3 axesFromAngles(const float* angles, bool brush);
void anglesFromAxes(const glm::mat3& m, float* out, bool brush);

// The box `model` is drawn in, in its axes, relative to the entity's origin, as vr_render.cpp
// transforms it: `scale` (the networked scale, an offset from 1) about `scaleOrigin`, the weapon
// scaling, then the model's own and `offset` (on raw vertices; brush models: before the scale).
void modelBox(const qmodel_t* model, const glm::vec3& scale, const glm::vec3& scaleOrigin, const glm::vec3& offset,
    glm::vec3& lo, glm::vec3& hi);

// The middle of client entity `num` as drawn.
[[nodiscard]] glm::vec3 drawnCentre(int num);

// Server side, as a hand grips `ent` (QC's carryfit, vr_carry.qc): how far to move it so that it
// sits against the palm instead of sunk into the fist (vr_held_surface_fit). It is pushed the way
// the palm faces (`palm`) until its drawn box, turned with it, clears a ball the size of a fist
// round the grip at `hand`; the rest of where it was gripped is kept, so it can still be taken by
// any part. Zero if it is clear already, or with the setting off.
[[nodiscard]] glm::vec3 surfaceFit(edict_t* ent, const glm::vec3& hand, const glm::vec3& palm);

} // namespace qvr::held
