// vr_permdecals.cpp -- see vr_permdecals.hpp.

#include "vr_permdecals.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::permdecals
{
namespace
{

// A page's own texels across, its slot's (a texel of its neighbours' round it: the bilinear filter's), the mip levels
// baked (32 to 4 texels a slot). QVR_PERMDECAL_FUNCTIONS (vr_glsl.h) has them too.
constexpr int pageTexels = 30;
constexpr int slotTexels = 32;
constexpr int levels = 4;
// A face's pages: across + down * pagesDown (in a float: both under 4096).
constexpr int pagesDown = 4096;
// The page table's length at most: its offsets go to the shader as floats (exact up to 2^24).
constexpr za::SizeT tableMax = za::SizeT{1} << 24;
// Stamps kept for the saves at most (the oldest forgotten first: by then their pages have likely given their slots up).
constexpr za::SizeT stampsMax = 65536;
// (An empty texel: no change: the stored value is half the factor, in 10 bits: 512 / 1023 is 1 exactly when read back;
// the bake program's emptied slots.)

// A face's layout: its plane's axes, its pages.
struct Layout
{
    glm::vec3 origin{0.f};        // the point of its plane at texel (0, 0)
    glm::vec3 t{0.f}, b{0.f};     // its axes along it (unit length)
    float t0 = 0.f, b0 = 0.f;     // where its texels start along them
    za::U32 table = 0;            // its first page in the table
    za::U16 across = 0, down = 0; // its pages (0: none)
};

// The map laid out (the main thread's): made with the brush models' vertex buffer (VR_PermDecalsLayout), kept while
// the same map is loaded again with the same density.
struct MapLayout
{
    const qmodel_t* model = nullptr;
    char name[MAX_QPATH] = {};
    int surfaces = 0;
    float density = 1.f;
    za::Vector<Layout> faces;               // by the world model's surface index (its brush models' too)
    za::Vector<za::U32> table;              // each page's slot + 1 (0: none)
    za::Vector<const qmodel_t*> submodels;  // by number (*n)
};
MapLayout map;

// A slot of the atlas: the page in it (its table index + 1; 0: free), the flush it was last baked for, its neighbours
// in the order the slots in use were last baked (none: noSlot).
constexpr za::U32 noSlot = 0xFFFFFFFFu;
struct Slot
{
    za::U32 page = 0;
    za::U32 used = 0;
    za::U32 older = noSlot, newer = noSlot;
};

// The atlas (the main thread's): a 2D array texture of `size` square layers, `perRow` slots a row; slot n is in layer
// n / perLayer. Its slots (on the CPU) grow a few layers at a time when full, up to vr_permdecal_atlas_max MB; the texture
// is made (and made larger, the layers baked so far copied over) by the next flush. GL objects made at the first flush
// with pages (the bake program at the map's load).
struct Atlas
{
    GLuint texture = 0, fbo = 0, readFbo = 0, program = 0;
    bool programFailed = false;
    int size = 0, perRow = 0, layers = 0; // the slots' (the CPU's)
    int textureSize = 0, textureLayers = 0; // the texture's
    za::SizeT perLayer = 0;
    za::Vector<Slot> slots;
    za::Vector<za::U32> free;              // free slots, the last taken first
    za::U32 oldest = noSlot, newest = noSlot; // the slots in use, baked least and most recently
    za::U32 serial = 1;                    // flushes so far: a slot baked since the last is not given up (its bake is pending)
};
Atlas atlas;
gfx::StorageBuffer tableBuffer;

// What the next flush draws (the main thread's): by layer, the bakes' quads (6 vertices each, in every level), the
// slots emptied first (drawn as quads too, without blending: one draw a layer and level); the page table's entries
// changed (all of it: `tableAll`).
struct BakeVertex
{
    glm::vec4 posSt; // the slot's corner in the level's clip space, the mark's coordinates there
    glm::vec4 cell;  // its cell in the marks' atlas (-1: the slot emptied), its colour's share
};
struct Pending
{
    za::Vector<za::Vector<BakeVertex>> quads, clears; // by layer
    za::SizeT quadCount = 0;
    za::Vector<za::U32> table; // entries changed
    bool tableAll = false;
};
Pending pending;

// What was baked, for the saves (in its model's space); the newest last. While the layer is off (vr_permdecals 0) the
// marks the live ring lets go of are kept too, not baked (from `unbakedFrom` on: baked once it is on, rebake; the atlas
// keeps what was baked before, untouched while off).
za::Vector<Stamp> stamps;
constexpr za::SizeT noneUnbaked = ~za::SizeT{0};
za::SizeT unbakedFrom = noneUnbaked;

// Marks on brush models (doors, lifts) that show later (the gore's flying blood: vr_decals.cpp's delay), in their
// model's space as they were laid: baked when they show (bakeDue), as the static world's live marks show then.
struct Waiting
{
    Stamp stamp;
    double due; // cl.time it shows at
};
za::Vector<Waiting> waiting;

// vr_permdecal_info's counts since the map's start.
struct Counts
{
    long long stamps = 0, faces = 0, pages = 0, evicted = 0, dropped = 0;
    // The flushes': how many drew, their quads (a page a mark), the most in one, the most slots emptied in one, the
    // page table's bytes uploaded.
    long long flushes = 0, quads = 0, worstQuads = 0, worstClears = 0, tableBytes = 0;
    long long grown = 0; // the atlas's growths (a layer or more each)
    double worstMs = 0.0; long long slowFlushes = 0; // the flushes' CPU time: the most, how many over 0.5 ms
};
Counts counts;

// A face's polygon cut to a mark (bakeFace).
struct PermScratch
{
    za::Vector<glm::vec3> poly, clipped;
    auto members() { return qvr::mem::list(poly, clipped); }
};
mem::Scratch<PermScratch> scratch{"permdecals"};

// A mark's frame, worked out once for every face it is tried on.
struct Frame
{
    glm::vec3 U{0.f}, V{0.f};
    float hu = 0.f, hv = 0.f;
    glm::vec3 lo{0.f}, hi{0.f};
    glm::vec3 sideNormal[4];
    float sideDist[4];
};

[[nodiscard]] bool frameOf(const Stamp& s, Frame& f)
{
    f.hu = glm::length(s.u);
    f.hv = glm::length(s.v);
    if(f.hu <= 1e-4f || f.hv <= 1e-4f)
    {
        return false;
    }
    f.U = s.u / f.hu;
    f.V = s.v / f.hv;
    const glm::vec3 extent = glm::abs(s.u) + glm::abs(s.v) + glm::abs(s.normal) * s.depth;
    f.lo = s.centre - extent;
    f.hi = s.centre + extent;
    f.sideNormal[0] = f.U;
    f.sideNormal[1] = -f.U;
    f.sideNormal[2] = f.V;
    f.sideNormal[3] = -f.V;
    f.sideDist[0] = glm::dot(s.centre, f.U) + f.hu;
    f.sideDist[1] = glm::dot(s.centre, -f.U) + f.hu;
    f.sideDist[2] = glm::dot(s.centre, f.V) + f.hv;
    f.sideDist[3] = glm::dot(s.centre, -f.V) + f.hv;
    return true;
}

// Keeps the part of `poly` on the inner side of the plane (dot(p, normal) <= dist) into `out` (as vr_decals.cpp's).
void clipPolygon(const za::Vector<glm::vec3>& poly, const glm::vec3& normal, float dist, za::Vector<glm::vec3>& out)
{
    out.clear();
    const za::SizeT n = poly.size();
    for(za::SizeT i = 0; i < n; i++)
    {
        const glm::vec3& a = poly[i];
        const glm::vec3& b = poly[(i + 1) % n];
        const float da = glm::dot(a, normal) - dist, db = glm::dot(b, normal) - dist;
        if(da <= 0.f)
        {
            out.pushBack(a);
        }
        if((da < 0.f && db > 0.f) || (da > 0.f && db < 0.f))
        {
            out.pushBack(a + (b - a) * (da / (da - db)));
        }
    }
}

[[nodiscard]] bool laidOut()
{
    return map.model && map.model == cl.worldmodel && !map.faces.empty();
}

[[nodiscard]] int atlasSizeWanted()
{
    int size = 1024;
    while(size < 8192 && size < static_cast<int>(vr_permdecal_atlas.value))
    {
        size <<= 1;
    }
    return size;
}

// A layer's bytes, its mip levels with it.
[[nodiscard]] double layerBytes(int size)
{
    double bytes = 0.0;
    for(int level = 0; level < levels; level++)
    {
        const double side = static_cast<double>(size >> level);
        bytes += side * side * 4.0;
    }
    return bytes;
}

// The atlas's layers at most: vr_permdecal_atlas_max MB, no more than the map's pages fill (one at least).
[[nodiscard]] int layersMax()
{
    if(atlas.perLayer == 0)
    {
        return 1;
    }
    const double mb = za::max(1.0, static_cast<double>(vr_permdecal_atlas_max.value));
    const za::SizeT need = (map.table.size() + atlas.perLayer - 1) / atlas.perLayer;
    const int byMemory = static_cast<int>(mb * 1024.0 * 1024.0 / layerBytes(atlas.size));
    return za::clamp(za::min(byMemory, static_cast<int>(za::min(need, za::SizeT{256}))), 1, 256);
}

// Slot `s` out of the order the slots in use were baked in.
void unlink(za::U32 s)
{
    Slot& x = atlas.slots[s];
    (x.older != noSlot ? atlas.slots[x.older].newer : atlas.oldest) = x.newer;
    (x.newer != noSlot ? atlas.slots[x.newer].older : atlas.newest) = x.older;
    x.older = x.newer = noSlot;
}

// ... into it, the newest.
void linkNewest(za::U32 s)
{
    Slot& x = atlas.slots[s];
    x.older = atlas.newest;
    x.newer = noSlot;
    (atlas.newest != noSlot ? atlas.slots[atlas.newest].newer : atlas.oldest) = s;
    atlas.newest = s;
}

void tableChanged(za::U32 page)
{
    if(!pending.tableAll)
    {
        pending.table.pushBack(page);
    }
}

// The atlas's slots, `layers` layers of them, all free, and the page table empty (all of it to upload).
void freeAllSlots(int layers)
{
    atlas.layers = layers;
    atlas.slots.assign(atlas.perLayer * static_cast<za::SizeT>(layers), Slot{});
    atlas.free.clear();
    for(za::SizeT i = atlas.slots.size(); i-- > 0;)
    {
        atlas.free.pushBack(static_cast<za::U32>(i));
    }
    atlas.oldest = atlas.newest = noSlot;
    for(za::U32& e : map.table)
    {
        e = 0;
    }
    pending.quads.clear();
    pending.clears.clear();
    pending.quadCount = 0;
    pending.table.clear();
    pending.tableAll = true;
}

// Full: a few layers more (half as many again: few copies of the texture), if vr_permdecal_atlas_max has room.
[[nodiscard]] bool grow()
{
    const int max = layersMax();
    if(atlas.layers >= max)
    {
        return false;
    }
    const int more = za::min(max - atlas.layers, za::max(1, atlas.layers / 2));
    const za::SizeT first = atlas.slots.size();
    atlas.slots.resize(first + atlas.perLayer * static_cast<za::SizeT>(more), Slot{});
    for(za::SizeT i = atlas.slots.size(); i-- > first;)
    {
        atlas.free.pushBack(static_cast<za::U32>(i));
    }
    atlas.layers += more;
    counts.grown++;
    return true;
}

void bakeStamp(const Stamp& s);

// The atlas's slots made (or made again at another size: empty, what was baked baked again; the texture at the next
// flush).
[[nodiscard]] bool ensureAtlas()
{
    const int size = atlasSizeWanted();
    if(atlas.layers > 0 && atlas.size == size)
    {
        return true;
    }
    atlas.size = size;
    atlas.perRow = size / slotTexels;
    atlas.perLayer = static_cast<za::SizeT>(atlas.perRow) * static_cast<za::SizeT>(atlas.perRow);
    freeAllSlots(1);
    if(laidOut())
    {
        for(const Stamp& s : stamps)
        {
            bakeStamp(s);
        }
    }
    return true;
}

// Slot `slot`'s corners in its layer's clip space; its layer.
int corners(za::U32 slot, float& x0, float& x1, float& y0, float& y1)
{
    const za::SizeT local = slot % atlas.perLayer;
    const int ox = static_cast<int>(local % static_cast<za::SizeT>(atlas.perRow)) * slotTexels;
    const int oy = static_cast<int>(local / static_cast<za::SizeT>(atlas.perRow)) * slotTexels;
    const float scale = 2.f / static_cast<float>(atlas.size);
    x0 = static_cast<float>(ox) * scale - 1.f;
    x1 = static_cast<float>(ox + slotTexels) * scale - 1.f;
    y0 = static_cast<float>(oy) * scale - 1.f;
    y1 = static_cast<float>(oy + slotTexels) * scale - 1.f;
    return static_cast<int>(slot / atlas.perLayer);
}

[[nodiscard]] za::Vector<BakeVertex>& layerList(za::Vector<za::Vector<BakeVertex>>& lists, int layer)
{
    if(lists.size() <= static_cast<za::SizeT>(layer))
    {
        lists.resize(static_cast<za::SizeT>(layer) + 1);
    }
    return lists[static_cast<za::SizeT>(layer)];
}

// The slot of page `page` (a table index), taken if it has none (emptied first: queued): a free one, the atlas grown
// if it has none, else the page baked least recently gives its slot up; -1 if the atlas is full of pages baked since
// the last flush.
[[nodiscard]] int takeSlot(za::U32 page)
{
    if(const za::U32 e = map.table[page]; e != 0)
    {
        if(atlas.slots[e - 1].used != atlas.serial)
        {
            atlas.slots[e - 1].used = atlas.serial;
            unlink(e - 1);
            linkNewest(e - 1);
        }
        return static_cast<int>(e - 1);
    }
    if(atlas.free.empty())
    {
        (void)grow();
    }
    za::U32 slot = 0;
    if(!atlas.free.empty())
    {
        slot = atlas.free.back();
        atlas.free.popBack();
    }
    else
    {
        slot = atlas.oldest;
        if(slot == noSlot || atlas.slots[slot].used == atlas.serial)
        {
            return -1;
        }
        unlink(slot);
        map.table[atlas.slots[slot].page - 1] = 0;
        tableChanged(atlas.slots[slot].page - 1);
        counts.evicted++;
    }
    atlas.slots[slot].page = page + 1;
    atlas.slots[slot].used = atlas.serial;
    linkNewest(slot);
    map.table[page] = slot + 1;
    tableChanged(page);
    float x0, x1, y0, y1;
    za::Vector<BakeVertex>& v = layerList(pending.clears, corners(slot, x0, x1, y0, y1));
    const glm::vec4 cell{-1.f, 0.f, 0.f, 0.f};
    v.pushBack({{x0, y0, 0.5f, 0.5f}, cell});
    v.pushBack({{x1, y0, 0.5f, 0.5f}, cell});
    v.pushBack({{x1, y1, 0.5f, 0.5f}, cell});
    v.pushBack({{x0, y0, 0.5f, 0.5f}, cell});
    v.pushBack({{x1, y1, 0.5f, 0.5f}, cell});
    v.pushBack({{x0, y1, 0.5f, 0.5f}, cell});
    counts.pages++;
    return static_cast<int>(slot);
}

// The quad that bakes mark `s` (frame `f`) into page (px, py) of face `l`, in slot `slot`.
void quad(int slot, int px, int py, const Layout& l, const Stamp& s, const Frame& f)
{
    const float density = map.density;
    float x0, x1, y0, y1;
    za::Vector<BakeVertex>& out = layerList(pending.quads, corners(static_cast<za::U32>(slot), x0, x1, y0, y1));
    pending.quadCount++;
    // The slot's corners on the face: its page's texels, a texel of the neighbours' before.
    const float g0x = static_cast<float>(px * pageTexels - 1), g0y = static_cast<float>(py * pageTexels - 1);
    const auto st = [&](float gx, float gy) {
        const glm::vec3 p = l.origin + l.t * (gx * density) + l.b * (gy * density);
        const glm::vec3 r = p - s.centre;
        return glm::vec2{glm::dot(r, f.U) / f.hu * 0.5f + 0.5f, glm::dot(r, f.V) / f.hv * 0.5f + 0.5f};
    };
    const glm::vec2 s00 = st(g0x, g0y), s10 = st(g0x + slotTexels, g0y);
    const glm::vec2 s11 = st(g0x + slotTexels, g0y + slotTexels), s01 = st(g0x, g0y + slotTexels);
    const glm::vec4 cell{static_cast<float>(s.cell), s.dark, 0.f, 0.f};
    out.pushBack({{x0, y0, s00}, cell});
    out.pushBack({{x1, y0, s10}, cell});
    out.pushBack({{x1, y1, s11}, cell});
    out.pushBack({{x0, y0, s00}, cell});
    out.pushBack({{x1, y1, s11}, cell});
    out.pushBack({{x0, y1, s01}, cell});
}

// Mark `s` (frame `f`, in model `m`'s space) baked on face `index` if it lies on it (facing about its way, its middle
// near the face's plane, its rectangle over the face: as vr_decals.cpp's clipToWorld lays the live marks). True if it
// lies on it, even with its pages' slots all dropped (the atlas full: the stamp still kept, for the saves). `dry`: only
// whether it lies on it, nothing baked.
bool bakeFace(const qmodel_t* m, int index, const Stamp& s, const Frame& f, bool dry)
{
    if(index < 0 || index >= static_cast<int>(map.faces.size()))
    {
        return false;
    }
    const Layout& l = map.faces[static_cast<za::SizeT>(index)];
    if(l.across == 0)
    {
        return false;
    }
    const msurface_t& surf = m->surfaces[index];
    if(surf.maxs[0] < f.lo.x || surf.mins[0] > f.hi.x || surf.maxs[1] < f.lo.y || surf.mins[1] > f.hi.y ||
        surf.maxs[2] < f.lo.z || surf.mins[2] > f.hi.z)
    {
        return false;
    }
    const float flip = (surf.flags & SURF_PLANEBACK) ? -1.f : 1.f;
    const glm::vec3 sn = glm::vec3{surf.plane->normal[0], surf.plane->normal[1], surf.plane->normal[2]} * flip;
    const float off = glm::dot(s.centre, sn) - surf.plane->dist * flip;
    if(glm::dot(sn, s.normal) < 0.5f || za::fabs(off) > s.depth)
    {
        return false;
    }
    za::Vector<glm::vec3>& poly = scratch.poly;
    za::Vector<glm::vec3>& clipped = scratch.clipped;
    poly.clear();
    for(int e = 0; e < surf.numedges; e++)
    {
        const int se = m->surfedges[surf.firstedge + e];
        const unsigned int vi = se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1];
        const float* p = m->vertexes[vi].position;
        poly.pushBack({p[0], p[1], p[2]});
    }
    for(int k = 0; k < 4 && poly.size() >= 3; k++)
    {
        clipPolygon(poly, f.sideNormal[k], f.sideDist[k], clipped);
        poly.swap(clipped);
    }
    if(poly.size() < 3)
    {
        return false;
    }
    if(dry)
    {
        return true;
    }
    // The pages its part on the face reaches (and a texel round: their neighbours' texels in a slot's rim).
    glm::vec2 lo{1e30f}, hi{-1e30f};
    for(const glm::vec3& p : poly)
    {
        const glm::vec2 g{(glm::dot(p, l.t) - l.t0) / map.density, (glm::dot(p, l.b) - l.b0) / map.density};
        lo = glm::min(lo, g);
        hi = glm::max(hi, g);
    }
    const auto page = [](float g, int count) {
        return za::clamp(static_cast<int>(za::floor(g / static_cast<float>(pageTexels))), 0, count - 1);
    };
    const int px0 = page(lo.x - 1.f, l.across), px1 = page(hi.x + 1.f, l.across);
    const int py0 = page(lo.y - 1.f, l.down), py1 = page(hi.y + 1.f, l.down);
    bool baked = false;
    for(int py = py0; py <= py1; py++)
    {
        for(int px = px0; px <= px1; px++)
        {
            const int slot = takeSlot(l.table + static_cast<za::U32>(py * l.across + px));
            if(slot < 0)
            {
                counts.dropped++;
                continue;
            }
            quad(slot, px, py, l, s, f);
            baked = true;
        }
    }
    counts.faces += baked;
    return true;
}

// Stamp `s` baked on its model's faces (the world's: those its box reaches down the BSP tree; a brush model's: all of
// its own); how many it lies on (`dry`: only those counted, nothing baked).
int bakeOn(const Stamp& s, bool dry = false)
{
    Frame f;
    if(!laidOut() || !frameOf(s, f) || !ensureAtlas())
    {
        return 0;
    }
    const qmodel_t* world = map.model;
    int faces = 0;
    if(s.model == 0)
    {
        const mnode_t* stack[256];
        int top = 0;
        stack[top++] = world->nodes + world->hulls[0].firstclipnode;
        const glm::vec3 extent = (f.hi - f.lo) * 0.5f;
        const glm::vec3 centre = (f.hi + f.lo) * 0.5f;
        while(top > 0)
        {
            const mnode_t* node = stack[--top];
            if(node->contents < 0)
            {
                continue; // a leaf
            }
            const mplane_t* plane = node->plane;
            const glm::vec3 pn{plane->normal[0], plane->normal[1], plane->normal[2]};
            const float side = glm::dot(centre, pn) - plane->dist;
            const float reach = glm::dot(extent, glm::abs(pn));
            if(side > reach)
            {
                stack[top++] = node->children[0];
                continue;
            }
            if(side < -reach)
            {
                stack[top++] = node->children[1];
                continue;
            }
            if(top + 2 <= 256)
            {
                stack[top++] = node->children[0];
                stack[top++] = node->children[1];
            }
            for(unsigned int i = 0; i < node->numsurfaces; i++)
            {
                faces += bakeFace(world, static_cast<int>(node->firstsurface + i), s, f, dry);
            }
        }
        return faces;
    }
    if(s.model < 0 || s.model >= static_cast<int>(map.submodels.size()) || !map.submodels[static_cast<za::SizeT>(s.model)])
    {
        return 0;
    }
    const qmodel_t* sub = map.submodels[static_cast<za::SizeT>(s.model)];
    for(int i = 0; i < sub->nummodelsurfaces; i++)
    {
        faces += bakeFace(world, sub->firstmodelsurface + i, s, f, dry);
    }
    return faces;
}

void bakeStamp(const Stamp& s)
{
    (void)bakeOn(s);
}

void keep(const Stamp& s)
{
    if(stamps.size() >= stampsMax)
    {
        stamps.erase(stamps.begin(), stamps.begin() + stampsMax / 8);
        if(unbakedFrom != noneUnbaked)
        {
            unbakedFrom = unbakedFrom > stampsMax / 8 ? unbakedFrom - stampsMax / 8 : 0;
        }
    }
    stamps.pushBack(s);
    counts.stamps++;
}

// ---- Saves -------------------------------------------------------------------------------------------------------
//
// The stamps go into the saved game as comment lines (COM_Parse skips them): "// qvrdecals 1 map <model> n <count>",
// then "// qvrd <base64>" lines of 35-byte records (the middle in 1/8 units, the half extents in 1/64, the normal in
// 1/32767, the depth in 1/4 units, the darkness, the cell, the model), baked again once the saved game's map has loaded.

constexpr int recordBytes = 35;
constexpr za::SizeT lineChars = 4000; // base64 characters a line (a multiple of 4)

// What the saves read and write (the main thread's): the lines of the last save (valid until the next: the save's
// thread writes them), the records of the save being loaded, and the map loads seen (a record set waits for the next).
struct SaveState
{
    za::String lines;
    za::Vector<za::U8> pending;
    char pendingMap[MAX_QPATH] = {};
    int pendingAfterLoad = -1; // the map load it waits for (-1: none)
    int mapLoads = 0;
    za::Vector<Stamp> live; // the live ring's marks for the save being written
};
SaveState saves;

constexpr const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] int base64Value(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A'
           : c >= 'a' && c <= 'z' ? c - 'a' + 26
           : c >= '0' && c <= '9' ? c - '0' + 52
           : c == '+' ? 62
           : c == '/' ? 63
                      : -1;
}

