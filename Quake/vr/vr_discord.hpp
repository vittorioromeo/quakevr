#pragma once

// vr_discord.hpp -- Discord Rich Presence: what the player plays shown on their Discord profile (the campaign, the map,
// the mode, VR or flat screen, the time on this map), through the Discord client's local IPC (vr_discord.cpp,
// docs/vr-port/DISCORD.md). Off by default (vr_discord); it needs a Discord application's id (vr_discord_appid).

namespace qvr::discord
{

void registerCommands(); // vr_discord_status, vr_discord_test
void frame();            // each frame (VR_HostFrameEnd): the game's state, handed to the thread when it changed
void finish();           // at quit (VR_Shutdown): the activity cleared, the thread stopped and joined

} // namespace qvr::discord
