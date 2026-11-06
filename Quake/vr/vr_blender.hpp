#pragma once

// vr_blender.hpp -- the game's side of the Blender workflow (vr_blender.cpp; docs/vr-port/BLENDER_WORKFLOW.md):
// vr_edit_in_blender opens a model's source .blend in Blender, vr_model_show draws a model in front of you, and the
// Quake VR add-on's Show in Game reaches the game through a request file (vr_blender_link).

namespace qvr::blender
{

void registerCommands(); // vr_edit_in_blender, vr_edit_skin, vr_model_show, vr_blender_test
void frame();            // each frame (VR_HostFrameEnd): the request file looked at every half second
void addPreview();       // each frame, with the view's entities: the model vr_model_show shows

} // namespace qvr::blender
