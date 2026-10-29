// vr_limbmodel.cpp -- see vr_limbmodel.hpp.

#include "vr_mdlbuild.hpp"
#include "vr_bytes.hpp"
#include "vr_limbmodel.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_ragdoll.hpp"
#include "vr_api.h"
#include "vr_jobs.hpp"
#include "vr_progs.hpp"
#include "vr_tracy.h"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::limbmodel
{
namespace
{

constexpr size_t headerSize = 84;

// The names handed out (keptName): never freed, the strings the QC and the precaches point at.
struct KeptName
{
    char text[MAX_QPATH]{};
};
za::Vector<za::UniquePtr<KeptName>> keptNames;
constexpr int stripRows = 32;         // the flesh strip under each skin (texels): its left square the cap's
constexpr int minStripRows = 8;       // (less room under a tall skin: as many rows as fit, at least these)
constexpr int maxSkinHeight = 480;    // Mod_LoadAliasModel's MAX_LBM_HEIGHT
constexpr float capTexelsPerUnit = 2.5f; // the cap's flesh as fine as the body stump's (vr_glsl.h StumpFlesh's cells)

// The cut's flesh (the same recipe as the body stump's, vr_glsl.h StumpFlesh: keep them alike), after Quake's own gibs'
// and heads' necks: mostly dark reds of the palette's blood ramp (64..79), blotches of brown meat and pale sinew, dark
// pits, a few wet glints; the bone in its middle. `c` a texel (cell) in the flesh's 3D noise, `r` how far out of the
// middle (0) to the rim (1). Palette colours (0..255): the skin takes the nearest (nearestPalette).
[[nodiscard]] uint32_t fleshHash(int x, int y, int z)
{
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u ^ static_cast<uint32_t>(z) * 83492791u;
    h = (h ^ (h >> 13u)) * 0x5bd1e995u;
    return h ^ (h >> 15u);
}

[[nodiscard]] float fleshRand(int x, int y, int z)
{
    return static_cast<float>(fleshHash(x, y, z) & 0xFFFFFFu) / 16777216.f;
}

[[nodiscard]] float fleshNoise(const glm::vec3& p)
{
    const glm::vec3 fl = glm::floor(p);
    const int x = static_cast<int>(fl.x), y = static_cast<int>(fl.y), z = static_cast<int>(fl.z);
    glm::vec3 f = p - fl;
    f = f * f * (3.f - 2.f * f);
    const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float a = lerp(lerp(fleshRand(x, y, z), fleshRand(x + 1, y, z), f.x), lerp(fleshRand(x, y + 1, z), fleshRand(x + 1, y + 1, z), f.x), f.y);
    const float b = lerp(lerp(fleshRand(x, y, z + 1), fleshRand(x + 1, y, z + 1), f.x),
        lerp(fleshRand(x, y + 1, z + 1), fleshRand(x + 1, y + 1, z + 1), f.x), f.y);
    return lerp(a, b, f.z);
}

[[nodiscard]] glm::vec3 fleshColour(const glm::vec3& c, float r)
{
    const glm::vec3 cell = glm::floor(c);
    const float w = fleshRand(static_cast<int>(cell.x), static_cast<int>(cell.y), static_cast<int>(cell.z));
    const float lv = 0.6f * fleshNoise(c * 0.2f) + 0.4f * fleshNoise(c * 0.43f + 31.f);
    const float meat = fleshNoise(c * 0.27f + 71.f);
    const float bone = 0.14f + 0.06f * (fleshNoise(c * 0.5f + 5.f) - 0.5f);
    if(r < bone * 0.4f)
    {
        return w < 0.5f ? glm::vec3{55.f, 43.f, 31.f} : glm::vec3{39.f, 31.f, 23.f}; // (its marrow)
    }
    if(r < bone)
    {
        return w < 0.4f ? glm::vec3{107.f, 87.f, 71.f} : w < 0.75f ? glm::vec3{95.f, 75.f, 59.f} : glm::vec3{83.f, 63.f, 51.f};
    }
    if(r < bone * 1.3f)
    {
        return w < 0.5f ? glm::vec3{31.f, 0.f, 0.f} : glm::vec3{23.f, 0.f, 0.f}; // (dark round the bone)
    }
    const float l = lv + (w - 0.5f) * 0.2f;
    if(l < 0.3f)
    {
        return w < 0.5f ? glm::vec3{15.f, 11.f, 7.f} : glm::vec3{23.f, 0.f, 0.f}; // a pit
    }
    if(w > 0.97f && lv > 0.45f)
    {
        return w > 0.985f ? glm::vec3{119.f, 0.f, 0.f} : glm::vec3{103.f, 0.f, 0.f}; // a wet glint
    }
    if(meat > 0.64f)
    {
        if(meat > 0.8f && w > 0.5f)
        {
            constexpr float sinew[3][3] = {{83.f, 55.f, 35.f}, {99.f, 63.f, 43.f}, {111.f, 71.f, 51.f}};
            const float* s = sinew[za::min(static_cast<int>((w - 0.5f) * 6.f), 2)];
            return glm::vec3{s[0], s[1], s[2]};
        }
        constexpr float brown[5][3] = {{35.f, 19.f, 7.f}, {47.f, 23.f, 11.f}, {59.f, 31.f, 15.f}, {75.f, 35.f, 19.f}, {87.f, 43.f, 23.f}};
        const float* s = brown[za::clamp(static_cast<int>((lv * 1.4f - 0.2f + (w - 0.5f) * 0.3f) * 5.f), 0, 4)];
        return glm::vec3{s[0], s[1], s[2]};
    }
    const int i = za::clamp(static_cast<int>(1.f + lv * lv * 12.f + (w - 0.5f) * 3.f), 1, 13);
    return glm::vec3{static_cast<float>(7 + 8 * i), 0.f, 0.f};
}

// A monster's gear left off its limbs' models (a piece of its own: no triangle crosses to the flesh), as its cut limb
// drops a weapon of ours in its place: AD's Freddie's super nailgun, its five barrels on his left forearm (QC
// vr_ad_enemyguns.qc VR_ADGun_LimbCut drops Quake VR's super nailgun; the corpse loses them with the limb).
struct LeftOff
{
    const char* model;
    int numVerts;
    int first, last;
};
constexpr LeftOff leftOff[] = {
    {"progs/mon_freddie.mdl", 660, 88, 182}, // AD support
};

[[nodiscard]] bool isLeftOff(const ragdoll::Rig& rig, int v)
{
    for(const LeftOff& l : leftOff)
    {
        if(rig.model && rig.numVerts == l.numVerts && v >= l.first && v <= l.last && !strcmp(rig.model->name, l.model))
        {
            return true;
        }
    }
    return false;
}

void put(za::Vector<byte>& out, int v)
{
    const int le = LittleLong(v);
    const byte* p = reinterpret_cast<const byte*>(&le);
    out.emplaceBackRange(p, 4);
}

void putf(za::Vector<byte>& out, float v)
{
    const float le = LittleFloat(v);
    const byte* p = reinterpret_cast<const byte*>(&le);
    out.emplaceBackRange(p, 4);
}

[[nodiscard]] byte nearestPalette(const float want[3])
{
    int best = 73; // (Quake's blood particles' colour: a palette that isn't loaded, a dedicated server)
    float bestD = 1e9f;
    for(int i = 0; i < 224; i++)
    {
        const unsigned v = d_8to24table[i];
        if(v == 0u)
        {
            continue; // (no palette loaded: a dedicated server, whose skins nobody sees)
        }
        const float dr = static_cast<float>(v & 255u) - want[0];
        const float dg = static_cast<float>((v >> 8) & 255u) - want[1];
        const float db = static_cast<float>((v >> 16) & 255u) - want[2];
        const float d = dr * dr + dg * dg + db * db;
        if(d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return static_cast<byte>(best);
}

// A limb model's name taken apart: its base model and bone; false if it isn't one.
[[nodiscard]] bool parse(const char* name, char* base, size_t baseSize, int& bone, uint32_t& bones)
{
    const char* at = strstr(name, suffix);
    if(!at || at == name)
    {
        return false;
    }
    const char* digits = at + strlen(suffix);
    if(*digits < '0' || *digits > '9')
    {
        return false;
    }
    bone = atoi(digits);
    const char* m = strchr(digits, 'm');
    bones = m ? static_cast<uint32_t>(strtoul(m + 1, nullptr, 10)) : 0u;
    const size_t len = static_cast<size_t>(at - name);
    if(len + 1 > baseSize)
    {
        return false;
    }
    memcpy(base, name, len);
    base[len] = 0;
    return true;
}

[[nodiscard]] const ragdoll::Rig* rigOf(const char* base)
{
    qmodel_t* src = Mod_ForName(base, false);
    if(!src || src->type != mod_alias)
    {
        return nullptr;
    }
    return ragdoll::rigFor(src);
}

struct Built
{
    za::Vector<byte> file;
    int tris{0}, cap{0}, verts{0};
    glm::vec3 size{0.f};
};

// The limb model of `bone` of `rig`'s .mdl (its file `src`, `srcSize` bytes) into `out`. False: it can't be.
[[nodiscard]] bool build(const ragdoll::Rig& rig, int bone, uint32_t only, const byte* src, size_t srcSize, bool zombie, Built& out, const char*& why)
{
    if(srcSize < headerSize || qvr::bytes::le32(src, 0) != IDPOLYHEADER)
    {
        why = "not an alias model";
        return false;
    }
    const int numSkins = qvr::bytes::le32(src, 48), skinW = qvr::bytes::le32(src, 52), skinH = qvr::bytes::le32(src, 56), numVerts = qvr::bytes::le32(src, 60), numTris = qvr::bytes::le32(src, 64),
              numFrames = qvr::bytes::le32(src, 68);
    if(numSkins < 1 || skinW <= 0 || skinH <= 0 || numVerts <= 0 || numTris <= 0 || numFrames <= 0)
    {
        why = "bad header";
        return false;
    }
    if(numVerts != rig.numVerts || static_cast<int>(rig.vertBone.size()) != numVerts)
    {
        why = "another model than its rig's";
        return false;
    }
    const glm::vec3 scale{qvr::bytes::lef32(src, 8), qvr::bytes::lef32(src, 12), qvr::bytes::lef32(src, 16)};
    const glm::vec3 origin{qvr::bytes::lef32(src, 20), qvr::bytes::lef32(src, 24), qvr::bytes::lef32(src, 28)};
    const size_t skinBytes = static_cast<size_t>(skinW) * static_cast<size_t>(skinH);
    size_t at = headerSize;
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 4 > srcSize)
        {
            why = "short";
            return false;
        }
        if(qvr::bytes::le32(src, at) == 0)
        {
            at += 4 + skinBytes;
        }
        else
        {
            const int n = at + 8 <= srcSize ? qvr::bytes::le32(src, at + 4) : 0;
            at += 8 + static_cast<size_t>(za::max(n, 0)) * (4 + skinBytes);
        }
    }
    const size_t skinsEnd = at;
    const size_t stAt = at;
    const size_t trisAt = stAt + static_cast<size_t>(numVerts) * 12;
    const size_t framesAt = trisAt + static_cast<size_t>(numTris) * 16;
    if(framesAt + 4 > srcSize)
    {
        why = "short";
        return false;
    }
    size_t poseAt = 0; // the first frame's vertices (the rig's rest pose)
    if(qvr::bytes::le32(src, framesAt) == 0)
    {
        poseAt = framesAt + 4 + 24;
    }
    else
    {
        const int n = framesAt + 8 <= srcSize ? qvr::bytes::le32(src, framesAt + 4) : 0;
        poseAt = framesAt + 16 + static_cast<size_t>(za::max(n, 0)) * 4 + 24;
    }
    if(poseAt + static_cast<size_t>(numVerts) * 4 > srcSize || skinsEnd > srcSize)
    {
        why = "short";
        return false;
    }

    const uint32_t bones = ragdoll::limbBones(rig, bone) & (only ? only : ~0u);
    const auto onLimb = [&](int v) {
        return v >= 0 && v < numVerts && (bones & (1u << rig.vertBone[static_cast<za::SizeT>(v)])) != 0 && !isLeftOff(rig, v);
    };
    const auto posOf = [&](int v) {
        const byte* t = src + poseAt + static_cast<size_t>(v) * 4;
        return glm::vec3{t[0], t[1], t[2]} * scale + origin;
    };

    // The vertices kept (their own st and normal), and the cap's: the crossing triangles' limb corners again, in blood.
    struct OutVert
    {
        glm::vec3 p{0.f};
        int onseam{0}, s{0}, t{0};
        byte normal{0};
    };
    za::Vector<OutVert> verts;
    za::Vector<int> keptIndex(static_cast<za::SizeT>(numVerts), -1);
    za::Vector<int> capIndex(static_cast<za::SizeT>(numVerts), -1);
    za::Vector<int> tris; // facesfront, v0, v1, v2 (output indices)
    za::Vector<int> capTris; // two output indices each (the third: the cap's middle, added last)
    za::Vector<int> capVerts; // the cap's vertices (output indices)
    glm::vec3 capSum{0.f};
    int capCorners = 0;
    for(int t = 0; t < numTris; t++)
    {
        const size_t o = trisAt + static_cast<size_t>(t) * 16;
        const int front = qvr::bytes::le32(src, o);
        int v[3];
        int on = 0;
        for(int c = 0; c < 3; c++)
        {
            v[c] = qvr::bytes::le32(src, o + 4 + static_cast<size_t>(c) * 4);
            if(v[c] < 0 || v[c] >= numVerts)
            {
                why = "bad triangle";
                return false;
            }
            on += onLimb(v[c]) ? 1 : 0;
        }
        if(on == 3)
        {
            tris.pushBack(front);
            for(int c = 0; c < 3; c++)
            {
                int& k = keptIndex[static_cast<za::SizeT>(v[c])];
                if(k < 0)
                {
                    OutVert ov;
                    ov.p = posOf(v[c]);
                    ov.onseam = qvr::bytes::le32(src, stAt + static_cast<size_t>(v[c]) * 12);
                    ov.s = qvr::bytes::le32(src, stAt + static_cast<size_t>(v[c]) * 12 + 4);
                    ov.t = qvr::bytes::le32(src, stAt + static_cast<size_t>(v[c]) * 12 + 8);
                    ov.normal = src[poseAt + static_cast<size_t>(v[c]) * 4 + 3];
                    k = static_cast<int>(verts.size());
                    verts.pushBack(ov);
                }
                tris.pushBack(k);
            }
        }
        else if(on == 2)
        {
            // The corner off the limb last, the winding kept: the cap triangle has the cap's middle in its place.
            int r = 0;
            while(onLimb(v[(r + 2) % 3]))
            {
                r++;
            }
            for(int c = 0; c < 2; c++)
            {
                const int ov = v[(r + c) % 3];
                int& k = capIndex[static_cast<za::SizeT>(ov)];
                if(k < 0)
                {
                    OutVert cv;
                    cv.p = posOf(ov);
                    k = static_cast<int>(verts.size());
                    verts.pushBack(cv);
                    capVerts.pushBack(k);
                    capSum += cv.p;
                    capCorners++;
                }
                capTris.pushBack(k);
            }
        }
    }
    if(tris.empty())
    {
        why = "no triangle of its own";
        return false;
    }
    const int rows = za::min(za::min(stripRows, maxSkinHeight - skinH), skinW);
    const bool strip = rows >= minStripRows;
    const ragdoll::Bone& root = rig.bones[bone];
    const glm::vec3 axis = root.pivot - root.end;
    const glm::vec3 capOut = glm::length(axis) > 1e-3f ? glm::normalize(axis) : glm::vec3{0.f, 0.f, -1.f};
    const byte capNormal = qvr::mdlbuild::nearestNormal(capOut);
    int capMid = -1;
    float capTexels = 1.f; // the cap's radius on the strip (texels): its rim, for the bone in its middle
    if(capCorners > 0)
    {
        OutVert m;
        m.p = capSum / static_cast<float>(capCorners);
        capMid = static_cast<int>(verts.size());
        verts.pushBack(m);
        // The cap's texels: laid flat on the strip's left square (seen down the limb's axis), its middle the square's,
        // as fine as the body stump's flesh (capTexelsPerUnit) unless it is wider than the square.
        capVerts.pushBack(capMid);
        const glm::vec3 side = glm::normalize(glm::abs(capOut.z) < 0.9f ? glm::cross(capOut, glm::vec3{0.f, 0.f, 1.f})
                                                                         : glm::cross(capOut, glm::vec3{1.f, 0.f, 0.f}));
        const glm::vec3 up = glm::cross(capOut, side);
        float reach = 1e-3f;
        for(const int i : capVerts)
        {
            reach = za::max(reach, glm::length(verts[static_cast<za::SizeT>(i)].p - m.p));
        }
        const float half = static_cast<float>(rows) * 0.5f;
        const float k = strip ? za::min(capTexelsPerUnit, (half - 1.f) / reach) : 0.f;
        capTexels = za::max(reach * k, 1.f);
        for(const int i : capVerts)
        {
            OutVert& cv = verts[static_cast<za::SizeT>(i)];
            const glm::vec3 d = cv.p - m.p;
            cv.onseam = 0;
            cv.s = strip ? static_cast<int>(za::lround(half + glm::dot(d, side) * k)) : skinW / 2;
            cv.t = strip ? skinH + static_cast<int>(za::lround(half + glm::dot(d, up) * k)) : skinH / 2;
            cv.normal = capNormal;
        }
    }

    // Centred on the limb's middle (ragdoll::limbMiddle: where its gib is placed), packed as the .mdl packs.
    const glm::vec3 mid = ragdoll::limbMiddle(rig, bones);
    glm::vec3 lo{1e30f}, hi{-1e30f};
    float radius = 0.f;
    for(OutVert& v : verts)
    {
        v.p -= mid;
        lo = glm::min(lo, v.p);
        hi = glm::max(hi, v.p);
        radius = za::max(radius, glm::length(v.p));
    }
    const glm::vec3 packScale = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});

    const int outH = skinH + (strip ? rows : 0);
    za::Vector<byte>& f = out.file;
    f.clear();
    put(f, IDPOLYHEADER);
    put(f, ALIAS_VERSION);
    putf(f, packScale.x);
    putf(f, packScale.y);
    putf(f, packScale.z);
    putf(f, lo.x);
    putf(f, lo.y);
    putf(f, lo.z);
    putf(f, radius);
    putf(f, 0.f);
    putf(f, 0.f);
    putf(f, 0.f);
    put(f, numSkins);
    put(f, skinW);
    put(f, outH);
    put(f, static_cast<int>(verts.size()));
    put(f, static_cast<int>(tris.size() / 4 + capTris.size() / 2 * (capMid >= 0 ? 1 : 0)));
    put(f, 1);
    put(f, 0);
    put(f, zombie ? EF_ZOMGIB : EF_GIB);
    putf(f, qvr::bytes::lef32(src, 80));

    // The skins, each with the strip under it: the cut's flesh (fleshColour), the bone in the left square's middle (the
    // cap's), the palette's nearest to each colour (looked up once a colour).
    za::Vector<byte> flesh;
    if(strip)
    {
        za::Vector<uint32_t> seen;
        za::Vector<byte> seenIndex;
        flesh.reserve(static_cast<za::SizeT>(rows * skinW));
        const float half = static_cast<float>(rows) * 0.5f;
        for(int y = 0; y < rows; y++)
        {
            for(int x = 0; x < skinW; x++)
            {
                const float r = x < rows ? glm::length(glm::vec2{x + 0.5f - half, y + 0.5f - half}) / capTexels : 2.f;
                const glm::vec3 c = fleshColour(glm::vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(bone * 7 + 3)}, r);
                const uint32_t key = static_cast<uint32_t>(c.x) | static_cast<uint32_t>(c.y) << 8 | static_cast<uint32_t>(c.z) << 16;
                za::SizeT i = 0;
                while(i < seen.size() && seen[i] != key)
                {
                    i++;
                }
                if(i == seen.size())
                {
                    const float want[3] = {c.x, c.y, c.z};
                    seen.pushBack(key);
                    seenIndex.pushBack(nearestPalette(want));
                }
                flesh.pushBack(seenIndex[i]);
            }
        }
    }
    const auto image = [&](const byte* pixels) {
        f.emplaceBackRange(pixels, skinBytes);
        if(strip)
        {
            f.emplaceBackRange(flesh.data(), static_cast<size_t>(flesh.size()));
        }
    };
    at = headerSize;
    for(int s = 0; s < numSkins; s++)
    {
        if(qvr::bytes::le32(src, at) == 0)
        {
            put(f, 0);
            image(src + at + 4);
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(qvr::bytes::le32(src, at + 4), 0);
            put(f, 1);
            put(f, n);
            f.emplaceBackRange(src + at + 8, static_cast<size_t>(n) * 4);
            const size_t first = at + 8 + static_cast<size_t>(n) * 4;
            for(int j = 0; j < n; j++)
            {
                image(src + first + static_cast<size_t>(j) * skinBytes);
            }
            at = first + static_cast<size_t>(n) * skinBytes;
        }
    }
    for(const OutVert& v : verts)
    {
        put(f, v.onseam);
        put(f, v.s);
        put(f, v.t);
    }
    for(za::SizeT i = 0; i < tris.size(); i += 4)
    {
        put(f, tris[i]);
        put(f, tris[i + 1]);
        put(f, tris[i + 2]);
        put(f, tris[i + 3]);
    }
    if(capMid >= 0)
    {
        for(za::SizeT i = 0; i < capTris.size(); i += 2)
        {
            put(f, 1);
            put(f, capTris[i]);
            put(f, capTris[i + 1]);
            put(f, capMid);
        }
    }
    put(f, 0); // a single frame
    za::Vector<byte> packed;
    byte bmin[4] = {255, 255, 255, 0}, bmax[4] = {0, 0, 0, 0};
    for(const OutVert& v : verts)
    {
        byte b[4];
        for(int i = 0; i < 3; i++)
        {
            b[i] = static_cast<byte>(za::clamp(static_cast<int>(za::lround((v.p[i] - lo[i]) / packScale[i])), 0, 255));
            bmin[i] = za::min(bmin[i], b[i]);
            bmax[i] = za::max(bmax[i], b[i]);
        }
        b[3] = v.normal;
        packed.emplaceBackRange(b, 4);
    }
    f.emplaceBackRange(bmin, 4);
    f.emplaceBackRange(bmax, 4);
    char frameName[16] = "limb";
    f.emplaceBackRange(reinterpret_cast<const byte*>(frameName), 16);
    f.emplaceBackRange(packed.data(), packed.size());

    out.tris = static_cast<int>(tris.size() / 4);
    out.cap = capMid >= 0 ? static_cast<int>(capTris.size() / 2) : 0;
    out.verts = static_cast<int>(verts.size());
    out.size = hi - lo;
    return true;
}

