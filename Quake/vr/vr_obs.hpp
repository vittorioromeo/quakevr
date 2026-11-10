#pragma once

// vr_obs.hpp -- OBS Studio's recording, from the game: its state in a row of the headset's menus (above the spectator
// camera's switch, vr_menuui.cpp), a press starts or stops it. Through OBS's own obs-websocket (v5: OBS 28 and later,
// Tools > WebSocket Server Settings), on a thread of its own (vr_obs.cpp).

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

namespace qvr::obs
{

void registerCommands(); // vr_obs_status, vr_obs_toggle, vr_obs_connect, vr_obs_request
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

// ---- Requests of other modules (takes and clips: vr_takes.cpp)

// Whether requests go now: connected and the recording's state known, or the mock (vr_obs_mock).
[[nodiscard]] bool ready();
// Whether the mock answers instead of OBS (vr_obs_mock 1: no connection; tests).
[[nodiscard]] bool mocked();
// Whether OBS records now (as last known).
[[nodiscard]] bool recording();
// Each frame: keep (or stop keeping) a connection as while a menu is open (attempts every 5 s, test runs too).
void stayConnected(bool on);
// A connection attempt now (as vr_obs_connect).
void connectNow();
// Any obs-websocket request: `type` its requestType, `data` its requestData (a JSON object's text) or null. Sent on
// the connection's thread (or answered by the mock); its answer comes back as a message (takeMessage). Its id (> 0),
// 0: not sent (not ready).
int request(const char* type, const char* data = nullptr);
// The next message for them, oldest first: the raw JSON ({"op": 7, ...}) of an answer to request() (its requestId
// "qvt<id>"), or of an Outputs event (RecordStateChanged, ReplayBufferStateChanged, ReplayBufferSaved).
bool takeMessage(za::String& out);
// `text` as a JSON string, quoted and escaped.
[[nodiscard]] za::String jsonString(za::StringView text);

} // namespace qvr::obs
