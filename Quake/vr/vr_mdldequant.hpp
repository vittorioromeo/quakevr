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

// The poses' order along the model's animations as the dequantiser splits them (the progs' loops, the frames' names, the
// jumps between unrelated animations; whatever vr_mdl_dequant is): per pose of `aliashdr` (a PV_QUAKE1 aliashdr_t, its
// model named `model`), the pose expected after it in `next` (-1: a run's end; a loop's last pose: its first) and in
// `progs` 1 where a progs' loop says so (r_lerpmodels 3's spline keys: vr_animsmooth.cpp). Both hold numposes values.
void poseSuccessors(const void* aliashdr, const char* model, short* next, unsigned char* progs);

} // namespace qvr::mdldequant
