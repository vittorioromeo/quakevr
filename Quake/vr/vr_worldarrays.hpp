// vr_worldarrays.hpp -- the world's textures in texture arrays for the bound (non-bindless) path: a draw a set of
// textures alike instead of a draw a texture (QUEST_PLAN.md, C2f; vr_worldarrays.cpp). The engine's calls
// (VR_WorldArrays*) are declared in vr_api_render.h.

#pragma once

namespace qvr::worldarrays
{

// VR_Init: the vr_world_texarrays_report command.
void init();

} // namespace qvr::worldarrays
