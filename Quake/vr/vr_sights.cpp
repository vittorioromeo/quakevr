// vr_sights.cpp -- see vr_sights.hpp.

#include "vr_sights.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_hue.hpp"
#include "vr_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace qvr;

namespace
{

// The models whose skins carry glowing sights in the fire indices, and nothing else in them that
// is drawn (their muzzle flashes, if any, in other colours). The other weapons' fire texels are
// muzzle flashes (nailguns, launchers), the lava nailguns' lava and the laser cannon's coils.
constexpr const char* sightedModels[] = {
    "progs/v_shot.mdl",   // the shotgun: three sights (two rear posts, a front post)
    "progs/v_shot2.mdl",  // the double shotgun: a ring sight
    "progs/g_shot0.mdl",  // the shotgun's pickup: its sights (Quake's dark red)
};

// The hue (degrees) the sights' gradient has as painted: its bright orange (palette 236, 227,151,79,
// is 29; the gradient runs from 6 at the deep red rim to 38 at the pale yellow core). The bright
// parts carry the colour one sees (and the glow): the chosen hue is theirs.
constexpr float ownHue = 30.f;

[[nodiscard]] bool isSightIndex(int i)
{
    return (i >= 224 && i <= 239) || i == 252 || i == 253;
}

// The model a skin texture belongs to: its name up to the ':' ("progs/v_shot.mdl:frame0",
// "progs/v_shot.mdl:frame0_glow", "progs/v_shot.mdl:frame0_1" for a skin group's).
[[nodiscard]] bool isSightedSkin(const char* texname)
{
    for(const char* m : sightedModels)
    {
        const std::size_t n = std::strlen(m);
        if(!q_strncasecmp(texname, m, static_cast<int>(n)) && texname[n] == ':')
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] float wrapDegrees(float h)
{
    return std::fmod(std::fmod(h, 360.f) + 360.f, 360.f);
}

// The sights' hue and saturation: their own (vr_sight_hue), or by default the player's
// (vr_player_hue, vr_player_saturation: vr_hue.hpp).
[[nodiscard]] float sightHue()
{
    return wrapDegrees(hue::of(vr_sight_hue));
}

[[nodiscard]] float sightSaturation()
{
    const float own = std::clamp(vr_sight_saturation.value, 0.f, 2.f);
    return hue::follows(vr_sight_hue) ? own * std::clamp(vr_player_saturation.value, 0.f, 2.f) : own;
}

// An RGB colour (0..255) turned `shift` degrees round the hue circle, its saturation times
// `saturation`, its value (the brightest channel) kept.
void recolor(std::uint8_t* rgb, float shift, float saturation)
{
    const float r = rgb[0] / 255.f, g = rgb[1] / 255.f, b = rgb[2] / 255.f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    if(mx <= 0.f)
    {
        return;
    }
    const float d = mx - mn;
    float h = 0.f;
    if(d > 0.f)
    {
        if(mx == r)
        {
            h = 60.f * std::fmod((g - b) / d + 6.f, 6.f);
        }
        else if(mx == g)
        {
            h = 60.f * ((b - r) / d + 2.f);
        }
        else
        {
            h = 60.f * ((r - g) / d + 4.f);
        }
    }
    const float s = std::clamp(d / mx * saturation, 0.f, 1.f);
    const glm::vec3 c = hsv(h + shift, s, mx);
    for(int k = 0; k < 3; ++k)
    {
        rgb[k] = static_cast<std::uint8_t>(std::clamp(std::lround(c[k] * 255.f), 0L, 255L));
    }
}

// The sights' colour as last applied, to reload the skins only when it changed.
float appliedHue = ownHue;
float appliedSaturation = 1.f;

void onSightColorChanged(cvar_t*)
{
    const float hue = sightHue();
    const float sat = sightSaturation();
    if(hue == appliedHue && sat == appliedSaturation)
    {
        return;
    }
    appliedHue = hue;
    appliedSaturation = sat;
    // Loaded skins are uploaded again (with VR_SightPalette's palette); ones not loaded yet take it
    // as they load.
    for(const char* m : sightedModels)
    {
        char prefix[MAX_QPATH];
        q_snprintf(prefix, sizeof(prefix), "%s:", m);
        TexMgr_ReloadImagesNamed(prefix);
    }
}

} // namespace

extern "C" unsigned int* VR_SightPalette(const char* texname, unsigned int* palette)
{
    // The cvars' callbacks, once they are registered (Cvar_RegisterVariable clears a callback): the
    // first sighted skin loads with a map, long after.
    static bool callbacks = false;
    if(!callbacks && Cvar_FindVar(vr_sight_hue.name) == &vr_sight_hue)
    {
        Cvar_SetCallback(&vr_sight_hue, onSightColorChanged);
        Cvar_SetCallback(&vr_sight_saturation, onSightColorChanged);
        Cvar_SetCallback(&vr_player_hue, onSightColorChanged);
        Cvar_SetCallback(&vr_player_saturation, onSightColorChanged);
        callbacks = true;
    }

    if(!texname || !isSightedSkin(texname))
    {
        return palette;
    }
    const float hue = sightHue();
    const float sat = sightSaturation();
    appliedHue = hue;
    appliedSaturation = sat;
    if(std::fabs(hue - ownHue) < 0.01f && std::fabs(sat - 1.f) < 0.001f)
    {
        return palette; // as painted: nothing changes
    }

    // TexMgr_LoadImage8 converts the texture with it straight away: one copy is enough.
    static unsigned int recolored[256];
    std::memcpy(recolored, palette, sizeof(recolored));
    for(int i = 0; i < 256; ++i)
    {
        if(isSightIndex(i))
        {
            recolor(reinterpret_cast<std::uint8_t*>(&recolored[i]), hue - ownHue, sat); // alpha (the byte after) kept
        }
    }
    return recolored;
}