void putInt(za::U8*& p, long long v, int bytes)
{
    for(int i = 0; i < bytes; i++)
    {
        *p++ = static_cast<za::U8>((static_cast<unsigned long long>(v) >> (8 * i)) & 0xFF);
    }
}

[[nodiscard]] long long getInt(const za::U8*& p, int bytes)
{
    unsigned long long v = 0;
    for(int i = 0; i < bytes; i++)
    {
        v |= static_cast<unsigned long long>(*p++) << (8 * i);
    }
    const unsigned long long sign = 1ull << (8 * bytes - 1);
    return static_cast<long long>((v ^ sign) - sign); // (sign-extended)
}

[[nodiscard]] long long quantise(float v, float scale, long long lo, long long hi)
{
    return za::clamp(static_cast<long long>(za::floor(v * scale + 0.5f)), lo, hi);
}

void encode(const Stamp& s, za::U8* p)
{
    for(int k = 0; k < 3; k++)
    {
        putInt(p, quantise(s.centre[k], 8.f, -2147483647ll, 2147483647ll), 4);
    }
    for(int k = 0; k < 3; k++)
    {
        putInt(p, quantise(s.u[k], 64.f, -32767, 32767), 2);
    }
    for(int k = 0; k < 3; k++)
    {
        putInt(p, quantise(s.v[k], 64.f, -32767, 32767), 2);
    }
    for(int k = 0; k < 3; k++)
    {
        putInt(p, quantise(s.normal[k], 32767.f, -32767, 32767), 2);
    }
    putInt(p, quantise(s.depth, 4.f, 0, 127), 1);
    putInt(p, quantise(s.dark, 127.f, 0, 127), 1);
    putInt(p, za::clamp(s.cell, 0, 127), 1);
    putInt(p, za::clamp(s.model, 0, 32767), 2);
}

