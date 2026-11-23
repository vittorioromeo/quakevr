// vr_rangerhead.cpp -- the body's head made of the Quake ranger's: see vr_rangerhead.hpp.

#include "vr_rangerhead.hpp"
#include "vr_geom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include "vr_api_render.h"
#include "vr_alloccount.h"
#include "vr_testkit.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

constexpr const char* sourceName = "progs/player.mdl";
constexpr const char* standFrame = "stand1";

// A player.mdl taken for the ranger's (Quake's: 212 vertices, 408 triangles, a 296 x 194 skin, 52 units tall in
// stand1). Anything else (a mod's own player, Quake VR's quakevr/progs/player.mdl) is passed over.
constexpr int minVerts = 150;
constexpr int maxVerts = 400;
constexpr int maxTris = 1000;
constexpr int maxSkinSide = 1024;
constexpr float minHeight = 40.f;
constexpr float maxHeight = 70.f;

// The neck: the triangles wholly above this fraction of stand1's height down from its top are the head (Quake's: the
// cut at 19.54 units, between the head's lowest vertex, 19.69, and the collar's highest, 19.39). No vertex may lie
// within neckClearance of it (else it is some other model: the cut unsure).
constexpr float neckFraction = 0.1474f;
constexpr float neckClearance = 0.05f;
// The head found (Quake's: 27 triangles, 18 vertices, about 7 x 8.5 x 7.5 units).
constexpr int minHeadTris = 12;
constexpr int maxHeadTris = 200;
constexpr float minHeadSide = 3.f;
constexpr float maxHeadSide = 15.f;
constexpr int maxBoundary = 64; // the neck's opening (Quake's: 7 edges), capped with a fan

// The detailed head (vr_body_ranger_head_model 1): Quake VR's own progs/player.mdl (733 vertices, 1062 triangles, a 256 x
// 256 skin, 144 unnamed frames in Quake's order: stand1 the 13th, 52.8 units tall), the one the Dawn of the Machine's
// ranger corpses wear. His head is two islands of their own (the hood, 89 triangles; the face, 42), every island whose
// lowest vertex is above `islandFraction` of stand1's height down from its top (the next island's lowest is 9.8 units
// further down). Drawn as a mesh of its own after the body's (shader `detailShader`) with his own skin.
constexpr int detailMinVerts = 400;
constexpr int detailMaxVerts = 3000;
constexpr int detailMaxTris = 6000;
constexpr int detailStandIndex = 12; // stand1 (when no frame is named so)
constexpr float islandFraction = 0.2f;
constexpr int detailMinHeadTris = 40;
constexpr int detailMaxHeadTris = 1000;
constexpr const char* detailShader = "vrrangerhead";
constexpr const char* headTexName = "progs/vrrangerhead_00_00"; // his skin, the mesh's texture (VR_RangerHeadSkins)
// His place on the body: `detailGrow` times as wide as the placeholder head, his bottom `detailSink` below its (his hood
// reaches lower than Quake's head: the neck goes up into it).
constexpr float detailGrow = 1.05f;
constexpr float detailSink = 1.5f;
// His texture coordinates on the body (packDetailed): the body's head block's head-only texels (the placeholder's, not
// drawn with his head; the neck's top ring keeps v <= neckTopV), a pad round them; his islands `detailGap` apart. The
// wound masks are the body's skin's layout: his wounds there, nothing else's.
constexpr float detailU0 = 0.008f;
constexpr float detailU1 = 0.492f;
constexpr float detailV0 = 0.447f;
constexpr float detailV1 = 0.496f;
constexpr float detailGap = 0.008f;
constexpr int maxPackedSide = 2048; // his skin laid out so: its texture's longest side

// On the body (progs/vrbody*: Quake units, 26.25 a metre, X forward, Z up): `grow` times as wide as the placeholder's
// head-only vertices (his: 20 x 20 x 18 cm), centred over them, its bottom `sink` below theirs (the neck's top ring,
// which flares to the placeholder's jaw, inside it: 4 cm); the crown about where the placeholder's was.
constexpr float grow = 1.2f;
constexpr float sink = 1.f;
// The neck's top ring keeps the neck's skin: its texture coordinates no higher than this (the head's texels above).
constexpr float neckTopV = 0.441f;

// The body's skins (make_vrbody.py BLOCKS["head"]: u 0 .. 0.5, v 0.4 .. 0.5, a 2/128 pad): the placeholder's head-only
// texels (v 0.450 .. 0.484) are where the ranger's go; the neck's (v 0.424 .. 0.441) keep the body's skin. His front
// half left, his back half right, `gap` between them; `paint*` the texels painted (the pads' too: clamped).
constexpr float destU0 = 0.016f;
constexpr float destU1 = 0.484f;
constexpr float destV0 = 0.447f;
constexpr float destV1 = 0.484f;
constexpr float gap = 0.008f;
constexpr float paintU0 = 0.f;
constexpr float paintU1 = 0.5f;
constexpr float paintV0 = 0.444f;
constexpr float paintV1 = 0.5f;
// The placeholder's head-only texels must lie in the block (else the body is another layout: left as it is).
constexpr float blockU0 = 0.f;
constexpr float blockU1 = 0.5f;
constexpr float blockV0 = 0.4f;
constexpr float blockV1 = 0.5f;

// The ranger's skin colours (Quake's palette row 7: his face, as the body's arms, 116-123): the skin mask's skin.
constexpr int skinFirst = 112;
constexpr int skinLast = 127;

struct Rect
{
    float x0{0.f};
    float y0{0.f};
    float x1{1.f};
    float y1{1.f};
};

struct Ranger
{
    bool looked{false}; // vr_body_ranger_head latched and the ranger looked for (once a session: the first body loaded)
    bool on{false};     // the option, as latched
    bool ok{false};     // found and its head taken
    bool detailed{false}; // Quake VR's detailed head (a mesh and skin of its own), else Quake's (painted on the body's)
    za::Vector<glm::vec3> pos;       // the head's vertices (units, stand1), the cap's last
    za::Vector<glm::vec2> src;       // their texels in his skin (continuous: s + 0.5; the back half's past skinW / 2)
    za::Vector<unsigned short> tris; // three a triangle, into pos
    glm::vec3 mins{0.f};
    glm::vec3 maxs{0.f};
    za::Vector<unsigned char> skin; // skin 0, palette indices
    int skinW{0};
    int skinH{0};
    Rect half[2]; // the texels his front (0) and back (1) halves take
    char path[MAX_OSPATH]{};
    // The detailed head (packDetailed): his skin's islands (their texels, `from`) side by side in the body's head block
    // (`to`, the body's texture coordinates), his vertices' coordinates there (`st`: the wound masks are the body's, read
    // there) and his skin laid out so (`packed`, packedW x packedH, palette indices: the mesh's own texture).
    struct Island
    {
        Rect from;
        Rect to;
    };
    za::Vector<Island> islands;
    za::Vector<glm::vec2> st;
    za::Vector<unsigned char> packed;
    int packedW{0};
    int packedH{0};
};
Ranger ranger;

// A little-endian reader over a file's bytes: `bad` once anything is out of range.
struct Reader
{
    const unsigned char* p{nullptr};
    za::SizeT n{0};
    za::SizeT at{0};
    bool bad{false};

    const unsigned char* take(za::SizeT k)
    {
        if(bad || k > n - at)
        {
            bad = true;
            return nullptr;
        }
        const unsigned char* r = p + at;
        at += k;
        return r;
    }
    int i32()
    {
        const unsigned char* b = take(4);
        int v = 0;
        if(b)
        {
            ZA_MEMCPY(&v, b, 4);
        }
        return LittleLong(v);
    }
    float f32()
    {
        const unsigned char* b = take(4);
        float v = 0.f;
        if(b)
        {
            ZA_MEMCPY(&v, b, 4);
        }
        return LittleFloat(v);
    }
};

struct StVert
{
    int onSeam;
    int s;
    int t;
};

struct Tri
{
    int front;
    int v[3];
};

[[nodiscard]] bool readFile(FILE* f, long offset, long length, za::Vector<unsigned char>& out)
{
    if(length <= 0 || length > 16 * 1024 * 1024)
    {
        return false;
    }
    out.resize(static_cast<za::SizeT>(length));
    return fseek(f, offset, SEEK_SET) == 0 && fread(out.data(), 1, out.size(), f) == out.size();
}

