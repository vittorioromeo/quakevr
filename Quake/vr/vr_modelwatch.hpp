#pragma once

// vr_modelwatch.hpp -- vr_model_watch (vr_modelwatch.cpp): the loaded models' files watched on disk, and a model whose
// file changed read again as vr_model_reload reads it (the Blender add-on's export on save: docs/vr-port/
// MODELS_IN_BLENDER.md, "See it in the game").

namespace qvr::modelwatch
{

void registerCommands(); // vr_model_watch_status
void frame();            // each frame (VR_HostFrameEnd): a share of the files' times looked at

} // namespace qvr::modelwatch
