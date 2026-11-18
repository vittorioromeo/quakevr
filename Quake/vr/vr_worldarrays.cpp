#include "vr_alloccount.h"
// vr_worldarrays.cpp -- the world's textures in texture arrays for the bound path (QUEST_PLAN.md, C2f).
//
// Without bindless textures (the Quest's OpenGL ES; PCs without ARB_bindless_texture) the world draws a call a texture:
// a bind and a draw each (r_world.c R_FlushBModelCalls). Here the map's textures go into arrays at its load: one set of
// four arrays (the texture, its fullbright, normal and specular maps) for the textures alike in size, mip levels and
// formats, a texture a layer, the same layer in all four. A map shared by the set's textures (the black of no
// fullbright, the flat normal, the white of no specular map: tiny, or used by more than one texture) is a one-layer
// array (the shader's layer clamps to it: the textureSize the bound path has). A flush then draws each set's calls as
// one draw with the world programs' array twins (gl_shaders.c GL_WorldArraysProgram): their indices one after another
// (world_arrays_merge_compute_shader), each vertex's call from its command (r_brush.c's vertex commands).
//
// The arrays are copies (glCopyImageSubData: every mip level, external packs' sizes and formats, the normal and
// specular maps as made); a call whose maps are not those copied (a setting changed them, a texture reloaded) draws on
// the bound path as before, and the arrays are made again at the next batch. Filtering follows the textures'
// (TexMgr_SetFilterModes' serial). vr_world_texarrays: 0 off, 1 (default) under the ES profile (-glescompat, Android),
// 2 on any bound path. vr_world_texarrays_report: the sets and the last frame's draws.

#include "vr_worldarrays.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_api.h"
#include "vr_api_render.h"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif
#include <string.h>

extern "C" int texmgr_filter_serial; // gl_texmgr.c: TexMgr_SetFilterModes' calls
extern "C" GLuint gl_bmodel_vertcmd_buffer; // r_brush.c: each vertex's command (0: not made, bindless)
extern "C" void R_BModelCallTextures(texture_t* t, gltexture_t* out[4]); // r_world.c: a call's four maps

namespace qvr::worldarrays
{
namespace
{

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_TEXTURE_IMMUTABLE_LEVELS
#define GL_TEXTURE_IMMUTABLE_LEVELS 0x82DF
#endif
#ifndef GL_TEXTURE_IMMUTABLE_FORMAT
#define GL_TEXTURE_IMMUTABLE_FORMAT 0x912F
#endif

using CopyImageFn = void(APIENTRY*)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint,
    GLint, GLsizei, GLsizei, GLsizei);

constexpr int kSlots = 4; // the texture, its fullbright, its normal map, its specular map (units 0, 1, 3, 11)
constexpr GLenum kUnits[kSlots] = {GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE3, GL_TEXTURE11};
constexpr int kMaxLayers = 256; // OpenGL ES 3's least GL_MAX_ARRAY_TEXTURE_LAYERS
constexpr int kTinyTexels = 64; // a map this small (the defaults) is shared: one layer

// A map as it is in OpenGL: its size, mip levels and format
struct MapInfo
{
    int w = 0, h = 0, levels = 0;
    GLint format = 0;
};

// A set's slot: one layer of a shared map, or a layer each of maps alike
struct Slot
{
    gltexture_t* shared = nullptr; // non-null: that map, one layer
    MapInfo info;
    GLuint array = 0;
};

struct Set
{
    Slot slots[kSlots];
    za::Vector<const texture_t*> members; // its layers, in order
};

// A world texture's place: its set and layer, and the maps copied there (checked at each call)
struct Entry
{
    int set = -1, layer = 0;
    gltexture_t* maps[kSlots] = {};
    GLuint texnums[kSlots] = {};
};

struct State
{
    za::Vector<Set> sets;
    ankerl::unordered_dense::map<const texture_t*, Entry> entries;
    CopyImageFn copyImage = nullptr;
    bool copyTried = false;
    bool wanted = false;     // a new map's arrays to make (VR_WorldArraysBegin)
    bool stale = false;      // a call's maps were not those copied: made again
    int rebuilds = 0;        // since the map's load (more than kMaxRebuilds: left to the bound path)
    int builtFrame = -1;     // the frame they were last made in (once a frame at most)
    int filterSerial = -1;   // texmgr_filter_serial when the filtering was copied
    double megabytes = 0.0;
    int textures = 0, skipped = 0;
    // the last frame's draws (vr_world_texarrays_report)
    int statFrame = -1;
    int merged[4] = {}, lastMerged[4] = {}; // draws of many calls, calls in them, calls drawn alone in an array twin, calls on the bound path
};
State st;
constexpr int kMaxRebuilds = 16;

bool wantedHere()
{
    if(gl_bindless_able || !gl_bmodel_vertcmd_buffer)
    {
        return false;
    }
    const int mode = static_cast<int>(vr_world_texarrays.value);
    return mode >= 2 || (mode == 1 && (VR_GLESCompat() || VR_Android()));
}

// An unsized format (TexMgr's GL_RGB, GL_RGBA) as the sized one it stands for: the arrays' storage needs one
GLint sizedFormat(GLint format)
{
    switch(format)
    {
    case GL_RGB: return GL_RGB8;
    case GL_RGBA: return GL_RGBA8;
    case GL_RG: return GL_RG8;
    case GL_RED: return GL_R8;
    default: return format;
    }
}

MapInfo query(gltexture_t* glt)
{
    MapInfo m;
    if(!glt || glt->target != GL_TEXTURE_2D || !glt->texnum)
    {
        return m;
    }
    GL_Bind(GL_TEXTURE0, glt);
    GLint w = 0, h = 0, format = 0, immutable = 0, levels = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
    if(immutable)
    {
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
    }
    else
    {
        // the levels given, while each is the one before halved (a full chain's start; a gap ends it)
        for(levels = 1; levels < 16; levels++)
        {
            GLint lw = 0, lh = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, levels, GL_TEXTURE_WIDTH, &lw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, levels, GL_TEXTURE_HEIGHT, &lh);
            if(lw != (w >> levels > 0 ? w >> levels : 1) || lh != (h >> levels > 0 ? h >> levels : 1))
            {
                break;
            }
        }
        if(!(glt->flags & TEXPREF_MIPMAP))
        {
            levels = 1;
        }
    }
    if(w <= 0 || h <= 0 || levels <= 0 || !format)
    {
        return MapInfo{};
    }
    m.w = w;
    m.h = h;
    m.levels = levels;
    m.format = sizedFormat(format);
    return m;
}