// The search path's progs/player.mdl (its pak's entry, or its folder's file), if it has one.
[[nodiscard]] bool readFrom(const searchpath_t* s, za::Vector<unsigned char>& out, char* where, za::SizeT whereSize)
{
    if(s->pack)
    {
        for(int i = 0; i < s->pack->numfiles; i++)
        {
            const packfile_t& e = s->pack->files[i];
            if(q_strcasecmp(e.name, sourceName))
            {
                continue;
            }
            FILE* f = Sys_fopen(s->pack->filename, "rb");
            if(!f)
            {
                return false;
            }
            const bool ok = readFile(f, e.filepos, e.filelen, out);
            fclose(f);
            q_snprintf(where, whereSize, "%s/%s", s->pack->filename, sourceName);
            return ok;
        }
        return false;
    }
    q_snprintf(where, whereSize, "%s/%s", s->filename, sourceName);
    FILE* f = Sys_fopen(where, "rb");
    if(!f)
    {
        return false;
    }
    bool ok = fseek(f, 0, SEEK_END) == 0;
    const long length = ok ? ftell(f) : 0;
    ok = ok && readFile(f, 0, length, out);
    fclose(f);
    return ok;
}

// Its head in stand1, into `out` (pos, src, tris, mins, maxs, half); false if it is not like Quake's ranger.
[[nodiscard]] bool takeHead(const za::Vector<glm::vec3>& p, const za::Vector<StVert>& st, const za::Vector<Tri>& tris,
    int skinW, Ranger& out)
{
    float zMin = p[0].z;
    float zMax = p[0].z;
    for(const glm::vec3& v : p)
    {
        zMin = za::min(zMin, v.z);
        zMax = za::max(zMax, v.z);
    }
    const float height = zMax - zMin;
    if(height < minHeight || height > maxHeight)
    {
        return false;
    }
    const float cut = zMax - neckFraction * height;
    for(const glm::vec3& v : p)
    {
        if(v.z > cut - neckClearance && v.z < cut + neckClearance)
        {
            return false;
        }
    }

    // The head's triangles; a vertex once for each half of the skin it is drawn from (a seam vertex's back copy).
    za::Vector<int> remap;
    remap.resize(p.size() * 2);
    for(int& r : remap)
    {
        r = -1;
    }
    za::Vector<int> origin; // each output vertex's own
    out.pos.clear();
    out.src.clear();
    out.tris.clear();
    bool seen[2]{false, false};
    for(const Tri& t : tris)
    {
        if(p[t.v[0]].z <= cut || p[t.v[1]].z <= cut || p[t.v[2]].z <= cut)
        {
            continue;
        }
        for(const int i : t.v)
        {
            const bool back = !t.front && st[i].onSeam;
            const int key = i * 2 + (back ? 1 : 0);
            if(remap[key] < 0)
            {
                remap[key] = static_cast<int>(out.pos.size());
                out.pos.pushBack(p[i]);
                out.src.pushBack({static_cast<float>(st[i].s + (back ? skinW / 2 : 0)) + 0.5f, static_cast<float>(st[i].t) + 0.5f});
                origin.pushBack(i);
            }
            out.tris.pushBack(static_cast<unsigned short>(remap[key]));
        }
    }
    const int headTris = static_cast<int>(out.tris.size() / 3);
    if(headTris < minHeadTris || headTris > maxHeadTris)
    {
        return false;
    }

    out.mins = out.maxs = out.pos[0];
    for(const glm::vec3& v : out.pos)
    {
        out.mins = glm::min(out.mins, v);
        out.maxs = glm::max(out.maxs, v);
    }
    const glm::vec3 size = out.maxs - out.mins;
    if(za::min(size.x, za::min(size.y, size.z)) < minHeadSide || za::max(size.x, za::max(size.y, size.z)) > maxHeadSide)
    {
        return false;
    }

    // The halves' texels: [first s, last s + 1) x [first t, last t + 1).
    for(za::SizeT i = 0; i < out.src.size(); i++)
    {
        const glm::vec2 s = out.src[i];
        const int h = s.x >= static_cast<float>(skinW / 2) ? 1 : 0;
        Rect& r = out.half[h];
        const Rect texel{s.x - 0.5f, s.y - 0.5f, s.x + 0.5f, s.y + 0.5f};
        r = seen[h] ? Rect{za::min(r.x0, texel.x0), za::min(r.y0, texel.y0), za::max(r.x1, texel.x1), za::max(r.y1, texel.y1)}
                    : texel;
        seen[h] = true;
    }
    if(!seen[0] && !seen[1])
    {
        return false;
    }
    if(!seen[0] || !seen[1])
    {
        out.half[seen[0] ? 1 : 0] = out.half[seen[0] ? 0 : 1];
    }

    // The neck's opening: the edges one head triangle has (by the model's own vertices: a seam's two copies are one),
    // closed with a fan to their middle, wound against them, one colour (the texel of its foremost vertex: the chin's).
    za::Vector<int> edges; // a, b (output vertices) per boundary edge, as its triangle winds it
    const za::SizeT headIndexes = out.tris.size();
    for(za::SizeT t = 0; t < headIndexes; t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = out.tris[t + k];
            const int b = out.tris[t + (k + 1) % 3];
            int uses = 0;
            for(za::SizeT u = 0; u < headIndexes; u += 3)
            {
                for(int m = 0; m < 3; m++)
                {
                    const int c = origin[out.tris[u + m]];
                    const int d = origin[out.tris[u + (m + 1) % 3]];
                    if((c == origin[a] && d == origin[b]) || (c == origin[b] && d == origin[a]))
                    {
                        uses++;
                    }
                }
            }
            if(uses == 1)
            {
                edges.pushBack(a);
                edges.pushBack(b);
            }
        }
    }
    const int boundary = static_cast<int>(edges.size() / 2);
    if(boundary >= 3 && boundary <= maxBoundary)
    {
        glm::vec3 middle{0.f};
        for(const int e : edges)
        {
            middle += out.pos[e];
        }
        middle /= static_cast<float>(edges.size());
        int front = edges[0];
        for(const int e : edges)
        {
            front = out.pos[e].x > out.pos[front].x ? e : front;
        }
        const glm::vec2 chin = out.src[front];
        const int centre = static_cast<int>(out.pos.size());
        out.pos.pushBack(middle);
        out.src.pushBack(chin);
        for(int e = 0; e < boundary; e++)
        {
            const int a = static_cast<int>(out.pos.size());
            out.pos.pushBack(out.pos[edges[e * 2]]);
            out.src.pushBack(chin);
            out.pos.pushBack(out.pos[edges[e * 2 + 1]]);
            out.src.pushBack(chin);
            out.tris.pushBack(static_cast<unsigned short>(a + 1));
            out.tris.pushBack(static_cast<unsigned short>(a));
            out.tris.pushBack(static_cast<unsigned short>(centre));
        }
    }
    return true;
}

// The detailed head in stand1, into `out` (pos, src, tris, mins, maxs): the islands (triangles joined by their vertices)
// wholly above the neck; false if there are none like a head.
[[nodiscard]] bool takeIslands(const za::Vector<glm::vec3>& p, const za::Vector<StVert>& st, const za::Vector<Tri>& tris,
    int skinW, Ranger& out)
{
    float zMin = p[0].z;
    float zMax = p[0].z;
    for(const glm::vec3& v : p)
    {
        zMin = za::min(zMin, v.z);
        zMax = za::max(zMax, v.z);
    }
    const float height = zMax - zMin;
    if(height < minHeight || height > maxHeight)
    {
        return false;
    }
    const float cut = zMax - islandFraction * height;

    za::Vector<int> parent;
    parent.resize(p.size());
    for(za::SizeT i = 0; i < p.size(); i++)
    {
        parent[i] = static_cast<int>(i);
    }
    const auto root = [&](int a) {
        while(parent[static_cast<za::SizeT>(a)] != a)
        {
            parent[static_cast<za::SizeT>(a)] = parent[static_cast<za::SizeT>(parent[static_cast<za::SizeT>(a)])];
            a = parent[static_cast<za::SizeT>(a)];
        }
        return a;
    };
    for(const Tri& t : tris)
    {
        const int a = root(t.v[0]);
        parent[static_cast<za::SizeT>(root(t.v[1]))] = a;
        parent[static_cast<za::SizeT>(root(t.v[2]))] = a;
    }
    za::Vector<float> lowest; // per island (by its root)
    lowest.resize(p.size(), zMax);
    for(za::SizeT i = 0; i < p.size(); i++)
    {
        const za::SizeT r = static_cast<za::SizeT>(root(static_cast<int>(i)));
        lowest[r] = za::min(lowest[r], p[i].z);
    }

    za::Vector<int> remap;
    remap.resize(p.size() * 2, -1);
    out.pos.clear();
    out.src.clear();
    out.tris.clear();
    for(const Tri& t : tris)
    {
        if(lowest[static_cast<za::SizeT>(root(t.v[0]))] <= cut)
        {
            continue;
        }
        for(const int i : t.v)
        {
            const bool back = !t.front && st[i].onSeam;
            const int key = i * 2 + (back ? 1 : 0);
            if(remap[static_cast<za::SizeT>(key)] < 0)
            {
                remap[static_cast<za::SizeT>(key)] = static_cast<int>(out.pos.size());
                out.pos.pushBack(p[i]);
                out.src.pushBack({static_cast<float>(st[i].s + (back ? skinW / 2 : 0)) + 0.5f, static_cast<float>(st[i].t) + 0.5f});
            }
            out.tris.pushBack(static_cast<unsigned short>(remap[static_cast<za::SizeT>(key)]));
        }
    }
    const int headTris = static_cast<int>(out.tris.size() / 3);
    if(headTris < detailMinHeadTris || headTris > detailMaxHeadTris)
    {
        return false;
    }
    out.mins = out.maxs = out.pos[0];
    for(const glm::vec3& v : out.pos)
    {
        out.mins = glm::min(out.mins, v);
        out.maxs = glm::max(out.maxs, v);
    }
    const glm::vec3 size = out.maxs - out.mins;
    return za::min(size.x, za::min(size.y, size.z)) >= minHeadSide && za::max(size.x, za::max(size.y, size.z)) <= maxHeadSide;
}

