// vr_admelee.cpp -- see vr_admelee.hpp.

#include "vr_admelee.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::admelee
{
namespace
{

constexpr size_t headerSize = 84;

// An AD weapon model (its gib, lying on the ground: one frame) as AD's release has it: the hand's place (model units),
// the handle's line towards the blade's tip (the head of a hammer or a mace) and the guard's (the head's width) across
// it, measured with Misc/quakevr scratch fits (the handle's middle on a sword, 6 units from a heavy handle's end).
struct Weapon
{
    const char* model;
    float grip[3];
    float axis[3];
    float guard[3];
};

constexpr Weapon weapons[] = {
    {"progs/w_dfurysword1.mdl", {0.9424f, -15.1979f, 3.8688f}, {-0.0876f, 0.9931f, -0.0785f}, {-0.9962f, -0.0873f, 0.0076f}},
    {"progs/w_dknightsword.mdl", {-0.0255f, -17.3005f, 3.8529f}, {0.f, 0.9969f, -0.0784f}, {1.f, 0.f, -0.0009f}},
    {"progs/w_zombiesword.mdl", {-0.0137f, 17.3584f, 2.7473f}, {0.f, -0.9986f, -0.0522f}, {1.f, 0.f, -0.0011f}},
    {"progs/w_dguardsword.mdl", {-0.0255f, -17.3027f, 3.8216f}, {0.f, 0.9969f, -0.0784f}, {1.f, 0.f, -0.0009f}},
    {"progs/w_ogreham.mdl", {-0.5749f, 16.4152f, 1.6627f}, {0.1017f, -0.9851f, 0.1386f}, {-0.9948f, -0.1008f, 0.0133f}},
    {"progs/w_ogremac.mdl", {0.0002f, 16.4026f, 1.7803f}, {-0.0002f, -0.9833f, 0.1820f}, {0.9996f, -0.0055f, -0.0284f}},
};

// The knight's sword (v_ksword.mdl, slot 19's HandAnchor and MuzzleAnchor): the hand's place, and its blade's line to
// the tip; its guard across that line in the model's x-z plane; AD's units to its (make_swords.py SCALE).
constexpr glm::vec3 swordHand{-0.9793f, 1.4925f, -2.6023f};
constexpr glm::vec3 swordTip{28.9816f, 0.121f, 76.4247f};
constexpr float swordScale = 2.6f;

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

[[nodiscard]] const Weapon* weaponOf(const char* base)
{
    for(const Weapon& w : weapons)
    {
        if(!strcmp(w.model, base))
        {
            return &w;
        }
    }
    return nullptr;
}

// "<base>#melee" taken apart; false if it isn't one.
[[nodiscard]] bool parse(const char* name, char* base, size_t baseSize)
{
    const char* at = strstr(name, suffix);
    if(!at || at[strlen(suffix)] != '\0')
    {
        return false;
    }
    const size_t n = static_cast<size_t>(at - name);
    if(n + 1 > baseSize)
    {
        return false;
    }
    memcpy(base, name, n);
    base[n] = '\0';
    return true;
}

struct Built
{
    za::Vector<byte> file;
    int verts{0}, tris{0};
    glm::vec3 lo{0.f}, hi{0.f};
    glm::vec3 tip{0.f}; // the far end's middle (vr_weapons.inc's MuzzleAnchor)
};

// The rotation and placement: AD's model frame (the weapon's own axes at its grip) to the knight sword's.
struct Frame
{
    glm::vec3 grip, a, g, n;    // source
    glm::vec3 A, G, N;          // the knight's sword's
    [[nodiscard]] glm::vec3 point(const glm::vec3& p) const
    {
        const glm::vec3 d = p - grip;
        return swordHand + swordScale * (A * glm::dot(d, a) + G * glm::dot(d, g) + N * glm::dot(d, n));
    }
    [[nodiscard]] glm::vec3 dir(const glm::vec3& v) const
    {
        return A * glm::dot(v, a) + G * glm::dot(v, g) + N * glm::dot(v, n);
    }
};

[[nodiscard]] Frame frameOf(const Weapon& w)
{
    Frame f;
    f.grip = glm::vec3{w.grip[0], w.grip[1], w.grip[2]};
    f.a = glm::normalize(glm::vec3{w.axis[0], w.axis[1], w.axis[2]});
    const glm::vec3 g{w.guard[0], w.guard[1], w.guard[2]};
    f.g = glm::normalize(g - f.a * glm::dot(g, f.a));
    f.n = glm::cross(f.a, f.g);
    f.A = glm::normalize(swordTip - swordHand);
    f.G = glm::normalize(glm::vec3{f.A.z, 0.f, -f.A.x});
    f.N = glm::cross(f.A, f.G);
    return f;
}

// AD's weapon model laid as the knight's sword (its file copied; the header's scale, corner, radius and eye, and each
// frame's box, vertices and normals rewritten).
[[nodiscard]] bool build(const Weapon& w, const byte* src, size_t size, Built& out, const char*& why)
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
    const Frame fr = frameOf(w);

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

    // Every pose's place in the file (a group's members), its box's, and the group's box.
    za::Vector<size_t> poses, groupBoxes;
    for(int f = 0; f < numFrames; f++)
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
            groupBoxes.pushBack(at + 4);
            at += 4 + 8 + static_cast<size_t>(members) * 4;
        }
        for(int m = 0; m < members; m++)
        {
            if(at + 24 + static_cast<size_t>(numVerts) * 4 > size)
            {
                why = "truncated (frame)";
                return false;
            }
            poses.pushBack(at);
            at += 24 + static_cast<size_t>(numVerts) * 4;
        }
    }

    // The new places, and the box they need.
    za::Vector<glm::vec3> pts;
    pts.reserve(poses.size() * static_cast<size_t>(numVerts));
    out.lo = glm::vec3{1e30f};
    out.hi = glm::vec3{-1e30f};
    for(const size_t pose : poses)
    {
        for(int v = 0; v < numVerts; v++)
        {
            const byte* b = src + pose + 24 + static_cast<size_t>(v) * 4;
            const glm::vec3 p = fr.point(glm::vec3{b[0], b[1], b[2]} * scale + origin);
            pts.pushBack(p);
            out.lo = glm::min(out.lo, p);
            out.hi = glm::max(out.hi, p);
        }
    }
    const glm::vec3 scale2 = glm::max((out.hi - out.lo) / 255.f, glm::vec3{1e-4f});
    const glm::vec3 origin2 = out.lo;

    out.file.clear();
    out.file.emplaceBackRange(src, size);
    byte* f = out.file.data();
    const auto setf = [&](size_t off, float v) {
        const float le = LittleFloat(v);
        memcpy(f + off, &le, 4);
    };
    float radius = 0.f;
    for(int i = 0; i < 3; i++)
    {
        setf(8 + static_cast<size_t>(i) * 4, scale2[i]);
        setf(20 + static_cast<size_t>(i) * 4, origin2[i]);
    }
    const glm::vec3 eye = fr.point(glm::vec3{rdf(src, 36), rdf(src, 40), rdf(src, 44)});
    for(int i = 0; i < 3; i++)
    {
        setf(36 + static_cast<size_t>(i) * 4, eye[i]);
    }

    out.tip = glm::vec3{0.f};
    const glm::vec3 A = fr.A;
    float farthest = -1e30f;
    for(const glm::vec3& p : pts)
    {
        farthest = za::max(farthest, glm::dot(p - swordHand, A));
    }
    int tipCount = 0;
    size_t k = 0;
    for(const size_t pose : poses)
    {
        byte lo[3]{255, 255, 255}, hi[3]{0, 0, 0};
        for(int v = 0; v < numVerts; v++, k++)
        {
            byte* b = f + pose + 24 + static_cast<size_t>(v) * 4;
            const glm::vec3& p = pts[k];
            for(int i = 0; i < 3; i++)
            {
                const float q = (p[i] - origin2[i]) / scale2[i];
                b[i] = static_cast<byte>(za::max(0.f, za::min(255.f, q + 0.5f)));
                lo[i] = za::min(lo[i], b[i]);
                hi[i] = za::max(hi[i], b[i]);
            }
            const byte n = src[pose + 24 + static_cast<size_t>(v) * 4 + 3];
            if(n < NUMVERTEXNORMALS)
            {
                b[3] = nearestNormal(fr.dir(glm::vec3{r_avertexnormals[n][0], r_avertexnormals[n][1], r_avertexnormals[n][2]}));
            }
            radius = za::max(radius, glm::length(p));
            if(pose == poses[0] && glm::dot(p - swordHand, A) > farthest - 2.f)
            {
                out.tip += p;
                tipCount++;
            }
        }
        for(int i = 0; i < 3; i++)
        {
            f[pose + i] = lo[i];
            f[pose + 4 + i] = hi[i];
        }
    }
    for(const size_t box : groupBoxes) // (a group's box: all of it)
    {
        for(int i = 0; i < 3; i++)
        {
            f[box + i] = 0;
            f[box + 4 + i] = 255;
        }
    }
    setf(32, radius);
    out.tip /= static_cast<float>(za::max(tipCount, 1));
    out.verts = numVerts;
    out.tris = numTris;
    return true;
}