// The limb model `name` built (its base's file loaded), or false.
[[nodiscard]] bool buildNamed(const char* name, unsigned int* path_id, Built& out, const char*& why)
{
    char base[MAX_QPATH];
    int bone = -1;
    uint32_t only = 0;
    if(!parse(name, base, sizeof(base), bone, only))
    {
        why = "not a limb model";
        return false;
    }
    const ragdoll::Rig* rig = rigOf(base);
    if(!rig || !ragdoll::limbJoint(*rig, bone))
    {
        why = "no rig, or not a limb";
        return false;
    }
    unsigned int pathId = 0;
    byte* src = COM_LoadMallocFile(VR_ModelFile(base), &pathId);
    if(!src)
    {
        why = "its model's file not found";
        return false;
    }
    const size_t size = static_cast<size_t>(com_filesize);
    const bool zombie = modelmeta::is(rig->model, modelmeta::Id::Zombie);
    const bool ok = build(*rig, bone, only, src, size, zombie, out, why);
    VR_HeapFree(src);
    if(path_id)
    {
        *path_id = pathId;
    }
    return ok;
}

// A kind of monster first seen after the map's load (spawnFrame): its limbs being built on the pool.
struct LateKind
{
    za::Vector<const char*> names; // its limbs' models not precached yet (keptName's)
    unsigned int pathId{0};        // its .mdl's
    jobs::Future<za::Vector<Built>> built; // in names' order (an empty file: it couldn't be)
};

