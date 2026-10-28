// vr_animsmooth.hpp -- smoother monster animations (r_lerpmodels 3): see vr_animsmooth.cpp. The renderer's side is
// vr_api_render.h's VR_AnimSmooth, VR_AnimSmoothBind and VR_AnimSmoothPinned.

#pragma once

struct entity_s;

namespace qvr::animsmooth
{

// vr_anim_smooth_pin, vr_anim_smooth_info, vr_anim_smooth_check, vr_anim_smooth_trace (Debug > Reports).
void registerCommands();
void registerDiagCommands(); // (vr_animsmooth_diag.cpp)

// Whether `e`'s model is drawn through its rig's bones (an alias model with a ragdoll rig that fits it): its hit
// reactions are its bones' (vr_hitreact.cpp), else its whole model's tilt.
[[nodiscard]] bool rigged(const entity_s* e);

// The map's load (VR_NewMap's prewarm, vr_load_prewarm): each precached Quake model's frame links and rig data made
// now, as its first smooth draw would (6-27 ms a model, in the frame it first came into view or into a shadow map; kept
// for the session: ROUND22.md "Frame spikes").
void prepare();

// r_lerpmodels 3 for an entity drawn on its bones elsewhere (the live grunts and enforcers: vr_rig2_game.cpp): the spline's
// four poses and its weights for its lerp (pose1 -> pose2 at blend), as its own drawing would take them. False: no
// spline now (another r_lerpmodels, at a pose itself): Quake's blend.
bool splineOf(const entity_s* e, const void* aliashdr, int pose1, int pose2, float blend, int poses[4], float w[4], float rot[3]);

} // namespace qvr::animsmooth
