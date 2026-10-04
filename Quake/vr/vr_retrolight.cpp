/*
Copyright (C) 2020-2026 Vittorio Romeo and Quake VR contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// vr_retrolight.cpp -- retro lighting (Graphics > Retro Lighting; docs/vr-port/ROUND21.md, "Retro lighting"): the
// settings into the frame data's RetroLight[6] (the shaders' side: vr_retrolight.h).
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::retrolight
{
namespace
{

float clampv(const cvar_t& c, float lo, float hi)
{
    return za::clamp(c.value, lo, hi);
}

// A grid's block in texels: 0 (none) or a power of two from `finest` up to 16 (a luxel; the faces' grids line up
// across them). `finest` is 1 for the grids fixed to a texture (the lightmap's, the dynamic lights': a block smaller
// than a texel would be the same as none); the shadows' may go below a texel (1/2, 1/4) -- the shaders turn a grid
// smooth where its blocks get smaller than a pixel, so those only show where a texel does.
float block(const cvar_t& c, const float finest = 1.f)
{
    const float v = c.value;
    if(v < finest * 0.5f)
    {
        return 0.f;
    }
    float b = finest;
    while(b < 16.f && b * 1.5f <= v)
    {
        b *= 2.f;
    }
    return b;
}

// The dither's cell, in texels: 1, 2 or 4 (its 4 x 4 pattern repeats every 16 texels or less: whole across faces).
float ditherCell(const cvar_t& c)
{
    return c.value >= 3.f ? 4.f : c.value >= 1.5f ? 2.f : 1.f;
}

} // namespace

bool on()
{
    return vr_retrolight.value != 0.f && vr_retrolight_ab.value == 0.f;
}

} // namespace qvr::retrolight

// R_SetupView: the frame data's RetroLight[6] (vr_retrolight.h's layout); all 0 when off.
extern "C" void VR_RetroLightFrameData(float out[24])
{
    using namespace qvr;
    ZA_MEMSET(out, 0, sizeof(float) * 24);
    if(!retrolight::on())
    {
        return;
    }
    const bool world = vr_retrolight_world.value != 0.f, models = vr_retrolight_models.value != 0.f;
    const float edge = retrolight::clampv(vr_retrolight_edge_soft, 0.f, 4.f);
    if(world)
    {
        out[0] = retrolight::clampv(vr_retrolight_world_steps, 0.f, 256.f);
        out[1] = retrolight::clampv(vr_retrolight_world_soft, 0.f, 1.f);
        out[2] = retrolight::clampv(vr_retrolight_world_dither, 0.f, 1.f);
        out[3] = retrolight::ditherCell(vr_retrolight_world_dither_size);
        out[4] = vr_retrolight_world_lightmap.value != 0.f ? 1.f : 0.f;
        out[5] = za::max(retrolight::block(vr_retrolight_world_luxel), 1.f);
        out[7] = retrolight::block(vr_retrolight_world_dyn_block);
        out[8] = retrolight::clampv(vr_retrolight_world_dyn_steps, 0.f, 256.f);
        out[11] = 1.f;
    }
    out[6] = edge;
    out[9] = vr_retrolight_spacing.value != 0.f ? 0.5f : 1.f;
    out[12] = static_cast<float>(za::clamp(static_cast<int>(vr_retrolight_shadow_filter.value), 0, 2));
    out[13] = retrolight::clampv(vr_retrolight_shadow_steps, 0.f, 64.f);
    out[14] = retrolight::block(vr_retrolight_shadow_block, 0.25f); // down to a quarter of a texel
    out[15] = retrolight::clampv(vr_retrolight_shadow_soft, 0.f, 1.f);
    if(models)
    {
        out[16] = retrolight::clampv(vr_retrolight_model_steps, 0.f, 256.f);
        out[17] = retrolight::clampv(vr_retrolight_model_soft, 0.f, 1.f);
        out[18] = retrolight::clampv(vr_retrolight_model_dither, 0.f, 1.f);
        out[19] = retrolight::ditherCell(vr_retrolight_model_dither_size);
        out[20] = retrolight::clampv(vr_retrolight_model_dyn_steps, 0.f, 256.f);
        out[21] = retrolight::block(vr_retrolight_model_dyn_block);
        out[22] = 1.f;
    }
}

// VR_AliasInstance: the share of the skin's texture that is a Quake texel (Retro.w: retro lighting's grid on models):
// a Quake .mdl's skin size over its texture's (an external HQ skin is larger); our own MD3 and IQM models' a quarter
// (painted at about four times a Quake skin's density, as retro textures take them).
extern "C" float VR_RetroLightSkinScale(const void* aliashdr, int skinnum)
{
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!hdr)
    {
        return 0.f;
    }
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return 0.25f;
    }
    const int skin = skinnum >= 0 && skinnum < hdr->numskins ? skinnum : 0;
    const gltexture_t* g = hdr->gltextures[skin][0];
    if(!g || g->width <= 0 || hdr->skinwidth <= 0)
    {
        return 0.f;
    }
    return static_cast<float>(hdr->skinwidth) / static_cast<float>(g->width);
}
