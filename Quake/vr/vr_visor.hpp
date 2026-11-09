// vr_visor.hpp -- flatscreen play's visor HUD (vr_flat_visor; docs/vr-port/ROUND24.md, "Flat-screen visor HUD"; his words
// 2026-11-09: "an immersive heads-up display ... some sort of helmet or visor overlay applied in front of the player's
// screen ... the same colour scheme and art style as the wrist gadget"). The HUD as the wrist gadget's screens in seven
// small panels in mounts on the screen's edges: health and armour top left, the bars and ammo grid top right, the
// console's notify lines at the top's middle, each hand's ammo at its bottom corner, keys and powerups at the sides'
// middles (gadget::renderVisor draws them). The panels lag a turn by a fraction of a degree and bob with the walk
// (vr_flat_visor_sway), still when standing still, and shake at a hit (vr_flat_visor_shake); the glass has a faint rim
// and a reflection streak, frosts the view behind the mounts (vr_flat_visor_glass_*), fogs up under water, the panels
// glitch at a hit, the health panel pulses red when low and the panels power on with a sweep at a spawn. The status bar
// is not drawn meanwhile (vr_flat_visor 0 puts it back).
//
// Drawn over the window's view after its look (VR_FlatLookApply) and before the 2D pass, so the menu and the console stay
// over it; never in an eye (a headset's visor could show the same images: gadget::visorImage).

#pragma once

#include <glm/vec4.hpp>

namespace qvr::visor
{

// The visor is the HUD now: flatscreen play, vr_flat_visor, in a map (not its intermission), viewsize under 120, no CSQC HUD.
[[nodiscard]] bool active();

// The notify lines are the visor's (vr_flat_visor_log), not the screen's top left.
[[nodiscard]] bool logShown();

// The window's post-process (vr_tonemap.cpp): with vr_flat_visor_no_bt_tint, the panels' layer drawn this frame (its
// alpha: where bullet time's look leaves the visor's colours alone) and the view's rectangle it covers in the window
// (x, y, w, h); 0 none.
[[nodiscard]] unsigned tintMask(glm::vec4& rect);

// VR_End2D's flat-screen branch: the panels' images (gadget::renderVisor), and the refdef recalculated when the visor comes
// or goes (the classic status bar's lines).
void renderPanels();

// A hit on him (VR_OnDamage: the server's damage message, its armor and blood): the visor's glitch and shake. Only a real
// hit does it, never health going down by itself (a megahealth's overheal ticking away).
void onDamage(int armor, int blood);

// vr_flat_visor_report, vr_flat_visor_fx.
void init();

} // namespace qvr::visor
