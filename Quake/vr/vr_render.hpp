// vr_render.hpp -- Quake VR additions to Ironwail's alias model rendering.

#pragma once

#include "vr_view.hpp"

namespace qvr::render
{

// The model matrix of a view entity as the renderer builds it (entity transform, mirroring,
// per-model weapon scaling), with `extra` offsets inserted after mirroring; used to locate
// anchor vertices consistently with what is drawn.
void anchorMatrix(const view::ViewEntity& ve, const glm::vec3& extra, float out[16]);

} // namespace qvr::render
