// vr_anchor.hpp -- positions of individual alias model vertices ("anchor vertices").
//
// Weapon settings attach the hand, muzzle, weapon button and ammo text to vertices of the
// weapon model. Their indices refer to the vertex order of QuakeSpasm's old triangle-strip
// builder (BuildTris), which the old engine used to lay out pose data; Ironwail keeps the MDL's
// original order. The strip order is rebuilt here from the model file to translate indices,
// so existing weapon settings keep working.

#pragma once

#include "vr_engine.hpp"

namespace qvr::anchor
{

// Vertex position in the model's byte space (0..255 per axis, before scale/scale_origin), for
// the entity's current frame blended towards frame 0 by `zeroBlend`.
[[nodiscard]] glm::vec3 posedVertex(const entity_t& ent, int anchorIndex, float zeroBlend);

// Pose index used for "zero blending" (frame 0, animated for frame groups).
[[nodiscard]] int zeroPose(const aliashdr_t* hdr);

// Pose index for the entity's current frame.
[[nodiscard]] int currentPose(const entity_t& ent, const aliashdr_t* hdr);

} // namespace qvr::anchor
