// vr_limbmodel.cpp -- see vr_limbmodel.hpp.

#include "vr_limbmodel.hpp"
#include "vr_ragdoll.hpp"
#include "vr_api.h"

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
constexpr int stripRows = 8;          // the blood strip under each skin (texels)
constexpr int maxSkinHeight = 480;    // Mod_LoadAliasModel's MAX_LBM_HEIGHT
// Blood reds (0..255) the strip is painted with: the palette's nearest (its ordinary colours, not the fullbright).
constexpr float bloodReds[4][3] = {{96.f, 8.f, 4.f}, {124.f, 14.f, 8.f}, {72.f, 4.f, 2.f}, {148.f, 24.f, 12.f}};

[[nodiscard]] int rd(const byte* b, size_t at)
{
    int v;
    memcpy(&v, b + at, 4);
    return LittleLong(v);
}

[[nodiscard]] float rdf(const byte* b, size_t at)
{
    float v;
    memcpy(&v, b + at, 4);
    return LittleFloat(v);
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

[[nodiscard]] byte nearestNormal(const glm::vec3& d)
{
    int best = 0;
    float bestDot = -2.f;
    for(int i = 0; i < NUMVERTEXNORMALS; i++)
    {
        const float k = d.x * r_avertexnormals[i][0] + d.y * r_avertexnormals[i][1] + d.z * r_avertexnormals[i][2];
        if(k > bestDot)
        {
            bestDot = k;
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
    if(srcSize < headerSize || rd(src, 0) != IDPOLYHEADER)
    {
        why = "not an alias model";
        return false;
    }
    const int numSkins = rd(src, 48), skinW = rd(src, 52), skinH = rd(src, 56), numVerts = rd(src, 60), numTris = rd(src, 64),
              numFrames = rd(src, 68);
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
    const glm::vec3 scale{rdf(src, 8), rdf(src, 12), rdf(src, 16)};
    const glm::vec3 origin{rdf(src, 20), rdf(src, 24), rdf(src, 28)};
    const size_t skinBytes = static_cast<size_t>(skinW) * static_cast<size_t>(skinH);
    size_t at = headerSize;
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 4 > srcSize)
        {
            why = "short";
            return false;
        }
        if(rd(src, at) == 0)
        {
            at += 4 + skinBytes;
        }
        else
        {
            const int n = at + 8 <= srcSize ? rd(src, at + 4) : 0;
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
    if(rd(src, framesAt) == 0)
    {
        poseAt = framesAt + 4 + 24;
    }
    else
    {
        const int n = framesAt + 8 <= srcSize ? rd(src, framesAt + 4) : 0;
        poseAt = framesAt + 16 + static_cast<size_t>(za::max(n, 0)) * 4 + 24;
    }
    if(poseAt + static_cast<size_t>(numVerts) * 4 > srcSize || skinsEnd > srcSize)
    {
        why = "short";
        return false;
    }

    const uint32_t bones = ragdoll::limbBones(rig, bone) & (only ? only : ~0u);
    const auto onLimb = [&](int v) { return v >= 0 && v < numVerts && (bones & (1u << rig.vertBone[static_cast<za::SizeT>(v)])) != 0; };
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
        const int front = rd(src, o);
        int v[3];
        int on = 0;
        for(int c = 0; c < 3; c++)
        {
            v[c] = rd(src, o + 4 + static_cast<size_t>(c) * 4);
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
                    ov.onseam = rd(src, stAt + static_cast<size_t>(v[c]) * 12);
                    ov.s = rd(src, stAt + static_cast<size_t>(v[c]) * 12 + 4);
                    ov.t = rd(src, stAt + static_cast<size_t>(v[c]) * 12 + 8);
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
    const bool strip = skinH + stripRows <= maxSkinHeight;
    const ragdoll::Bone& root = rig.bones[bone];
    const glm::vec3 axis = root.pivot - root.end;
    const glm::vec3 capOut = glm::length(axis) > 1e-3f ? glm::normalize(axis) : glm::vec3{0.f, 0.f, -1.f};
    const byte capNormal = nearestNormal(capOut);
    int capMid = -1;
    if(capCorners > 0)
    {
        OutVert m;
        m.p = capSum / static_cast<float>(capCorners);
        capMid = static_cast<int>(verts.size());
        verts.pushBack(m);
        // The cap's texels: along the blood strip (its middle row), every corner its own column.
        capVerts.pushBack(capMid);
        int k = 0;
        for(const int i : capVerts)
        {
            OutVert& cv = verts[static_cast<za::SizeT>(i)];
            cv.onseam = 0;
            cv.s = (k * 13 + 3) % skinW;
            cv.t = strip ? skinH + stripRows / 2 : skinH / 2;
            cv.normal = capNormal;
            k++;
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

    const int outH = skinH + (strip ? stripRows : 0);
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
    putf(f, rdf(src, 80));

    // The skins, each with the strip under it.
    byte reds[4];
    for(int i = 0; i < 4; i++)
    {
        reds[i] = nearestPalette(bloodReds[i]);
    }
    const auto image = [&](const byte* pixels) {
        f.emplaceBackRange(pixels, skinBytes);
        if(strip)
        {
            for(int y = 0; y < stripRows; y++)
            {
                for(int x = 0; x < skinW; x++)
                {
                    const unsigned h = static_cast<unsigned>(x * 73856093) ^ static_cast<unsigned>(y * 19349663);
                    f.pushBack(reds[(h >> 3) & 3u]);
                }
            }
        }
    };
    at = headerSize;
    for(int s = 0; s < numSkins; s++)
    {
        if(rd(src, at) == 0)
        {
            put(f, 0);
            image(src + at + 4);
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(rd(src, at + 4), 0);
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

} // namespace

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
    const uint32_t bones = ragdoll::limbBones(*rig, bone) & (only ? only : ~0u);
    for(const uint8_t b : rig->vertBone)
    {
        if(bones & (1u << b))
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

} // namespace qvr::limbmodel
