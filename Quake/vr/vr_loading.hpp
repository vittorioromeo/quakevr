#pragma once

// vr_loading.hpp -- "Loading..." in the headset before a level change (vr_loading_notice, vr_loading.cpp).
//
// A level change (changelevel: a map's exit or a map transition teleporter; map: the menus' map starts; load: a save;
// restart: a death's reload) blocks the main thread for the whole load, and while it does the runtime keeps showing the
// last frame the game submitted (then, after a few seconds, its own "not responding" view). So the command is put off:
// the notice is drawn in front of the head (text3d's overlay: over everything, white on a dark backing) and the command
// runs again once two headset frames showed it (VR_LoadingDefer, vr_loading_wait); the frozen frame the runtime keeps
// showing during the load is then the notice. The command buffer's later commands wait behind it (their order kept). With
// no world drawn (the runtime's panel: a map started from the menus before any map) the 2D canvas shows Quake's loading
// plaque instead (VR_LoadingPlaque).

namespace qvr::loading
{

void registerCommands();

// VR_BeginFrame, after the texts are cleared: the notice queued again while a command waits.
void frame();

// After the eyes were submitted (vr_stereo.cpp): `eyes` when the frame had the eye images (the notice in them).
void submitted(bool eyes);

// Whether a level change's command waits for the notice.
[[nodiscard]] bool waiting();

} // namespace qvr::loading