[[nodiscard]] Stamp decode(const za::U8* p)
{
    Stamp s;
    for(int k = 0; k < 3; k++)
    {
        s.centre[k] = static_cast<float>(getInt(p, 4)) / 8.f;
    }
    for(int k = 0; k < 3; k++)
    {
        s.u[k] = static_cast<float>(getInt(p, 2)) / 64.f;
    }
    for(int k = 0; k < 3; k++)
    {
        s.v[k] = static_cast<float>(getInt(p, 2)) / 64.f;
    }
    for(int k = 0; k < 3; k++)
    {
        s.normal[k] = static_cast<float>(getInt(p, 2)) / 32767.f;
    }
    s.depth = static_cast<float>(getInt(p, 1)) / 4.f;
    s.dark = static_cast<float>(getInt(p, 1)) / 127.f;
    s.cell = static_cast<int>(getInt(p, 1));
    s.model = static_cast<int>(getInt(p, 2));
    return s;
}

// The flushes' program: a slot's quad, the mark's texel read as the live marks read it, its factor halved (the blend
// doubles it: GL_DST_COLOR, GL_SRC_COLOR multiplies the slot by it, in 0..2).
constexpr const char* bakeVertexSource = R"(#version 430
layout(location=0) in vec4 in_posst;
layout(location=1) in vec4 in_cell;
layout(location=0) out vec2 out_st;
layout(location=1) flat out vec2 out_cell;
void main()
{
	gl_Position = vec4(in_posst.xy, 0.0, 1.0);
	out_st = in_posst.zw;
	out_cell = in_cell.xy;
}
)";

