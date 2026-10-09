// vr_explosiondebris.hpp -- an explosion's incandescent chunks: the server's Box3D props (entities), their trails and
// glow drawn by the client. See vr_explosiondebris.cpp and docs/vr-port/ROUND21.md, "Explosion debris as Box3D bodies".
#pragma once
#include <glm/glm.hpp>

namespace qvr::explosiondebris
{
// The server's.
void noteBroadcast(const glm::vec3& origin); // an explosion's temp entity broadcast (vr_server.cpp): chunks at the frame's end
void serverFrame();                          // VR_ServerFrameEnd: the new chunks, the ended, the cap, the fades
void serverReset();                          // a new map, a loaded game (resetServerWorld)
void serverClear();                          // every chunk removed now (Debug > Cheats' cleanups; vr_explosion_debris_test clear)

// The client's.
void frame(); // each frame, after the entities: the nearest chunks' glow
void prepare();
void clear();
void registerCommands();
int liveCount(); // chunks drawn in the last frame
}
