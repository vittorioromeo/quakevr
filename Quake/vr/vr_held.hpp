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
#include "vr_throw.hpp"

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
// sits against the curled fingers instead of sunk into the fist or held short of it
// (vr_held_surface_fit). It is moved along the way the palm faces (`palm`) until its drawn surface
// (the alias model's frame or the brush model's faces, turned with it) just touches the drawn fist
// (a measured height field round the grip at `hand`, in the gripping player's hand axes), plus
// vr_held_fit_gap and the model's vr_held_fit_gaps; the rest of where it was gripped is kept, so it
// can still be taken by any part. Zero if nothing of it is over the fist, or with the setting off.
// Client side: the entity the local player's `hand` (0 off, 1 main) holds, drawn in that hand this frame (0: none).
[[nodiscard]] int heldEntity(int hand);

// Client side: a prop held in both hands (vr_carry2h.hpp) is drawn from both, and each hand on its grip on it: `pos`
// and `angles`, the controller's pose of `hand`, are moved there (vr_view.cpp draws the hand from them); let go of, a
// hand eases back onto its controller. False (unchanged) otherwise.
bool drawnHand(int hand, glm::vec3& pos, glm::vec3& angles);

// Client side: the throw estimate of `hand` (tracking clock `at`) holding a prop in both hands: the prop's own motion
// from both hands (throwing::estimateBothAt). `release`: the hand lets go now, of a prop held in both, or held in both
// until the other let go at most vr_carry_two_hands_window before (a two-handed throw); else the estimate as if let
// go now, while held in both. False: not held in both (the hand's own estimate stands).
bool bothHandsThrow(int hand, double at, bool release, throwing::Estimate& out);

[[nodiscard]] glm::vec3 surfaceFit(edict_t* ent, const glm::vec3& hand, const glm::vec3& palm);

// Round 21, second pass: the distance (units) from `point` to the drawn surface of `ent` (its model as drawn: the
// networked scale and offset), its nearest point in `nearest`; -1 if it has no surface to measure.
[[nodiscard]] float surfaceDistance(edict_t* ent, const glm::vec3& point, glm::vec3* nearest = nullptr);

// vr_debug_carry: what a hand's touch test found (the server's), and drawn by the view (lines, this frame).
void noteCarryProbe(int hand, edict_t* ent, const glm::vec3& at, float distance, const glm::vec3& nearest, float reach);
void drawCarryProbes();

// Client side, a new map or a loaded game (VR_OnClientClearState): what the hands held is forgotten (it is taken
// again from the carry stats and where the server has it), as the client's clock starts over.
void resetClientState();

} // namespace qvr::held
