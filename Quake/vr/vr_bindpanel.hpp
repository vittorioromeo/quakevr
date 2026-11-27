#pragma once

// vr_bindpanel.hpp -- flatscreen play's bindings reminder (wheelui; his words 2026-11-28: "a reminder of all the most
// important flatscreen bindings displayed above the weapon wheel and MMB menu ... Also add the same panel in the menu"):
// the most used flat actions (fire, aim, reload, the wheel, the holster menu, swap, grab, force grab, throw, shove, punch,
// grenade, crouch, flashlight) with their keys as they are bound now (vr_flat_key_*, both keys where two are set; fire
// and aim by their binds), as key caps. One list for both places: over the holster menu and the weapon wheel it is the
// gadget's image (gadget::renderBindPanel, drawn on the window as the holster menu is); in the main menu vr_menuui.cpp
// draws the same rows in the menu's look, with Setup Bindings under them (setupBindings). vr_flat_bind_panel 0: none.

#include "vr_gadget.hpp"

#include <glm/glm.hpp>

namespace qvr::bindpanel
{

constexpr int maxRows = 16;

// The rows now (their texts valid until the next call): how many.
int rows(gadget::BindPanelRow* out, int max);

// Over the holster menu and the weapon wheel in flatscreen play (vr_flat_bind_panel): shown, and its size in the menus'
// virtual pixels.
[[nodiscard]] bool shown();
[[nodiscard]] glm::vec2 size();
// Drawn on the window with its top left at (x, y) (pixels, y down), `scale` window pixels a virtual pixel; its image made
// at most once a frame.
void drawOnWindow(float x, float y, float scale, float alpha);

// The main menu's panel (flat screen): shown there (vr_flat_bind_panel, not in a headset).
[[nodiscard]] bool inMainMenu();
// Its Setup Bindings button: the controller binding screen (bindsetup::open) for a controller in use, else Options >
// Controls at the flat keys (keyboard and mouse).
void setupBindings();

// vr_bindpanel_report, vr_bindpanel_setup (the tests').
void init();

} // namespace qvr::bindpanel
