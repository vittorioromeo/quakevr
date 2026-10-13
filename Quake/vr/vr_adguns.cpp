// vr_adguns.cpp -- see vr_adguns.hpp.

#include "vr_adguns.hpp"
#include "vr_modelmetadata.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::adguns
{
namespace
{

constexpr size_t headerSize = 84;

// A gun of AD's: whose model, its vertices there, the frame it is taken from (the gun pointing ahead, as fired: its
// barrel along the model's x), its length as a weapon (model units), the hand's place (source units along the barrel
// from the gun's middle: make_enemyguns.py's grip_x) and the corner its file is packed from (scale_origin: the weapon
// settings scale about it, so the same corner as the Quake VR gun whose settings it inherits keeps them right).
struct Gun
{
    modelmeta::Id model;
    int numVerts, numTris; // the model as measured (another release: no gun)
    int ranges[2][2];      // its vertices: [first, last], up to two runs (last < first: none)
    const char* frame;
    float length;
    float gripX;
    float corner[3];
};

// The guns (vr_monstermods.cpp hides the same ones in the soldiers' deaths: they drop them): the soldier's (Quake VR's
// grunt's mesh: its gun is v_gruntgun.mdl's source, make_enemyguns.py) laid as v_gruntgun.mdl lies (slot 27 inherits
// slot 21's settings: its corner); the enforcer's (id's mesh), the centurion's, the pyro's and the fumigator's (one gun,
// the pyro's hose not taken) as v_enfrifle.mdl (slots 28-31 inherit slot 22's; their corner 1.41 units further out on
// y: wider guns; their Offset Y and hotspots moved for it: vr_weapons.inc).
constexpr Gun guns[] = {
    {modelmeta::Id::AdSoldier, 555, 810, {{463, 548}, {1, 0}}, "shoot5", 46.f, -6.5f, {-29.9f, -6.170675f, -4.6f}},
    {modelmeta::Id::AdEnforcer, 222, 424, {{45, 60}, {1, 0}}, "attack5", 46.f, -9.f, {-16.397949f, -7.5f, -4.6f}},
    {modelmeta::Id::AdCenturion, 284, 544, {{45, 60}, {1, 0}}, "attack5", 46.f, -9.f, {-16.397949f, -7.5f, -4.6f}},
    {modelmeta::Id::AdPyro, 277, 530, {{17, 32}, {1, 0}}, "attack5", 46.f, -9.f, {-16.397949f, -7.5f, -4.6f}},
    {modelmeta::Id::AdFumigator, 277, 530, {{17, 32}, {1, 0}}, "attack5", 46.f, -9.f, {-16.397949f, -7.5f, -4.6f}},
};

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

// "<base>#gun<skin>" taken apart; false if it isn't one.
[[nodiscard]] bool parse(const char* name, char* base, size_t baseSize, int& skin)
{
    const char* at = strstr(name, suffix);
    if(!at || at == name || static_cast<size_t>(at - name) >= baseSize)
    {
        return false;
    }
    memcpy(base, name, static_cast<size_t>(at - name));
    base[at - name] = 0;
    const char* digits = at + strlen(suffix);
    skin = 0;
    for(const char* p = digits; *p; p++)
    {
        if(*p < '0' || *p > '9')
        {
            return false;
        }
        skin = skin * 10 + (*p - '0');
    }
    return true;
}

[[nodiscard]] const Gun* gunOf(const char* base)
{
    const modelmeta::Id id = modelmeta::identifyPath(base);
    for(const Gun& g : guns)
    {
        if(g.model == id)
        {
            return &g;
        }
    }
    return nullptr;
}

[[nodiscard]] bool isGunVertex(const Gun& g, int v)
{
    for(const auto& r : g.ranges)
    {
        if(v >= r[0] && v <= r[1])
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool build(const char* base, int skin, const byte* src, size_t size, Built& out, const char*& why)
{
    const Gun* gun = gunOf(base);
    if(!gun)
    {
        why = "not an AD soldier's model";
        return false;
    }
    if(size < headerSize || rd(src, 0) != IDPOLYHEADER || rd(src, 4) != ALIAS_VERSION)
    {
        why = "not an alias model";
        return false;
    }
    const int numSkins = rd(src, 48), skinW = rd(src, 52), skinH = rd(src, 56);
    const int numVerts = rd(src, 60), numTris = rd(src, 64), numFrames = rd(src, 68);
    if(numVerts != gun->numVerts || numTris != gun->numTris || numSkins < 1 || skinW <= 0 || skinH <= 0)
    {
        why = "another release of the model than the one measured";
        return false;
    }
    const glm::vec3 scale{rdf(src, 8), rdf(src, 12), rdf(src, 16)};
    const glm::vec3 origin{rdf(src, 20), rdf(src, 24), rdf(src, 28)};
    const size_t skinBytes = static_cast<size_t>(skinW) * static_cast<size_t>(skinH);

    // The skins: the one asked for (a group's first picture), else the first.
    size_t at = headerSize;
    const byte* chosen = nullptr;
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 8 > size)
        {
            why = "truncated (skins)";
            return false;
        }
        const byte* picture = nullptr;
        if(rd(src, at) == 0)
        {
            picture = src + at + 4;
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(rd(src, at + 4), 1);
            picture = src + at + 8 + static_cast<size_t>(n) * 4;
            at += 8 + static_cast<size_t>(n) * 4 + static_cast<size_t>(n) * skinBytes;
        }
        if(s == 0 || s == skin)
        {
            chosen = picture;
        }
    }
    const size_t stAt = at;
    const size_t triAt = stAt + static_cast<size_t>(numVerts) * 12;
    at = triAt + static_cast<size_t>(numTris) * 16;
    if(at > size || !chosen || chosen + skinBytes > src + size)
    {
        why = "truncated (vertices)";
        return false;
    }

    // The frame it is taken from (singles and groups' members by name).
    size_t poseAt = 0;
    for(int f = 0; f < numFrames && !poseAt; f++)
    {
        if(at + 4 > size)
        {
            break;
        }
        const int type = rd(src, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            members = za::max(rd(src, at), 1);
            at += 4 + 8 + static_cast<size_t>(members) * 4;
        }
        for(int m = 0; m < members; m++)
        {
            const size_t nameAt = at + 8;
            const size_t vertsAt = at + 24;
            at = vertsAt + static_cast<size_t>(numVerts) * 4;
            if(at > size)
            {
                break;
            }
            char frameName[17] = {};
            memcpy(frameName, src + nameAt, 16);
            if(!strcmp(frameName, gun->frame))
            {
                poseAt = vertsAt;
                break;
            }
        }
    }
    if(!poseAt)
    {
        why = "its firing frame not found";
        return false;
    }
    const auto posOf = [&](int v) {
        const byte* p = src + poseAt + static_cast<size_t>(v) * 4;
        return glm::vec3{p[0] * scale.x + origin.x, p[1] * scale.y + origin.y, p[2] * scale.z + origin.z};
    };

    // Its vertices and triangles (all three corners on the gun).
    za::Vector<int> kept(static_cast<za::SizeT>(numVerts), -1);
    za::Vector<int> verts; // source vertex of each kept one
    za::Vector<int> tris;  // facesfront, a, b, c (kept vertices)
    for(int t = 0; t < numTris; t++)
    {
        const size_t tAt = triAt + static_cast<size_t>(t) * 16;
        int v[3];
        bool all = true;
        for(int c = 0; c < 3; c++)
        {
            v[c] = rd(src, tAt + 4 + static_cast<size_t>(c) * 4);
            all = all && v[c] >= 0 && v[c] < numVerts && isGunVertex(*gun, v[c]);
        }
        if(!all)
        {
            continue;
        }
        tris.pushBack(rd(src, tAt));
        for(int c = 0; c < 3; c++)
        {
            int& k = kept[static_cast<za::SizeT>(v[c])];
            if(k < 0)
            {
                k = static_cast<int>(verts.size());
                verts.pushBack(v[c]);
            }
            tris.pushBack(k);
        }
    }
    if(verts.size() < 3)
    {
        why = "no gun triangles";
        return false;
    }

    // Its frame: the barrel (the principal axis, towards +x) along x, up along z, y left.
    glm::vec3 centre{0.f};
    for(const int v : verts)
    {
        centre += posOf(v);
    }
    centre /= static_cast<float>(verts.size());
    float cov[3][3] = {};
    for(const int v : verts)
    {
        const glm::vec3 d = posOf(v) - centre;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                cov[i][j] += d[i] * d[j];
            }
        }
    }
    glm::vec3 axis{1.f, 0.3f, 0.2f};
    for(int it = 0; it < 100; it++)
    {
        glm::vec3 w{0.f};
        for(int i = 0; i < 3; i++)
        {
            w[i] = cov[i][0] * axis.x + cov[i][1] * axis.y + cov[i][2] * axis.z;
        }
        const float n = glm::length(w);
        if(n < 1e-9f)
        {
            break;
        }
        axis = w / n;
    }
    if(axis.x < 0.f)
    {
        axis = -axis;
    }
    const glm::vec3 up = glm::normalize(glm::vec3{0.f, 0.f, 1.f} - axis * axis.z);
    const glm::vec3 left = glm::cross(up, axis);
    za::Vector<glm::vec3> local;
    float lo = 1e30f, hi = -1e30f;
    for(const int v : verts)
    {
        const glm::vec3 d = posOf(v) - centre;
        const glm::vec3 l{glm::dot(d, axis), glm::dot(d, left), glm::dot(d, up)};
        local.pushBack(l);
        lo = za::min(lo, l.x);
        hi = za::max(hi, l.x);
    }
    const float k = gun->length / za::max(hi - lo, 1e-3f);
    const float gripX = (hi + lo) * 0.5f + gun->gripX;
    float gripZ = 1e30f;
    for(const glm::vec3& l : local)
    {
        if(za::abs(l.x - gripX) < 4.f)
        {
            gripZ = za::min(gripZ, l.z);
        }
    }
    if(gripZ > 1e29f)
    {
        for(const glm::vec3& l : local)
        {
            gripZ = za::min(gripZ, l.z);
        }
    }
    // The hand's place the origin (the middle of where a pistol grip would hang under the receiver: as
    // make_enemyguns.py's guns' origin, 3.15 model units under it).
    glm::vec3 boxLo{gun->corner[0], gun->corner[1], gun->corner[2]}, boxHi{-1e30f};
    float radius = 0.f;
    for(glm::vec3& l : local)
    {
        l = glm::vec3{(l.x - gripX) * k, l.y * k, (l.z - gripZ) * k - 3.15f};
        boxLo = glm::min(boxLo, l);
        boxHi = glm::max(boxHi, l);
        radius = za::max(radius, glm::length(l));
    }
    const glm::vec3 pack = glm::max((boxHi - boxLo) / 255.f, glm::vec3{1e-4f});
    out.lo = boxLo;
    out.hi = boxHi;
    out.cornerKept = boxLo == glm::vec3{gun->corner[0], gun->corner[1], gun->corner[2]};
    out.muzzle = glm::vec3{0.f};
    int tip = 0;
    for(const glm::vec3& l : local)
    {
        if(l.x > boxHi.x - 1.f)
        {
            out.muzzle += l;
            tip++;
        }
    }
    out.muzzle /= static_cast<float>(za::max(tip, 1));

    za::Vector<byte>& f = out.file;
    f.clear();
    put(f, IDPOLYHEADER);
    put(f, ALIAS_VERSION);
    putf(f, pack.x);
    putf(f, pack.y);
    putf(f, pack.z);
    putf(f, boxLo.x);
    putf(f, boxLo.y);
    putf(f, boxLo.z);
    putf(f, radius);
    putf(f, 0.f);
    putf(f, 0.f);
    putf(f, 0.f);
    put(f, 1); // its one skin
    put(f, skinW);
    put(f, skinH);
    put(f, static_cast<int>(verts.size()));
    put(f, static_cast<int>(tris.size() / 4));
    put(f, 1); // one frame
    put(f, 0);
    put(f, 0); // no effects
    putf(f, rdf(src, 80));
    put(f, 0);
    f.emplaceBackRange(chosen, skinBytes);
    for(const int v : verts)
    {
        const size_t vAt = stAt + static_cast<size_t>(v) * 12;
        put(f, rd(src, vAt));
        put(f, rd(src, vAt + 4));
        put(f, rd(src, vAt + 8));
    }
    for(za::SizeT i = 0; i < tris.size(); i += 4)
    {
        put(f, tris[i]);
        put(f, tris[i + 1]);
        put(f, tris[i + 2]);
        put(f, tris[i + 3]);
    }
    put(f, 0); // a single frame
    za::Vector<byte> packed;
    byte bmin[4] = {255, 255, 255, 0}, bmax[4] = {0, 0, 0, 0};
    for(za::SizeT i = 0; i < verts.size(); i++)
    {
        byte b[4];
        for(int c = 0; c < 3; c++)
        {
            const float q = (local[i][c] - boxLo[c]) / pack[c];
            b[c] = static_cast<byte>(za::max(0, za::min(255, static_cast<int>(q + 0.5f))));
            bmin[c] = za::min(bmin[c], b[c]);
            bmax[c] = za::max(bmax[c], b[c]);
        }
        // Its normal turned with it.
        const byte n = src[poseAt + static_cast<size_t>(verts[i]) * 4 + 3];
        const glm::vec3 nv = n < NUMVERTEXNORMALS ? glm::vec3{r_avertexnormals[n][0], r_avertexnormals[n][1], r_avertexnormals[n][2]}
                                                  : glm::vec3{0.f, 0.f, 1.f};
        b[3] = nearestNormal(glm::vec3{glm::dot(nv, axis), glm::dot(nv, left), glm::dot(nv, up)});
        packed.emplaceBackRange(b, 4);
    }
    f.emplaceBackRange(bmin, 4);
    f.emplaceBackRange(bmax, 4);
    char frameName[16] = "gun";
    f.emplaceBackRange(reinterpret_cast<const byte*>(frameName), 16);
    f.emplaceBackRange(packed.data(), packed.size());
    out.verts = static_cast<int>(verts.size());
    out.tris = static_cast<int>(tris.size() / 4);
    return true;
}

