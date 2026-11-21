#pragma once

// The dynamic lights in the lightmap (vr_gl_world_dlmap, the ES profile; vr_dlightmap.cpp).
namespace qvr::dlightmap
{
// VR_WorldLightsCheck, each view: the frame's lights added into the lightmap (the frame's first view does it), or the
// map's own texels put back when off. True: they are there, the view draws the world without the lights' loop.
bool frame();
void registerCommands(); // vr_gl_world_dlmap_info
} // namespace qvr::dlightmap