constexpr const char* bakeFragmentSource = R"(#version 430
layout(binding=0) uniform sampler2D DecalAtlas;
layout(location=0) in vec2 in_st;
layout(location=1) flat in vec2 in_cell;
layout(location=0) out vec4 out_color;
void main()
{
	if (in_cell.x < -0.5) // (a slot emptied: drawn without blending)
	{
		out_color = vec4(vec3(512.0 / 1023.0), 1.0);
		return;
	}
	if (any(lessThan(in_st, vec2(0.0))) || any(greaterThan(in_st, vec2(1.0))))
		discard;
	const vec2 inset = vec2(0.5 / 2048.0, 0.5 / 1024.0);
	const vec2 cellsize = vec2(1.0 / 8.0, 1.0 / 4.0);
	int cell = int(in_cell.x + 0.5);
	vec2 at = vec2(float(cell % 8), float(cell / 8)) * cellsize + inset + in_st * (cellsize - 2.0 * inset);
	vec4 t = texture(DecalAtlas, at);
	out_color = vec4((t.rgb * in_cell.y + (1.0 - t.a)) * 0.5, 0.5);
}
)";

} // namespace

bool on()
{
    return vr_permdecals.value != 0.f && vr_decals.value != 0.f && cl.worldmodel && laidOut();
}

