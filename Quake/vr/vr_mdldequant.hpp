// vr_mdldequant.hpp -- .mdl vertices dequantised at load (vr_mdl_dequant): see vr_mdldequant.cpp. The engine's side is
// vr_api_render.h's VR_MdlDequantCompute and VR_MdlDequantOn (gl_mesh.c).

#pragma once

#include <glm/glm.hpp>

namespace qvr::mdldequant
{

// vr_mdl_dequant's callback, vr_mdl_dequant_check (Debug > Reports).
void init();
void registerCommands();

// A .mdl vertex of a pose (an index into its poses' vertices) as it is drawn (anchors: vr_anchor.cpp), in the model's byte units (times the
// header's scale plus scale_origin: model units): the stored bytes, moved by the dequantised offset while
// vr_mdl_dequant is on. `aliashdr` is a PV_QUAKE1 aliashdr_t.
[[nodiscard]] glm::vec3 poseVertex(const void* aliashdr, int pose, int vertex);

} // namespace qvr::mdldequant
