// vr_mdldequant.hpp -- .mdl vertices dequantised at load (vr_mdl_dequant): see vr_mdldequant.cpp. The engine's side is
// vr_api_render.h's VR_MdlDequantCompute and VR_MdlDequantOn (gl_mesh.c).

#pragma once

#include <glm/glm.hpp>

struct qmodel_s;

namespace qvr::mdldequant
{

// vr_mdl_dequant's callback, vr_mdl_dequant_check (Debug > Reports).
void init();
void registerCommands();

// A .mdl vertex of a pose (an index into its poses' vertices) as it is drawn (anchors: vr_anchor.cpp), in the model's byte units (times the
// header's scale plus scale_origin: model units): the stored bytes, moved by the dequantised offset while
// vr_mdl_dequant is on. `aliashdr` is a PV_QUAKE1 aliashdr_t.
[[nodiscard]] glm::vec3 poseVertex(const void* aliashdr, int pose, int vertex);

// The same in model units (poseVertex times the header's scale plus scale_origin), blended from `pose1` to `pose2` by
// `blend`: the one decode for code that fits or solves on the drawn shape (the armed humanoids' arms and legs, their
// grips and anchor: ROUND23.md, "Refactor C1"). prepare() first where it may run before the model's first upload.
[[nodiscard]] glm::vec3 modelVertex(const void* aliashdr, int pose1, int pose2, float blend, int vertex);

// Mesh vertex `meshVertex` (of its numverts_vbo: an aliasmesh_t's) of a .mdl's rest pose (pose 0) as the alias renderer
// draws it (gl_mesh.c, PV_QUAKE1): its place in model units (the stored bytes moved by the dequantised offset while
// vr_mdl_dequant is on and the offsets are ready: `prepare`) and its normal, 127 long (the smoothed one then, else the
// table's). For the skeletal copies of a .mdl drawn in its place (a ragdoll's "<model>#rag", the authored rig's "#rig2"):
// shaded as the .mdl is. `aliashdr` is a PV_QUAKE1 aliashdr_t.
void restVertex(const void* aliashdr, int meshVertex, float xyz[3], signed char normal[3]);

// The model's dequantised offsets computed now if vr_mdl_dequant is on and its first upload has not yet (the models a
// map's server side loads are uploaded after it spawns): for code reading its vertices as drawn (poseVertex) before
// it is drawn, such as the held guns' shapes prepared at map load. The main thread.
void prepare(const qmodel_s* model);

// The poses' order along the model's animations as the dequantiser splits them (the progs' loops, the frames' names, the
// jumps between unrelated animations; whatever vr_mdl_dequant is): per pose of `aliashdr` (a PV_QUAKE1 aliashdr_t, its
// model named `model`), the pose expected after it in `next` (-1: a run's end; a loop's last pose: its first) and in
// `progs` 1 where a progs' loop says so (r_lerpmodels 3's spline keys: vr_animsmooth.cpp). Both hold numposes values.
void poseSuccessors(const void* aliashdr, const char* model, short* next, unsigned char* progs);

} // namespace qvr::mdldequant
