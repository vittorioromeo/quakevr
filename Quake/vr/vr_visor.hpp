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

#include <glm/vec2.hpp>
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

// The headset's centre panel (vr_center_panel, vr_panel.cpp): the flat visor's own centre popup. claimCenter: a centre
// print (or a map's view message) up this frame with Quake's fade `alpha` (the flat one's VR_CenterPrintOnVisor too: a
// new text pops in anew). renderHeadsetCenter (VR_End2D, the headset's frames): the popup's fade and pop in stepped as the
// flat one's, then its contents, mount and composite (the flat visor's drawPanel and layer program; no lens, sway nor
// glass) into a texture of its own. headsetCenter: that texture, shown in the eyes next frame, its sizes in visor pixels
// (the whole box with its glows, the contents' rectangle in it from the box's top left, y down; texture rows bottom
// first, premultiplied alpha).
struct HeadsetCenter
{
    unsigned texture{0};
    glm::vec2 size{0.f};
    glm::vec2 contentsAt{0.f};
    glm::vec2 contentsSize{0.f};
    bool shown{false};
};
void claimCenter(const char* text, float alpha);
void renderHeadsetCenter();
[[nodiscard]] HeadsetCenter headsetCenter();
void headsetCenterStatus(); // (vr_center_panel_info's)

// A hit on him (VR_OnDamage: the server's damage message, its armor and blood): the visor's glitch and shake. Only a real
// hit does it, never health going down by itself (a megahealth's overheal ticking away).
void onDamage(int armor, int blood);

// vr_seat_test: the current seat's hits the visor has not taken yet (onDamage's armor and blood since its frame).
[[nodiscard]] int damagePending();

// vr_flat_visor_report, vr_flat_visor_fx.
void init();

} // namespace qvr::visor