// The palette index the detailed head's packed skin has at the body's texture coordinates (u, v): the island nearest
// across (its texels clamped to it), anywhere (the pads and the texture round the block take the nearest).
[[nodiscard]] int detailIndexAt(const Ranger& r, float u, float v)
{
    const Ranger::Island* best = nullptr;
    float bestGap = 1e30f;
    for(const Ranger::Island& is : r.islands)
    {
        const float gapU = u < is.to.x0 ? is.to.x0 - u : u > is.to.x1 ? u - is.to.x1 : 0.f;
        if(gapU < bestGap)
        {
            bestGap = gapU;
            best = &is;
        }
    }
    if(!best)
    {
        return 0;
    }
    const Rect& f = best->from;
    const Rect& t = best->to;
    const float x = za::clamp(f.x0 + (u - t.x0) / (t.x1 - t.x0) * (f.x1 - f.x0), f.x0, f.x1 - 0.01f);
    const float y = za::clamp(f.y0 + (v - t.y0) / (t.y1 - t.y0) * (f.y1 - f.y0), f.y0, f.y1 - 0.01f);
    const int xi = za::clamp(static_cast<int>(x), 0, r.skinW - 1);
    const int yi = za::clamp(static_cast<int>(y), 0, r.skinH - 1);
    return r.skin[static_cast<za::SizeT>(yi) * static_cast<za::SizeT>(r.skinW) + static_cast<za::SizeT>(xi)];
}

// The detailed head's islands packed into the body's head block (the placeholder's head-only texels, free once his head
// is drawn instead): left to right, `detailGap` apart, all stretched alike to fill it (the wound masks are painted where
// they are struck, in the world: a stretch only changes how fine they are there). His vertices' coordinates (`st`) and
// his skin laid out so (`packed`, each axis as fine as his skin's texels or finer, up to maxPackedSide). False: none.
[[nodiscard]] bool packDetailed(Ranger& r)
{
    const za::SizeT nv = r.src.size();
    if(nv == 0 || r.skin.empty() || r.skinW <= 0 || r.skinH <= 0)
    {
        return false;
    }
    za::Vector<int> parent;
    parent.resize(nv);
    for(za::SizeT i = 0; i < nv; i++)
    {
        parent[i] = static_cast<int>(i);
    }
    const auto root = [&](int a) {
        while(parent[static_cast<za::SizeT>(a)] != a)
        {
            parent[static_cast<za::SizeT>(a)] = parent[static_cast<za::SizeT>(parent[static_cast<za::SizeT>(a)])];
            a = parent[static_cast<za::SizeT>(a)];
        }
        return a;
    };
    for(za::SizeT t = 0; t + 2 < r.tris.size(); t += 3)
    {
        const int a = root(r.tris[t]);
        parent[static_cast<za::SizeT>(root(r.tris[t + 1]))] = a;
        parent[static_cast<za::SizeT>(root(r.tris[t + 2]))] = a;
    }
    za::Vector<int> islandOf; // per vertex
    islandOf.resize(nv, -1);
    za::Vector<int> islandOfRoot;
    islandOfRoot.resize(nv, -1);
    r.islands.clear();
    for(za::SizeT i = 0; i < nv; i++)
    {
        const za::SizeT root_ = static_cast<za::SizeT>(root(static_cast<int>(i)));
        if(islandOfRoot[root_] < 0)
        {
            islandOfRoot[root_] = static_cast<int>(r.islands.size());
            r.islands.pushBack({Rect{r.src[i].x, r.src[i].y, r.src[i].x, r.src[i].y}, Rect{}});
        }
        const int k = islandOfRoot[root_];
        islandOf[i] = k;
        Rect& f = r.islands[static_cast<za::SizeT>(k)].from;
        f.x0 = za::min(f.x0, r.src[i].x);
        f.y0 = za::min(f.y0, r.src[i].y);
        f.x1 = za::max(f.x1, r.src[i].x);
        f.y1 = za::max(f.y1, r.src[i].y);
    }
    float sumW = 0.f;
    float maxH = 0.f;
    for(Ranger::Island& is : r.islands)
    {
        is.from.x0 -= 0.5f; // (the texels the corners are the middles of, whole)
        is.from.y0 -= 0.5f;
        is.from.x1 += 0.5f;
        is.from.y1 += 0.5f;
        sumW += is.from.x1 - is.from.x0;
        maxH = za::max(maxH, is.from.y1 - is.from.y0);
    }
    const float n = static_cast<float>(r.islands.size());
    const float ku = (detailU1 - detailU0 - detailGap * (n - 1.f)) / sumW; // body coordinates a texel of his
    const float kv = (detailV1 - detailV0) / maxH;
    if(ku <= 0.f || kv <= 0.f)
    {
        return false;
    }
    float u = detailU0;
    for(Ranger::Island& is : r.islands)
    {
        is.to = Rect{u, detailV0, u + (is.from.x1 - is.from.x0) * ku, detailV0 + (is.from.y1 - is.from.y0) * kv};
        u = is.to.x1 + detailGap;
    }
    r.st.resize(nv);
    for(za::SizeT i = 0; i < nv; i++)
    {
        const Ranger::Island& is = r.islands[static_cast<za::SizeT>(islandOf[i])];
        r.st[i] = {is.to.x0 + (r.src[i].x - is.from.x0) * ku, is.to.y0 + (r.src[i].y - is.from.y0) * kv};
    }
    const auto side = [](float texels) {
        int s = 64;
        while(s < maxPackedSide && static_cast<float>(s) < texels)
        {
            s *= 2;
        }
        return s;
    };
    r.packedW = side(1.f / ku);
    r.packedH = side(1.f / kv);
    r.packed.resize(static_cast<za::SizeT>(r.packedW) * static_cast<za::SizeT>(r.packedH));
    for(int y = 0; y < r.packedH; y++)
    {
        for(int x = 0; x < r.packedW; x++)
        {
            r.packed[static_cast<za::SizeT>(y) * static_cast<za::SizeT>(r.packedW) + static_cast<za::SizeT>(x)] = static_cast<unsigned char>(
                detailIndexAt(r, (static_cast<float>(x) + 0.5f) / static_cast<float>(r.packedW), (static_cast<float>(y) + 0.5f) / static_cast<float>(r.packedH)));
        }
    }
    return true;
}