// A limb built, waiting for its precache (one a frame) or its first cut, whichever comes first (derivedFile).
struct LateLimb
{
    const char* name{nullptr};
    unsigned int pathId{0};
    za::Vector<byte> file;
};

struct Late
{
    za::Vector<unsigned char> seen; // by model index: a kind looked at on this map (its limbs precached or under way)
    za::Vector<LateKind> kinds;
    za::Vector<LateLimb> ready;
};
Late late;

} // namespace

void newMap()
{
    late.kinds.clear(); // (each waits for its job)
    late.ready.clear();
    late.seen.clear();
    late.seen.resize(MAX_MODELS, 0);
}

bool name(const char* base, int bone, uint32_t bones, char* out, size_t size)
{
    const int n = bones ? q_snprintf(out, size, "%s%s%dm%u", base, suffix, bone, bones) : q_snprintf(out, size, "%s%s%d", base, suffix, bone);
    return n > 0 && static_cast<size_t>(n) < size;
}

const char* keptName(const char* base, int bone, uint32_t bones)
{
    char text[MAX_QPATH];
    if(!name(base, bone, bones, text, sizeof(text)))
    {
        return "";
    }
    for(const za::UniquePtr<KeptName>& k : keptNames)
    {
        if(!strcmp(k->text, text))
        {
            return k->text;
        }
    }
    keptNames.pushBack(za::makeUnique<KeptName>());
    q_strlcpy(keptNames.back()->text, text, sizeof(keptNames.back()->text));
    return keptNames.back()->text;
}

