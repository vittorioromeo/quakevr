#pragma once

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::pbr
{
void init();

// A map load's replacement images hashed on the prefetch's decode tasks (vr_imgprefetch.cpp), not on the main thread:
// whether they are wanted (vr_pbr; main thread), an RGBA image's session key and content hash as VR_PBRNormal makes them
// (any thread), and one noted for it to find (main thread: as the load takes each image, in its order).
[[nodiscard]] bool hashesWanted();
void hashRgba(const unsigned char* rgba, int w, int h, za::U64& key, char (&hex)[65]);
void noteHash(za::U64 key, const char (&hex)[65]);
} // namespace qvr::pbr
