// vr_anchor.hpp -- anchors: points on alias models that follow their frame animation.
//
// The weapon settings attach the muzzle, the weapon button and the ammo text (and, in old configs, the hands) to anchors
// on the weapon model (vr_weapons.inc's *_av keys); vr_view.cpp's magazine mounts, vr_shells.cpp's ejection ports and
// vr_weaponfx.cpp's monster guns have theirs. An anchor is text, "x y z": a point on the model's surface, in its own
// coordinates as its file's frame 0 has them (scale and origin applied: the coordinates vr_anchor_info prints and
// Blender shows). It is bound, once per model and text, to the model's nearest triangle of the rest pose: barycentric
// weights over its three vertices (snapped to a corner the point is on; among triangles as near, those with the point on
// a corner, the first). Each frame the same weights over the posed vertices (as drawn, blended towards the zero pose)
// give where it is: it follows the animation (kick, pump, the super shotgun opening) and does not depend on the vertex
// numbering, so the models can be edited freely (an edit that moves the surface by a little moves the anchor with it;
// one that takes the surface away binds it to what is nearest now). Text that is not three numbers (the vertex indices
// of the settings before vr_wofs_version 41, which the migration replaced) binds to the model's first vertex, with a
// warning printed once per model and text.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"


namespace qvr::anchor
{

// Where `anchor` (the text above) is on the entity's model in the model's byte space (0..255 per axis, before
// scale/scale_origin), for its current frame as drawn, blended towards frame 0 by `zeroBlend`.
[[nodiscard]] glm::vec3 posedAnchor(const entity_t& ent, const char* anchor, float zeroBlend);

// The model's mesh read and `anchor` bound on it now, as posedAnchor does the first time (a map's load: the world
// weapons' screens, ~10 ms in the frame a gun first lay near you: ROUND22.md "Frame spikes").
void prepare(const qmodel_t* model, const char* anchor);

// Whether `text` is an anchor (three numbers).
[[nodiscard]] bool isAnchor(const char* text);

// The same for the model's vertex `vertex` (its file's order): not an attachment (the melee HUD's embers, off a random
// vertex).
[[nodiscard]] glm::vec3 posedVertex(const entity_t& ent, int vertex, float zeroBlend);

// Pose index used for "zero blending" (frame 0, animated for frame groups).
[[nodiscard]] int zeroPose(const aliashdr_t* hdr);

// Every vertex of the entity's model in the model's byte space (as posedAnchor): at rest (the zero pose) and as drawn
// this frame (blended towards the zero pose by `zeroBlend`). False for a model without Quake vertices.
[[nodiscard]] bool posedVertices(const entity_t& ent, float zeroBlend, za::Vector<glm::vec3>& rest, za::Vector<glm::vec3>& now);

// Developer commands for making weapon settings: vr_anchor_info <model> <x y z> (where it binds), vr_anchor_nearest
// <model> <x> <y> <z> (the anchor of the surface point nearest a point).
void registerCommands();

// The game directory changed (VR_OnGameDirChanged): Mod_ResetAll reuses the models' slots for other models, and the
// files are another game's (the anchors' cache is also emptied by mem::GameDirChange and mem::ModelReload).
void onGameDirChanged();

} // namespace qvr::anchor