// A player.mdl's bytes: Quake's ranger, his head taken (into `out`), or false. `detailed`: Quake VR's detailed one.
[[nodiscard]] bool parse(const za::Vector<unsigned char>& file, Ranger& out, bool detailed)
{
    Reader r{file.data(), file.size()};
    const unsigned char* ident = r.take(4);
    if(!ident || ZA_MEMCMP(ident, "IDPO", 4) || r.i32() != ALIAS_VERSION)
    {
        return false;
    }
    glm::vec3 scale{0.f};
    glm::vec3 origin{0.f};
    for(int k = 0; k < 3; k++)
    {
        scale[k] = r.f32();
    }
    for(int k = 0; k < 3; k++)
    {
        origin[k] = r.f32();
    }
    r.take(16); // radius, eye position
    const int numSkins = r.i32();
    const int w = r.i32();
    const int h = r.i32();
    const int numVerts = r.i32();
    const int numTris = r.i32();
    const int numFrames = r.i32();
    r.take(12); // sync type, flags, size
    if(r.bad || numSkins < 1 || w < 8 || h < 8 || w > maxSkinSide || h > maxSkinSide ||
        numVerts < (detailed ? detailMinVerts : minVerts) || numVerts > (detailed ? detailMaxVerts : maxVerts) || numTris < 1 ||
        numTris > (detailed ? detailMaxTris : maxTris) || numFrames <= (detailed ? detailStandIndex : 0))
    {
        return false;
    }
    const za::SizeT skinBytes = static_cast<za::SizeT>(w) * static_cast<za::SizeT>(h);
    for(int i = 0; i < numSkins; i++)
    {
        int pictures = 1;
        if(r.i32() != ALIAS_SKIN_SINGLE)
        {
            pictures = r.i32();
            if(pictures < 1 || pictures > 64)
            {
                return false;
            }
            r.take(static_cast<za::SizeT>(pictures) * 4);
        }
        const unsigned char* pixels = r.take(skinBytes * static_cast<za::SizeT>(pictures));
        if(!pixels)
        {
            return false;
        }
        if(i == 0)
        {
            out.skin.resize(skinBytes);
            ZA_MEMCPY(out.skin.data(), pixels, skinBytes);
        }
    }
    za::Vector<StVert> st;
    for(int i = 0; i < numVerts; i++)
    {
        StVert v{r.i32(), r.i32(), r.i32()};
        if(v.s < 0 || v.s >= w || v.t < 0 || v.t >= h || (v.onSeam && v.s + w / 2 >= w))
        {
            return false;
        }
        st.pushBack(v);
    }
    za::Vector<Tri> tris;
    for(int i = 0; i < numTris; i++)
    {
        Tri t{r.i32(), {r.i32(), r.i32(), r.i32()}};
        for(const int v : t.v)
        {
            if(v < 0 || v >= numVerts)
            {
                return false;
            }
        }
        tris.pushBack(t);
    }
    // stand1 (a frame of its own, or in a group; the detailed head's: by its place when no frame is named so).
    za::Vector<glm::vec3> pose;
    const za::SizeT frameVerts = static_cast<za::SizeT>(numVerts) * 4;
    int frameAt = 0; // (the frames read, a group's each)
    for(int i = 0; i < numFrames && pose.empty() && !r.bad; i++)
    {
        int count = 1;
        if(r.i32() != ALIAS_SINGLE)
        {
            count = r.i32();
            if(count < 1 || count > 1024)
            {
                return false;
            }
            r.take(8 + static_cast<za::SizeT>(count) * 4); // its bounds, its intervals
        }
        for(int j = 0; j < count && pose.empty(); j++)
        {
            r.take(8); // the frame's bounds
            const unsigned char* name = r.take(16);
            const unsigned char* v = r.take(frameVerts);
            if(!name || !v)
            {
                return false;
            }
            char frameName[17]{};
            ZA_MEMCPY(frameName, name, 16);
            const bool byPlace = detailed && !frameName[0] && frameAt == detailStandIndex;
            frameAt++;
            if(q_strcasecmp(frameName, standFrame) && !byPlace)
            {
                continue;
            }
            for(int k = 0; k < numVerts; k++)
            {
                pose.pushBack(glm::vec3{v[k * 4], v[k * 4 + 1], v[k * 4 + 2]} * scale + origin);
            }
        }
    }
    if(pose.empty() || !(detailed ? takeIslands(pose, st, tris, w, out) : takeHead(pose, st, tris, w, out)))
    {
        return false;
    }
    out.skinW = w;
    out.skinH = h;
    return true;
}

[[nodiscard]] char* lastSeparator(char* path)
{
    char* last = nullptr;
    for(char* p = path; *p; p++)
    {
        if(*p == '/' || *p == '\\')
        {
            last = p;
        }
    }
    return last;
}

// Which pass of load() a search path is looked at in: 0 the game's own folder, 1 id1 (Quake's), 2 the rest (the
// mission packs and campaigns Quake VR mounts besides).
[[nodiscard]] int passOf(const searchpath_t* s)
{
    char folder[MAX_OSPATH];
    q_strlcpy(folder, s->pack ? s->pack->filename : s->filename, sizeof(folder));
    if(s->pack)
    {
        if(char* cut = lastSeparator(folder))
        {
            *cut = 0;
        }
    }
    if(COM_SamePath(folder, com_gamedir))
    {
        return 0;
    }
    const char* last = lastSeparator(folder);
    return !q_strcasecmp(last ? last + 1 : folder, "id1") ? 1 : 2;
}

// The first progs/player.mdl that is Quake's ranger (`detailed`: Quake VR's detailed one), the game's own folder's
// first, then id1's, then the rest's: his head.
[[nodiscard]] bool load(bool detailed)
{
    ranger.detailed = detailed;
    za::Vector<unsigned char> file;
    char where[MAX_OSPATH];
    for(int pass = 0; pass < 3; pass++)
    {
        for(const searchpath_t* s = com_searchpaths; s; s = s->next)
        {
            if(passOf(s) != pass || !readFrom(s, file, where, sizeof(where)))
            {
                continue;
            }
            if(parse(file, ranger, detailed) && (!detailed || packDetailed(ranger)))
            {
                q_strlcpy(ranger.path, where, sizeof(ranger.path));
                Con_DPrintf("vr_body_ranger_head: the %shead of %s (%d triangles, %d vertices)\n", detailed ? "detailed " : "",
                    where, static_cast<int>(ranger.tris.size() / 3), static_cast<int>(ranger.pos.size()));
                if(detailed)
                {
                    Con_DPrintf("vr_body_ranger_head: his %d islands in the body's head block, his skin laid out %d x %d\n",
                        static_cast<int>(ranger.islands.size()), ranger.packedW, ranger.packedH);
                }
                return true;
            }
            Con_DPrintf("vr_body_ranger_head: %s is not %s, passed over\n", where, detailed ? "Quake VR's detailed ranger" : "Quake's ranger");
        }
    }
    Con_DPrintf("vr_body_ranger_head: no %sranger found (progs/player.mdl)\n", detailed ? "detailed " : "");
    return false;
}

// The option latched and the ranger looked for, the first time (the first body's mesh: Mod_LoadMD5MeshModel).
[[nodiscard]] bool active()
{
    if(!ranger.looked)
    {
        ranger.looked = true;
        ranger.on = vr_body_ranger_head.value != 0.f;
        // (the detailed head first when chosen, else, or without it, Quake's; neither: the placeholder)
        ranger.ok = ranger.on && ((vr_body_ranger_head_model.value != 0.f && load(true)) || load(false));
    }
    return ranger.on && ranger.ok;
}

// A texel of his skin (continuous) to the body's skins' texture coordinates, and back.
[[nodiscard]] glm::vec2 destHalf(int h, float& u0, float& u1)
{
    const float mid = (destU0 + destU1) * 0.5f;
    u0 = h ? mid + gap * 0.5f : destU0;
    u1 = h ? destU1 : mid - gap * 0.5f;
    return {destV0, destV1};
}

[[nodiscard]] glm::vec2 toBody(const glm::vec2& s)
{
    const int h = s.x >= static_cast<float>(ranger.skinW / 2) ? 1 : 0;
    float u0;
    float u1;
    const glm::vec2 v = destHalf(h, u0, u1);
    const Rect& r = ranger.half[h];
    return {u0 + (s.x - r.x0) / (r.x1 - r.x0) * (u1 - u0), v.x + (s.y - r.y0) / (r.y1 - r.y0) * (v.y - v.x)};
}

// The palette index his skin has at the body's texture coordinates (u, v) in the painted region.
[[nodiscard]] int fromBody(float u, float v)
{
    if(ranger.detailed)
    {
        return detailIndexAt(ranger, u, v);
    }
    const int h = u >= (destU0 + destU1) * 0.5f ? 1 : 0;
    float u0;
    float u1;
    const glm::vec2 dv = destHalf(h, u0, u1);
    const Rect& r = ranger.half[h];
    const float x = za::clamp(r.x0 + (u - u0) / (u1 - u0) * (r.x1 - r.x0), r.x0, r.x1 - 0.01f);
    const float y = za::clamp(r.y0 + (v - dv.x) / (dv.y - dv.x) * (r.y1 - r.y0), r.y0, r.y1 - 0.01f);
    const int xi = za::clamp(static_cast<int>(x), 0, ranger.skinW - 1);
    const int yi = za::clamp(static_cast<int>(y), 0, ranger.skinH - 1);
    return ranger.skin[static_cast<za::SizeT>(yi) * static_cast<za::SizeT>(ranger.skinW) + static_cast<za::SizeT>(xi)];
}