[[nodiscard]] bool buildNamed(const char* name, unsigned int* path_id, Built& out, const char*& why)
{
    char base[MAX_QPATH];
    if(!parse(name, base, sizeof(base)))
    {
        why = "not an AD melee weapon's model";
        return false;
    }
    const Weapon* w = weaponOf(base);
    if(!w)
    {
        why = "not an AD melee weapon";
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
    const bool ok = build(*w, src, size, out, why);
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
    const char* at = strstr(name, suffix);
    if(!at || at[strlen(suffix)] != '\0')
    {
        return nullptr;
    }
    Built b;
    const char* why = "";
    if(!buildNamed(name, path_id, b, why))
    {
        // Not made (AD's data not there, another release's file): the knight's own sword's file under the weapon's
        // name (as QC's VR_ADMelee_Model without AD's data), never a failed map load.
        Con_DPrintf("AD melee model: %s: %s: progs/v_ksword.mdl stands in\n", name, why);
        return COM_LoadMallocFile("progs/v_ksword.mdl", path_id);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    memcpy(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("AD melee model: %s made: %d triangles, %d vertices\n", name, b.tris, b.verts);
    return out;
}

void info_f()
{
    for(const Weapon& w : weapons)
    {
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%s", w.model, suffix);
        Built b;
        const char* why = "";
        if(!buildNamed(name, nullptr, b, why))
        {
            Con_Printf("%s: %s\n", name, why);
            continue;
        }
        Con_Printf("%s: %d triangles, %d vertices, box %.2f %.2f %.2f .. %.2f %.2f %.2f, tip %.4f %.4f %.4f\n", name, b.tris,
            b.verts, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z, b.tip.x, b.tip.y, b.tip.z);
    }
}

} // namespace qvr::admelee
