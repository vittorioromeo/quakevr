// vr_sights.cpp -- see vr_sights.hpp.

#include "vr_sights.hpp"
#include "vr_alloccount.h"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_hue.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"
#include "vr_api_render.h"


using namespace qvr;

namespace
{

// The models whose skins carry glowing sights in the fire indices, and nothing else in them that
// is drawn (their muzzle flashes, if any, in other colours). The other weapons' fire texels are
// muzzle flashes (nailguns, launchers), the lava nailguns' lava and the laser cannon's coils.
constexpr const char* sightedModels[] = {
    "progs/v_shot.mdl",   // the shotgun: three inserts (the rear housing's two ears, the front post)
    "progs/v_shot2.mdl",  // the double shotgun: three inserts (the rear notch sight's two ears, the front post)
    "progs/g_shot0.mdl",  // the shotgun's pickup: its sights (Quake's dark red)
    "progs/v_light.mdl",  // the lightning gun: two rear posts, a front post (round 20)
    "progs/v_light_plasma.mdl", // its alternate, the plasma gun: the same (its coils in other reds)
    // The nail pistol (2026-11-07): its modelled sights (two rear ears, a front post) painted to glow. It is its own
    // pickup (QC impl_weapon_item); the art preview carries the same skin's colours. Its magazines' skins have no sights.
    "progs/v_nailpistol.mdl",
    "progs/v_nailpistol_lava.mdl", // its lava nails' look (nplava): the same sights (its lava glows in other fullbrights)
    "progs/vr_nailpistol_preview.mdl",
    // The guns' parts drawn in their place, made from their skins (the sights recoloured, as on the gun: the author saw
    // them change colour as the shotgun pumped and the super shotgun broke open).
    "progs/vr_pumpbody_on_v_shot.mdl",       // the shotgun's auto pump: the gun without its fore-end,
    "progs/vr_pump_on_v_shot.mdl",           // and the fore-end
    "progs/vr_ssg_frame_on_v_shot2.mdl",     // the super shotgun broken open: its frame,
    "progs/vr_ssg_barrels_on_v_shot2.mdl",   // and its barrels (the notch sight, the front post)
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
        const za::SizeT n = ZA_STRLEN(m);
        if(!q_strncasecmp(texname, m, static_cast<int>(n)) && texname[n] == ':')
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] float wrapDegrees(float h)
{
    return za::fmod(za::fmod(h, 360.f) + 360.f, 360.f);
}

// The sights' hue and saturation: their own (vr_sight_hue), or by default the player's
// (vr_player_hue, vr_player_saturation: vr_hue.hpp).
[[nodiscard]] float sightHue()
{
    return wrapDegrees(hue::of(vr_sight_hue));
}

[[nodiscard]] float sightSaturation()
{
    const float own = za::clamp(vr_sight_saturation.value, 0.f, 2.f);
    return hue::follows(vr_sight_hue) ? own * za::clamp(vr_player_saturation.value, 0.f, 2.f) : own;
}

// The front post's hue, turned this far (degrees) from the rear notches' (vr_sight_front_hue_shift).
[[nodiscard]] float frontShift()
{
    return wrapDegrees(vr_sight_front_hue_shift.value);
}

// The front posts in the skins: the texels (inclusive, in the skin's own size) the front post's triangles map
// its sight texels to, set apart from the rear notches' (which each skin maps both ears onto one patch of).
// Misc/quakevr/sight_front_posts.py finds them from the models' geometry (the patch furthest forward) and
// prints this table; a skin of another size (repainted, or a mod's) keeps one colour.
struct FrontPost
{
    const char* model;
    int skinWidth, skinHeight;
    int x0, y0, x1, y1;
};
constexpr FrontPost frontPosts[] = {
    {"progs/v_shot.mdl", 512, 219, 90, 94, 98, 102},
    {"progs/vr_pumpbody_on_v_shot.mdl", 512, 219, 90, 94, 98, 102},
    {"progs/vr_pump_on_v_shot.mdl", 512, 219, 90, 94, 98, 102},
    {"progs/v_shot2.mdl", 512, 146, 170, 103, 175, 113},
    {"progs/vr_ssg_frame_on_v_shot2.mdl", 512, 218, 170, 103, 175, 113},
    {"progs/vr_ssg_barrels_on_v_shot2.mdl", 512, 218, 170, 103, 175, 113},
    {"progs/v_light.mdl", 512, 178, 42, 96, 48, 102},
    {"progs/v_light_plasma.mdl", 512, 178, 42, 96, 48, 102},
    {"progs/v_nailpistol.mdl", 256, 512, 62, 470, 66, 475},
    {"progs/v_nailpistol_lava.mdl", 256, 512, 62, 470, 66, 475},
};
// The texels round each patch taken with it (vr_skin_edge_fill copies a sight texel into its triangles' edges): far
// less than the gap to the rear patch in every skin.
constexpr int frontMargin = 2;

// The front post table's entry for a skin texture ("progs/v_shot.mdl:frame0"), or null.
[[nodiscard]] const FrontPost* frontPostOf(const char* texname)
{
    for(const FrontPost& p : frontPosts)
    {
        const za::SizeT n = ZA_STRLEN(p.model);
        if(!q_strncasecmp(texname, p.model, static_cast<int>(n)) && texname[n] == ':')
        {
            return &p;
        }
    }
    return nullptr;
}

[[nodiscard]] bool inFrontPost(const FrontPost& p, int x, int y)
{
    return x >= p.x0 - frontMargin && x <= p.x1 + frontMargin && y >= p.y0 - frontMargin && y <= p.y1 + frontMargin;
}

// An RGB colour's (0..255) hue (degrees), saturation and value (0..1).
void toHsv(const za::U8* rgb, float& h, float& s, float& v)
{
    const float r = rgb[0] / 255.f, g = rgb[1] / 255.f, b = rgb[2] / 255.f;
    const float mx = za::max(r, g, b), mn = za::min(r, g, b);
    const float d = mx - mn;
    h = 0.f;
    if(d > 0.f)
    {
        if(mx == r)
        {
            h = 60.f * za::fmod((g - b) / d + 6.f, 6.f);
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
    s = mx > 0.f ? d / mx : 0.f;
    v = mx;
}

// An RGB colour (0..255) turned `shift` degrees round the hue circle, its saturation times
// `saturation`, its value (the brightest channel) kept.
void recolor(za::U8* rgb, float shift, float saturation)
{
    float h = 0.f, s = 0.f, v = 0.f;
    toHsv(rgb, h, s, v);
    if(v <= 0.f)
    {
        return;
    }
    const glm::vec3 c = hsv(h + shift, za::clamp(s * saturation, 0.f, 1.f), v);
    for(int k = 0; k < 3; ++k)
    {
        rgb[k] = static_cast<za::U8>(za::clamp(za::lround(c[k] * 255.f), 0L, 255L));
    }
}

// The sights' colour as last applied, to reload the skins only when it changed.
float appliedHue = ownHue;
float appliedSaturation = 1.f;
float appliedShift = 0.f;

void onSightColorChanged(cvar_t*)
{
    const float hue = sightHue();
    const float sat = sightSaturation();
    const float shift = frontShift();
    if(hue == appliedHue && sat == appliedSaturation && shift == appliedShift)
    {
        return;
    }
    appliedHue = hue;
    appliedSaturation = sat;
    appliedShift = shift;
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

namespace
{
// VR_SightPalette's state (the main thread: textures load there).
struct SightPalette
{
    bool callbacks = false;       // the cvars' callbacks set
    unsigned int recolored[256]; // the palette handed back (TexMgr_LoadImage8 converts the texture with it at once)
    unsigned int front[256];     // the front post's (VR_SightFront, straight after)
    const FrontPost* post = nullptr; // the texture's front post, while VR_SightFront has it to set apart
};
SightPalette sightPalette;

// `palette` with the sight entries recoloured: turned `shift` degrees, saturation times `sat`.
void recolorPalette(unsigned int (&out)[256], const unsigned int* palette, float shift, float sat)
{
    ZA_MEMCPY(out, palette, sizeof(out));
    for(int i = 0; i < 256; ++i)
    {
        if(isSightIndex(i))
        {
            recolor(reinterpret_cast<za::U8*>(&out[i]), shift, sat); // alpha (the byte after) kept
        }
    }
}
} // namespace

extern "C" unsigned int* VR_SightPalette(const char* texname, unsigned int* palette)
{
    // The cvars' callbacks, once they are registered (Cvar_RegisterVariable clears a callback): the
    // first sighted skin loads with a map, long after.
    bool& callbacks = sightPalette.callbacks;
    if(!callbacks && Cvar_FindVar(vr_sight_hue.name) == &vr_sight_hue)
    {
        Cvar_SetCallback(&vr_sight_hue, onSightColorChanged);
        Cvar_SetCallback(&vr_sight_saturation, onSightColorChanged);
        Cvar_SetCallback(&vr_player_hue, onSightColorChanged);
        Cvar_SetCallback(&vr_player_saturation, onSightColorChanged);
        Cvar_SetCallback(&vr_sight_front_hue_shift, onSightColorChanged);
        callbacks = true;
    }

    sightPalette.post = nullptr;
    if(!texname || !isSightedSkin(texname))
    {
        return palette;
    }
    const float hue = sightHue();
    const float sat = sightSaturation();
    const float shift = frontShift();
    appliedHue = hue;
    appliedSaturation = sat;
    appliedShift = shift;

    // TexMgr_LoadImage8 converts the texture with them straight away: one copy of each is enough.
    if(shift > 0.01f && shift < 359.99f)
    {
        sightPalette.post = frontPostOf(texname);
        if(sightPalette.post)
        {
            recolorPalette(sightPalette.front, palette, hue - ownHue + shift, sat);
        }
    }
    if(za::fabs(hue - ownHue) < 0.01f && za::fabs(sat - 1.f) < 0.001f)
    {
        return palette; // as painted: nothing changes
    }
    recolorPalette(sightPalette.recolored, palette, hue - ownHue, sat);
    return sightPalette.recolored;
}

extern "C" void VR_SightFront(const char* texname, const unsigned char* data8, unsigned int* data32, int width, int height,
                              unsigned int srcw, unsigned int srch)
{
    const FrontPost* post = sightPalette.post;
    sightPalette.post = nullptr;
    if(!post || !texname || frontPostOf(texname) != post || static_cast<int>(srcw) != post->skinWidth ||
       static_cast<int>(srch) != post->skinHeight || width < post->skinWidth || height < post->skinHeight)
    {
        return;
    }
    const int x0 = za::max(post->x0 - frontMargin, 0), x1 = za::min(post->x1 + frontMargin, post->skinWidth - 1);
    const int y0 = za::max(post->y0 - frontMargin, 0), y1 = za::min(post->y1 + frontMargin, post->skinHeight - 1);
    for(int y = y0; y <= y1; ++y)
    {
        for(int x = x0; x <= x1; ++x)
        {
            const int i = y * width + x;
            if(isSightIndex(data8[i]))
            {
                data32[i] = sightPalette.front[data8[i]];
            }
        }
    }
}

namespace
{

// vr_sight_front_test: each front post table entry against its model's skin (its size; sight texels in the front
// patch and outside it) and the upload's colours with the current settings: the front post's texels at the rear's
// hue turned vr_sight_front_hue_shift, the rest at the rear's. "vr_sight_front_test: N passed, M failed".
void frontTest_f()
{
    testkit::Tally tally;
    const float rearTurn = sightHue() - ownHue;
    const float shift = frontShift();
    for(const FrontPost& p : frontPosts)
    {
        byte* file = COM_LoadMallocFile(p.model, nullptr);
        const int size = file ? com_filesize : 0;
        const char* why = nullptr;
        int w = 0, h = 0;
        const byte* skin = nullptr;
        if(!file || size < 92 || ZA_MEMCMP(file, "IDPO", 4) != 0)
        {
            why = "no model";
        }
        else
        {
            ZA_MEMCPY(&w, file + 52, 4);
            ZA_MEMCPY(&h, file + 56, 4);
            int group = 0;
            ZA_MEMCPY(&group, file + 84, 4);
            int offset = 88;
            if(group != 0)
            {
                int n = 0;
                ZA_MEMCPY(&n, file + 88, 4);
                offset = 92 + 4 * n;
            }
            if(w != p.skinWidth || h != p.skinHeight)
            {
                why = "its skin's size changed (Misc/quakevr/sight_front_posts.py)";
            }
            else if(offset + w * h > size)
            {
                why = "a short skin";
            }
            else
            {
                skin = file + offset;
            }
        }
        int front = 0, rear = 0, wrong = 0;
        if(skin)
        {
            char name[MAX_QPATH];
            q_snprintf(name, sizeof(name), "%s:frame0", p.model);
            za::Vector<unsigned int> out;
            out.resize(static_cast<za::SizeT>(w * h));
            const unsigned int* pal = VR_SightPalette(name, d_8to24table);
            for(int i = 0; i < w * h; ++i)
            {
                out[static_cast<za::SizeT>(i)] = pal[skin[i]];
            }
            VR_SightFront(name, skin, out.data(), w, h, static_cast<unsigned int>(w), static_cast<unsigned int>(h));
            for(int y = 0; y < h; ++y)
            {
                for(int x = 0; x < w; ++x)
                {
                    const int i = y * w + x;
                    if(!isSightIndex(skin[i]))
                    {
                        continue;
                    }
                    const bool isFront = inFrontPost(p, x, y);
                    (isFront ? front : rear) += 1;
                    float h0 = 0.f, s0 = 0.f, v0 = 0.f, h1 = 0.f, s1 = 0.f, v1 = 0.f;
                    toHsv(reinterpret_cast<const za::U8*>(&d_8to24table[skin[i]]), h0, s0, v0);
                    toHsv(reinterpret_cast<const za::U8*>(&out[static_cast<za::SizeT>(i)]), h1, s1, v1);
                    if(s1 < 0.2f || v1 < 0.3f)
                    {
                        continue; // too pale or dark for its hue to be read back
                    }
                    const float want = wrapDegrees(h0 + rearTurn + (isFront ? shift : 0.f));
                    const float off = za::fabs(wrapDegrees(h1 - want + 180.f) - 180.f);
                    wrong += off > 8.f ? 1 : 0;
                }
            }
            if(!front || !rear)
            {
                why = "no sight texels in the front patch or outside it";
            }
            else if(wrong)
            {
                why = "texels of the wrong hue";
            }
        }
        if(file)
        {
            VR_HeapFree(file);
        }
        Con_Printf("vr_sight_front_test: %s: %s (front %d, rear %d, wrong %d, shift %.0f)\n", p.model,
                   why ? why : "ok", front, rear, wrong, shift);
        tally.check(!why);
    }
    tally.verdict("vr_sight_front_test");
}

} // namespace

namespace qvr::sights
{

void init()
{
    Cmd_AddCommand("vr_sight_front_test", frontTest_f);
}

} // namespace qvr::sights
