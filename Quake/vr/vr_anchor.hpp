// vr_anchor.hpp -- anchors: points on alias models that follow their frame animation.
//
// Weapon settings attach the muzzle, the weapon button and the ammo text (and, in old configs, the hands) to anchors on
// the weapon model (vr_weapons.inc's *_av keys); vr_view.cpp's magazine mounts and vr_shells.cpp's ejection ports have
// one each. An anchor is text:
//   "x y z"    a point on the model's surface, in its own coordinates as its file's frame 0 has them (scale and origin
//              applied: the coordinates vr_anchor_info prints and Blender shows);
//   "x y z t"  the same, t a triangle (the file's order) taken among triangles as near as the nearest (two parts
//              touching at the point, which move apart in the animation); a stale t is harmless (it only picks
//              among equally near triangles);
//   "i"        a legacy anchor index (one number): a vertex in QuakeSpasm's old triangle-strip order (BuildTris), the
//              format before vr_wofs_version 41. Still read as before; the weapon settings' migration converts a
//              config's once (legacyToText).
// The point is bound, once per model and text, to the model's nearest triangle of the rest pose: barycentric weights over
// its three vertices (snapped to a corner the point is on). Each frame the same weights over the posed vertices (as drawn,
// blended towards the zero pose) give where it is: it follows the animation (kick, pump, the super shotgun opening) and
// does not depend on the vertex numbering, so the models can be edited freely (an edit that moves the surface by a little
// moves the anchor with it; one that takes the surface away binds it to what is nearest now).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"


namespace qvr::anchor
{

// Where `anchor` (the text above) is on the entity's model in the model's byte space (0..255 per axis, before
// scale/scale_origin), for its current frame as drawn, blended towards frame 0 by `zeroBlend`.
[[nodiscard]] glm::vec3 posedAnchor(const entity_t& ent, const char* anchor, float zeroBlend);

// The same for the model's vertex `vertex` (its file's order).
[[nodiscard]] glm::vec3 posedVertex(const entity_t& ent, int vertex, float zeroBlend);

// Pose index used for "zero blending" (frame 0, animated for frame groups).
[[nodiscard]] int zeroPose(const aliashdr_t* hdr);

// Every vertex of the entity's model in the model's byte space (as posedAnchor): at rest (the zero pose) and as drawn
// this frame (blended towards the zero pose by `zeroBlend`). False for a model without Quake vertices.
[[nodiscard]] bool posedVertices(const entity_t& ent, float zeroBlend, za::Vector<glm::vec3>& rest, za::Vector<glm::vec3>& now);

// Whether `anchor` is a legacy anchor index (one number).
[[nodiscard]] bool isLegacy(const char* anchor);

// The point anchor for the legacy anchor index `index` on the model file `modelName` (its vertex's place at rest, and a
// triangle hint where another vertex is there too that moves elsewhere), as `out`: one that is where the vertex is in
// every pose of the file (`exact`, if given, false where none is: the plain point then). False: no such model file.
[[nodiscard]] bool legacyToText(const char* modelName, int index, za::String& out, bool* exact = nullptr);

// Developer commands for making weapon settings: vr_anchor_info <model> <anchor> (where it binds), vr_anchor_nearest
// <model> <x> <y> <z> (the anchor of the surface point nearest a point), vr_anchor_from_index <model> <index> (a legacy
// index's point), vr_anchor_compare <model> <index> <anchor> (the two in every pose: a conversion's check).
void registerCommands();

// The game directory changed (VR_OnGameDirChanged): Mod_ResetAll reuses the models' slots for other models, and the
// files are another game's (the anchors' cache is also emptied by mem::GameDirChange and mem::ModelReload).
void onGameDirChanged();

} // namespace qvr::anchor
