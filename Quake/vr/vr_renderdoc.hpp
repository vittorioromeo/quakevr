#pragma once

// vr_renderdoc.hpp -- RenderDoc's in-application API (vr_renderdoc.cpp): whether the game runs under RenderDoc, and a
// capture of a frame from the menus (the corner's Capture Frame button, Menu Detail: Developer) or the console.

namespace qvr::renderdoc
{

void registerCommands(); // vr_renderdoc_capture [frames] [delay], vr_renderdoc_status
void poll();             // each frame (VR_HostFrameEnd): the delayed capture's steps

// RenderDoc is attached: renderdoc.dll is already in this process (never loaded here) and gave its API.
[[nodiscard]] bool attached();

// A capture of `frames` frames after `delay` seconds (menu closed meanwhile if vr_renderdoc_hide_menu). Prints why not
// when it cannot. The button's press: the cvars' frames and delay.
bool capture(int frames, float delay);
bool captureFromCvars();
// The toolgun's panel is left out of the frames being captured (vr_renderdoc_hide_menu).
[[nodiscard]] bool hidingPanel();
void presented(); // the desktop window was presented (GL_EndRendering)

} // namespace qvr::renderdoc
