// vr_flatxhair.hpp -- flatscreen play's visor crosshair (vr_flat_xhair; docs/vr-port/FLATPLAY.md, "The visor crosshair"; his
// words 2026-11-09: "a new flatscreen-only crosshair option that follows the visor's style and colour scheme, and displays
// the target location in screen space ... Most of the time, the crosshair should be centered ... But if the player is near
// an obstacle and the guns move a bit, the crosshair should point to the actual place where the shot will hit").
//
// Each gun's shot is traced as the server will trace it (the muzzle, the shot's angles and the hand this frame's move sent:
// QC FireBullets from behind the muzzle, VR_ShotBackPoint), and where it stops is projected with the window view's own
// camera (r_matviewproj): the true screen place of the hit. Within vr_flat_xhair_snap of the screen's crosshair centre it is
// drawn exactly there; past it, at the projected place, smoothed over vr_flat_xhair_smooth. Two guns whose places are
// further apart than the snap get one each, marked on their hand's side; else one, between them.
//
// Drawn over the view after the visor (R_RenderView, VR_DrawFlatCrosshair), outside the visor's lens, zoom and sway (so it
// stays on the true point), in the visor's colours (the frame's green brightened, its face as the outline, the frame's glow),
// antialiased by its own edge coverage. The 2D crosshair (crosshair, SCR_DrawCrosshair) is not drawn meanwhile.

#pragma once

#include <glm/vec3.hpp>

namespace qvr
{
struct VrMove;
}

namespace qvr::flatxhair
{

// The mode runs and vr_flat_xhair is on: the 2D crosshair gives way (this one shows whatever `crosshair` is: its default is 0).
[[nodiscard]] bool replacesCrosshair();

// vr_client.cpp buildMove: each hand's muzzle, shot angles and hand place as sent (the shot's own line).
void noteMove(const VrMove& move);

// vr_weaponfx.cpp parseTracer: one of his own pellets' line ended at `to` (vr_flat_xhair_probe compares them with the drawn
// crosshair).
void noteTracer(int hand, const glm::vec3& to);

// vr_flat_xhair_probe, vr_flat_xhair_turntest.
void init();

// Once a frame, from flatplay::frame before the move (the mode on): vr_flat_xhair_turntest's mouse.
void frame();

} // namespace qvr::flatxhair
