#pragma once

// vr_reloadstudio.hpp -- the Reload Studio (vr_reloadstudio.cpp): the procedural reloads' keyframes (vr_reloadsys.hpp)
// edited in an ImGui window over flat-screen play.

namespace qvr::reloadstudio
{

void init();     // the command: vr_reloadstudio [open|close|toggle|test]
void frame();    // vr_main's client frame, with the other ImGui tools (it closes when one of them opens)
void shutdown(); // closed (the input grab given back)
[[nodiscard]] bool open();

} // namespace qvr::reloadstudio