bool kept()
{
    return vr_permdecals.value == 0.f && vr_decals.value != 0.f && cl.worldmodel && laidOut();
}

void record(const Stamp& s)
{
    if(kept())
    {
        Stamp w = s;
        w.model = 0;
        unbakedFrom = za::min(unbakedFrom, stamps.size());
        keep(w);
    }
}

bool rebake()
{
    if(unbakedFrom == noneUnbaked || !on())
    {
        return false;
    }
    QVR_PROFILE("permanent decals baked again");
    const za::SizeT first = za::min(unbakedFrom, stamps.size());
    unbakedFrom = noneUnbaked;
    if(atlas.layers == 0)
    {
        (void)ensureAtlas(); // (made: bakes them all)
        return true;
    }
    for(za::SizeT i = first; i < stamps.size(); i++)
    {
        bakeStamp(stamps[i]);
    }
    return true;
}

void newestWorld(int max, za::Vector<Stamp>& out)
{
    out.clear();
    za::SizeT first = stamps.size();
    for(int n = 0; first > 0 && n < max;)
    {
        n += stamps[--first].model == 0;
    }
    for(za::SizeT i = first; i < stamps.size(); i++)
    {
        if(stamps[i].model == 0)
        {
            out.pushBack(stamps[i]);
        }
    }
}

int pendingPages()
{
    return static_cast<int>(pending.quadCount);
}

int bakeWorld(const Stamp& s)
{
    if(!on())
    {
        return 0;
    }
    Stamp w = s;
    w.model = 0;
    const int faces = bakeOn(w);
    if(faces > 0)
    {
        keep(w);
    }
    return faces;
}

int bakeEntities(const Stamp& s, double due)
{
    if(!on())
    {
        return 0;
    }
    const bool later = due > cl.time; // (shown later: kept waiting, in the model's space as it is now)
    int faces = 0;
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t* e = &cl_entities[i];
        const qmodel_t* m = e->model;
        if(!m || m->type != mod_brush || m->name[0] != '*' || VR_IsRuntimeBrushModel(m))
        {
            continue;
        }
        const int number = atoi(m->name + 1);
        if(number <= 0 || number >= static_cast<int>(map.submodels.size()) || map.submodels[static_cast<za::SizeT>(number)] != m)
        {
            continue;
        }
        // Far from it (its box's middle where drawn, roughly: its origin's offset), not looked at further.
        const glm::vec3 middle = glm::vec3{e->origin[0], e->origin[1], e->origin[2]} +
                                 (glm::vec3{m->mins[0], m->mins[1], m->mins[2]} + glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]}) * 0.5f;
        const float half = glm::length(glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]} - glm::vec3{m->mins[0], m->mins[1], m->mins[2]}) * 0.5f;
        if(glm::distance(middle, s.centre) > half * za::max(1.f, e->scale > 0 ? static_cast<float>(e->scale) / 16.f : 1.f) +
                                                 glm::length(s.u) + glm::length(s.v) + s.depth + 64.f)
        {
            continue;
        }
        float mat[16];
        VR_BModelMatrix(e, mat);
        const glm::mat4 toWorld = glm::make_mat4(mat);
        const glm::mat4 toModel = glm::inverse(toWorld);
        const glm::mat3 turn{toModel};
        Stamp ms = s;
        ms.centre = glm::vec3{toModel * glm::vec4{s.centre, 1.f}};
        ms.u = turn * s.u;
        ms.v = turn * s.v;
        ms.normal = glm::normalize(turn * s.normal);
        ms.model = number;
        // Its box near the model's?
        const float reach = glm::length(ms.u) + glm::length(ms.v) + ms.depth;
        if(ms.centre.x + reach < m->mins[0] || ms.centre.x - reach > m->maxs[0] || ms.centre.y + reach < m->mins[1] ||
            ms.centre.y - reach > m->maxs[1] || ms.centre.z + reach < m->mins[2] || ms.centre.z - reach > m->maxs[2])
        {
            continue;
        }
        const int n = bakeOn(ms, later);
        if(n > 0)
        {
            if(later)
            {
                waiting.pushBack({ms, due});
            }
            else
            {
                keep(ms);
            }
            faces += n;
        }
    }
    return faces;
}

int bakeDue()
{
    if(waiting.empty() || !on())
    {
        return 0;
    }
    int baked = 0;
    za::SizeT left = 0;
    for(za::SizeT i = 0; i < waiting.size(); i++)
    {
        // (due over a minute ahead: the clock went back, a load or a new map's: baked now)
        if(const Waiting& w = waiting[i]; w.due > cl.time && w.due < cl.time + 60.0)
        {
            waiting[left++] = w;
            continue;
        }
        if(bakeOn(waiting[i].stamp) > 0)
        {
            keep(waiting[i].stamp);
            baked++;
        }
    }
    waiting.resize(left);
    return baked;
}

void waitingStamps(za::Vector<Stamp>& out)
{
    for(const Waiting& w : waiting)
    {
        out.pushBack(w.stamp);
    }
}

namespace
{

// The texture made for the atlas's slots (made larger: the layers baked so far copied over; made at another size: empty,
// every slot in use emptied by this flush since they were all taken again).
void syncTexture()
{
    if(atlas.texture && atlas.textureSize == atlas.size && atlas.textureLayers == atlas.layers)
    {
        return;
    }
    QVR_GPU_PROFILE("permdecal atlas grown");
    GLuint texture = 0;
    glGenTextures(1, &texture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, texture);
    GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, levels, GL_RGB10_A2, atlas.size, atlas.size, atlas.layers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, levels - 1);
    GL_ObjectLabelFunc(GL_TEXTURE, texture, -1, "vr permanent decals");
    if(!atlas.fbo)
    {
        GL_GenFramebuffersFunc(1, &atlas.fbo);
        GL_GenFramebuffersFunc(1, &atlas.readFbo);
    }
    if(atlas.texture && atlas.textureSize == atlas.size)
    {
        // (No glGet to restore what was bound, and 0 bound after: flush's.)
        glDisable(GL_SCISSOR_TEST);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, atlas.readFbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, atlas.fbo);
        const int copy = za::min(atlas.textureLayers, atlas.layers);
        for(int level = 0; level < levels; level++)
        {
            const int side = atlas.size >> level;
            for(int layer = 0; layer < copy; layer++)
            {
                GL_FramebufferTextureLayerFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.texture, level, layer);
                GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, level, layer);
                GL_BlitFramebufferFunc(0, 0, side, side, 0, 0, side, side, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
        }
        GL_FramebufferTextureLayerFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
        GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);
    }
    if(atlas.texture)
    {
        GL_DeleteNativeTexture(atlas.texture);
    }
    atlas.texture = texture;
    atlas.textureSize = atlas.size;
    atlas.textureLayers = atlas.layers;
}

