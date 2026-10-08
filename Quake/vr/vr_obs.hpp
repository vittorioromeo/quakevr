#pragma once

// vr_obs.hpp -- OBS Studio's recording, from the game: its state in a row of the headset's menus (above the spectator
// camera's switch, vr_menuui.cpp), a press starts or stops it. Through OBS's own obs-websocket (v5: OBS 28 and later,
// Tools > WebSocket Server Settings), on a thread of its own (vr_obs.cpp).

#include "Zancle/Base/SizeT.hpp"

namespace qvr::obs
{

void registerCommands(); // vr_obs_status, vr_obs_toggle, vr_obs_connect
void poll();             // each frame (VR_HostFrameEnd): the menus' state to the thread, its answers back
void finish();           // the thread stopped and joined (VR_StopDownloads: before the engine's winsock goes)

// The menus' row: whether it shows, its text (long, and short for a narrow panel: "OBS: Recording 00:12:34"),
// and whether OBS is recording (its light). Main thread, no allocation.
struct Banner
{
    bool shown{false};
    bool recording{false}; // recording or paused: the light red
    bool paused{false};
    char text[48]{};
    char shortText[32]{};
};
[[nodiscard]] Banner banner();

// The row pressed: recording started or stopped (OBS's ToggleRecord); a hint's row asks again now. The same press
// within 1.5 s does nothing (a double click, both triggers). True: taken.
bool press();

// Graphics > Recording's status line.
[[nodiscard]] const char* statusLine();

} // namespace qvr::obs
