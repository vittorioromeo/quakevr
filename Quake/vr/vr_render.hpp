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

// The gloves' painted leather (vr_glove_paint, vr_render.cpp): read again from its files (vr_gloves_reload); and what a
// hand has of it (0 none, 1 its detail and wear, 2 its relief too: added to its instance's Glove.w times 4).
void glovesReload_f();
[[nodiscard]] int glovePaintFlags(const entity_t* e);

} // namespace qvr::render
