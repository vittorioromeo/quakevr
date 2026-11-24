// vr_flatholster.hpp -- flatscreen play's holster menu (vr_flat_holsters, on by default; docs/vr-port/FLATPLAY.md,
// "Holsters"; his words 2026-11-10: "holding the mouse wheel button slows down time to 0.30x (configurable), blurs the
// screen a little bit, fades out the crosshair, and presents a menu in the style of the HUD visor in the center of the
// screen that displays the six holsters in a rough humanoid shape arrangement").
//
// While the wheel's button (vr_flat_key_holster_menu) is held: time at vr_flat_holster_menu_timescale (bullet time's slow
// motion path: timescale::wanted), the view blurred (the visor's glass blur: visor::menuBlur), the crosshair faded, the
// mouse the menu's cursor (the view stays). The six holsters on a figure on the left, each with its weapon's thumbnail and
// [L] [DROP] [R]; the two hands on the right with a DROP each, SWAP between them (3's swap) and DROP BOTH under them. A
// click sends the game code an impulse (QC vr_flatholster.qc). vr_flat_holster_menu_mode 0: letting go of the key clicks
// the button under the cursor (and closes it); 1: a press opens it, left clicks, the next press closes it. Esc closes it
// either way. The weapon keys do nothing while the holsters are on.

#pragma once

#include <glm/glm.hpp>

namespace qvr::flatholster
{

// What the holsters hold this frame (the server's stats: STAT_QVR_HOLSTERWEAPON0 + i, QC holsterweapon i), one model
// for every picker of them: this menu's slots and the headset's weapon wheel (vr_weaponwheel.cpp's holster provider).
// Holster i (0 .. holsterSlots - 1) by its title ("L SHOULDER"), its weapon's name ("" none: AD's and Dwell's variants by
// their model, a sword's dagger by its kind: notectx::weaponName) and its model's file (its thumbnail; "" none).
constexpr int holsterSlots = 6;
struct HolsterSlot
{
    const char* title{""};
    const char* name{""};
    const char* model{""};
};
[[nodiscard]] HolsterSlot holsterSlot(int i);
// The weapon (and model's file) in hand `hand` (0 the left, the off hand; 1 the right), "" none.
[[nodiscard]] HolsterSlot handSlot(int hand);
// Hand `hand`'s DROP would drop something (a weapon, a carried prop), its UNLOAD would move rounds back into the reserve
// (the server's test: STAT_QVR_FLATUNLOADFULL bits 4, 5): this menu's buttons lit, and the weapon wheel's DROP / UNLOAD.
[[nodiscard]] bool canDrop(int hand);
[[nodiscard]] bool canUnload(int hand);

// The menu's image (gadget::renderHolsterMenu's texture, `size` its virtual pixels) on its face, as the visor's screens
// (Shade::Screen: phosphor, scanlines, flicker at vr_flat_holster_menu_crt): the quad `corner` (low left, low right, up
// right, up left) through `mvp`, faded by `alpha`. This menu's on the window, the weapon wheel's in the world.
void drawImage(unsigned texture, const glm::vec2& size, const glm::vec3 (&corner)[4], const glm::mat4& mvp, float alpha);
// That image on the window in `rect` (x, y, w, h: window pixels, y down) with this menu's mount: its glow, the face and
// screen (drawImage), its frame; and this menu's mouse arrow, its tip at `at`. This menu's, and flatscreen play's weapon
// wheel (vr_weaponwheel.cpp drawFlat).
void drawOnWindow(unsigned texture, const glm::vec2& size, const glm::vec4& rect, float alpha);
void drawCursorOnWindow(const glm::vec2& at, float alpha);

// flatplay's frame (pollKeysFlat): the key, the open state and its fade, a click; `on`: flatscreen play runs.
void frame(bool on);

// The menu is open (the key held), or still fading out.
[[nodiscard]] bool shown();
// It is open (the key held): the mouse's buttons fire nothing (flatplay::routeFire).
[[nodiscard]] bool open();
// vr_seat_test: the current seat's +flatholsters held (the command alone).
[[nodiscard]] bool holdCommand();
// split11 (split_join_test): how far its cursor is from the middle of the panel, where it opens (panel units).
[[nodiscard]] float cursorFromMiddleX();

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