bool available(const char* base, int bone, uint32_t only)
{
    const ragdoll::Rig* rig = rigOf(base);
    if(!rig || !ragdoll::limbJoint(*rig, bone))
    {
        return false;
    }
    // (A triangle wholly on the bones: build's own ones; vertices alone are not enough, the centroid's tail's first
    // bone, its tip cut off, has none: its model would not load, a Host_Error at its precache.)
    const uint32_t bones = ragdoll::limbBones(*rig, bone) & (only ? only : ~0u);
    bool any = false;
    for(const uint32_t t : rig->triBones)
    {
        any = any || (t && !(t & ~bones));
    }
    if(!any)
    {
        return false;
    }
    // A monster with gear left off its limbs (leftOff): the triangles themselves, the gear's not counted (Freddie's gun
    // forearm is little but the gun: none of its own, no piece; his super nailgun drops instead, QC VR_ADGun_LimbCut).
    bool leftOffModel = false;
    for(const LeftOff& l : leftOff)
    {
        leftOffModel = leftOffModel || (rig->model && rig->numVerts == l.numVerts && !strcmp(rig->model->name, l.model));
    }
    if(!leftOffModel)
    {
        return true;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig->model)));
    if(!hdr || hdr->numverts != rig->numVerts)
    {
        return true;
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        bool own = true;
        for(int k = 0; k < 3 && own; k++)
        {
            const int v = desc[idx[i + k]].vertindex;
            own = (bones & (1u << rig->vertBone[static_cast<za::SizeT>(v)])) != 0 && !isLeftOff(*rig, v);
        }
        if(own)
        {
            return true;
        }
    }
    return false;
}

