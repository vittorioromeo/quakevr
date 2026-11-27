// vr_animsmooth.hpp -- smoother monster animations (r_lerpmodels 3): see vr_animsmooth.cpp. The renderer's side is
// vr_api_render.h's VR_AnimSmooth, VR_AnimSmoothBind and VR_AnimSmoothPinned.

#pragma once

#include <glm/fwd.hpp>

struct entity_s;

namespace qvr::animsmooth
{

// vr_anim_smooth_pin, vr_anim_smooth_info, vr_anim_smooth_check, vr_anim_smooth_trace (Debug > Reports).
void registerCommands();
void registerDiagCommands(); // (vr_animsmooth_diag.cpp)

// Whether `e`'s model is drawn through its rig's bones (an alias model with a ragdoll rig that fits it): its hit
// reactions are its bones' (vr_hitreact.cpp), else its whole model's tilt.
[[nodiscard]] bool rigged(const entity_s* e);

// The map's load (VR_NewMap's prewarm): each precached Quake model's frame links and rig data made
// now, as its first smooth draw would (6-27 ms a model, in the frame it first came into view or into a shadow map; kept
// for the session: ROUND22.md "Frame spikes").
void prepare();

// r_lerpmodels 3 for an entity drawn on its bones elsewhere (the live grunts and enforcers: vr_rig2_game.cpp): the spline's
// four poses and its weights for its lerp (pose1 -> pose2 at blend), as its own drawing would take them. False: no
// spline now (another r_lerpmodels, at a pose itself): Quake's blend.
bool splineOf(const entity_s* e, const void* aliashdr, int pose1, int pose2, float blend, int poses[4], float w[4], float rot[3]);

// Client entity `e`'s .mdl vertices `verts` as the shader draws them at its lerp pose1 -> pose2 (blend: R_SetupAliasFrame's,
// VR_AliasLerpNow): the spline through its four poses, its limbs' swing, its rebuild from its skeleton (the renderer's own
// rows for it this frame: made here if the draw has not yet, and the draw then reuses them), on the dequantised poses, in
// the poses' byte units (times the header's scale plus scale_origin: model units). Not a hit reaction's kick (RigKick).
// The beasts' armour (hitmodel::drawnVertexPoints with a client lerp; ROUND26.md "armourrebuild"). False: not a Quake
// .mdl, a pose or a vertex out of range.
bool drawnPoints(const entity_s* e, int pose1, int pose2, float blend, const int* verts, int n, glm::vec3* out);

// vr_drawnlerp_watch: `e`'s shader inputs kept from each frame's first draw of it (VR_AnimSmooth: its poses, weights, rows,
// a kick's), null none; capturedPoints replays that draw on the CPU for its vertices `verts` (byte units, as drawnPoints)
// if it was drawn at client time `time`.
void captureDraws(const entity_s* e);
bool capturedPoints(double time, const int* verts, int n, glm::vec3* out);

} // namespace qvr::animsmooth
