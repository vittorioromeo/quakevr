// vr_adguns.cpp -- see vr_adguns.hpp.

#include "vr_adguns.hpp"
#include "vr_adcrossbow.hpp"
#include "vr_modelmetadata.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
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

// AD's own weapon models, whole, laid as our guns are (ROUND22 "AD crossbow: turned to face +x"): AD's crossbow
// knight's crossbow (progs/w_dcrossbow.mdl, its gib) lies flat with its front at -y and the bow across x. As it was,
// its weapon slot needed a Yaw of 90, and every Pitch and Roll then turned it about the wrong axis (its own: the
// slot's angles are applied in the model's frame), so it felt different in the hand from every other gun. Turned a
// quarter about z (its front +x, the bow across y, up z), the tiller behind the bow (`pivot`, in its own space) its
// origin; nothing else changed (every vertex exactly where it was, re-packed losslessly: x and y swap their scales).
// The crossbow gets two more frames, its string and limbs as a shot looses them (vr_adcrossbow.cpp: `bow`).
struct Turned
{
    const char* model;
    float pivot[3];
    bool bow;
};
constexpr Turned turnedModels[] = {
    {"progs/w_dcrossbow.mdl", {0.f, 8.f, 3.f}, true},
};

[[nodiscard]] const Turned* turnedOf(const char* base)
{
    for(const Turned& t : turnedModels)
    {
        if(!strcmp(t.model, base))
        {
            return &t;
        }
    }
    return nullptr;
}

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

// A model of turnedModels turned (its file copied; the header's scale, corner, radius and eye, and each frame's box and
// vertices rewritten: x' = pivot.y - y, y' = x - pivot.x, z' = z - pivot.z).
[[nodiscard]] bool turn(const Turned& t, const byte* src, size_t size, Built& out, const char*& why)
{
    if(size < headerSize || rd(src, 0) != IDPOLYHEADER || rd(src, 4) != ALIAS_VERSION)
    {
        why = "not an alias model";
        return false;
    }
    const int numSkins = rd(src, 48), skinW = rd(src, 52), skinH = rd(src, 56);
    const int numVerts = rd(src, 60), numTris = rd(src, 64), numFrames = rd(src, 68);
    if(numSkins < 1 || skinW <= 0 || skinH <= 0 || numVerts <= 0 || numTris <= 0 || numFrames <= 0)
    {
        why = "bad header";
        return false;
    }
    const glm::vec3 scale{rdf(src, 8), rdf(src, 12), rdf(src, 16)};
    const glm::vec3 origin{rdf(src, 20), rdf(src, 24), rdf(src, 28)};
    const glm::vec3 c{t.pivot[0], t.pivot[1], t.pivot[2]};
    const glm::vec3 scale2{scale.y, scale.x, scale.z};
    const glm::vec3 origin2{c.y - origin.y - 255.f * scale.y, origin.x - c.x, origin.z - c.z};
    const glm::vec3 eye{rdf(src, 36), rdf(src, 40), rdf(src, 44)};

    out.file.clear();
    out.file.emplaceBackRange(src, size);
    byte* f = out.file.data();
    const auto setf = [&](size_t at, float v) {
        const float le = LittleFloat(v);
        memcpy(f + at, &le, 4);
    };
    for(int i = 0; i < 3; i++)
    {
        setf(8 + static_cast<size_t>(i) * 4, scale2[i]);
        setf(20 + static_cast<size_t>(i) * 4, origin2[i]);
    }
    setf(36, c.y - eye.y);
    setf(40, eye.x - c.x);
    setf(44, eye.z - c.z);

    const size_t skinBytes = static_cast<size_t>(skinW) * static_cast<size_t>(skinH);
    size_t at = headerSize;
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 8 > size)
        {
            why = "truncated (skins)";
            return false;
        }
        if(rd(src, at) == 0)
        {
            at += 4 + skinBytes;
        }
        else
        {
            const int n = za::max(rd(src, at + 4), 1);
            at += 8 + static_cast<size_t>(n) * 4 + static_cast<size_t>(n) * skinBytes;
        }
    }
    at += static_cast<size_t>(numVerts) * 12 + static_cast<size_t>(numTris) * 16;

    const auto turnBox = [&](size_t minAt) { // a box's bboxmin and bboxmax
        byte* lo = f + minAt;
        byte* hi = f + minAt + 4;
        const byte loX = lo[0], loY = lo[1], hiX = hi[0], hiY = hi[1];
        lo[0] = static_cast<byte>(255 - hiY);
        hi[0] = static_cast<byte>(255 - loY);
        lo[1] = loX;
        hi[1] = hiX;
    };
    float radius = 0.f;
    out.lo = glm::vec3{1e30f};
    out.hi = glm::vec3{-1e30f};
    size_t firstPose = 0;
    for(int fr = 0; fr < numFrames; fr++)
    {
        if(at + 4 > size)
        {
            why = "truncated (frames)";
            return false;
        }
        const int type = rd(src, at);
        at += 4;
        int members = 1;
        if(type != 0)
        {
            if(at + 12 > size)
            {
                why = "truncated (frame group)";
                return false;
            }
            members = za::max(rd(src, at), 1);
            turnBox(at + 4);
            at += 4 + 8 + static_cast<size_t>(members) * 4;
        }
        for(int m = 0; m < members; m++)
        {
            const size_t vertsAt = at + 24;
            if(vertsAt + static_cast<size_t>(numVerts) * 4 > size)
            {
                why = "truncated (frame)";
                return false;
            }
            turnBox(at);
            firstPose = firstPose ? firstPose : vertsAt;
            for(int v = 0; v < numVerts; v++)
            {
                byte* b = f + vertsAt + static_cast<size_t>(v) * 4;
                const byte x = b[0], y = b[1], n = b[3];
                b[0] = static_cast<byte>(255 - y);
                b[1] = x;
                if(n < NUMVERTEXNORMALS)
                {
                    b[3] = nearestNormal(glm::vec3{-r_avertexnormals[n][1], r_avertexnormals[n][0], r_avertexnormals[n][2]});
                }
                const glm::vec3 p = glm::vec3{b[0], b[1], b[2]} * scale2 + origin2;
                radius = za::max(radius, glm::length(p));
                out.lo = glm::min(out.lo, p);
                out.hi = glm::max(out.hi, p);
            }
            at = vertsAt + static_cast<size_t>(numVerts) * 4;
        }
    }
    setf(32, radius);

    // Its front end's middle (the muzzle) in its first pose.
    out.muzzle = glm::vec3{0.f};
    int tip = 0;
    for(int v = 0; v < numVerts; v++)
    {
        const byte* b = f + firstPose + static_cast<size_t>(v) * 4;
        const glm::vec3 p = glm::vec3{b[0], b[1], b[2]} * scale2 + origin2;
        if(p.x > out.hi.x - 1.f)
        {
            out.muzzle += p;
            tip++;
        }
    }
    out.muzzle /= static_cast<float>(za::max(tip, 1));

    // The bow's frames (vr_adcrossbow.cpp), after the file's (the frames are its last part): its first pose's vertices
    // moved, packed as it is (clamped to its box's bytes), the normals kept.
    if(t.bow && numFrames == 1 && at == size && adcrossbow::knownModel(numVerts, numTris))
    {
        za::Vector<byte> pose;
        pose.emplaceBackRange(f + firstPose, static_cast<size_t>(numVerts) * 4);
        for(int k = 1; k <= adcrossbow::addedFrames; k++)
        {
            byte lo[4] = {255, 255, 255, 0}, hi[4] = {0, 0, 0, 0};
            za::Vector<byte> verts;
            for(int v = 0; v < numVerts; v++)
            {
                const byte* b = pose.data() + static_cast<size_t>(v) * 4;
                const glm::vec3 p = adcrossbow::framePoint(k, v, glm::vec3{b[0], b[1], b[2]} * scale2 + origin2);
                byte q[4] = {0, 0, 0, b[3]};
                for(int i = 0; i < 3; i++)
                {
                    const float s = (p[i] - origin2[i]) / scale2[i];
                    q[i] = static_cast<byte>(za::clamp(static_cast<int>(s + 0.5f), 0, 255));
                    lo[i] = za::min(lo[i], q[i]);
                    hi[i] = za::max(hi[i], q[i]);
                }
                const glm::vec3 r = glm::vec3{q[0], q[1], q[2]} * scale2 + origin2;
                radius = za::max(radius, glm::length(r));
                out.lo = glm::min(out.lo, r);
                out.hi = glm::max(out.hi, r);
                verts.emplaceBackRange(q, 4);
            }
            put(out.file, 0); // a single frame
            out.file.emplaceBackRange(lo, 4);
            out.file.emplaceBackRange(hi, 4);
            char name[16] = {};
            q_strlcpy(name, adcrossbow::frameNames[k - 1], sizeof(name));
            out.file.emplaceBackRange(reinterpret_cast<const byte*>(name), sizeof(name));
            out.file.emplaceBackRange(verts.data(), verts.size());
        }
        f = out.file.data();
        const int frames = LittleLong(1 + adcrossbow::addedFrames);
        memcpy(f + 68, &frames, 4);
        setf(32, radius);
    }
    out.cornerKept = true;
    out.verts = numVerts;
    out.tris = numTris;
    return true;
}