enum class Kind
{
    None,
    Skin,
    Normals,
    Mask,
};

// progs/vrbody_NN_00 (a skin), its _norm or its _skinmask, with or without an extension.
[[nodiscard]] Kind kindOf(const char* name)
{
    constexpr const char* prefix = "progs/vrbody_";
    const za::SizeT n = ZA_STRLEN(prefix);
    if(!name || q_strncasecmp(name, prefix, n))
    {
        return Kind::None;
    }
    const char* p = name + n;
    if(!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9' && p[2] == '_' && p[3] == '0' && p[4] == '0'))
    {
        return Kind::None;
    }
    p += 5;
    const char* dot = strchr(p, '.');
    const za::SizeT len = dot ? static_cast<za::SizeT>(dot - p) : ZA_STRLEN(p);
    if(len == 0)
    {
        return Kind::Skin;
    }
    if(len == 5 && !q_strncasecmp(p, "_norm", 5))
    {
        return Kind::Normals;
    }
    if(len == 9 && !q_strncasecmp(p, "_skinmask", 9))
    {
        return Kind::Mask;
    }
    return Kind::None;
}

// --- Placed live (vr_body_head_*: Appearance > Ranger Head) ---------------------------------------------------------
// Each body mesh given his head keeps where it put him (the first version's place: `grow`, `sink`); the settings turn
// and size him about the bottom of his head and move him, the neck's top ring with him (across, and down with the sink
// no lower than the neck below it). Applied as the mesh is read and again, in the model's own vertices and its vertex
// buffer, whenever a setting changes.

constexpr float unitsPerCm = 0.2625f; // the body's 26.25 units a metre
constexpr float ringClearance = 0.1f; // the neck's top ring kept this far above the neck's next ring

struct Pose
{
    glm::mat3 turn{1.f};
    float size{1.f};
    glm::vec3 move{0.f}; // his (the sink's part in it)
    float upUnits{0.f};
    float sinkUnits{0.f};
};

// About axis 0 (X, forward), 1 (Y, left) or 2 (Z, up), right-handed.
[[nodiscard]] glm::mat3 axisTurn(int axis, float degrees)
{
    const float c = glm::cos(glm::radians(degrees));
    const float s = glm::sin(glm::radians(degrees));
    const int a = (axis + 1) % 3;
    const int b = (axis + 2) % 3;
    glm::mat3 m{1.f};
    m[a][a] = c;
    m[a][b] = s;
    m[b][a] = -s;
    m[b][b] = c;
    return m;
}

[[nodiscard]] Pose pose()
{
    Pose q;
    // Yaw about Z (turned left), pitch about Y (X to -Z: the chin down), roll about X (Y to Z: tilted right).
    q.turn = axisTurn(2, za::clamp(vr_body_head_yaw.value, -45.f, 45.f)) * axisTurn(1, za::clamp(vr_body_head_pitch.value, -45.f, 45.f)) *
             axisTurn(0, za::clamp(vr_body_head_roll.value, -45.f, 45.f));
    q.size = za::clamp(vr_body_head_size.value, 0.5f, 2.f);
    q.upUnits = za::clamp(vr_body_head_up.value, -10.f, 10.f) * unitsPerCm;
    q.sinkUnits = za::clamp(vr_body_head_sink.value, 0.f, 12.f) * unitsPerCm;
    q.move = glm::vec3{za::clamp(vr_body_head_forward.value, -10.f, 10.f) * unitsPerCm,
        -za::clamp(vr_body_head_side.value, -10.f, 10.f) * unitsPerCm, q.upUnits - q.sinkUnits};
    return q;
}

struct Placed
{
    qmodel_t* mod{nullptr};
    char name[MAX_QPATH]{};
    int meshVerts{0}; // the mesh's vertex count (which of the model's meshes)
    int first{0};     // his first vertex in it (the detailed head: in his own mesh, the model's last)
    bool detailed{false};
    bool flip{false}; // his triangles turned over in it
    float scale{1.f}; // the first version's place: ranger.pos * scale + offset
    glm::vec3 offset{0.f};
    glm::vec3 pivot{0.f};    // there, the bottom of his head at its middle
    float ringDropMax{0.f};  // how far the neck's top ring may go down
    za::Vector<unsigned short> ring; // the neck's top ring's vertices (the placeholder's jaw, narrowed)
    za::Vector<glm::vec3> ringBase;  // and where they were
};
za::Vector<Placed> placed;

[[nodiscard]] glm::vec3 headPoint(const Placed& p, const Pose& q, za::SizeT i)
{
    const glm::vec3 base = ranger.pos[i] * p.scale + p.offset;
    return p.pivot + q.turn * ((base - p.pivot) * q.size) + q.move;
}

// His vertices placed by `q` in `h` (his first) and the neck's top ring in `v` (the body's mesh), either null; his
// normals too (as MD5_ComputeNormals: the faces' d2 x d1 summed at each place, his copies at one place, his seam,
// welded) unless the loader makes them.
void writeHead(const Placed& p, const Pose& q, iqmvert_t* v, iqmvert_t* h, bool normals)
{
    const za::SizeT n = ranger.pos.size();
    for(za::SizeT i = 0; i < n && h; i++)
    {
        const glm::vec3 x = headPoint(p, q, i);
        for(int k = 0; k < 3; k++)
        {
            h[i].xyz[k] = x[k];
        }
    }
    const float dz = za::max(q.upUnits - q.sinkUnits, -p.ringDropMax);
    for(za::SizeT k = 0; k < p.ring.size() && v; k++)
    {
        const glm::vec3 x = p.ringBase[k] + glm::vec3{q.move.x, q.move.y, dz};
        for(int a = 0; a < 3; a++)
        {
            v[p.ring[k]].xyz[a] = x[a];
        }
    }
    if(!normals || !h)
    {
        return;
    }
    const auto at = [&](za::SizeT i) { return geom::vec(h[i].xyz); };
    za::Vector<unsigned short> weld;
    weld.resize(n);
    for(za::SizeT i = 0; i < n; i++)
    {
        weld[i] = static_cast<unsigned short>(i);
        for(za::SizeT j = 0; j < i; j++)
        {
            if(at(j) == at(i))
            {
                weld[i] = weld[j];
                break;
            }
        }
    }
    za::Vector<glm::vec3> sum;
    sum.resize(n, glm::vec3{0.f});
    for(za::SizeT t = 0; t + 2 < ranger.tris.size(); t += 3)
    {
        const za::SizeT i0 = weld[ranger.tris[t]];
        const za::SizeT i1 = weld[ranger.tris[t + (p.flip ? 2 : 1)]];
        const za::SizeT i2 = weld[ranger.tris[t + (p.flip ? 1 : 2)]];
        const glm::vec3 face = glm::cross(at(i2) - at(i0), at(i1) - at(i0));
        sum[i0] += face;
        sum[i1] += face;
        sum[i2] += face;
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        const glm::vec3 s = sum[weld[i]];
        const float len = glm::length(s);
        const glm::vec3 nrm = len > 1e-12f ? s / len : glm::vec3{0.f, 0.f, 1.f};
        for(int k = 0; k < 3; k++)
        {
            h[i].norm[k] = static_cast<int8_t>(static_cast<int>(nrm[k] * 127.f));
        }
        h[i].norm[3] = 0;
    }
}

// A vr_body_head_* setting changed: every body loaded with his head placed again, its vertex buffer uploaded again (a
// model flushed from the cache is placed as it loads again).
void onPoseChanged(cvar_t*)
{
    if(!ranger.on || !ranger.ok)
    {
        return;
    }
    const Pose q = pose();
    for(const Placed& p : placed)
    {
        if(!p.mod || p.mod->type != mod_alias || ZA_STRCMP(p.mod->name, p.name))
        {
            continue;
        }
        auto* hdr = static_cast<aliashdr_t*>(Cache_Check(&p.mod->cache));
        if(!hdr)
        {
            continue;
        }
        bool found = false;
        for(aliashdr_t* s = hdr; s; s = Mod_NextSurface(s))
        {
            if(s->poseverttype != aliashdr_t::PV_IQM)
            {
                continue;
            }
            auto* v = reinterpret_cast<iqmvert_t*>(reinterpret_cast<byte*>(s) + s->vertexes);
            if(s == hdr && s->numverts == p.meshVerts) // the body's (the detailed head: its neck's top ring alone)
            {
                writeHead(p, q, v, p.detailed ? nullptr : v + p.first, true);
                found = true;
            }
            else if(p.detailed && !s->nextsurface && s->numverts == static_cast<int>(ranger.pos.size())) // his own, the last
            {
                writeHead(p, q, nullptr, v, true);
                found = true;
            }
        }
        if(found)
        {
            GLMesh_LoadVertexBuffer(p.mod, hdr);
        }
    }
}

} // namespace

