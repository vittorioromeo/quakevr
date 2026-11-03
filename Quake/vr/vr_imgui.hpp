// vr_imgui.hpp -- Dear ImGui in the game (Quake/thirdparty/imgui, v1.92.8; docs/vr-port/RIGSTUDIO.md): the flat-screen
// tools' UI (the Rig Studio, vr_rigstudio.cpp). Made lazily, the first time a tool opens, and only on the desktop screen
// (no headset session): nothing of it exists before, and while no tool wants it nothing of it runs (a frame costs one
// flag's test in the input loop and at the swap).
//
// - Input: while a tool wants it (want()), the SDL events go to ImGui first (ImGui_ImplSDL2_ProcessEvent) and the game's
//   keys and mouse are held back (the game's own key handling sees none of them), except the console's key while ImGui
//   has no text field, so the console still opens. The menus and the console (key_dest not the game) get every event.
// - A frame: beginFrame() (the host frame's start, after the events: VR_BeginFrame), the tool's windows built, then
//   render() last in the frame (GL_EndRendering, after the post-process, before the swap and vr_screenshot_frames'
//   shot), into the window's framebuffer, the GL state the engine relies on saved and put back.
// - Its settings (the windows' places and sizes) in quakevr/imgui.ini.
// - In a headset session the desktop window is the mirror: a tool that works there (the Arena Wave Editor) draws into a
//   target of its own (setPanel), shows it on a quad and feeds ImGui its laser and trigger (feedMouse); the SDL mouse is
//   then ignored, the keyboard still types.

#pragma once

namespace qvr::imgui
{

// A tool wants ImGui this frame (it routes the input, draws). Set by the tool each frame it is open.
void want(bool on);
[[nodiscard]] bool wanted();

// Made (the context, the SDL2 and OpenGL3 backends): false if it can't be (no window, a headset session).
bool ensure();
[[nodiscard]] bool ready();

// The frame's start (once a host frame while wanted): ImGui::NewFrame. False: none begun (not ready).
bool beginFrame();
[[nodiscard]] bool frameOpen();

// A tool's frames go to framebuffer `framebuffer`, a window of width x height pixels, `texels` texels each, not the
// desktop window (0: the desktop window, the default; only then does a headset session keep ImGui from starting).
void setPanel(unsigned framebuffer, int width, int height, float texels);
// The mouse for the next frame: where it is (-FLT_MAX: nowhere), its button, the wheel's turn. Goes in after the SDL
// backend's own, so it wins.
void feedMouse(float x, float y, bool down, float wheel);

// The style: 0 dark, 1 light, 2 classic.
void setTheme(int theme);

// VR_Shutdown: the backends and the context let go (GL objects with the context still current).
void shutdown();

} // namespace qvr::imgui