[[nodiscard]] bool buildNamed(const char* name, unsigned int* path_id, Built& out, const char*& why)
{
    char base[MAX_QPATH];
    int skin = 0;
    if(!parse(name, base, sizeof(base), skin))
    {
        why = "not a gun model";
        return false;
    }
    const Turned* turned = turnedOf(base);
    if(!gunOf(base) && !turned)
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
    const bool ok = turned ? turn(*turned, src, size, out, why) : build(base, skin, src, size, out, why);
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
    if(strstr(name, flashSuffix))
    {
        return flashFile(name, path_id); // (the plasma guns' flashes: vr_adgunart.cpp)
    }
    if(strstr(name, variantSuffix))
    {
        return variantFile(name, path_id); // (our burst rifle's variants: vr_adgunart.cpp)
    }
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

bool slotNames(const char* id, const char* name)
{
    // Our burst rifle's variants: "<base>#adv<digits>" names each variant listed ("#adv" alone: all of them).
    if(const char* v = strstr(name, variantSuffix))
    {
        const char* own = strstr(id, variantSuffix);
        const size_t n = static_cast<size_t>(v - name) + strlen(variantSuffix);
        if(!own || static_cast<size_t>(own - id) + strlen(variantSuffix) != n || strncmp(id, name, n) != 0)
        {
            return false;
        }
        const char digit = name[n];
        const char* listed = id + n;
        return digit >= '0' && digit <= '9' && name[n + 1] == 0 && (!listed[0] || strchr(listed, digit) != nullptr);
    }
    const char* gun = strstr(name, suffix);
    if(!gun)
    {
        return false;
    }
    const size_t n = static_cast<size_t>(gun - name) + strlen(suffix);
    return strlen(id) == n && !strncmp(id, name, n);
}

void info_f()
{
    static constexpr const char* names[] = {"progs/mon_soldier.mdl#gun4", "progs/mon_enforcer.mdl#gun3", "progs/mon_centurion.mdl#gun0",
        "progs/mon_pyro.mdl#gun0", "progs/mon_fumigator.mdl#gun0", "progs/w_dcrossbow.mdl#gun0"};
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
