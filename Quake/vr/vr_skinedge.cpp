#include "vr_alloccount.h"
// vr_skinedge.cpp -- skin edge fill: the texels a model's triangles touch only at an edge filled from inside (see
// vr_skinedge.hpp).

#include "vr_skinedge.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_strings.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr float eps = 1e-3f; // texels: a middle this near a triangle's edge is in it; a touch this thin is none

// A model with edge texels: its skin's size, its (texel, texel copied into it; y * width + x) pairs at `first` in the
// pairs (values, two a pair).
struct Entry
{
    char model[MAX_QPATH]{};
    int width{0};
    int height{0};
    za::SizeT first{0};
    za::SizeT count{0};
    int filled{0}; // uploads filled
};

// The models loaded since the game dir was (main thread: models and textures load there). A model loaded again
// replaces its entry.
struct EdgeCache
{
    za::Vector<Entry> entries;
    za::Vector<za::U32> pairs;
    auto members() { return mem::list(entries, pairs); }
};
mem::Cache<EdgeCache> cache{"skin edges", mem::GameDirChange};

// A model's working masks while its pairs are made.
struct EdgeScratch
{
    za::Vector<za::U8> inside;  // per texel: 1 a triangle has its middle
    za::Vector<za::U32> source; // per texel: 1 + the texel it copies (0 none yet)
    za::Vector<za::U32> touched; // the edge texels found, in the order found
    auto members() { return mem::list(inside, source, touched); }
};
mem::Scratch<EdgeScratch> scratch{"skin edges"};

struct Tri
{
    float x[3];
    float y[3];
    float ex[3]; // edge j: corner j to corner j + 1
    float ey[3];
    float len[3];
    float area2; // twice the signed area (0: a line or a point)
};

[[nodiscard]] Tri makeTri(const float (&x)[3], const float (&y)[3])
{
    Tri t;
    for(int j = 0; j < 3; j++)
    {
        t.x[j] = x[j];
        t.y[j] = y[j];
    }
    for(int j = 0; j < 3; j++)
    {
        const int k = (j + 1) % 3;
        t.ex[j] = t.x[k] - t.x[j];
        t.ey[j] = t.y[k] - t.y[j];
        t.len[j] = za::sqrt(t.ex[j] * t.ex[j] + t.ey[j] * t.ey[j]);
    }
    t.area2 = t.ex[0] * (t.y[2] - t.y[0]) - t.ey[0] * (t.x[2] - t.x[0]);
    return t;
}

// The point (px, py) in the triangle (closed, eps); a line or a point: on it.
[[nodiscard]] bool contains(const Tri& t, float px, float py)
{
    if(za::abs(t.area2) > 1e-6f)
    {
        const float sign = t.area2 > 0.f ? 1.f : -1.f;
        for(int j = 0; j < 3; j++)
        {
            if((t.ex[j] * (py - t.y[j]) - t.ey[j] * (px - t.x[j])) * sign < -eps * t.len[j])
            {
                return false;
            }
        }
        return true;
    }
    for(int j = 0; j < 3; j++)
    {
        const float dx = px - t.x[j], dy = py - t.y[j];
        if(t.len[j] <= 0.f)
        {
            if(za::abs(dx) < eps && za::abs(dy) < eps)
            {
                return true;
            }
            continue;
        }
        const float along = (dx * t.ex[j] + dy * t.ey[j]) / (t.len[j] * t.len[j]);
        if(along >= -eps && along <= 1.f + eps && za::abs(t.ex[j] * dy - t.ey[j] * dx) <= eps * t.len[j])
        {
            return true;
        }
    }
    return false;
}

// The triangle (closed) meets the texel (x, y)'s open square more than eps (separating axes: the square's two and the
// triangle's edges' normals).
[[nodiscard]] bool touches(const Tri& t, int x, int y)
{
    const float x0 = static_cast<float>(x), y0 = static_cast<float>(y);
    const float tx0 = za::min(t.x[0], za::min(t.x[1], t.x[2])), tx1 = za::max(t.x[0], za::max(t.x[1], t.x[2]));
    const float ty0 = za::min(t.y[0], za::min(t.y[1], t.y[2])), ty1 = za::max(t.y[0], za::max(t.y[1], t.y[2]));
    if(tx1 <= x0 + eps || tx0 >= x0 + 1.f - eps || ty1 <= y0 + eps || ty0 >= y0 + 1.f - eps)
    {
        return false;
    }
    const float sx[4] = {x0, x0 + 1.f, x0, x0 + 1.f};
    const float sy[4] = {y0, y0, y0 + 1.f, y0 + 1.f};
    for(int j = 0; j < 3; j++)
    {
        if(t.len[j] <= 0.f)
        {
            continue;
        }
        const float nx = -t.ey[j] / t.len[j], ny = t.ex[j] / t.len[j];
        float tmin = 1e30f, tmax = -1e30f, smin = 1e30f, smax = -1e30f;
        for(int k = 0; k < 3; k++)
        {
            const float d = t.x[k] * nx + t.y[k] * ny;
            tmin = za::min(tmin, d);
            tmax = za::max(tmax, d);
        }
        for(int k = 0; k < 4; k++)
        {
            const float d = sx[k] * nx + sy[k] * ny;
            smin = za::min(smin, d);
            smax = za::max(smax, d);
        }
        if(tmax <= smin + eps || tmin >= smax - eps)
        {
            return false;
        }
    }
    return true;
}