void qvr::rangerhead::init()
{
    cvar_t* const settings[] = {&vr_body_head_forward, &vr_body_head_side, &vr_body_head_up, &vr_body_head_pitch,
        &vr_body_head_yaw, &vr_body_head_roll, &vr_body_head_size, &vr_body_head_sink};
    for(cvar_t* c : settings)
    {
        Cvar_SetCallback(c, onPoseChanged);
    }
}

void qvr::rangerhead::info_f()
{
    const Pose q = pose();
    Con_Printf("rangerhead: %s, %d meshes placed; size %.2f, moved %.2f %.2f %.2f units, sink %.2f\n",
        !ranger.on ? "off" : !ranger.ok ? "no ranger" : ranger.detailed ? "the detailed head" : "Quake's head",
        static_cast<int>(placed.size()), q.size, q.move.x, q.move.y, q.move.z, q.sinkUnits);
    if(ranger.ok)
    {
        Con_Printf("  from %s: %d triangles, %d vertices, skin %d x %d\n", ranger.path, static_cast<int>(ranger.tris.size() / 3),
            static_cast<int>(ranger.pos.size()), ranger.skinW, ranger.skinH);
    }
    for(const Placed& p : placed)
    {
        const glm::vec3 top = headPoint(p, q, 0);
        float crown = top.z;
        float bottom = top.z;
        for(za::SizeT i = 0; i < ranger.pos.size(); i++)
        {
            crown = za::max(crown, headPoint(p, q, i).z);
            bottom = za::min(bottom, headPoint(p, q, i).z);
        }
        float ringZ = p.ringBase.empty() ? 0.f : p.ringBase[0].z;
        for(const glm::vec3& r : p.ringBase)
        {
            ringZ = za::min(ringZ, r.z);
        }
        Con_Printf("  %s: head %.2f..%.2f, neck top %.2f (down at most %.2f), pivot %.2f\n", p.name, bottom, crown,
            ringZ + za::max(q.upUnits - q.sinkUnits, -p.ringDropMax), p.ringDropMax, p.pivot.z);
    }
}