byte* derivedFile(const char* name, unsigned int* path_id)
{
    if(!strstr(name, suffix))
    {
        return nullptr;
    }
    for(za::SizeT i = 0; i < late.ready.size(); i++) // built ahead (spawnFrame)
    {
        if(!strcmp(late.ready[i].name, name))
        {
            LateLimb& l = late.ready[i];
            byte* out = static_cast<byte*>(VR_HeapMalloc(l.file.size() + 1));
            memcpy(out, l.file.data(), l.file.size());
            out[l.file.size()] = 0;
            com_filesize = static_cast<int>(l.file.size());
            if(path_id)
            {
                *path_id = l.pathId;
            }
            late.ready.erase(late.ready.begin() + i);
            Con_DPrintf("limb model: %s made (built ahead)\n", name);
            return out;
        }
    }
    Built b;
    const char* why = "";
    if(!buildNamed(name, path_id, b, why))
    {
        Con_DPrintf("limb model: %s: %s\n", name, why);
        return nullptr;
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    memcpy(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("limb model: %s made: %d triangles, %d in its cap, %d vertices\n", name, b.tris, b.cap, b.verts);
    return out;
}

void info_f()
{
    const char* base = Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/soldier.mdl";
    const ragdoll::Rig* rig = rigOf(base);
    if(!rig)
    {
        Con_Printf("vr_limb_models: %s has no ragdoll rig\n", base);
        return;
    }
    Con_Printf("vr_limb_models: %s, %d bones\n", base, rig->numBones);
    for(int b = 0; b < rig->numBones; b++)
    {
        if(!ragdoll::limbJoint(*rig, b))
        {
            continue;
        }
        char text[MAX_QPATH];
        Built built;
        const char* why = "";
        if(!name(base, b, 0, text, sizeof(text)) || !buildNamed(text, nullptr, built, why))
        {
            Con_Printf("  %2d %-12s -: %s\n", b, rig->bones[b].name, why);
            continue;
        }
        Con_Printf("  %2d %-12s bones %04x: %d triangles, cap %d, %d vertices, %.0f x %.0f x %.0f units, %d bytes\n", b,
            rig->bones[b].name, ragdoll::limbBones(*rig, b), built.tris, built.cap, built.verts, built.size.x, built.size.y,
            built.size.z, static_cast<int>(built.file.size()));
    }
}

namespace
{

// The precached alias model a monster's `mdl` names (0: none, or not a monster's classname).
[[nodiscard]] int mdlModel(edict_t* ent)
{
    const int mdl = progs::fields().mdl;
    if(mdl < 0 || strncmp(PR_GetString(ent->v.classname), "monster_", 8) != 0)
    {
        return 0;
    }
    const char* name = PR_GetString(progs::fieldInt(ent, mdl));
    for(int i = 1; name[0] && i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            return sv.models[i] && sv.models[i]->type == mod_alias ? i : 0;
        }
    }
    return 0;
}

// The model index `ent` is a kind of monster by (0: none): a monster's own, or the model an Arcane Dimensions monster
// takes as it appears (`mdl`, precached by its spawn function). AD's monsters have none as the map loads (or the
// placeholder misc_broken.mdl that sizes them) until ad_monster_spawn sets it, 0.1-0.5 s in or at their trigger: their
// kinds' limbs were made in play then, 5-25 ms a limb, one a server frame (ad_swampy's spawns' "server other" hitches).
[[nodiscard]] int kindModel(edict_t* ent)
{
    if(ent->free)
    {
        return 0;
    }
    const int index = static_cast<int>(ent->v.modelindex);
    const bool monster = static_cast<int>(ent->v.flags) & FL_MONSTER;
    const int appears = mdlModel(ent);
    if(appears > 0 && appears != index)
    {
        return appears;
    }
    return monster && index > 0 && index < MAX_MODELS && sv.model_precache[index] && sv.models[index] ? index : 0;
}

} // namespace

// (each kind of monster once: the first of its model's; a limb its first cut takes whole, as box3d::limbModel names it)
void prebuild()
{
    if(!vr_limbs.value || !vr_limbs_prebuild.value || !VR_AllowLatePrecache())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    // Their rigs made first, all at once on the pool (warmRigs): limbInfo below asks for each kind's rig in turn, which
    // derived them one after another here (tens of milliseconds each), before the server's own warmRigs.
    za::Vector<qmodel_t*> rigged;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        const int index = kindModel(EDICT_NUM(num));
        if(index > 0 && za::find(rigged.begin(), rigged.end(), sv.models[index]) == rigged.end())
        {
            rigged.pushBack(sv.models[index]);
        }
    }
    ragdoll::warmRigs(rigged.data(), static_cast<int>(rigged.size()));
    VR_TimeAdd("  their monsters' rigs (warmRigs)", Sys_DoubleTime() - t0);
    za::Vector<int> seen;
    int made = 0, waiting = 0;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const int index = kindModel(ent);
        if(index <= 0 || za::find(seen.begin(), seen.end(), index) != seen.end())
        {
            continue;
        }
        seen.pushBack(index);
        if(late.seen.empty())
        {
            newMap();
        }
        late.seen[static_cast<za::SizeT>(index)] = 1; // (spawnFrame: the kinds made here are not new)
        // (an AD monster not yet appeared posed with the model it takes for the lookups, set back after)
        const float oldIndex = ent->v.modelindex;
        waiting += static_cast<int>(oldIndex) != index;
        ent->v.modelindex = static_cast<float>(index);
        const uint32_t joints = box3d::limbInfo(ent, 0, -1);
        for(int bone = 0; bone < 32; bone++)
        {
            const char* name = (joints & (1u << bone)) ? box3d::limbModel(ent, bone) : "";
            if(name[0])
            {
                VR_LatePrecacheModel(name);
                made++;
            }
        }
        ent->v.modelindex = oldIndex;
    }
    VR_TimeAdd("VR after spawn: limb models", Sys_DoubleTime() - t0);
    Con_DPrintf("limb models: %d made for %d kinds of monster (%d of them not yet appeared), %.1f ms\n", made,
        static_cast<int>(seen.size()), waiting, (Sys_DoubleTime() - t0) * 1000.0);
}

