#pragma once

// vr_reloadstudio.hpp -- the Reload Studio (vr_reloadstudio.cpp): the procedural reloads' keyframes (vr_reloadsys.hpp)
// edited in an ImGui window over flat-screen play.

namespace qvr::reloadstudio
{

void init();     // the command: vr_reloadstudio [open|close|toggle|test]
void frame();    // vr_main's client frame, with the other ImGui tools (it closes when one of them opens)
void shutdown(); // closed (the input grab given back)
[[nodiscard]] bool open();
// V_CalcRefdef (VR_ReloadStudioView, after the fly camera): the orbit camera over the view while the window is open and the
// camera is on Orbit (reloadstudio2); the hands stay posed from his own eye (flycam::borrowView). False: left as it is.
bool cameraView(float* vieworg, float* viewangles);

} // namespace qvr::reloadstudio