bool sameInfo(const MapInfo& a, const MapInfo& b)
{
    return a.w == b.w && a.h == b.h && a.levels == b.levels && a.format == b.format;
}

void clearArrays()
{
    for(Set& s : st.sets)
    {
        for(Slot& slot : s.slots)
        {
            if(slot.array)
            {
                GL_DeleteNativeTexture(slot.array);
            }
        }
    }
    st.sets.clear();
    st.entries.clear();
    st.megabytes = 0.0;
    st.textures = st.skipped = 0;
}

// The array's filtering and wrapping: its first map's (TexMgr_SetFilterModes')
void copyFiltering(Slot& slot, gltexture_t* from)
{
    GLint minf = 0, magf = 0, ws = 0, wt = 0;
    GLfloat aniso = 1.f, bias = 0.f;
    GL_Bind(GL_TEXTURE0, from);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minf);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magf);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &ws);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wt);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
    if(!VR_GLESCompat())
    {
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &bias);
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, slot.array);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, minf);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, magf);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, ws);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, wt);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
    if(!VR_GLESCompat())
    {
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_LOD_BIAS, bias);
    }
}

gltexture_t* slotSource(const Set& s, int slot)
{
    return s.slots[slot].shared ? s.slots[slot].shared : st.entries[s.members[0]].maps[slot];
}

void refreshFiltering()
{
    for(Set& s : st.sets)
    {
        for(int i = 0; i < kSlots; i++)
        {
            if(s.slots[i].array)
            {
                copyFiltering(s.slots[i], slotSource(s, i));
            }
        }
    }
    st.filterSerial = texmgr_filter_serial;
}

double bytesOf(const MapInfo& m, int layers)
{
    double texels = 0.0;
    for(int l = 0; l < m.levels; l++)
    {
        texels += static_cast<double>((m.w >> l) > 0 ? m.w >> l : 1) * ((m.h >> l) > 0 ? m.h >> l : 1);
    }
    return texels * layers * 4.0; // (compressed formats less: a bound)
}

