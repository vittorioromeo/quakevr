// vr_doom.hpp -- Doom on the wrist gadget: a real game of Doom running on the gadget's screen in real time, beside
// Quake (which keeps running), played with the touchscreen (vr_touch.hpp) and, while it has the focus, the off hand's
// stick and buttons. Doom itself is qvr_doom.dll (the vendored doomgeneric: Quake/vr/doom/qvr_doom_api.h), loaded on
// first use; until then this costs nothing. docs/vr-port/DOOM.md has the design, the controls and the WADs.
//
// Open (vr_doom 1, the menu's Doom page): the gadget's screen shows Doom instead of the HUD, Doom runs at its 35 tics a
// second; closed, the HUD is back and Doom waits where it was (still loaded). Quake's pause (a menu, the console, the
// runtime's menu) pauses Doom too; Quake's deaths, level changes, saves and loads leave it alone. Doom's sounds come
// from the gadget, quieter than the game's (vr_doom_volume). Its settings and saves: quakevr/doom/.
//
// The focus: Doom takes the off hand's stick and buttons (vr_doom_controls 1 or 2) only while it has the focus: a touch
// on its screen gives it, or the screen raised and looked at for vr_doom_focus_look s; it goes when the screen has been
// out of view (lowered, looked away from) for vr_doom_focus_grace s, or the menu button opens Quake's menu. The main
// hand stays Quake's. Touch only (vr_doom_controls 0): the sticks and buttons are always Quake's.

#pragma once

#include "vr_backend.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"

namespace qvr::doom
{

void init();     // commands (vr_doom, vr_doom_info, vr_doom_wads, ...)
void shutdown(); // Doom saved and unloaded

// Once a host frame (vr_main.cpp, after the input): Doom's clock, its controls, its sounds kept at the gadget.
void frame();

// Doom is shown on the gadget's screen now (its HUD replaced).
[[nodiscard]] bool onGadget();
// The texture the gadget's screen shows meanwhile (renderScreen made it; 0: none yet) and its virtual screen's size.
[[nodiscard]] gfx::Texture gadgetTexture();
// Its target (vr_gadget_screen_dump writes it while Doom shows).
[[nodiscard]] const gfx::Target& screenTarget();
// Draws Doom's screen into its texture, in the 2D pass (gadget::renderScreen): only while the gadget is shown.
void renderScreen();
// The floating panel (vr_doom_panel 1), in each eye's scene after the opaque entities (with the gadget's screen).
void drawPanel();

// The off hand's controls while Doom has the focus (vr_input.cpp): true when it took them this frame (Quake then sees
// none of the off hand's buttons or its stick; its keys held before are let go).
[[nodiscard]] bool takesOffHand(const InputState& in);

// The menu's Doom page (vr_menu.cpp): the state in a line, the WADs found (searched when first asked; row -1 is "the
// best found"), each row's text and help (its path), and picking one (vr_doom_wad; Doom restarted on it if running).
[[nodiscard]] const char* menuStatus();
[[nodiscard]] int wadCount();
[[nodiscard]] const char* wadRow(int i);
[[nodiscard]] const char* wadHelp(int i);
void pickWad(int i);

} // namespace qvr::doom