// The page table's changed entries uploaded (runs of them; all of it if most changed or it was made again).
void uploadTable()
{
    const za::SizeT bytes = map.table.size() * sizeof(za::U32);
    if(pending.tableAll || tableBuffer.size != bytes || pending.table.size() * 8 > map.table.size())
    {
        QVR_GPU_PROFILE("permdecal table");
        gfx::upload(tableBuffer, map.table.data(), bytes);
        counts.tableBytes += static_cast<long long>(bytes);
    }
    else if(!pending.table.empty())
    {
        QVR_GPU_PROFILE("permdecal table");
        za::quickSort(pending.table.begin(), pending.table.end());
        constexpr za::U32 gap = 64; // entries between two changed that are sent along rather than split in two
        za::SizeT i = 0;
        while(i < pending.table.size())
        {
            const za::U32 first = pending.table[i];
            za::U32 last = first;
            while(++i < pending.table.size() && pending.table[i] <= last + gap)
            {
                last = pending.table[i];
            }
            const za::SizeT n = static_cast<za::SizeT>(last - first) + 1;
            gfx::update(tableBuffer, map.table.data() + first, first * sizeof(za::U32), n * sizeof(za::U32));
            counts.tableBytes += static_cast<long long>(n * sizeof(za::U32));
        }
    }
    pending.table.clear();
    pending.tableAll = false;
}

void drawList(const za::Vector<BakeVertex>& v)
{
    constexpr za::SizeT chunk = 6 * 8192; // vertices an upload
    for(za::SizeT first = 0; first < v.size(); first += chunk)
    {
        const za::SizeT n = za::min(chunk, v.size() - first);
        GLuint buf;
        GLbyte* ofs;
        GL_Upload(GL_ARRAY_BUFFER, v.data() + first, n * sizeof(BakeVertex), &buf, &ofs);
        GL_BindBuffer(GL_ARRAY_BUFFER, buf);
        GL_VertexAttribPointerFunc(0, 4, GL_FLOAT, GL_FALSE, sizeof(BakeVertex), ofs);
        GL_VertexAttribPointerFunc(1, 4, GL_FLOAT, GL_FALSE, sizeof(BakeVertex), ofs + sizeof(glm::vec4));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(n));
    }
}

// The texture made now, its first layer emptied and the bake program drawn with once (the memory taken and the program
// readied by the driver now: the map's load, not the first mark's frame).
void prepareTexture()
{
    syncTexture();
    if(!atlas.texture)
    {
        return;
    }
    GLint framebuffer = 0, viewport[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    const GLboolean scissorOn = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, atlas.fbo);
    const GLfloat empty[4] = {512.f / 1023.f, 512.f / 1023.f, 512.f / 1023.f, 1.f};
    for(int level = 0; level < levels; level++)
    {
        GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.texture, level, 0);
        GL_ClearBufferfvFunc(GL_COLOR, 0, empty);
    }
    if(atlas.program)
    {
        float x0, x1, y0, y1;
        (void)corners(0, x0, x1, y0, y1);
        const glm::vec4 cell{-1.f, 0.f, 0.f, 0.f};
        const za::Vector<BakeVertex> quad{{{x0, y0, 0.5f, 0.5f}, cell}, {{x1, y0, 0.5f, 0.5f}, cell},
            {{x1, y1, 0.5f, 0.5f}, cell}, {{x0, y0, 0.5f, 0.5f}, cell}, {{x1, y1, 0.5f, 0.5f}, cell},
            {{x0, y1, 0.5f, 0.5f}, cell}};
        glViewport(0, 0, atlas.size, atlas.size);
        GL_UseProgram(atlas.program);
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(2));
        GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.texture, 0, 0);
        drawList(quad);
    }
    GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if(scissorOn)
    {
        glEnable(GL_SCISSOR_TEST);
    }
}

[[nodiscard]] bool anyIn(const za::Vector<za::Vector<BakeVertex>>& lists)
{
    for(const za::Vector<BakeVertex>& v : lists)
    {
        if(!v.empty())
        {
            return true;
        }
    }
    return false;
}

} // namespace

void flush(unsigned decalAtlas)
{
    if(atlas.layers > 0 && atlas.size != atlasSizeWanted() && laidOut())
    {
        (void)ensureAtlas(); // (vr_permdecal_atlas changed: made again at its size, what was baked baked again)
    }
    const bool draws = anyIn(pending.quads) || anyIn(pending.clears);
    const bool textureOld = atlas.layers > 0 && (atlas.textureSize != atlas.size || atlas.textureLayers != atlas.layers);
    if(atlas.layers == 0 || (!draws && !textureOld && pending.table.empty() && !pending.tableAll))
    {
        atlas.serial++;
        return;
    }
    QVR_GPU_PROFILE("permanent decals bake");
    const za::I64 started = za::Clock::nowNanoseconds();
    za::SizeT clears = 0;
    for(const za::Vector<BakeVertex>& v : pending.clears)
    {
        clears += v.size() / 6;
    }
    counts.flushes++;
    counts.quads += static_cast<long long>(pending.quadCount);
    counts.worstQuads = za::max(counts.worstQuads, static_cast<long long>(pending.quadCount));
    counts.worstClears = za::max(counts.worstClears, static_cast<long long>(clears));
    if(!atlas.program && !atlas.programFailed)
    {
        atlas.program = gfx::glProgram(bakeVertexSource, bakeFragmentSource, "vr permanent decals bake");
        atlas.programFailed = !atlas.program;
    }
    syncTexture();
    uploadTable();
    if(atlas.program && draws)
    {
        // (Nothing asked of GL to restore after: a glGet waits for the driver and the GPU, 54 ms measured with the GPU
        // busy. R_SetupView calls this before R_SetupGL binds the scene's framebuffer and viewport, as VR_RenderShadowMaps
        // after it leaves framebuffer 0 bound; no scissor is on in the 3D view.)
        glDisable(GL_SCISSOR_TEST);

        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, atlas.fbo);
        GL_UseProgram(atlas.program);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, decalAtlas);
        constexpr unsigned state = GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(2);
        for(int level = 0; level < levels; level++)
        {
            const int side = atlas.size >> level;
            glViewport(0, 0, side, side);
            for(int layer = 0; layer < atlas.layers; layer++)
            {
                const za::SizeT k = static_cast<za::SizeT>(layer);
                const bool clearsHere = k < pending.clears.size() && !pending.clears[k].empty();
                const bool quadsHere = k < pending.quads.size() && !pending.quads[k].empty();
                if(!clearsHere && !quadsHere)
                {
                    continue;
                }
                GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atlas.texture, level, layer);
                if(clearsHere)
                {
                    QVR_GPU_PROFILE("permdecal clears");
                    GL_SetState(GLS_BLEND_OPAQUE | state);
                    drawList(pending.clears[k]);
                }
                if(quadsHere)
                {
                    QVR_GPU_PROFILE("permdecal quads");
                    GL_SetState(GLS_BLEND_MULTIPLY | state);
                    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
                    drawList(pending.quads[k]);
                    glBlendFunc(GL_ZERO, GL_SRC_COLOR); // (what GLS_BLEND_MULTIPLY set: the state cache's)
                }
            }
        }
        GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);
    }
    for(za::Vector<BakeVertex>& v : pending.quads)
    {
        v.clear();
    }
    for(za::Vector<BakeVertex>& v : pending.clears)
    {
        v.clear();
    }
    pending.quadCount = 0;
    atlas.serial++;
    const double ms = static_cast<double>(za::Clock::nowNanoseconds() - started) / 1e6;
    counts.worstMs = za::max(counts.worstMs, ms);
    counts.slowFlushes += ms > 0.5;
}