namespace
{

[[nodiscard]] bool buildNamed(const char* name, unsigned int* path_id, Built& out, const char*& why)
{
    char base[MAX_QPATH];
    int skin = 0;
    if(!parse(name, base, sizeof(base), skin))
    {
        why = "not a gun model";
        return false;
    }
    if(!gunOf(base))
    {
        why = "not an AD soldier's model";
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
    const bool ok = build(base, skin, src, size, out, why);
    VR_HeapFree(src);
    if(path_id)
    {
        *path_id = pathId;
    }
    return ok;
}

} // namespace

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
        // Not made (another release of AD's model, a replacement pack, its file missing): Quake VR's own stand-in's
        // file under the gun's name (as QC's VR_ADGun_Model without AD's data), never a failed map load.
        char base[MAX_QPATH];
        int skin = 0;
        const Gun* g = parse(name, base, sizeof(base), skin) ? gunOf(base) : nullptr;
        const char* standIn = (g && g->model == modelmeta::Id::AdSoldier) ? "progs/v_gruntgun.mdl" : "progs/v_enfrifle.mdl";
        Con_DPrintf("AD gun model: %s: %s: %s stands in\n", name, why, standIn);
        return COM_LoadMallocFile(standIn, path_id);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    memcpy(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("AD gun model: %s made: %d triangles, %d vertices\n", name, b.tris, b.verts);
    return out;
}

void info_f()
{
    static constexpr const char* names[] = {"progs/mon_soldier.mdl#gun4", "progs/mon_enforcer.mdl#gun3", "progs/mon_centurion.mdl#gun0",
        "progs/mon_pyro.mdl#gun0", "progs/mon_fumigator.mdl#gun0"};
    for(const char* name : names)
    {
        Built b;
        const char* why = "";
        if(!buildNamed(name, nullptr, b, why))
        {
            Con_Printf("%s: %s\n", name, why);
            continue;
        }
        Con_Printf("%s: %d triangles, %d vertices, box %.2f %.2f %.2f .. %.2f %.2f %.2f%s, muzzle %.4f %.4f %.4f\n", name, b.tris,
            b.verts, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z, b.cornerKept ? "" : " (its corner moved: past the settings')",
            b.muzzle.x, b.muzzle.y, b.muzzle.z);
    }
}

} // namespace qvr::adguns