/*
===============
VR_RangerHeadMesh -- Mod_LoadMD5MeshModel: a mesh just read (its vertices' influences baked, before its normals): for
the body's (shader "vrbody") with vr_body_ranger_head, its placeholder head's triangles dropped and the ranger's added
(vr_rangerhead.hpp). The new arrays are the hunk's; 1 if it changed them.
===============
*/
extern "C" int VR_RangerHeadMesh(void* mod, const char* model, int mesh, const char* shader, const void* bonesIn, int numBones,
    void** verts, int* numVerts, unsigned short** indexes, int* numIndexes)
{
    const bool detailMesh = mesh > 0 && !q_strcasecmp(shader, detailShader);
    if(!(mesh == 0 && VR_MD5SharedSkins(shader)) && !detailMesh)
    {
        return 0;
    }
    if(!active())
    {
        return 0;
    }
    const auto* bones = static_cast<const boneinfo_t*>(bonesIn);
    int head = -1;
    for(int j = 0; j < numBones; j++)
    {
        if(!q_strcasecmp(bones[j].name, "head"))
        {
            head = j;
        }
    }
    if(head < 0)
    {
        return 0;
    }
    if(detailMesh)
    {
        // The detailed head's own mesh (VR_RangerHeadExtend's): his vertices placed as the body's mesh just before it
        // said, his skin's texture coordinates, wholly the head joint's; in place.
        const Placed* p = nullptr;
        for(const Placed& e : placed)
        {
            p = e.mod == mod && e.detailed ? &e : p;
        }
        if(!p || !ranger.detailed || *numVerts != static_cast<int>(ranger.pos.size()) ||
            *numIndexes != static_cast<int>(ranger.tris.size()))
        {
            return 0;
        }
        auto* v = static_cast<iqmvert_t*>(*verts);
        for(za::SizeT i = 0; i < ranger.pos.size(); i++)
        {
            v[i].st[0] = ranger.st[i].x;
            v[i].st[1] = ranger.st[i].y;
            for(int k = 0; k < 4; k++)
            {
                v[i].weight[k] = k ? 0 : 255;
                v[i].idx[k] = static_cast<uint8_t>(k ? 0 : head);
            }
        }
        unsigned short* idx = *indexes;
        for(za::SizeT t = 0; t + 2 < ranger.tris.size(); t += 3)
        {
            idx[t] = ranger.tris[t];
            idx[t + 1] = ranger.tris[t + (p->flip ? 2 : 1)];
            idx[t + 2] = ranger.tris[t + (p->flip ? 1 : 2)];
        }
        writeHead(*p, pose(), nullptr, v, false);
        return 1;
    }
    const auto* in = static_cast<const iqmvert_t*>(*verts);
    const unsigned short* inIdx = *indexes;
    const int nv = *numVerts;
    const int ni = *numIndexes;
    za::Vector<unsigned char> headOnly;
    headOnly.resize(static_cast<za::SizeT>(nv));
    glm::vec3 mins{0.f};
    glm::vec3 maxs{0.f};
    int count = 0;
    glm::vec3 neckMins{0.f};
    glm::vec3 neckMaxs{0.f};
    int neckCount = 0;
    za::Vector<unsigned char> moved;
    moved.resize(static_cast<za::SizeT>(nv), static_cast<unsigned char>(0));
    for(int i = 0; i < nv; i++)
    {
        int w = 0;
        for(int k = 0; k < 4; k++)
        {
            w += in[i].idx[k] == head ? in[i].weight[k] : 0;
        }
        headOnly[i] = w >= 250;
        if(!headOnly[i])
        {
            if(w > 0) // the neck (weighted to the neck and the head)
            {
                const glm::vec3 p{in[i].xyz[0], in[i].xyz[1], in[i].xyz[2]};
                neckMins = neckCount ? glm::min(neckMins, p) : p;
                neckMaxs = neckCount ? glm::max(neckMaxs, p) : p;
                neckCount++;
            }
            continue;
        }
        const iqmvert_t& v = in[i];
        if(v.st[0] < blockU0 || v.st[0] > blockU1 || v.st[1] < blockV0 || v.st[1] > blockV1)
        {
            Con_DWarning("%s: its head is not where the body's is (u %.3f v %.3f): the placeholder kept\n", model, v.st[0], v.st[1]);
            return 0;
        }
        const glm::vec3 p{v.xyz[0], v.xyz[1], v.xyz[2]};
        mins = count ? glm::min(mins, p) : p;
        maxs = count ? glm::max(maxs, p) : p;
        count++;
    }
    // (the detailed head: in a mesh of his own after this one, VR_RangerHeadExtend's)
    const bool detailed = ranger.detailed;
    const za::SizeT total = static_cast<za::SizeT>(nv) + (detailed ? 0 : ranger.pos.size());
    if(!count || total > 65535)
    {
        return 0;
    }

    // Placed: `grow` times as wide (Y) as the placeholder, centred over it, its bottom `sink` below the placeholder's.
    const float scale = (detailed ? detailGrow : grow) * (maxs.y - mins.y) / (ranger.maxs.y - ranger.mins.y);
    const glm::vec3 rangerCentre = (ranger.mins + ranger.maxs) * 0.5f;
    const glm::vec3 centre = (mins + maxs) * 0.5f;
    const glm::vec3 offset{centre.x - rangerCentre.x * scale, centre.y - rangerCentre.y * scale,
        mins.z - (detailed ? detailSink : sink) - ranger.mins.z * scale};

    auto* out = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t) * total)));
    ZA_MEMCPY(out, in, sizeof(iqmvert_t) * static_cast<za::SizeT>(nv));
    for(za::SizeT i = 0; i < ranger.pos.size() && !detailed; i++)
    {
        iqmvert_t& v = out[static_cast<za::SizeT>(nv) + i];
        const glm::vec3 p = ranger.pos[i] * scale + offset;
        const glm::vec2 st = toBody(ranger.src[i]);
        for(int k = 0; k < 3; k++)
        {
            v.xyz[k] = p[k];
        }
        v.st[0] = st.x;
        v.st[1] = st.y;
        v.weight[0] = 255;
        v.idx[0] = static_cast<uint8_t>(head);
    }

    // Wound as the body is: the placeholder's head (open at its neck) and the ranger's (closed) enclose their
    // volumes with the same sign about their middles, else his triangles are turned over.
    const auto volume = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& o) {
        return glm::dot(a - o, glm::cross(b - o, c - o));
    };
    const auto at = [&](int i) { return geom::vec(out[i].xyz); };
    float placeholderVolume = 0.f;
    int kept = 0;
    for(int t = 0; t + 2 < ni; t += 3)
    {
        if(headOnly[inIdx[t]] && headOnly[inIdx[t + 1]] && headOnly[inIdx[t + 2]])
        {
            placeholderVolume += volume(at(inIdx[t]), at(inIdx[t + 1]), at(inIdx[t + 2]), centre);
        }
        else
        {
            kept += 3;
        }
    }
    float rangerVolume = 0.f;
    const glm::vec3 rangerMiddle = rangerCentre * scale + offset;
    const auto placedAt = [&](za::SizeT i) { return ranger.pos[i] * scale + offset; };
    for(za::SizeT t = 0; t + 2 < ranger.tris.size(); t += 3)
    {
        rangerVolume += volume(placedAt(ranger.tris[t]), placedAt(ranger.tris[t + 1]), placedAt(ranger.tris[t + 2]), rangerMiddle);
    }
    const bool flip = (placeholderVolume < 0.f) != (rangerVolume < 0.f);

    const za::SizeT indexCount = static_cast<za::SizeT>(kept) + (detailed ? 0 : ranger.tris.size());
    auto* outIdx = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short) * indexCount)));
    za::SizeT o = 0;
    for(int t = 0; t + 2 < ni; t += 3)
    {
        if(!(headOnly[inIdx[t]] && headOnly[inIdx[t + 1]] && headOnly[inIdx[t + 2]]))
        {
            for(int k = 0; k < 3; k++)
            {
                const unsigned short i = inIdx[t + k];
                outIdx[o++] = i;
                if(headOnly[i] && !moved[i]) // the neck's top ring (the placeholder's jaw): the neck's skin, its width
                {
                    moved[i] = 1;
                    out[i].st[1] = za::min(out[i].st[1], neckTopV);
                    for(int a = 0; a < 2 && neckCount; a++)
                    {
                        const float half = (maxs[a] - mins[a]) * 0.5f;
                        const float neckHalf = (neckMaxs[a] - neckMins[a]) * 0.5f;
                        out[i].xyz[a] = (neckMins[a] + neckMaxs[a]) * 0.5f + (out[i].xyz[a] - centre[a]) * (half > 0.f ? neckHalf / half : 1.f);
                    }
                }
            }
        }
    }
    for(za::SizeT t = 0; t + 2 < ranger.tris.size() && !detailed; t += 3)
    {
        outIdx[o++] = static_cast<unsigned short>(nv + ranger.tris[t]);
        outIdx[o++] = static_cast<unsigned short>(nv + ranger.tris[t + (flip ? 2 : 1)]);
        outIdx[o++] = static_cast<unsigned short>(nv + ranger.tris[t + (flip ? 1 : 2)]);
    }

    // Kept to be placed again live (vr_body_head_*), and placed now by them (the loader then makes the normals).
    Placed* p = nullptr;
    for(Placed& e : placed)
    {
        if(e.mod == mod && e.meshVerts == static_cast<int>(total))
        {
            p = &e;
        }
    }
    if(!p)
    {
        p = &placed.emplaceBack();
    }
    p->mod = static_cast<qmodel_t*>(mod);
    q_strlcpy(p->name, p->mod ? p->mod->name : model, sizeof(p->name));
    p->meshVerts = static_cast<int>(total);
    p->first = detailed ? 0 : nv;
    p->detailed = detailed;
    p->flip = flip;
    p->scale = scale;
    p->offset = offset;
    p->pivot = glm::vec3{centre.x, centre.y, ranger.mins.z * scale + offset.z};
    p->ring.clear();
    p->ringBase.clear();
    float ringZ = 0.f;
    for(int i = 0; i < nv; i++)
    {
        if(moved[i])
        {
            ringZ = p->ring.empty() ? out[i].xyz[2] : za::min(ringZ, out[i].xyz[2]);
            p->ring.pushBack(static_cast<unsigned short>(i));
            p->ringBase.pushBack(geom::vec(out[i].xyz));
        }
    }
    p->ringDropMax = neckCount ? za::max(0.f, ringZ - neckMaxs.z - ringClearance) : 0.f;
    writeHead(*p, pose(), out, detailed ? nullptr : out + nv, false);

    Con_DPrintf("%s: the ranger's %shead (%d triangles for the placeholder's %d; scaled %.3f, %.2f x %.2f x %.2f units, "
                "crown %.2f for %.2f%s)\n",
        model, detailed ? "detailed " : "", static_cast<int>(ranger.tris.size() / 3), (ni - kept) / 3, scale, (ranger.maxs.x - ranger.mins.x) * scale,
        (ranger.maxs.y - ranger.mins.y) * scale, (ranger.maxs.z - ranger.mins.z) * scale, ranger.maxs.z * scale + offset.z,
        maxs.z, flip ? ", turned over" : "");
    *verts = out;
    *numVerts = static_cast<int>(total);
    *indexes = outIdx;
    *numIndexes = static_cast<int>(indexCount);
    return 1;
}

/*
===============
VR_RangerHeadImage -- Image_LoadImage: an image just decoded. The body's skins (progs/vrbody_NN_00), their normal maps
and skin masks get the ranger's head (vr_rangerhead.hpp) where the mesh draws it, once the body's mesh has taken it. The
detailed head's mesh has its own skin but reads the body's skin mask there (VR_AliasSkinMask: the entity's): his too.
===============
*/
extern "C" void VR_RangerHeadImage(const char* name, unsigned char* data, int width, int height, int format)
{
    if(!data || format != SRC_RGBA || width < 8 || height < 8 || !ranger.looked || !ranger.on || !ranger.ok)
    {
        return;
    }
    const Kind kind = kindOf(name);
    if(kind == Kind::None)
    {
        return;
    }
    const int x0 = za::clamp(static_cast<int>(paintU0 * static_cast<float>(width)), 0, width);
    const int x1 = za::clamp(static_cast<int>(paintU1 * static_cast<float>(width)), 0, width);
    const int y0 = za::clamp(static_cast<int>(paintV0 * static_cast<float>(height)), 0, height);
    const int y1 = za::clamp(static_cast<int>(paintV1 * static_cast<float>(height)), 0, height);
    for(int y = y0; y < y1; y++)
    {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        for(int x = x0; x < x1; x++)
        {
            unsigned char* px = data + (static_cast<za::SizeT>(y) * static_cast<za::SizeT>(width) + static_cast<za::SizeT>(x)) * 4;
            if(kind == Kind::Normals) // flat
            {
                px[0] = 128;
                px[1] = 128;
                px[2] = 255;
                px[3] = 255;
                continue;
            }
            const int index = fromBody((static_cast<float>(x) + 0.5f) / static_cast<float>(width), v);
            if(kind == Kind::Mask) // R the skin; no glove (G, B); A the glove's relief, flat
            {
                px[0] = index >= skinFirst && index <= skinLast ? 255 : 0;
                px[1] = 0;
                px[2] = 0;
                px[3] = 128;
                continue;
            }
            const unsigned c = d_8to24table[index];
            px[0] = static_cast<unsigned char>(c & 255u);
            px[1] = static_cast<unsigned char>((c >> 8) & 255u);
            px[2] = static_cast<unsigned char>((c >> 16) & 255u);
            px[3] = 255;
        }
    }
}

