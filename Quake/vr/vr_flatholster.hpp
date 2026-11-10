// vr_flatholster.hpp -- flatscreen play's holster menu (vr_flat_holsters, on by default; docs/vr-port/FLATPLAY.md,
// "Holsters"; his words 2026-11-10: "holding the mouse wheel button slows down time to 0.30x (configurable), blurs the
// screen a little bit, fades out the crosshair, and presents a menu in the style of the HUD visor in the center of the
// screen that displays the six holsters in a rough humanoid shape arrangement").
//
// While the wheel's button (vr_flat_key_holster_menu) is held: time at vr_flat_holster_menu_timescale (bullet time's slow
// motion path: timescale::wanted), the view blurred (the visor's glass blur: visor::menuBlur), the crosshair faded, the
// mouse the menu's cursor (the view stays). The six holsters on a figure on the left, each with its weapon's thumbnail and
// [L] [DROP] [R]; the two hands on the right with a DROP each and SWAP between them (3's swap). A click sends the game
// code an impulse (QC vr_flatholster.qc); release or Esc closes it. The weapon keys do nothing while the holsters are on.

#pragma once

namespace qvr::flatholster
{

// flatplay's frame (pollKeysFlat): the key, the open state and its fade, a click; `on`: flatscreen play runs.
void frame(bool on);

// The menu is open (the key held), or still fading out.
[[nodiscard]] bool shown();
// It is open (the key held): the mouse's buttons fire nothing (flatplay::routeFire).
[[nodiscard]] bool open();

// The time scale it asks for (timescale::wanted): vr_flat_holster_menu_timescale while open, else 1.
[[nodiscard]] float timeScale();
// The flat crosshair's opacity share (vr_flatxhair.cpp): faded out as the menu fades in.
[[nodiscard]] float crosshairAlpha();
// The view's blur radius now (pixels at 1080p; vr_visor.cpp menuBlur): 0 when closed.
[[nodiscard]] float blurRadius();

// IN_MouseMotion (VR_FlatToolgunMouse): the cursor's, while open. True when taken (the view stays).
[[nodiscard]] bool mouse(int dx, int dy);
// Key_Event's Esc in the game: closes it (until the key is pressed again). True when taken.
[[nodiscard]] bool escape();
// vr_flat_mouse_move to <name> (tests): the counts that bring the cursor onto that button's middle; false: no such one.
[[nodiscard]] bool countsTo(const char* name, int& dx, int& dy);

// VR_End2D (vr_panel.cpp): the menu over the window, its cursor on it.
void draw();

// +flatholsters / -flatholsters, vr_flat_holster_menu_dump, vr_flatholster_test.
void init();

} // namespace qvr::flatholster
