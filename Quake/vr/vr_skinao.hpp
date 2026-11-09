// vr_skinao.hpp -- ambient occlusion baked into a model's skin texels as it loads (vr_ao_skin_weapons for the weapons'
// view models, vr_ao_skin_models for the rest; strength vr_ao_skin_strength, reach vr_ao_skin_radius). On top of the
// per-vertex bake (vr_ao.hpp, vr_ao_models: the same rays, reach and near-hit cut): contact shadows smaller than a
// triangle (round rivets, under sights, in grooves). How: vr_skinao.cpp's header. The engine's calls (VR_SkinAOModel
// at the end of Mod_LoadAliasModel, VR_SkinAOApply in TexMgr_LoadImage8) are declared in vr_api_render.h.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::skinao
{

struct BakeInfo
{
    double seconds{0.0};
    int texels{0};  // skin texels covered by a face
    int samples{0}; // texel uses (shared texels counted once each use)
    int faces{0};
    float reach{0.f};
};

// `hdr`'s first pose baked into a `w` x `h` visibility map (255 open). False: nothing to bake.
[[nodiscard]] bool bake(const aliashdr_t* hdr, int w, int h, za::Vector<unsigned char>& vis, BakeInfo* info);

// The map kept for the skin texture `texName`, or null.
[[nodiscard]] const unsigned char* mapFor(const char* texName, int& w, int& h);

// The maps dropped (a game change: the textures are another game's).
void clear();

// Its test command (vr_skinao_test).
void init();

} // namespace qvr::skinao
