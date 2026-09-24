// vr_server.hpp -- server-side Quake VR state shared with other VR sources.

#pragma once

namespace qvr::server
{

// Called when a server finishes spawning: model precaches made so far are known to clients.
void onSpawnServerAfterLoad();

void init(); // registers commands

} // namespace qvr::server