// Every world texture drawn by the array twins (not the sky, not the liquids), its maps as R_AddBModelCall picks them
void build()
{
    clearArrays();
    if(!st.copyTried)
    {
        st.copyTried = true;
        st.copyImage = reinterpret_cast<CopyImageFn>(SDL_GL_GetProcAddress("glCopyImageSubData"));
    }
    qmodel_t* world = cl.worldmodel;
    if(!st.copyImage || !world || !world->textures)
    {
        return;
    }

    struct Candidate
    {
        const texture_t* t;
        gltexture_t* maps[kSlots];
        MapInfo info[kSlots];
    };
    za::Vector<Candidate> cands;
    ankerl::unordered_dense::map<const gltexture_t*, int> uses[kSlots];
    for(int i = 0; i < world->numtextures; i++)
    {
        texture_t* t = world->textures[i];
        if(!t || !t->gltexture || t->type == TEXTYPE_SKY || TEXTYPE_ISLIQUID(t->type))
        {
            continue;
        }
        Candidate c{};
        c.t = t;
        R_BModelCallTextures(t, c.maps);
        bool ok = true;
        for(int k = 0; k < kSlots && ok; k++)
        {
            c.info[k] = query(c.maps[k]);
            ok = c.info[k].levels > 0;
        }
        if(!ok)
        {
            st.skipped++;
            continue;
        }
        for(int k = 0; k < kSlots; k++)
        {
            uses[k][c.maps[k]]++;
        }
        cands.pushBack(c);
    }

    for(const Candidate& c : cands)
    {
        // its slots' kinds: shared (one layer), or a layer of maps alike
        Slot want[kSlots];
        for(int k = 0; k < kSlots; k++)
        {
            const bool shared = uses[k][c.maps[k]] > 1 || c.info[k].w * c.info[k].h <= kTinyTexels;
            want[k].shared = shared ? c.maps[k] : nullptr;
            want[k].info = c.info[k];
        }
        if(want[0].shared) // the texture itself shared (two names, one image): left to the bound path
        {
            st.skipped++;
            continue;
        }
        int found = -1;
        for(za::SizeT si = 0; si < st.sets.size() && found < 0; si++)
        {
            const Set& s = st.sets[si];
            bool same = static_cast<int>(s.members.size()) < kMaxLayers;
            for(int k = 0; k < kSlots && same; k++)
            {
                same = s.slots[k].shared == want[k].shared && sameInfo(s.slots[k].info, want[k].info);
            }
            if(same)
            {
                found = static_cast<int>(si);
            }
        }
        if(found < 0)
        {
            Set s;
            for(int k = 0; k < kSlots; k++)
            {
                s.slots[k].shared = want[k].shared;
                s.slots[k].info = want[k].info;
            }
            st.sets.pushBack(s);
            found = static_cast<int>(st.sets.size()) - 1;
        }
        Set& s = st.sets[static_cast<za::SizeT>(found)];
        Entry e;
        e.set = found;
        e.layer = static_cast<int>(s.members.size());
        for(int k = 0; k < kSlots; k++)
        {
            e.maps[k] = c.maps[k];
            e.texnums[k] = c.maps[k]->texnum;
        }
        s.members.pushBack(c.t);
        st.entries[c.t] = e;
    }

    // the arrays: storage, then every layer's levels copied (an OpenGL error on the way: none, the bound path)
    while(glGetError() != GL_NO_ERROR)
    {
    }
    for(Set& s : st.sets)
    {
        for(int k = 0; k < kSlots; k++)
        {
            Slot& slot = s.slots[k];
            const int layers = slot.shared ? 1 : static_cast<int>(s.members.size());
            glGenTextures(1, &slot.array);
            GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, slot.array);
            GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, slot.info.levels, static_cast<GLenum>(slot.info.format), slot.info.w,
                slot.info.h, layers);
            for(int layer = 0; layer < layers; layer++)
            {
                gltexture_t* src = slot.shared ? slot.shared : st.entries[s.members[static_cast<za::SizeT>(layer)]].maps[k];
                for(int l = 0; l < slot.info.levels; l++)
                {
                    const int lw = (slot.info.w >> l) > 0 ? slot.info.w >> l : 1;
                    const int lh = (slot.info.h >> l) > 0 ? slot.info.h >> l : 1;
                    st.copyImage(src->texnum, GL_TEXTURE_2D, l, 0, 0, 0, slot.array, GL_TEXTURE_2D_ARRAY, l, 0, 0, layer,
                        lw, lh, 1);
                }
            }
            st.megabytes += bytesOf(slot.info, layers) / (1024.0 * 1024.0);
        }
        st.textures += static_cast<int>(s.members.size());
    }
    const GLenum err = glGetError();
    if(err != GL_NO_ERROR)
    {
        Con_Printf("world arrays: OpenGL error 0x%X making them: the world draws a texture at a time\n", err);
        while(glGetError() != GL_NO_ERROR)
        {
        }
        clearArrays();
        st.rebuilds = kMaxRebuilds + 1; // (not again this map)
        return;
    }
    refreshFiltering();
}

