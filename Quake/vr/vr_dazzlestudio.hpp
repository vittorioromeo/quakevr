#pragma once

// vr_dazzlestudio.hpp -- the Dazzle Studio (vr_dazzlestudio.cpp; Debug > Tools > Dazzle Studio; ROUND26.md "dazzlestudio"):
// the flashlight dazzle's shield pose tweaked per armed humanoid kind (vr_dazzledata.hpp) in an ImGui window over flat-screen
// play: a subject of the kind held in its shield pose ahead of you (frozen, or its hand rising and falling), the shared
// gizmo (vr_gizmo.hpp) on its shielding hand, its elbow and its gun hand, sliders, Save to quakevr/data/dazzle.txt.

namespace qvr::dazzlestudio
{

void init();     // the command: vr_dazzlestudio [open|close|toggle|test]
void frame();    // vr_main's client frame, with the other ImGui tools (it closes when one of them opens)
void shutdown(); // closed (its subject removed, the input grab given back)
[[nodiscard]] bool open();

} // namespace qvr::dazzlestudio