// The model's name for texture `name`: an 8-bit skin's "progs/v_shot.mdl:frame0" (its group's "..._1", its
// fullbrights' "..._glow"), a full-colour replacement's "progs/v_shot.mdl_0" (and its "_glow" / "_luma"; not a normal
// map's "_norm" / "_bump"). False for any other texture.
[[nodiscard]] bool modelOf(const char* name, int bpp, char (&model)[MAX_QPATH])
{
    const char* at = strstr(name, bpp == 1 ? ".mdl:frame" : ".mdl_");
    if(!at || (bpp != 1 && (qvr::strings::endsWith(name, "_norm") || qvr::strings::endsWith(name, "_bump"))))
    {
        return false;
    }
    q_strlcpy(model, name, za::min(sizeof(model), static_cast<za::SizeT>(at - name) + 5)); // up to ".mdl"
    return true;
}

[[nodiscard]] Entry* entryFor(const char* model)
{
    for(Entry& e : cache.entries)
    {
        if(!strcmp(e.model, model))
        {
            return &e;
        }
    }
    return nullptr;
}

// A model's old pairs dropped (it loaded again): the later entries' moved down.
void forget(const char* model)
{
    for(za::SizeT i = 0; i < cache.entries.size(); i++)
    {
        Entry& e = cache.entries[i];
        if(strcmp(e.model, model) != 0)
        {
            continue;
        }
        const za::SizeT first = e.first, n = e.count * 2;
        for(za::SizeT k = first; k + n < cache.pairs.size(); k++)
        {
            cache.pairs[k] = cache.pairs[k + n];
        }
        cache.pairs.resize(cache.pairs.size() - n);
        for(Entry& other : cache.entries)
        {
            if(other.first > first)
            {
                other.first -= n;
            }
        }
        cache.entries.erase(cache.entries.data() + i);
        return;
    }
}

void onFillChanged(cvar_t*)
{
    // The skins with edge texels, uploaded again (VR_SkinEdgeFill fills them or not as they load).
    for(const Entry& e : cache.entries)
    {
        char prefix[MAX_QPATH];
        q_snprintf(prefix, sizeof(prefix), "%s:frame", e.model);
        TexMgr_ReloadImagesNamed(prefix);
        q_snprintf(prefix, sizeof(prefix), "%s_", e.model);
        TexMgr_ReloadImagesNamed(prefix);
    }
}

} // namespace

