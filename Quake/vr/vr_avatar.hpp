// vr_avatar.hpp -- the player's skinned body (progs/vrbody.md5mesh, from
// Misc/quakevr/make_vrbody.py), posed each frame by inverse kinematics from the head and the
// drawn hands: the spine follows the head (crouching bends the legs and the back), the
// shoulders rise and swing when reaching, and the arms are two-bone chains to the wrists, with
// the elbows placed by heuristics after Parger et al., "Human upper-body inverse kinematics for
// increased embodiment in consumer-grade virtual reality" (VRST 2018). Legs, optionally, stand
// under the hips.
//
// The pose is drawn through the renderer's skeletal (MD5) path, with the entity's own bone
// matrices (VR_AliasBonePoses in r_alias.c) instead of the model's animation.

#pragma once

#include "vr_hands.hpp"

namespace qvr::avatar
{

struct Frame
{
    glm::vec3 pos{0.f};
    glm::mat3 rot{1.f}; // columns: along the bone (up the spine), side, forward
};

struct Torso
{
    Frame pelvis;
    Frame chest;
};

// The spine's pose from the head alone (usable before the hands are placed).
[[nodiscard]] Torso torso(const hands::State& s);

// `s` standing upright under the same head position, looking straight ahead at the body's yaw.
[[nodiscard]] hands::State standing(const hands::State& s);

enum class Part
{
    Pelvis,
    Chest
};

// Where a point given for the standing body (see standing()) is now, carried by `part` as the
// body leans and crouches.
[[nodiscard]] glm::vec3 follow(const hands::State& s, Part part, const glm::vec3& standingPoint);

// Whether `model` is the skinned body with the expected skeleton.
[[nodiscard]] bool usable(qmodel_t* model);

// Poses the body for this frame; `wrist` and `handUp` (the back of the hand's direction) are
// per hand (HAND_OFF, HAND_MAIN), in world space. Returns the entity origin (the pelvis).
glm::vec3 pose(const hands::State& s, qmodel_t* model, const entity_t* ent, const glm::vec3 wrist[2],
    const glm::vec3 handUp[2], bool legs);

// Not drawn this frame.
void hide();

// World units per model unit of the posed entity (0 for any other entity).
[[nodiscard]] float modelScale(const entity_t* e);

} // namespace qvr::avatar