namespace
{

[[nodiscard]] bool precached(const char* name)
{
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            return true;
        }
    }
    return false;
}

// `ent`'s kind's limbs not yet precached, built on the pool (its rig and .mdl read here, the building there).
void startKind(edict_t* ent, int index)
{
    LateKind kind;
    const uint32_t joints = box3d::limbInfo(ent, 0, -1);
    for(int bone = 0; bone < 32; bone++)
    {
        const char* name = (joints & (1u << bone)) ? box3d::limbModel(ent, bone) : "";
        if(name[0] && !precached(name))
        {
            kind.names.pushBack(name);
        }
    }
    if(kind.names.empty())
    {
        return;
    }
    const char* base = sv.model_precache[index];
    const ragdoll::Rig* rig = rigOf(base); // (warm: the map's start warms every precached model's rig)
    if(!rig)
    {
        return;
    }
    byte* src = COM_LoadMallocFile(VR_ModelFile(base), &kind.pathId);
    if(!src)
    {
        return;
    }
    za::Vector<byte> bytes;
    bytes.resize(static_cast<za::SizeT>(com_filesize));
    memcpy(bytes.data(), src, bytes.size());
    VR_HeapFree(src);
    struct Part
    {
        int bone{-1};
        uint32_t only{0};
    };
    za::Vector<Part> parts;
    for(const char* name : kind.names)
    {
        char b[MAX_QPATH];
        Part p;
        if(!parse(name, b, sizeof(b), p.bone, p.only))
        {
            p.bone = -1;
        }
        parts.pushBack(p);
    }
    const bool zombie = modelmeta::is(rig->model, modelmeta::Id::Zombie);
    // (the job owns copies of what it reads: a map or game change while it runs frees nothing it uses)
    kind.built = jobs::async([rig = *rig, bytes = ZA_MOVE(bytes), parts = ZA_MOVE(parts), zombie] {
        QVR_TRACY_SCOPE(tracyScope, "limb models built ahead");
        za::Vector<Built> out;
        for(const Part& p : parts)
        {
            Built b;
            const char* why = "";
            if(p.bone < 0 || !build(rig, p.bone, p.only, bytes.data(), bytes.size(), zombie, b, why))
            {
                b.file.clear();
            }
            out.pushBack(ZA_MOVE(b));
        }
        return out;
    });
    Con_DPrintf("limb models: %s first spawned after the load: %d limbs being built\n", base,
        static_cast<int>(kind.names.size()));
    late.kinds.pushBack(ZA_MOVE(kind));
}

} // namespace