extern "C" void VR_SkinEdgesModel(
    const char* model, int holey, int width, int height, const int* st, int numverts, const int* tris, int numtris)
{
    if(!model || isDedicated)
    {
        return;
    }
    forget(model);
    if(holey || width < 2 || height < 2 || numverts <= 0 || numtris <= 0 || !st || !tris)
    {
        return;
    }
    const za::SizeT texels = static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height);
    EdgeScratch& s = scratch;
    s.inside.clear();
    s.inside.resize(texels, 0);
    s.source.clear();
    s.source.resize(texels, 0u);
    s.touched.clear();
    const auto corners = [&](int i, Tri& out) {
        float x[3], y[3];
        const int* t = tris + i * 4;
        for(int j = 0; j < 3; j++)
        {
            const int v = t[1 + j];
            if(v < 0 || v >= numverts)
            {
                return false;
            }
            const int* sv = st + v * 3;
            // the GPU's: corners on texel middles (gl_mesh.c), the back's seam vertices half a skin right
            x[j] = static_cast<float>(sv[1] + (!t[0] && sv[0] ? width / 2 : 0)) + 0.5f;
            y[j] = static_cast<float>(sv[2]) + 0.5f;
        }
        out = makeTri(x, y);
        return true;
    };
    const auto box = [&](const Tri& t, int& x0, int& x1, int& y0, int& y1) {
        x0 = za::clamp(static_cast<int>(za::floor(za::min(t.x[0], za::min(t.x[1], t.x[2])))) - 1, 0, width - 1);
        x1 = za::clamp(static_cast<int>(za::ceil(za::max(t.x[0], za::max(t.x[1], t.x[2])))) + 1, 0, width - 1);
        y0 = za::clamp(static_cast<int>(za::floor(za::min(t.y[0], za::min(t.y[1], t.y[2])))) - 1, 0, height - 1);
        y1 = za::clamp(static_cast<int>(za::ceil(za::max(t.y[0], za::max(t.y[1], t.y[2])))) + 1, 0, height - 1);
    };
    // the texels whose middles a triangle has
    for(int i = 0; i < numtris; i++)
    {
        Tri t;
        if(!corners(i, t))
        {
            continue;
        }
        int x0, x1, y0, y1;
        box(t, x0, x1, y0, y1);
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                if(contains(t, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f))
                {
                    s.inside[static_cast<za::SizeT>(y) * width + x] = 1;
                }
            }
        }
    }
    // the texels a triangle touches with no triangle's middle in them: each the first touching triangle's texel next
    // to it (its sides first, then its corners) that it has inside, else any neighbour some triangle has inside
    constexpr int nx[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    constexpr int ny[8] = {1, -1, 0, 0, 1, -1, 1, -1};
    for(int i = 0; i < numtris; i++)
    {
        Tri t;
        if(!corners(i, t))
        {
            continue;
        }
        int x0, x1, y0, y1;
        box(t, x0, x1, y0, y1);
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                const za::SizeT k = static_cast<za::SizeT>(y) * width + x;
                if(s.inside[k] || s.source[k] || !touches(t, x, y))
                {
                    continue;
                }
                za::U32 from = 0;
                for(int pass = 0; pass < 2 && !from; pass++)
                {
                    for(int n = 0; n < 8 && !from; n++)
                    {
                        const int X = x + nx[n], Y = y + ny[n];
                        if(X < 0 || Y < 0 || X >= width || Y >= height)
                        {
                            continue;
                        }
                        const za::SizeT kn = static_cast<za::SizeT>(Y) * width + X;
                        const bool ok = pass == 0 ? s.inside[kn] && contains(t, static_cast<float>(X) + 0.5f, static_cast<float>(Y) + 0.5f)
                                                  : s.inside[kn] != 0;
                        if(ok)
                        {
                            from = static_cast<za::U32>(kn) + 1u;
                        }
                    }
                }
                if(from)
                {
                    s.source[k] = from;
                    s.touched.pushBack(static_cast<za::U32>(k));
                }
            }
        }
    }
    if(s.touched.empty())
    {
        return;
    }
    Entry& e = cache.entries.emplaceBack();
    q_strlcpy(e.model, model, sizeof(e.model));
    e.width = width;
    e.height = height;
    e.first = cache.pairs.size();
    e.count = s.touched.size();
    for(const za::U32 k : s.touched)
    {
        cache.pairs.pushBack(k);
        cache.pairs.pushBack(s.source[k] - 1u);
    }
}

extern "C" byte* VR_SkinEdgeFill(const char* name, byte* data, int width, int height, int bpp)
{
    char model[MAX_QPATH];
    if(!name || !data || isDedicated || vr_skin_edge_fill.value == 0.f || (bpp != 1 && bpp != 4) || !modelOf(name, bpp, model))
    {
        return data;
    }
    Entry* e = entryFor(model);
    if(!e || e->width != width || e->height != height) // (a replacement of another size: left as it is)
    {
        return data;
    }
    const za::SizeT stride = static_cast<za::SizeT>(bpp);
    const za::SizeT size = static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) * stride;
    byte* out = static_cast<byte*>(Hunk_AllocNoFill(static_cast<int>(size))); // freed with the upload's (TexMgr's mark)
    memcpy(out, data, size);
    const za::U32* pair = cache.pairs.data() + e->first;
    for(za::SizeT i = 0; i < e->count; i++, pair += 2)
    {
        memcpy(out + pair[0] * stride, data + pair[1] * stride, stride);
    }
    e->filled++;
    return out;
}

namespace qvr::skinedge
{

void init()
{
    Cvar_SetCallback(&vr_skin_edge_fill, onFillChanged);
    Cmd_AddCommand("vr_skin_edges", list_f);
}

void list_f()
{
    const char* only = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
    za::SizeT total = 0;
    for(const Entry& e : cache.entries)
    {
        total += e.count;
        if(only && !strstr(e.model, only))
        {
            continue;
        }
        Con_Printf("%-36s %4d x %-4d %6d edge texels  uploads filled %d\n", e.model, e.width, e.height,
            static_cast<int>(e.count), e.filled);
        if(only)
        {
            const za::U32* pair = cache.pairs.data() + e.first;
            for(za::SizeT i = 0; i < e.count; i++, pair += 2)
            {
                if(Cmd_Argc() > 3 && static_cast<int>(pair[0] % e.width) == atoi(Cmd_Argv(2)) &&
                   static_cast<int>(pair[0] / e.width) == atoi(Cmd_Argv(3)))
                {
                    Con_Printf("  texel %u %u from %u %u\n", pair[0] % e.width, pair[0] / e.width, pair[1] % e.width,
                        pair[1] / e.width);
                }
            }
        }
    }
    Con_Printf("skin edges: %s; %d models, %d edge texels\n", vr_skin_edge_fill.value != 0.f ? "on" : "off",
        static_cast<int>(cache.entries.size()), static_cast<int>(total));
}

} // namespace qvr::skinedge