void bind()
{
    if(!on() || !atlas.texture || atlas.free.size() == atlas.slots.size())
    {
        return;
    }
    GL_BindNative(GL_TEXTURE0 + 18, GL_TEXTURE_2D_ARRAY, atlas.texture);
    gfx::bindStorage(10, tableBuffer);
}

void clear()
{
    stamps.clear();
    waiting.clear();
    unbakedFrom = noneUnbaked;
    counts = Counts{};
    freeAllSlots(atlas.layers > 0 ? 1 : 0); // (a new map: the atlas one layer again, at the next flush)
}

void info_f()
{
    const za::SizeT used = atlas.slots.size() - atlas.free.size();
    za::SizeT laid = 0;
    for(const Layout& l : map.faces)
    {
        laid += l.across != 0;
    }
    Con_Printf("permdecals: %s, %.2f units/texel; %d faces laid out, %d pages in the table\n",
        vr_permdecals.value ? (on() ? "on" : "on (no map)") : "off", map.density,
        static_cast<int>(laid), static_cast<int>(map.table.size()));
    Con_Printf("permdecals: atlas %d^2 x %d layers (%.1f MB with mips; %d at most), %d of %d slots used, grown %lld times\n",
        atlas.size, atlas.layers, layerBytes(atlas.size) * atlas.layers / (1024.0 * 1024.0), layersMax(),
        static_cast<int>(used), static_cast<int>(atlas.slots.size()), counts.grown);
    int onModels = 0;
    for(const Stamp& s : stamps)
    {
        onModels += s.model != 0;
    }
    Con_Printf("permdecals: %d stamps kept (%d on brush models); baked %lld stamps on %lld faces, %lld pages taken, %lld "
               "given up, %lld dropped\n",
        static_cast<int>(stamps.size()), onModels, counts.stamps, counts.faces, counts.pages, counts.evicted, counts.dropped);
    if(!waiting.empty())
    {
        Con_Printf("permdecals: %d marks on brush models waiting to show\n", static_cast<int>(waiting.size()));
    }
    Con_Printf("permdecals: %lld flushes drew %lld quads (%lld the most in one, %lld slots emptied the most in one), "
               "%.1f KB of page table uploaded%s\n",
        counts.flushes, counts.quads, counts.worstQuads, counts.worstClears, static_cast<double>(counts.tableBytes) / 1024.0,
        unbakedFrom != noneUnbaked ? "; some kept while off, to bake once on" : "");
    Con_Printf("permdecals: flush CPU %.3f ms at most, %lld over 0.5 ms\n", counts.worstMs, counts.slowFlushes);
}

const char* saveLines(const za::Vector<Stamp>& live)
{
    SaveState& s = saves;
    s.lines.clear();
    if(!laidOut() || (stamps.empty() && live.empty()))
    {
        return "";
    }
    const za::SizeT count = stamps.size() + live.size();
    za::Vector<za::U8> bytes; // (a save's: no scratch kept for it)
    bytes.resize(count * recordBytes);
    for(za::SizeT i = 0; i < count; i++)
    {
        encode(i < stamps.size() ? stamps[i] : live[i - stamps.size()], bytes.data() + i * recordBytes);
    }
    s.lines += va("// qvrdecals 1 map %s n %d\n", map.name, static_cast<int>(count));
    za::SizeT column = 0;
    for(za::SizeT i = 0; i < bytes.size(); i += 3)
    {
        if(column == 0)
        {
            s.lines += "// qvrd ";
        }
        const za::SizeT left = bytes.size() - i;
        const unsigned v = (static_cast<unsigned>(bytes[i]) << 16) | (left > 1 ? static_cast<unsigned>(bytes[i + 1]) << 8 : 0u) |
                           (left > 2 ? static_cast<unsigned>(bytes[i + 2]) : 0u);
        s.lines += base64Chars[(v >> 18) & 63];
        s.lines += base64Chars[(v >> 12) & 63];
        s.lines += left > 1 ? base64Chars[(v >> 6) & 63] : '=';
        s.lines += left > 2 ? base64Chars[v & 63] : '=';
        column += 4;
        if(column >= lineChars || i + 3 >= bytes.size())
        {
            s.lines += '\n';
            column = 0;
        }
    }
    return s.lines.cStr();
}

void readSaveLines(const char* text)
{
    SaveState& s = saves;
    s.pending.clear();
    s.pendingAfterLoad = -1;
    const char* header = text ? strstr(text, "\n// qvrdecals 1 map ") : nullptr;
    int count = 0;
    if(!header || sscanf(header + 1, "// qvrdecals 1 map %63s n %d", s.pendingMap, &count) != 2 || count <= 0)
    {
        return;
    }
    unsigned v = 0;
    int bits = 0;
    for(const char* p = strstr(text, "\n// qvrd "); p; p = strstr(p + 1, "\n// qvrd "))
    {
        for(const char* c = p + 9; *c && *c != '\r' && *c != '\n' && *c != '='; c++)
        {
            const int x = base64Value(*c);
            if(x < 0)
            {
                continue;
            }
            v = (v << 6) | static_cast<unsigned>(x);
            bits += 6;
            if(bits >= 8)
            {
                bits -= 8;
                s.pending.pushBack(static_cast<za::U8>((v >> bits) & 0xFF));
            }
        }
    }
    if(s.pending.size() < static_cast<za::SizeT>(count) * recordBytes)
    {
        Con_Printf("permdecals: the saved game's marks are cut short (%d of %d): not baked again\n",
            static_cast<int>(s.pending.size() / recordBytes), count);
        s.pending.clear();
        return;
    }
    s.pending.resize(static_cast<za::SizeT>(count) * recordBytes);
    s.pendingAfterLoad = s.mapLoads + 1;
}

int restore()
{
    SaveState& s = saves;
    if(s.pendingAfterLoad < 0 || s.mapLoads < s.pendingAfterLoad || !laidOut())
    {
        return 0;
    }
    s.pendingAfterLoad = -1;
    int result = 0;
    const int count = static_cast<int>(s.pending.size() / recordBytes);
    if((on() || kept()) && !ZA_STRCMP(s.pendingMap, map.name))
    {
        int baked = 0;
        for(int i = 0; i < count; i++)
        {
            const Stamp st = decode(s.pending.data() + static_cast<za::SizeT>(i) * recordBytes);
            if(kept())
            {
                unbakedFrom = za::min(unbakedFrom, stamps.size());
                keep(st); // (off: kept, the newest shown live: vr_decals.cpp's showRecord)
                result = 2;
            }
            else if(bakeOn(st) > 0)
            {
                keep(st);
                baked++;
                result = 1;
            }
        }
        Con_DPrintf("permdecals: %d of the saved game's %d marks %s\n", result == 2 ? count : baked, count,
            result == 2 ? "kept (the layer off)" : "baked again");
    }
    s.pending.clear();
    return result;
}

} // namespace qvr::permdecals