/*
===============
VR_RangerHeadExtend -- loadMd5Replacement: a body's file (progs/vrbody*, shader "vrbody") with the detailed head's mesh
added after its own (and the gloves' cuff shell: VR_ModelReplacementExtend), shader "vrrangerhead": his vertices, each
wholly the head joint's (VR_RangerHeadMesh places them and gives their texture coordinates), and his triangles; null
when he is not drawn so (vr_body_ranger_head_model 0, no detailed head found). VR_HeapFree it.
===============
*/
extern "C" char* VR_RangerHeadExtend(const char* name, const char* md5mesh)
{
    if(!name || !md5mesh || q_strncasecmp(name, "progs/vrbody", 12) || !ZA_STRSTR(md5mesh, "shader \"vrbody\"") || !active() ||
        !ranger.detailed)
    {
        return nullptr;
    }
    const char* count = ZA_STRSTR(md5mesh, "numMeshes");
    const char* joints = ZA_STRSTR(md5mesh, "joints");
    const char* open = joints ? strchr(joints, '{') : nullptr;
    if(!count || !open)
    {
        return nullptr;
    }
    count += ZA_STRLEN("numMeshes");
    char* end = nullptr;
    const long meshes = strtol(count, &end, 10);
    if(meshes < 1 || meshes > 8 || end == count)
    {
        return nullptr;
    }
    // The head joint's index: the joints are one a line, each named first ("head" -1 ( ... ) ( ... )).
    int head = -1;
    int index = 0;
    for(const char* line = strchr(open, '\n'); line && head < 0; line = strchr(line + 1, '\n'))
    {
        const char* p = line + 1;
        while(*p == ' ' || *p == '\t' || *p == '\r')
        {
            p++;
        }
        if(*p == '}')
        {
            break;
        }
        if(*p != '"')
        {
            continue;
        }
        head = !q_strncasecmp(p, "\"head\"", 6) ? index : head;
        index++;
    }
    if(head < 0)
    {
        return nullptr;
    }

    const int nv = static_cast<int>(ranger.pos.size());
    const int nt = static_cast<int>(ranger.tris.size() / 3);
    za::String out{md5mesh, static_cast<za::SizeT>(count - md5mesh)};
    char buf[160];
    q_snprintf(buf, sizeof(buf), " %ld", meshes + 1);
    out += buf;
    out += end;
    q_snprintf(buf, sizeof(buf),
        "\nmesh {\n\t// the detailed ranger's head (%s): Quake VR makes it as the body loads (vr_rangerhead.cpp)\n\tshader \"%s\"\n\n"
        "\tnumverts %d\n",
        "progs/player.mdl", detailShader, nv);
    out += buf;
    for(int i = 0; i < nv; i++)
    {
        q_snprintf(buf, sizeof(buf), "\tvert %d ( %.6f %.6f ) %d 1\n", i,
            static_cast<double>(ranger.st[static_cast<za::SizeT>(i)].x), static_cast<double>(ranger.st[static_cast<za::SizeT>(i)].y), i);
        out += buf;
    }
    q_snprintf(buf, sizeof(buf), "\n\tnumtris %d\n", nt);
    out += buf;
    for(int t = 0; t < nt; t++)
    {
        const za::SizeT k = static_cast<za::SizeT>(t) * 3;
        q_snprintf(buf, sizeof(buf), "\ttri %d %d %d %d\n", t, ranger.tris[k], ranger.tris[k + 1], ranger.tris[k + 2]);
        out += buf;
    }
    q_snprintf(buf, sizeof(buf), "\n\tnumweights %d\n", nv);
    out += buf;
    for(int i = 0; i < nv; i++)
    {
        q_snprintf(buf, sizeof(buf), "\tweight %d %d 1 ( 0 0 0 )\n", i, head);
        out += buf;
    }
    out += "}\n";
    return VR_HeapStrdup(out.cStr());
}

/*
===============
VR_RangerHeadSkins -- Mod_LoadMD5Skins: the detailed head's mesh (shader "vrrangerhead") given his own skin (the player
model's first, as its loader makes it: palette indices, padded, its fullbrights in the alpha), one texture for every body
and every skin number (kept for the session, read again from his texels when the textures are). 1 if it did.
===============
*/
extern "C" int VR_RangerHeadSkins(void* surfIn, const char* shader)
{
    if(!shader || q_strcasecmp(shader, detailShader) || !ranger.ok || !ranger.detailed || ranger.packed.empty())
    {
        return 0;
    }
    auto* surf = static_cast<aliashdr_t*>(surfIn);
    gltexture_t* tex = TexMgr_FindTexture(nullptr, headTexName);
    if(!tex)
    {
        // (his skin laid out as his texture coordinates on the body are: packDetailed)
        byte* data = ranger.packed.data();
        const int size = ranger.packedW * ranger.packedH;
        const unsigned flags = TEXPREF_PAD | TEXPREF_PERSIST | (Mod_CheckFullbrights(data, size) ? TEXPREF_ALPHABRIGHT : 0u);
        tex = TexMgr_LoadImage(nullptr, headTexName, ranger.packedW, ranger.packedH, SRC_INDEXED, data, "", reinterpret_cast<src_offset_t>(data), flags);
    }
    for(int s = 0; s < MAX_SKINS; s++)
    {
        for(int f = 0; f < 4; f++)
        {
            surf->gltextures[s][f] = tex;
            surf->fbtextures[s][f] = nullptr;
        }
    }
    surf->numskins = MAX_SKINS;
    surf->skinwidth = tex ? static_cast<int>(tex->width) : 1;
    surf->skinheight = tex ? static_cast<int>(tex->height) : 1;
    return 1;
}

/*
===============
VR_RangerHeadPaints -- R_PaintAliasWounds: whether a body's surface after its first is the detailed head's mesh (its
wounds painted into the body's mask too: his texture coordinates are the body's head block's, packDetailed).
===============
*/
extern "C" int VR_RangerHeadPaints(const void* surfIn)
{
    const auto* surf = static_cast<const aliashdr_t*>(surfIn);
    const gltexture_t* tex = surf && surf->numskins > 0 ? surf->gltextures[0][0] : nullptr;
    return tex && !ZA_STRCMP(tex->name, headTexName) ? 1 : 0;
}

bool qvr::rangerhead::headBlock(float lo[2], float hi[2])
{
    lo[0] = paintU0;
    lo[1] = paintV0;
    hi[0] = paintU1;
    hi[1] = paintV1;
    return ranger.looked && ranger.on && ranger.ok;
}

bool qvr::rangerhead::hasOwnMesh(const char* modelName)
{
    for(const Placed& p : placed)
    {
        if(p.detailed && modelName && !ZA_STRCMP(p.name, modelName))
        {
            return true;
        }
    }
    return false;
}

// vr_body_head_check <detailed | classic | off>: the head the bodies loaded have, against the one expected (the tests:
// the detailed head found and drawn as a mesh of its own with his skin; Quake's painted on; or none).
void qvr::rangerhead::check_f()
{
    const char* want = Cmd_Argc() > 1 ? Cmd_Argv(1) : "detailed";
    const bool wantDetailed = !q_strcasecmp(want, "detailed");
    const bool wantClassic = !q_strcasecmp(want, "classic");
    testkit::Tally tally;
    const auto expect = [&](bool ok, const char* what) { tally.check(ok, "rangerhead_check", what); };
    expect(ranger.looked, "the head looked for (a body loaded)");
    expect(ranger.on && ranger.ok ? (ranger.detailed ? wantDetailed : wantClassic) : !wantDetailed && !wantClassic, want);
    int bodies = 0;
    int headMeshes = 0;
    int skinned = 0;
    for(const Placed& p : placed)
    {
        auto* hdr = p.mod && p.mod->type == mod_alias && !ZA_STRCMP(p.mod->name, p.name) ? static_cast<aliashdr_t*>(Cache_Check(&p.mod->cache)) : nullptr;
        if(!hdr)
        {
            continue;
        }
        bodies++;
        aliashdr_t* last = hdr;
        for(aliashdr_t* s = hdr; s; s = Mod_NextSurface(s))
        {
            last = s;
        }
        const bool own = last != hdr && last->numverts == static_cast<int>(ranger.pos.size()) && last->numindexes == static_cast<int>(ranger.tris.size());
        headMeshes += own ? 1 : 0;
        skinned += own && last->gltextures[0][0] && !ZA_STRCMP(last->gltextures[0][0]->name, headTexName) ? 1 : 0;
    }
    Con_Printf("rangerhead_check: %d bodies placed, %d with the detailed head's mesh, %d with his skin\n", bodies, headMeshes, skinned);
    if(wantDetailed)
    {
        expect(bodies > 0 && headMeshes == bodies && skinned == bodies, "every body has the detailed head's mesh and skin");
    }
    else if(wantClassic)
    {
        expect(bodies > 0 && headMeshes == 0, "the bodies have Quake's head painted on, no mesh of its own");
    }
    tally.verdict("rangerhead_check");
}
