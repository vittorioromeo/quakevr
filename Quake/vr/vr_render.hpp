// vr_render.hpp -- Quake VR additions to Ironwail's alias model rendering.

#pragma once

#include "vr_view.hpp"

namespace qvr::render
{

// The model matrix of a view entity as the renderer builds it (entity transform, mirroring,
// per-model weapon scaling), with `extra` offsets inserted after mirroring; used to locate
// anchor vertices consistently with what is drawn.
void anchorMatrix(const view::ViewEntity& ve, const glm::vec3& extra, float out[16]);

// The same for any alias entity (a weapon lying in the world: its networked scale and offset too), drawn mirrored
// or not, at `scale` (ENTSCALE_*).
void entityMatrix(const entity_t& e, bool mirrored, int scale, const glm::vec3& extra, float out[16]);

// Where an entity is drawn now, and turned how: a stepping monster's place and angles along its move (r_lerpmove, as
// R_SetupEntityTransform draws it), not the last message's (e.origin, which jumps 10-15 units a step as it runs).
void drawnPlace(const entity_t& e, glm::vec3& origin, glm::vec3& angles);

// entityMatrix (not mirrored, no extra) at the place drawnPlace gives.
void drawnEntityMatrix(const entity_t& e, int scale, float out[16]);

// The gloves' painted leather (vr_glove_paint, vr_render.cpp): read again from its files (vr_gloves_reload); and what a
// hand has of it (0 none, 1 its detail and wear, 2 its relief too: added to its instance's Glove.w times 4).
void glovesReload_f();
[[nodiscard]] int glovePaintFlags(const entity_t* e);
// A jointed hand's painted nails' mask (vr_nails on, looked up: make_nail_mask.py), or null (none drawn).
[[nodiscard]] gltexture_t* nailMask(const entity_t* e);

} // namespace qvr::render