void reportCmd()
{
    Con_Printf("vr_world_texarrays: %s (%d)%s\n", wantedHere() ? "on" : "off", static_cast<int>(vr_world_texarrays.value),
        gl_bindless_able ? ", bindless (not used)" : "");
    Con_Printf("world arrays: %d sets, %d textures, %d left out, %.1f MB, %d rebuilds\n", static_cast<int>(st.sets.size()),
        st.textures, st.skipped, st.megabytes, st.rebuilds);
    Con_Printf("world arrays last frame: %d draws of many, %d calls in them, %d alone, %d on the bound path\n",
        st.lastMerged[0], st.lastMerged[1], st.lastMerged[2], st.lastMerged[3]);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_world_texarrays_report", reportCmd);
}

} // namespace qvr::worldarrays

using namespace qvr::worldarrays;

// GL_BuildBModelMarkBuffers: a map's buffers made: its arrays at the next batch
extern "C" void VR_WorldArraysNewMap(void)
{
    st.wanted = true;
    st.stale = false;
    st.rebuilds = 0;
    st.builtFrame = -1; // (R_NewMap starts r_framecount again: the last map's frame numbers would skip this one's)
    st.statFrame = -1;
}

// GL_DeleteBModelBuffers
extern "C" void VR_WorldArraysClear(void)
{
    clearArrays();
    st.wanted = false;
}

// R_ResetBModelCalls (no call pending): the arrays made, or made again, when wanted; nonzero: in use (2: made just now)
extern "C" int VR_WorldArraysBegin(void)
{
    if(!wantedHere())
    {
        if(!st.sets.empty())
        {
            clearArrays();
            st.wanted = true; // (made when turned on again)
        }
        return 0;
    }
    if(st.statFrame != r_framecount)
    {
        memcpy(st.lastMerged, st.merged, sizeof(st.merged));
        memset(st.merged, 0, sizeof(st.merged));
        st.statFrame = r_framecount;
    }
    bool built = false;
    if((st.wanted || st.stale) && st.builtFrame != r_framecount && st.rebuilds <= kMaxRebuilds)
    {
        built = true;
        st.rebuilds += st.wanted ? 0 : 1;
        st.wanted = st.stale = false;
        st.builtFrame = r_framecount;
        build();
    }
    else if(st.filterSerial != texmgr_filter_serial && !st.sets.empty())
    {
        refreshFiltering();
    }
    return st.sets.empty() ? 0 : built ? 2 : 1;
}

// R_AddBModelCall: texture t's set (-1: none, the bound path) and its layer, `maps` the call's four
extern "C" int VR_WorldArraysLookup(const texture_t* t, gltexture_t* const maps[4], int* layer)
{
    if(!t || st.sets.empty())
    {
        return -1;
    }
    const auto it = st.entries.find(t);
    if(it == st.entries.end())
    {
        return -1;
    }
    const Entry& e = it->second;
    for(int k = 0; k < kSlots; k++)
    {
        if(e.maps[k] != maps[k] || e.texnums[k] != maps[k]->texnum)
        {
            st.stale = true;
            return -1;
        }
    }
    *layer = e.layer;
    return e.set;
}

// R_FlushBModelCalls: set `set`'s four arrays on the world shader's units
extern "C" void VR_WorldArraysBind(int set)
{
    if(set < 0 || set >= static_cast<int>(st.sets.size()))
    {
        return;
    }
    const Set& s = st.sets[static_cast<za::SizeT>(set)];
    for(int k = 0; k < kSlots; k++)
    {
        GL_BindNative(kUnits[k], GL_TEXTURE_2D_ARRAY, s.slots[k].array);
    }
}

// R_FlushBModelCalls: a flush's draws (vr_world_texarrays_report)
extern "C" void VR_WorldArraysCount(int manyDraws, int callsInMany, int alone, int bound)
{
    st.merged[0] += manyDraws;
    st.merged[1] += callsInMany;
    st.merged[2] += alone;
    st.merged[3] += bound;
}
