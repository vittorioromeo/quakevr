#pragma once

// vr_imgprefetch.hpp -- the images the start-up and the first map load decode, decoded ahead on worker threads.

namespace qvr::imgprefetch
{

void start();    // VR_Init: the last session's list read, its files read, their decoding started
void end();      // the first map load's end: the workers joined, the rest freed, the list written
void shutdown(); // VR_Shutdown

} // namespace qvr::imgprefetch