using namespace qvr;

// SaveData_Fill: the permanent decals as the save's "// qvrdecals" and "// qvrd" lines ("" none; valid until the next).
extern "C" const char* VR_SaveDecalLines(void)
{
    qvr::decals::liveStamps(qvr::permdecals::saves.live); // (the live ring's marks not baked yet: kept too)
    qvr::permdecals::waitingStamps(qvr::permdecals::saves.live); // (and the brush models' not shown yet)
    return qvr::permdecals::saveLines(qvr::permdecals::saves.live);
}

// GL_BuildBModelVertexBuffer: the world model's faces laid out (kept if the same map is loaded again at the same
// density: what was baked stays).
extern "C" void VR_PermDecalsLayout(void)
{
    using namespace qvr::permdecals;
    const qmodel_t* m = cl.worldmodel;
    const float density = za::clamp(vr_permdecal_density.value, 0.25f, 8.f);
    if(!m)
    {
        return;
    }
    saves.mapLoads++; // (a saved game's marks wait for its map: restore)
    if(map.model == m && map.surfaces == m->numsurfaces && map.density == density && !ZA_STRCMP(map.name, m->name))
    {
        return;
    }
    q_strlcpy(map.name, m->name, sizeof(map.name));
    map.model = m;
    map.surfaces = m->numsurfaces;
    map.density = density;
    map.faces.assign(static_cast<za::SizeT>(m->numsurfaces), Layout{});
    map.table.clear();
    map.submodels.assign(MAX_MODELS, nullptr);
    for(int j = 1; j < MAX_MODELS; j++)
    {
        const qmodel_t* sub = cl.model_precache[j];
        if(sub && sub->type == mod_brush && sub->name[0] == '*')
        {
            const int n = atoi(sub->name + 1);
            if(n > 0 && n < MAX_MODELS)
            {
                map.submodels[static_cast<za::SizeT>(n)] = sub;
            }
        }
    }
    for(int i = 0; i < m->numsurfaces; i++)
    {
        const msurface_t& s = m->surfaces[i];
        if(s.flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_DRAWFENCE | SURF_NOTEXTURE | SURF_DRAWTILED) || s.numedges < 3)
        {
            continue;
        }
        const glm::vec3 n{s.plane->normal[0], s.plane->normal[1], s.plane->normal[2]};
        const float* sv = s.texinfo->vecs[0];
        glm::vec3 t = glm::vec3{sv[0], sv[1], sv[2]};
        t -= n * glm::dot(t, n);
        if(glm::length(t) < 1e-4f)
        {
            t = glm::cross(n, za::fabs(n.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f});
        }
        t = glm::normalize(t);
        const glm::vec3 b = glm::cross(n, t);
        float t0 = 1e30f, t1 = -1e30f, b0 = 1e30f, b1 = -1e30f;
        for(int e = 0; e < s.numedges; e++)
        {
            const int se = m->surfedges[s.firstedge + e];
            const unsigned int vi = se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1];
            const glm::vec3 p{m->vertexes[vi].position[0], m->vertexes[vi].position[1], m->vertexes[vi].position[2]};
            t0 = za::min(t0, glm::dot(p, t));
            t1 = za::max(t1, glm::dot(p, t));
            b0 = za::min(b0, glm::dot(p, b));
            b1 = za::max(b1, glm::dot(p, b));
        }
        const int across = za::max(1, static_cast<int>(za::ceil((t1 - t0) / density / pageTexels - 1e-4f)));
        const int down = za::max(1, static_cast<int>(za::ceil((b1 - b0) / density / pageTexels - 1e-4f)));
        if(across >= pagesDown || down >= pagesDown ||
            map.table.size() + static_cast<za::SizeT>(across) * static_cast<za::SizeT>(down) > tableMax)
        {
            continue;
        }
        Layout& l = map.faces[static_cast<za::SizeT>(i)];
        l.t = t;
        l.b = b;
        l.t0 = t0;
        l.b0 = b0;
        l.origin = n * s.plane->dist + t * t0 + b * b0;
        l.table = static_cast<za::U32>(map.table.size());
        l.across = static_cast<za::U16>(across);
        l.down = static_cast<za::U16>(down);
        map.table.resize(map.table.size() + static_cast<za::SizeT>(across) * static_cast<za::SizeT>(down), 0u);
    }
    stamps.clear();
    waiting.clear();
    unbakedFrom = noneUnbaked;
    counts = Counts{};
    freeAllSlots(atlas.layers > 0 ? 1 : 0);
    // The bake program and the atlas's first layer made now, in the load (with the layer on): not at the first mark (a
    // hitch of 5 ms there: the texture's memory taken at its first draw).
    if(!atlas.program && !atlas.programFailed)
    {
        atlas.program = gfx::glProgram(bakeVertexSource, bakeFragmentSource, "vr permanent decals bake");
        atlas.programFailed = !atlas.program;
    }
    if(vr_permdecals.value != 0.f && ensureAtlas())
    {
        prepareTexture();
    }
}

// GL_BuildBModelVertexBuffer: a vertex's coordinates in the layer (QVR_WORLD_VS_PERMDECALS): its texel on face
// `surface` of model `m`, the face's first page and its pages (across + 4096 down); zero: none.
extern "C" void VR_PermDecalsVertex(const qmodel_t* m, int surface, const float* pos, float out[4])
{
    using namespace qvr::permdecals;
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(m != map.model || surface < 0 || surface >= static_cast<int>(map.faces.size()))
    {
        return;
    }
    const Layout& l = map.faces[static_cast<za::SizeT>(surface)];
    if(l.across == 0)
    {
        return;
    }
    const glm::vec3 p{pos[0], pos[1], pos[2]};
    out[0] = (glm::dot(p, l.t) - l.t0) / map.density;
    out[1] = (glm::dot(p, l.b) - l.b0) / map.density;
    out[2] = static_cast<float>(l.table);
    out[3] = static_cast<float>(l.across) + static_cast<float>(l.down) * static_cast<float>(pagesDown);
}

// R_DrawBrushModels_Real: the layer read this frame (vertex attribute 6 enabled for the solid pass).
extern "C" int VR_PermDecalsOn(void)
{
    using namespace qvr::permdecals;
    return on() && atlas.texture && atlas.free.size() != atlas.slots.size();
}

// R_SetupView: the shader's settings (PermDecals: x 1 on, y slots a row, z one over the atlas's side, w 1: the pages
// shown, vr_permdecal_show).
extern "C" void VR_PermDecalsFrame(float out[4])
{
    using namespace qvr::permdecals;
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!VR_PermDecalsOn())
    {
        return;
    }
    out[0] = 1.f + static_cast<float>(TexMgr_NearestKind()); // (2, 3: read on the texels of the textures drawn nearest)
    out[1] = static_cast<float>(atlas.perRow);
    out[2] = 1.f / static_cast<float>(atlas.size);
    out[3] = vr_permdecal_show.value != 0.f ? 1.f : 0.f;
}