// (PROFILE_20261016 B5: a knight spawned by the firing range's dispenser had its 12 limb models and their normal maps
// made inside its first cut's frame, 32 ms.)
void spawnFrame()
{
    if(!sv.active || !vr_limbs.value || !vr_limbs_prebuild.value || !VR_AllowLatePrecache())
    {
        return;
    }
    if(late.seen.empty())
    {
        newMap();
    }
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const int index = static_cast<int>(ent->v.modelindex);
        if(ent->free || !(static_cast<int>(ent->v.flags) & FL_MONSTER) || index <= 0 || index >= MAX_MODELS ||
            late.seen[static_cast<za::SizeT>(index)] || !sv.model_precache[index] || !sv.models[index])
        {
            continue;
        }
        late.seen[static_cast<za::SizeT>(index)] = 1;
        startKind(ent, index);
    }
    for(za::SizeT k = 0; k < late.kinds.size();)
    {
        LateKind& kind = late.kinds[k];
        if(!kind.built.ready())
        {
            k++;
            continue;
        }
        za::Vector<Built> built = kind.built.get();
        for(za::SizeT i = 0; i < kind.names.size() && i < built.size(); i++)
        {
            if(!built[i].file.empty())
            {
                late.ready.pushBack(LateLimb{kind.names[i], kind.pathId, ZA_MOVE(built[i].file)});
            }
        }
        late.kinds.erase(late.kinds.begin() + k);
    }
    // One a frame (its skins, their normal maps): ~2-3 ms, never all of a kind's in one frame.
    if(!late.ready.empty())
    {
        const char* name = late.ready.front().name;
        if(!precached(name))
        {
            VR_LatePrecacheModel(name); // (derivedFile takes its file)
        }
        if(!late.ready.empty() && late.ready.front().name == name)
        {
            late.ready.erase(late.ready.begin());
        }
    }
}

} // namespace qvr::limbmodel
