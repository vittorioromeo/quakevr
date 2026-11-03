// vr_waveeditor.hpp -- the Arena Wave Editor (Debug > Tools > Arena Wave Editor; `vr_waveeditor`; docs/vr-port/ROUND23.md,
// "Arena wave editor"): the presets of vr_arenawaves.hpp edited in the game, with Dear ImGui (vr_imgui.hpp). One window,
// two ways to reach it:
// - a flat screen (-flatscreenmode): a desktop window, with the mouse and keyboard, as the Rig Studio;
// - in the headset: the same window drawn on a panel floating in front of you (a target of its own, as the toolgun's
//   panel and the perk cards), pointed at with either hand's laser, its trigger the click, the stick the wheel. Names are
//   typed on the window's own keys. Recentre puts the panel in front of your head again.
// While it is open the game is held (the server's clock stops, as in a menu): nothing comes at you while you edit. Close
// lets it go; Play closes it and starts the arena on the edited preset (unsaved edits included) from the wave chosen.

#pragma once

#include "vr_hands.hpp"
#include "vr_posing.hpp"

#include <glm/glm.hpp>

namespace qvr::waveeditor
{

void init();     // its commands
void shutdown(); // VR_Shutdown: closed

[[nodiscard]] bool shown();
// Nonzero while the game is held (VR_RuntimeMenuPause).
[[nodiscard]] bool holds();

// VR_BeginFrame (after the input events): its ImGui frame and window.
void clientFrame();
// The host frame, with the hands of this frame: the panel's place, the lasers.
void frame(const hands::State& s);
// A controller's button (vr_input.cpp): a trigger while it is open is its click, on the panel or not.
[[nodiscard]] bool button(int hand, posing::Button b, bool down);
// The controllers' sticks: the wheel (true: taken while it is open).
[[nodiscard]] bool sticks(const glm::vec2& off, const glm::vec2& main);
// In each eye (vr_panel.cpp): the panel and the lasers.
void drawInEye(const hands::State& s);

} // namespace qvr::waveeditor
