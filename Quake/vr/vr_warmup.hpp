// vr_warmup.hpp -- the derived-data cache's background warm-up (vr_cache_warmup; docs/vr-port/CACHES.md).
//
// A few seconds after the start-up, every installed game's .mdl files (each base directory's game folders: their .pak
// files' and their loose progs/ files) are listed on the pool, then made one at a time on the pool, below the game's
// threads (Windows' idle priority), into the disk cache (vr_diskcache.hpp): the products a model's file alone decides
// (its occlusion bake, vr_ao.cpp). A model whose file is there costs its parse and its key. Nothing is started while a
// map loads, nor for two seconds after a frame that ran late (over 14 ms); the model under way finishes. The pak files
// and loose files done are listed in `<cache root>/warmup/done_<version>.txt`: the next session goes on from there.
// Off in test runs (QVR_TEST_HIDDEN) unless vr_cache_warmup_debug 2, and in the installer's preparation run.
// The products made from a loaded model (the skins' normal maps, the dequantised poses, the rigs, the guns' pieces) and
// from a loaded map (the compiled hulls) are made by the loads themselves (and the map load's look-ahead, its probes),
// or ahead by the installer's preparation run (vr_prepare.hpp).

#pragma once

#include <stddef.h>

namespace qvr::warmup
{

void registerCommands(); // vr_cache_warmup_info

// VR_HostFrameEnd: started, paused, resumed, the next model given to the pool.
void frame();

// VR_Shutdown: the model under way finished, nothing more started, the done list written.
void stop();

// vr_cache_info's and vr_cache_warmup_info's lines.
void printProgress();

// The Debug menu's line ("Cache warm-up: ...").
[[nodiscard]] const char* statusLine(int);

} // namespace qvr::warmup
