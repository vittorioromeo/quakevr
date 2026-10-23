// vr_admelee.cpp -- see vr_admelee.hpp.

#include "vr_admelee.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <algorithm>
#include <math.h>
#include <string.h>

namespace qvr::admelee
{
namespace
{

constexpr size_t headerSize = 84;

// Units below are AD's, in the weapon's own frame at the hand: x along the handle towards the tip (the head), y along
// the guard (the head's width), z across the blade (the head's depth).

// A sword made anew: its handle turned round x from the pommel to the guard, the guard a bar along y, the blade a
// flat, bevelled one along its spine.
struct Sword
{
    float pommel;           // the pommel's end
    float grip;             // the grip's radius
    float guard, guardHalf; // the guard's middle along the handle, its half-length
    float guardSlant;       // along the handle per unit along the guard (the fury's guard leans)
    float guardX, guardZ;   // the bar's half-sizes along the handle and across the blade
    float thick;            // the blade's half-thickness
    float skinWiden;        // AD's blade's width to ours (its skin's whole width across our narrower blade)
    int points;
    float spine[6][3]; // the blade's spine (x, y, half-width), its first in the guard, its last the tip (width 0)
};

// A hammer's or mace's handle made anew (AD's head kept): turned round the handle's line, a pommel, a wrapped grip, a
// collar (its guard) and the shaft into the head. AD's skin keeps the handle as one strip (s along, t round) and the
// collar apart.
struct Heavy
{
    float from[3], to[3]; // the handle's line: its end and its top inside the head
    float cut;            // AD's head: its triangles all beyond this x (the rest, AD's handle, is made anew)
    float pommelS[2], gripS[2], shaftS[2], wrapT[2];
    float collarS[2], collarT[2];
};

// An AD weapon model (its gib, lying on the ground: one frame) as AD's release has it: the hand's place (model units),
// the handle's line towards the blade's tip (the head of a hammer or a mace) and the guard's (the head's width) across
// it, measured with Misc/quakevr scratch fits (the handle's middle on a sword, 6 units from a heavy handle's end).
struct Weapon
{
    const char* model;
    float grip[3];
    float axis[3];
    float guard[3];
    bool flip; // turned half round the handle: the fury's curve, the hammer's spike away from the player
    const Sword* sword;
    const Heavy* heavy;
    bool flail{false}; // the death sergeant's flail: AD's handle kept (its chain and head the engine's: flailPart)
};

constexpr Sword furySword{-4.f, 0.62f, 3.5f, 5.8f, 0.18f, 0.5f, 0.42f, 0.32f, 1.25f, 6,
    {{3.3f, -0.5f, 1.65f}, {14.5f, -2.6f, 1.95f}, {24.5f, -2.4f, 1.9f}, {32.4f, 0.5f, 1.5f}, {35.2f, 2.5f, 0.8f},
        {36.4f, 3.7f, 0.f}}};
constexpr Sword knightSword{-3.5f, 0.62f, 4.1f, 6.f, 0.f, 0.52f, 0.44f, 0.3f, 1.35f, 5,
    {{4.45f, 0.f, 1.85f}, {20.f, 0.f, 1.72f}, {31.5f, 0.f, 1.5f}, {36.3f, 0.f, 0.95f}, {39.7f, 0.f, 0.f}}};
constexpr Sword zombieSword{-3.f, 0.58f, 3.55f, 3.8f, 0.f, 0.42f, 0.36f, 0.26f, 1.1f, 5,
    {{3.85f, 0.f, 1.35f}, {20.f, 0.f, 1.25f}, {31.f, 0.f, 1.1f}, {35.6f, 0.f, 0.7f}, {38.8f, 0.f, 0.f}}};
constexpr Heavy hammer{{-6.f, -0.55f, 0.f}, {22.9f, 2.f, 0.f}, 22.5f, {5.f, 10.f}, {10.f, 21.f}, {21.f, 64.f},
    {54.f, 60.f}, {100.f, 105.f}, {37.f, 51.f}};
constexpr Heavy mace{{-6.f, 0.f, -0.15f}, {17.7f, 0.f, -0.3f}, 17.6f, {5.f, 9.f}, {9.f, 20.f}, {20.f, 64.f},
    {54.f, 60.f}, {100.f, 105.f}, {37.f, 51.f}};

constexpr Weapon weapons[] = {
    {"progs/w_dfurysword1.mdl", {0.9424f, -15.1979f, 3.8688f}, {-0.0876f, 0.9931f, -0.0785f},
        {-0.9962f, -0.0873f, 0.0076f}, true, &furySword, nullptr},
    {"progs/w_dknightsword.mdl", {-0.0255f, -17.3005f, 3.8529f}, {0.f, 0.9969f, -0.0784f}, {1.f, 0.f, -0.0009f}, false,
        &knightSword, nullptr},
    {"progs/w_zombiesword.mdl", {-0.0137f, 17.3584f, 2.7473f}, {0.f, -0.9986f, -0.0522f}, {1.f, 0.f, -0.0011f}, false,
        &zombieSword, nullptr},
    {"progs/w_dguardsword.mdl", {-0.0255f, -17.3027f, 3.8216f}, {0.f, 0.9969f, -0.0784f}, {1.f, 0.f, -0.0009f}, false,
        &knightSword, nullptr},
    {"progs/w_ogreham.mdl", {-0.5749f, 16.4152f, 1.6627f}, {0.1017f, -0.9851f, 0.1386f},
        {-0.9948f, -0.1008f, 0.0133f}, true, nullptr, &hammer},
    {"progs/w_ogremac.mdl", {0.0002f, 16.4026f, 1.7803f}, {-0.0002f, -0.9833f, 0.1820f}, {0.9996f, -0.0055f, -0.0284f},
        false, nullptr, &mace},
    // The death sergeant's flail (AD's gib lies flat: its handle from the pommel at 10.5 -11.3 to the chain's ring at
    // 3 -8.5; the hand 40% along it from the pommel).
    {"progs/w_blueflail.mdl", {7.506f, -10.16f, 1.56f}, {-0.9345f, 0.3523f, 0.0371f}, {0.f, 0.f, 1.f}, false, nullptr,
        nullptr, true},
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

// "<the flail's model>#flailhead" taken apart; false if it isn't one.
[[nodiscard]] bool flailHeadOf(const char* name, char* base, size_t baseSize)
{
    const char* at = strstr(name, flailHeadSuffix);
    if(!at || at[strlen(flailHeadSuffix)] != '\0')
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
    int openEdges{0}, overEdges{0}; // edges of one triangle, of more than two (welded by place)
    float volume{0.f};              // signed, the MDL's winding (negative: closed the right way round)
    glm::vec3 lo{0.f}, hi{0.f};
    glm::vec3 tip{0.f}; // the far end's middle (vr_weapons.inc's MuzzleAnchor)
    za::Vector<glm::vec3> spine; // a sword's blade's spine as made (its rings' middles), in the model's frame
    za::Vector<glm::vec3> head;  // a hammer's or mace's head's vertices (AD's head), in the model's frame
};

// AD's frame (the weapon's own axes at its grip) to the knight sword's.
struct Frame
{
    glm::vec3 grip, a, g, n; // source
    glm::vec3 A, G, N;       // the knight's sword's
    bool flip{false};
    glm::vec3 base{swordHand}; // where the source's grip goes, and its scale (the flail's head: 0, flailHeadScale)
    float k{swordScale};
    [[nodiscard]] glm::vec3 local(const glm::vec3& p) const
    {
        const glm::vec3 d = p - grip;
        return {glm::dot(d, a), glm::dot(d, g), glm::dot(d, n)};
    }
    [[nodiscard]] glm::vec3 localDir(const glm::vec3& v) const
    {
        return {glm::dot(v, a), glm::dot(v, g), glm::dot(v, n)};
    }
    [[nodiscard]] glm::vec3 dir(glm::vec3 l) const
    {
        if(flip)
        {
            l.y = -l.y;
            l.z = -l.z;
        }
        return A * l.x + G * l.y + N * l.z;
    }
    [[nodiscard]] glm::vec3 place(const glm::vec3& l) const
    {
        return base + k * dir(l);
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
    f.flip = w.flip;
    return f;
}

// AD's model as read: its first pose in the weapon's frame, its skin's places, its triangles (each one's outward
// normal and its island: the triangles joined on AD's skin).
struct Source
{
    struct Tri
    {
        int v[3];
        float s[3], t[3];
        glm::vec3 out;
        int island;
    };
    int skinW{0}, skinH{0};
    size_t skinsEnd{0}, pose{0};
    za::Vector<glm::vec3> pos, nrm;
    za::Vector<Tri> tris;
};

[[nodiscard]] bool readSource(const Frame& fr, const byte* src, size_t size, Source& out, const char*& why)
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
    out.skinW = skinW;
    out.skinH = skinH;
    out.skinsEnd = at;
    const size_t st = at, tr = st + static_cast<size_t>(numVerts) * 12;
    at = tr + static_cast<size_t>(numTris) * 16;
    if(at + 4 + 24 + static_cast<size_t>(numVerts) * 4 > size || rd(src, at) != 0)
    {
        why = "truncated, or its first frame a group";
        return false;
    }
    out.pose = at + 4;
    out.pos.clear();
    out.nrm.clear();
    for(int v = 0; v < numVerts; v++)
    {
        const byte* b = src + out.pose + 24 + static_cast<size_t>(v) * 4;
        out.pos.pushBack(fr.local(glm::vec3{b[0], b[1], b[2]} * scale + origin));
        out.nrm.pushBack(b[3] < NUMVERTEXNORMALS
                             ? fr.localDir(glm::vec3{r_avertexnormals[b[3]][0], r_avertexnormals[b[3]][1],
                                   r_avertexnormals[b[3]][2]})
                             : glm::vec3{1.f, 0.f, 0.f});
    }

    // Triangles: AD's winding is clockwise seen from outside (as every MDL's).
    out.tris.clear();
    za::Vector<int> owner, parent;
    owner.resize(static_cast<size_t>(numVerts) * 2, -1);
    for(int i = 0; i < numTris; i++)
    {
        const size_t o = tr + static_cast<size_t>(i) * 16;
        Source::Tri t;
        const bool front = rd(src, o) != 0;
        int keys[3];
        for(int c = 0; c < 3; c++)
        {
            const int v = rd(src, o + 4 + static_cast<size_t>(c) * 4);
            if(v < 0 || v >= numVerts)
            {
                why = "bad triangle";
                return false;
            }
            const size_t sv = st + static_cast<size_t>(v) * 12;
            const bool back = !front && rd(src, sv) != 0;
            t.v[c] = v;
            t.s[c] = static_cast<float>(rd(src, sv + 4) + (back ? skinW / 2 : 0));
            t.t[c] = static_cast<float>(rd(src, sv + 8));
            keys[c] = v * 2 + (back ? 1 : 0);
        }
        const glm::vec3 c = glm::cross(out.pos[t.v[2]] - out.pos[t.v[0]], out.pos[t.v[1]] - out.pos[t.v[0]]);
        const float len = glm::length(c);
        t.out = len > 1e-6f ? c / len : glm::vec3{0.f};
        t.island = i;
        parent.pushBack(i);
        for(const int k : keys) // (joined where they share a vertex and its place on the skin)
        {
            if(owner[k] < 0)
            {
                owner[k] = i;
                continue;
            }
            int x = owner[k], y = i;
            while(parent[x] != x)
            {
                x = parent[x];
            }
            while(parent[y] != y)
            {
                y = parent[y];
            }
            parent[x] = y;
        }
        out.tris.pushBack(t);
    }
    for(Source::Tri& t : out.tris)
    {
        int x = t.island;
        while(parent[x] != x)
        {
            x = parent[x];
        }
        t.island = x;
    }
    return true;
}

// The new model, in the weapon's frame: each vertex's place, normal, skin place (or, while it isn't skinned yet, its
// place along and round a strip of AD's skin), shading group and patch; triangles in the MDL's winding.
struct Patch
{
    bool strip{false};               // skinned as a strip of AD's skin (s0..s1 along, t0..t1 round)
    float s0{0}, s1{0}, t0{0}, t1{0};
    int island{-1};
};

struct Mesh
{
    za::Vector<glm::vec3> pos, nrm;
    za::Vector<glm::vec3> look; // where AD's model is looked at to skin it (its place, or the blade's widened)
    za::Vector<glm::vec2> uv;
    za::Vector<int> group; // -1: its normal given; else the vertices of one group in one place share theirs
    za::Vector<int> patch; // -1: skinned already
    za::Vector<int> tri;
    za::Vector<Patch> patches;
    za::Vector<glm::vec3> spine; // the blade's rings' middles (AD's frame at the hand), the guard to the tip
    int groups{0};

    int vert(const glm::vec3& p, int grp, int pat, const glm::vec2& st = glm::vec2{0.f}, const glm::vec3& n = {})
    {
        pos.pushBack(p);
        look.pushBack(p);
        nrm.pushBack(n);
        uv.pushBack(st);
        group.pushBack(grp);
        patch.pushBack(pat);
        return static_cast<int>(pos.size()) - 1;
    }

    // A triangle facing away from `inside`; none if it has no area.
    void face(int a, int b, int c, const glm::vec3& inside)
    {
        const glm::vec3 n = glm::cross(pos[b] - pos[a], pos[c] - pos[a]);
        if(glm::dot(n, n) < 1e-10f)
        {
            return;
        }
        if(glm::dot(n, (pos[a] + pos[b] + pos[c]) / 3.f - inside) < 0.f)
        {
            const int x = b;
            b = c;
            c = x;
        }
        tri.pushBack(a); // (counter-clockwise outside: the MDL's clockwise)
        tri.pushBack(c);
        tri.pushBack(b);
    }
};

// A closed tube through `count` rings of `around` points (ring r's k-th at pts[r * around + k]; a ring all one point
// closes it there), each ring's middle in `mids`. Each column round it (points k to k + 1) gets its own vertices: a
// patch of its own, skinned from AD's model where it lies, or, given `strip`, from AD's skin's strip (`along`: each
// ring's place along it, 0..1). `smooth`: shaded round (the columns' normals shared), else each column flat.
void tube(Mesh& m, const za::Vector<glm::vec3>& pts, const za::Vector<glm::vec3>& mids, int around, bool smooth,
    const Patch* strip = nullptr, const za::Vector<float>* along = nullptr, const za::Vector<glm::vec3>* look = nullptr)
{
    const int count = static_cast<int>(mids.size());
    const int shared = m.groups++;
    int stripPatch = -1;
    if(strip)
    {
        m.patches.pushBack(*strip);
        stripPatch = static_cast<int>(m.patches.size()) - 1;
    }
    for(int k = 0; k < around; k++)
    {
        const int k1 = (k + 1) % around;
        const int grp = smooth ? shared : m.groups++;
        int pat = stripPatch;
        if(!strip)
        {
            m.patches.pushBack(Patch{});
            pat = static_cast<int>(m.patches.size()) - 1;
        }
        const int first = static_cast<int>(m.pos.size());
        for(int r = 0; r < count; r++)
        {
            const float u = along ? (*along)[r] : 0.f;
            const int v = m.vert(pts[r * around + k], grp, pat, glm::vec2{u, static_cast<float>(k) / around});
            m.vert(pts[r * around + k1], grp, pat, glm::vec2{u, static_cast<float>(k + 1) / around});
            if(look)
            {
                m.look[v] = (*look)[r * around + k];
                m.look[v + 1] = (*look)[r * around + k1];
            }
        }
        for(int r = 0; r + 1 < count; r++)
        {
            const int a = first + r * 2, b = a + 1, c = a + 3, d = a + 2;
            const glm::vec3 inside = (mids[r] + mids[r + 1]) * 0.5f;
            m.face(a, b, c, inside);
            m.face(a, c, d, inside);
        }
    }
}

// A part turned round the line from `from` along `dir`: its profile's (x along the weapon, radius) rings.
void lathe(Mesh& m, const glm::vec3& from, const glm::vec3& dir, const za::Vector<glm::vec2>& profile, int around,
    const Patch* strip)
{
    const glm::vec3 u = glm::normalize(glm::cross(glm::vec3{0.f, 0.f, 1.f}, dir));
    const glm::vec3 w = glm::cross(dir, u);
    za::Vector<glm::vec3> pts, mids;
    za::Vector<float> along;
    const float x0 = profile[0].x, x1 = profile.back().x;
    for(const glm::vec2& p : profile)
    {
        const glm::vec3 c = from + dir * ((p.x - from.x) / dir.x);
        mids.pushBack(c);
        along.pushBack((p.x - x0) / za::max(x1 - x0, 1e-4f));
        for(int k = 0; k < around; k++)
        {
            const float t = 2.f * static_cast<float>(M_PI) * (static_cast<float>(k) + 0.5f) / around;
            pts.pushBack(p.y > 0.f ? c + (u * cosf(t) + w * sinf(t)) * p.y : c);
        }
    }
    tube(m, pts, mids, around, true, strip, &along);
}

// The blade along its spine (Hermite through the spine's points, chord-length tangents): flat, its edges bevelled.
void blade(Mesh& m, const Sword& s)
{
    constexpr float profile[6][2] = {{1.f, 0.f}, {0.42f, 1.f}, {-0.42f, 1.f}, {-1.f, 0.f}, {-0.42f, -1.f}, {0.42f, -1.f}};
    za::Vector<glm::vec3> pts, mids, look;
    const auto ring = [&](const glm::vec3& c, const glm::vec2& tangent, float half) {
        const glm::vec3 across{-tangent.y, tangent.x, 0.f};
        const float thick = half > 0.f ? s.thick * powf(za::min(1.f, half / 0.9f), 0.7f) : 0.f;
        mids.pushBack(c);
        for(const auto& p : profile)
        {
            pts.pushBack(c + across * (p[0] * half) + glm::vec3{0.f, 0.f, p[1] * thick});
            look.pushBack(c + across * (p[0] * half * s.skinWiden) + glm::vec3{0.f, 0.f, p[1] * thick});
        }
    };
    const auto P = [&](int i) { return glm::vec3{s.spine[i][0], s.spine[i][1], s.spine[i][2]}; };
    const auto chord = [&](int i) { return glm::length(glm::vec2{P(i + 1)} - glm::vec2{P(i)}); };
    const int n = s.points;
    const auto tangent = [&](int i, int seg) { // at point i, for segment seg (its chord's length)
        if(i == 0)
        {
            return P(1) - P(0);
        }
        if(i == n - 1)
        {
            return P(n - 1) - P(n - 2);
        }
        return (P(i + 1) - P(i - 1)) * (chord(seg) / (chord(i - 1) + chord(i)));
    };
    const glm::vec2 t0 = glm::normalize(glm::vec2{P(1) - P(0)});
    // (Closed inside the guard, in the blade's plane: a spine point's z is its half-width, not a place; the closing
    // point once kept it and stood out of the handle's side by the blade's half-width.)
    ring(glm::vec3{s.spine[0][0], s.spine[0][1], 0.f} - glm::vec3{t0, 0.f} * 0.45f, t0, 0.f);
    for(int i = 0; i + 1 < n; i++)
    {
        const glm::vec3 p0 = P(i), p1 = P(i + 1), m0 = tangent(i, i), m1 = tangent(i + 1, i);
        const int steps = i + 2 == n ? 6 : za::max(2, static_cast<int>(ceilf(chord(i) / 1.6f)));
        for(int k = 0; k < steps + (i + 2 == n ? 1 : 0); k++)
        {
            const float t = static_cast<float>(k) / steps, t2 = t * t, t3 = t2 * t;
            const glm::vec3 p = (2 * t3 - 3 * t2 + 1) * p0 + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * p1 +
                                (t3 - t2) * m1;
            const glm::vec3 d = (6 * t2 - 6 * t) * p0 + (3 * t2 - 4 * t + 1) * m0 + (-6 * t2 + 6 * t) * p1 +
                                (3 * t2 - 2 * t) * m1;
            ring(glm::vec3{p.x, p.y, 0.f}, glm::normalize(glm::vec2{d}), za::max(p.z, 0.f));
        }
    }
    for(size_t i = 1; i < mids.size(); i++) // (not the closing point inside the guard)
    {
        m.spine.pushBack(mids[i]);
    }
    tube(m, pts, mids, 6, false, nullptr, nullptr, &look);
}

// The guard: a bar along y, its middle wider (the blade through it), its ends finials, curled a little towards the
// blade.
void guard(Mesh& m, const Sword& s)
{
    constexpr float station[16][2] = {{-1.f, 0.f}, {-0.965f, 0.62f}, {-0.91f, 1.12f}, {-0.83f, 1.12f},
        {-0.74f, 0.84f}, {-0.5f, 0.86f}, {-0.2f, 0.95f}, {-0.12f, 1.22f}, {0.12f, 1.22f}, {0.2f, 0.95f}, {0.5f, 0.86f},
        {0.74f, 0.84f}, {0.83f, 1.12f}, {0.91f, 1.12f}, {0.965f, 0.62f}, {1.f, 0.f}};
    constexpr float profile[8][2] = {{1.f, 0.5f}, {0.5f, 1.f}, {-0.5f, 1.f}, {-1.f, 0.5f}, {-1.f, -0.5f},
        {-0.5f, -1.f}, {0.5f, -1.f}, {1.f, -0.5f}};
    za::Vector<glm::vec3> pts, mids;
    for(const auto& st : station)
    {
        const float y = st[0] * s.guardHalf;
        const glm::vec3 c{s.guard + s.guardSlant * y + 0.3f * st[0] * st[0], y, 0.f};
        mids.pushBack(c);
        for(const auto& p : profile)
        {
            pts.pushBack(c + glm::vec3{p[0] * s.guardX * st[1], 0.f, p[1] * s.guardZ * st[1]});
        }
    }
    tube(m, pts, mids, 8, false);
}

// The handle: pommel, wrapped grip, ferrule, into the guard.
void swordHandle(Mesh& m, const Sword& s)
{
    const float p = s.pommel, r = s.grip, g = s.guard;
    za::Vector<glm::vec2> prof;
    for(const glm::vec2 q : {glm::vec2{p, 0.f}, glm::vec2{p + 0.05f, 0.38f}, glm::vec2{p + 0.22f, 0.74f},
            glm::vec2{p + 0.52f, 0.93f}, glm::vec2{p + 0.85f, 0.88f}, glm::vec2{p + 1.1f, 0.66f},
            glm::vec2{p + 1.28f, 0.47f}, glm::vec2{p + 1.42f, 0.5f}, glm::vec2{p + 1.52f, r * 1.06f}})
    {
        prof.pushBack(q);
    }
    const float from = p + 1.6f, to = g - 0.9f;
    const int wraps = za::max(2, static_cast<int>((to - from) / 0.45f + 0.5f));
    for(int k = 0; k <= wraps; k++)
    {
        prof.pushBack(glm::vec2{from + (to - from) * k / wraps, r * (k % 2 ? 1.07f : 1.f)});
    }
    for(const glm::vec2 q : {glm::vec2{g - 0.8f, r * 1.2f}, glm::vec2{g - 0.5f, r * 1.2f}, glm::vec2{g - 0.45f, r * 0.95f},
            glm::vec2{g, r * 0.95f}, glm::vec2{g + 0.02f, 0.f}})
    {
        prof.pushBack(q);
    }
    lathe(m, glm::vec3{p, 0.f, 0.f}, glm::vec3{1.f, 0.f, 0.f}, prof, 10, nullptr);
}

// A heavy weapon's handle, each part skinned from its strip of AD's skin.
void heavyHandle(Mesh& m, const Heavy& h)
{
    const glm::vec3 from{h.from[0], h.from[1], h.from[2]}, to{h.to[0], h.to[1], h.to[2]};
    const glm::vec3 dir = glm::normalize(to - from);
    const float top = to.x;
    za::Vector<glm::vec2> prof;
    const auto part = [&](std::initializer_list<glm::vec2> pts, const float* s, const float* t) {
        prof.clear();
        for(const glm::vec2 q : pts)
        {
            prof.pushBack(q);
        }
        const Patch strip{true, s[0], s[1], t[0], t[1]};
        lathe(m, from, dir, prof, 12, &strip);
    };
    part({{-6.35f, 0.f}, {-6.3f, 0.5f}, {-6.12f, 0.92f}, {-5.8f, 1.18f}, {-5.4f, 1.22f}, {-5.05f, 1.05f},
             {-4.8f, 0.88f}},
        h.pommelS, h.wrapT);
    prof.clear();
    za::Vector<glm::vec2> grip;
    grip.pushBack(glm::vec2{-4.8f, 0.88f});
    for(int k = 0; k <= 8; k++)
    {
        grip.pushBack(glm::vec2{-4.6f + 4.2f * k / 8, k % 2 ? 0.9f : 0.97f});
    }
    grip.pushBack(glm::vec2{-0.32f, 0.95f});
    {
        const Patch strip{true, h.gripS[0], h.gripS[1], h.wrapT[0], h.wrapT[1]};
        lathe(m, from, dir, grip, 12, &strip);
    }
    part({{-0.32f, 0.95f}, {-0.2f, 1.45f}, {0.05f, 1.85f}, {1.55f, 1.85f}, {1.8f, 1.45f}, {1.95f, 1.08f}}, h.collarS,
        h.collarT);
    part({{1.95f, 1.08f}, {2.3f, 1.02f}, {top - 3.f, 1.06f}, {top - 0.8f, 1.2f}, {top + 0.6f, 1.2f}, {top + 0.62f, 0.f}},
        h.shaftS, h.wrapT);
}

// AD's head (its triangles beyond the cut), as AD has it; each hole closed (where AD's handle went in, and the open
// bases of its spikes).
[[nodiscard]] bool heavyHead(Mesh& m, const Source& src, const Heavy& h)
{
    za::Vector<int> keyed; // (AD's vertex: ours, for its place on the skin last met)
    keyed.resize(src.pos.size(), -1);
    const int firstTri = static_cast<int>(m.tri.size());
    for(const Source::Tri& t : src.tris)
    {
        if(src.pos[t.v[0]].x < h.cut || src.pos[t.v[1]].x < h.cut || src.pos[t.v[2]].x < h.cut)
        {
            continue;
        }
        for(int c = 0; c < 3; c++)
        {
            int& k = keyed[t.v[c]];
            if(k < 0 || m.uv[k] != glm::vec2{t.s[c], t.t[c]})
            {
                k = m.vert(src.pos[t.v[c]], -1, -1, glm::vec2{t.s[c], t.t[c]}, src.nrm[t.v[c]]);
            }
            m.tri.pushBack(k);
        }
    }
    const int headTris = (static_cast<int>(m.tri.size()) - firstTri) / 3;
    if(headTris < 8)
    {
        return false;
    }

    // Open edges (one triangle's, welded by place) chained into loops; each loop fanned from its middle.
    struct Edge
    {
        int from, to; // vertices
        bool used;
    };
    za::Vector<Edge> open;
    const auto same = [&](int a, int b) { return m.pos[a] == m.pos[b]; };
    for(int i = firstTri; i < static_cast<int>(m.tri.size()); i += 3)
    {
        for(int c = 0; c < 3; c++)
        {
            const int a = m.tri[i + c], b = m.tri[i + (c + 1) % 3];
            bool paired = false;
            for(int j = firstTri; j < static_cast<int>(m.tri.size()) && !paired; j += 3)
            {
                for(int d = 0; d < 3; d++)
                {
                    if(same(m.tri[j + d], b) && same(m.tri[j + (d + 1) % 3], a))
                    {
                        paired = true;
                        break;
                    }
                }
            }
            if(!paired)
            {
                open.pushBack(Edge{a, b, false});
            }
        }
    }
    for(size_t e = 0; e < open.size(); e++)
    {
        if(open[e].used)
        {
            continue;
        }
        za::Vector<int> loop;
        size_t at = e;
        while(!open[at].used)
        {
            open[at].used = true;
            loop.pushBack(static_cast<int>(at));
            for(size_t f = 0; f < open.size(); f++)
            {
                if(!open[f].used && same(open[f].from, open[at].to))
                {
                    at = f;
                    break;
                }
            }
        }
        if(loop.size() < 3)
        {
            continue;
        }
        glm::vec3 mid{0.f};
        glm::vec2 st{0.f};
        for(const int k : loop)
        {
            mid += m.pos[open[k].from];
            st += m.uv[open[k].from];
        }
        const float n = static_cast<float>(loop.size());
        const int c = m.vert(mid / n, m.groups++, -1, st / n);
        for(const int k : loop) // (the cap's edge the other way round: the MDL's winding kept)
        {
            m.tri.pushBack(open[k].to);
            m.tri.pushBack(open[k].from);
            m.tri.pushBack(c);
        }
    }
    return true;
}

// Each vertex's normal: given, or its triangles' (those of its group in its place).
void shade(Mesh& m)
{
    const size_t nv = m.pos.size();
    za::Vector<glm::vec3> acc;
    acc.resize(nv, glm::vec3{0.f});
    for(size_t i = 0; i + 2 < m.tri.size(); i += 3)
    {
        const int a = m.tri[i], b = m.tri[i + 1], c = m.tri[i + 2];
        const glm::vec3 n = glm::cross(m.pos[c] - m.pos[a], m.pos[b] - m.pos[a]); // (outward: MDL winding)
        acc[a] += n;
        acc[b] += n;
        acc[c] += n;
    }
    za::Vector<int> order;
    for(size_t v = 0; v < nv; v++)
    {
        if(m.group[v] >= 0)
        {
            order.pushBack(static_cast<int>(v));
        }
    }
    const auto less = [&](int x, int y) {
        if(m.group[x] != m.group[y])
        {
            return m.group[x] < m.group[y];
        }
        const glm::vec3 &p = m.pos[x], &q = m.pos[y];
        return p.x != q.x ? p.x < q.x : (p.y != q.y ? p.y < q.y : p.z < q.z);
    };
    std::sort(order.begin(), order.end(), less);
    for(size_t i = 0; i < order.size();)
    {
        size_t j = i + 1;
        while(j < order.size() && !less(order[i], order[j]) && !less(order[j], order[i]))
        {
            j++;
        }
        glm::vec3 sum{0.f};
        for(size_t k = i; k < j; k++)
        {
            sum += acc[order[k]];
        }
        const float len = glm::length(sum);
        for(size_t k = i; k < j; k++)
        {
            m.nrm[order[k]] = len > 1e-8f ? sum / len : glm::vec3{1.f, 0.f, 0.f};
        }
        i = j;
    }
}

// The barycentric weights of the point of triangle abc nearest p (Ericson, Real-Time Collision Detection 5.1.5).
[[nodiscard]] glm::vec3 nearestOn(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return {1.f, 0.f, 0.f};
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return {0.f, 1.f, 0.f};
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        const float v = d1 / (d1 - d3);
        return {1.f - v, v, 0.f};
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return {0.f, 0.f, 1.f};
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        const float w = d2 / (d2 - d6);
        return {1.f - w, 0.f, w};
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && d4 - d3 >= 0.f && d5 - d6 >= 0.f)
    {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return {0.f, 1.f - w, w};
    }
    const float denom = 1.f / (va + vb + vc);
    const float v = vb * denom, w = vc * denom;
    return {1.f - v - w, v, w};
}

// AD's triangle nearest a place facing so (of one island, unless -1), and the place's barycentric weights on it; -1 if
// none faces so.
[[nodiscard]] int nearestTri(const Source& src, const glm::vec3& p, const glm::vec3& n, int island, glm::vec3& bary)
{
    int best = -1;
    float bestScore = 1e30f;
    for(int pass = 0; pass < 2 && best < 0; pass++)
    {
        for(int i = 0; i < static_cast<int>(src.tris.size()); i++)
        {
            const Source::Tri& t = src.tris[i];
            const float facing = glm::dot(t.out, n);
            if((island >= 0 && t.island != island) || t.out == glm::vec3{0.f} || (pass == 0 && facing < 0.2f))
            {
                continue;
            }
            const glm::vec3 w = nearestOn(p, src.pos[t.v[0]], src.pos[t.v[1]], src.pos[t.v[2]]);
            const glm::vec3 q = src.pos[t.v[0]] * w.x + src.pos[t.v[1]] * w.y + src.pos[t.v[2]] * w.z;
            const float score = glm::length(p - q) - 0.1f * facing; // (ties: the one facing it more)
            if(score < bestScore)
            {
                bestScore = score;
                best = i;
                bary = w;
            }
        }
    }
    return best;
}

// Each patch's vertices skinned: a strip's from their place along and round it; the rest from AD's model's nearest
// triangle facing as they do, all of a patch from one island of AD's skin (no triangle across two places of it).
void skin(Mesh& m, const Source& src)
{
    for(int p = 0; p < static_cast<int>(m.patches.size()); p++)
    {
        Patch& pt = m.patches[p];
        if(pt.strip)
        {
            continue;
        }
        glm::vec3 mid{0.f}, n{0.f};
        int count = 0;
        for(size_t v = 0; v < m.pos.size(); v++)
        {
            if(m.patch[v] == p)
            {
                mid += m.look[v];
                n += m.nrm[v];
                count++;
            }
        }
        if(count == 0)
        {
            continue;
        }
        glm::vec3 bary;
        const int t = nearestTri(src, mid / static_cast<float>(count), glm::length(n) > 1e-6f ? glm::normalize(n) : n,
            -1, bary);
        pt.island = t >= 0 ? src.tris[t].island : -1;
    }
    for(size_t v = 0; v < m.pos.size(); v++)
    {
        if(m.patch[v] < 0)
        {
            continue;
        }
        const Patch& pt = m.patches[m.patch[v]];
        if(pt.strip)
        {
            m.uv[v] = glm::vec2{pt.s0 + (pt.s1 - pt.s0) * m.uv[v].x, pt.t0 + (pt.t1 - pt.t0) * m.uv[v].y};
            continue;
        }
        glm::vec3 w;
        const int t = nearestTri(src, m.look[v], m.nrm[v], pt.island, w);
        if(t >= 0)
        {
            const Source::Tri& tr = src.tris[t];
            m.uv[v] = glm::vec2{tr.s[0] * w.x + tr.s[1] * w.y + tr.s[2] * w.z, tr.t[0] * w.x + tr.t[1] * w.y + tr.t[2] * w.z};
        }
    }
}

// Edges welded by place: those of one triangle, of more than two; and the signed volume.
void check(const Mesh& m, Built& out)
{
    za::Vector<int> id;
    id.resize(m.pos.size(), -1);
    za::Vector<int> order;
    for(int v = 0; v < static_cast<int>(m.pos.size()); v++)
    {
        order.pushBack(v);
    }
    const auto less = [&](int x, int y) {
        const glm::vec3 &p = m.pos[x], &q = m.pos[y];
        return p.x != q.x ? p.x < q.x : (p.y != q.y ? p.y < q.y : p.z < q.z);
    };
    std::sort(order.begin(), order.end(), less);
    int ids = 0;
    for(size_t i = 0; i < order.size(); i++)
    {
        if(i > 0 && less(order[i - 1], order[i]))
        {
            ids++;
        }
        id[order[i]] = ids;
    }
    za::Vector<long long> edges;
    out.volume = 0.f;
    for(size_t i = 0; i + 2 < m.tri.size(); i += 3)
    {
        for(int c = 0; c < 3; c++)
        {
            const long long a = id[m.tri[i + c]], b = id[m.tri[i + (c + 1) % 3]];
            edges.pushBack(za::min(a, b) * 1000000 + za::max(a, b));
        }
        out.volume += glm::dot(m.pos[m.tri[i]], glm::cross(m.pos[m.tri[i + 1]], m.pos[m.tri[i + 2]]));
    }
    std::sort(edges.begin(), edges.end());
    out.openEdges = out.overEdges = 0;
    for(size_t i = 0; i < edges.size();)
    {
        size_t j = i + 1;
        while(j < edges.size() && edges[j] == edges[i])
        {
            j++;
        }
        out.openEdges += j - i == 1 ? 1 : 0;
        out.overEdges += j - i > 2 ? 1 : 0;
        i = j;
    }
}

// The death sergeant's flail (AD's release's w_blueflail.mdl: 384 vertices, 286 triangles): its handle's vertices
// (0..63, 207..212), its head's (the spiked ball 64..122 and its spikes to 206, the ring the chain holds 123..158 and
// 213..218); the chain (219..383) is drawn by the engine (vr_flail.cpp), from the handle's end to the head.
constexpr int flailVerts = 384, flailTris = 286;
[[nodiscard]] bool flailHandleVert(int v)
{
    return v < 64 || (v >= 207 && v <= 212);
}
[[nodiscard]] bool flailHeadVert(int v)
{
    return (v >= 64 && v < 207) || (v >= 213 && v <= 218);
}

// AD's triangles of the flail's handle (or its head) kept as they are, skinned as AD's.
[[nodiscard]] bool flailPart(Mesh& m, const Source& src, bool head)
{
    za::Vector<int> keyed;
    keyed.resize(src.pos.size(), -1);
    for(const Source::Tri& t : src.tris)
    {
        bool in = true;
        for(int c = 0; c < 3; c++)
        {
            in = in && (head ? flailHeadVert(t.v[c]) : flailHandleVert(t.v[c]));
        }
        if(!in)
        {
            continue;
        }
        for(int c = 0; c < 3; c++)
        {
            int& k = keyed[t.v[c]];
            if(k < 0 || m.uv[k] != glm::vec2{t.s[c], t.t[c]})
            {
                k = m.vert(src.pos[t.v[c]], -1, -1, glm::vec2{t.s[c], t.t[c]}, src.nrm[t.v[c]]);
            }
            m.tri.pushBack(k);
        }
    }
    return m.tri.size() >= 24;
}

// The flail's head's frame: its ball's middle at the origin, +x towards the ring the chain holds, at the size the
// held weapons are drawn (the knight sword's 2.6 times AD's, drawn at 0.34): drawn at scale 1 where the physics has it.
constexpr float flailHeadScale = swordScale * 0.34f;
[[nodiscard]] bool flailHeadFrame(const byte* src, size_t size, Frame& out, const char*& why)
{
    Frame raw;
    raw.grip = glm::vec3{0.f};
    raw.a = glm::vec3{1.f, 0.f, 0.f};
    raw.g = glm::vec3{0.f, 1.f, 0.f};
    raw.n = glm::vec3{0.f, 0.f, 1.f};
    raw.A = raw.a;
    raw.G = raw.g;
    raw.N = raw.n;
    Source s;
    if(!readSource(raw, src, size, s, why))
    {
        return false;
    }
    if(static_cast<int>(s.pos.size()) != flailVerts || static_cast<int>(s.tris.size()) != flailTris)
    {
        why = "not the flail AD's release has";
        return false;
    }
    glm::vec3 ball{0.f}, ring{0.f};
    int rings = 0;
    for(int v = 64; v < 123; v++)
    {
        ball += s.pos[v];
    }
    ball /= 59.f;
    for(int v = 123; v < 219; v++)
    {
        if(v < 159 || v >= 213)
        {
            ring += s.pos[v];
            rings++;
        }
    }
    ring /= static_cast<float>(rings);
    out = raw;
    out.grip = ball;
    out.a = glm::normalize(ring - ball);
    const glm::vec3 up{0.f, 0.f, 1.f};
    out.g = glm::normalize(glm::cross(up, out.a));
    out.n = glm::cross(out.a, out.g);
    out.base = glm::vec3{0.f};
    out.k = flailHeadScale;
    return true;
}

// AD's weapon made anew (its skins and, on the heavy ones, its head kept), laid as the knight's sword; `flailHead`: the
// flail's head alone (flailHeadFrame).
[[nodiscard]] bool build(const Weapon& w, const byte* src, size_t size, Built& out, const char*& why, bool flailHead = false)
{
    Frame fr = frameOf(w);
    if(flailHead && !flailHeadFrame(src, size, fr, why))
    {
        return false;
    }
    Source s;
    if(!readSource(fr, src, size, s, why))
    {
        return false;
    }
    Mesh m;
    size_t headVerts = 0; // (the head's vertices come first)
    if(w.flail)
    {
        if(static_cast<int>(s.pos.size()) != flailVerts || static_cast<int>(s.tris.size()) != flailTris)
        {
            why = "not the flail AD's release has";
            return false;
        }
        if(!flailPart(m, s, flailHead))
        {
            why = "its part not found";
            return false;
        }
    }
    else if(w.sword)
    {
        swordHandle(m, *w.sword);
        guard(m, *w.sword);
        blade(m, *w.sword);
    }
    else
    {
        if(!heavyHead(m, s, *w.heavy))
        {
            why = "its head not found";
            return false;
        }
        headVerts = m.pos.size();
        heavyHandle(m, *w.heavy);
    }
    shade(m);
    skin(m, s);
    check(m, out);
    const int nv = static_cast<int>(m.pos.size()), nt = static_cast<int>(m.tri.size() / 3);
    if(nv > MAXALIASVERTS_QS || nt > MAXALIASTRIS_QS)
    {
        why = "too big";
        return false;
    }

    // The places in the knight sword's frame, and the box they need.
    za::Vector<glm::vec3> pts;
    out.lo = glm::vec3{1e30f};
    out.hi = glm::vec3{-1e30f};
    float radius = 0.f;
    for(const glm::vec3& l : m.pos)
    {
        const glm::vec3 p = fr.place(l);
        pts.pushBack(p);
        out.lo = glm::min(out.lo, p);
        out.hi = glm::max(out.hi, p);
        radius = za::max(radius, glm::length(p));
    }
    const glm::vec3 scale = glm::max((out.hi - out.lo) / 255.f, glm::vec3{1e-4f});
    const glm::vec3 origin = out.lo;

    const size_t skins = s.skinsEnd - headerSize;
    out.file.clear();
    out.file.resize(headerSize + skins + static_cast<size_t>(nv) * 12 + static_cast<size_t>(nt) * 16 + 4 + 24 +
                        static_cast<size_t>(nv) * 4,
        byte{0});
    byte* f = out.file.data();
    memcpy(f, src, headerSize);
    memcpy(f + headerSize, src + headerSize, skins);
    const auto setf = [&](size_t off, float v) {
        const float le = LittleFloat(v);
        memcpy(f + off, &le, 4);
    };
    const auto seti = [&](size_t off, int v) {
        const int le = LittleLong(v);
        memcpy(f + off, &le, 4);
    };
    const glm::vec3 eye = fr.place(fr.local(glm::vec3{rdf(src, 36), rdf(src, 40), rdf(src, 44)}));
    for(int i = 0; i < 3; i++)
    {
        setf(8 + static_cast<size_t>(i) * 4, scale[i]);
        setf(20 + static_cast<size_t>(i) * 4, origin[i]);
        setf(36 + static_cast<size_t>(i) * 4, eye[i]);
    }
    setf(32, radius);
    seti(60, nv);
    seti(64, nt);
    seti(68, 1);
    size_t at = s.skinsEnd;
    for(int v = 0; v < nv; v++, at += 12)
    {
        seti(at, 0);
        seti(at + 4, za::max(0, za::min(s.skinW, static_cast<int>(floorf(m.uv[v].x + 0.5f)))));
        seti(at + 8, za::max(0, za::min(s.skinH, static_cast<int>(floorf(m.uv[v].y + 0.5f)))));
    }
    for(int t = 0; t < nt; t++, at += 16)
    {
        seti(at, 1);
        for(int c = 0; c < 3; c++)
        {
            seti(at + 4 + static_cast<size_t>(c) * 4, m.tri[static_cast<size_t>(t) * 3 + c]);
        }
    }
    seti(at, 0); // (one frame, a single pose: AD's first's name)
    memcpy(f + at + 12, src + s.pose + 8, 16);
    byte* lo = f + at + 4;
    byte* hi = f + at + 8;
    at += 28;
    lo[0] = lo[1] = lo[2] = 255;
    const glm::vec3 A = fr.A;
    float farthest = -1e30f;
    for(const glm::vec3& p : pts)
    {
        farthest = za::max(farthest, glm::dot(p - fr.base, A));
    }
    out.tip = glm::vec3{0.f};
    int tipCount = 0;
    for(int v = 0; v < nv; v++, at += 4)
    {
        const glm::vec3& p = pts[v];
        for(int i = 0; i < 3; i++)
        {
            const float q = (p[i] - origin[i]) / scale[i];
            f[at + i] = static_cast<byte>(za::max(0.f, za::min(255.f, q + 0.5f)));
            lo[i] = za::min(lo[i], f[at + i]);
            hi[i] = za::max(hi[i], f[at + i]);
        }
        f[at + 3] = nearestNormal(fr.dir(m.nrm[v]));
        if(glm::dot(p - fr.base, A) > farthest - (w.flail ? 0.5f : 2.f))
        {
            out.tip += p;
            tipCount++;
        }
    }
    out.tip /= static_cast<float>(za::max(tipCount, 1));
    out.spine.clear();
    for(const glm::vec3& l : m.spine)
    {
        out.spine.pushBack(fr.place(l));
    }
    out.head.clear();
    for(size_t v = 0; v < headVerts && v < pts.size(); v++)
    {
        out.head.pushBack(pts[v]);
    }
    out.verts = nv;
    out.tris = nt;
    return true;
}

[[nodiscard]] bool buildNamed(const char* name, unsigned int* path_id, Built& out, const char*& why)
{
    char base[MAX_QPATH];
    const bool flailHead = flailHeadOf(name, base, sizeof(base));
    if(!flailHead && !parse(name, base, sizeof(base)))
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
    const bool ok = build(*w, src, size, out, why, flailHead);
    VR_HeapFree(src);
    if(path_id)
    {
        *path_id = pathId;
    }
    return ok;
}

// A sword's bend (QC VR_Melee_At: the melee's points on a curved blade): its spine's way off the straight line from the
// hand (swordHand) to its tip (Built::tip, the slot's MuzzleAnchor), at bendSamples places evenly along that line, as
// shares of the line's length: `off` along the line's normal in the blade's plane (P: Y x the line, Y the model's +y,
// the blade's flat's normal), `side` along Y. Each weapon's made once a game folder (AD's data).
constexpr int bendSamples = 17;

struct Bend
{
    bool made{false};
    bool curved{false};
    glm::vec3 hand{0.f}, line{0.f}, normal{0.f};
    float length{0.f};
    float off[bendSamples]{};
    float side[bendSamples]{};
    // A hammer's or mace's head (QC admeleehead): the box round its vertices in the same frame and shares (x along the
    // line, y along P, z along Y; z made even about the line: the off hand's model is mirrored across Y).
    bool head{false};
    glm::vec3 headLo{0.f}, headHi{0.f};
};

constexpr size_t weaponCount = sizeof(weapons) / sizeof(weapons[0]);
Bend bends[weaponCount];
char bendsGameDir[MAX_OSPATH]; // the game folder they were made in

[[nodiscard]] const Bend* bendOf(size_t w)
{
    if(strcmp(bendsGameDir, com_gamedir) != 0)
    {
        q_strlcpy(bendsGameDir, com_gamedir, sizeof(bendsGameDir));
        for(Bend& b : bends)
        {
            b = Bend{};
        }
    }
    Bend& bd = bends[w];
    if(bd.made)
    {
        return &bd;
    }
    bd.made = true;
    const Weapon& wp = weapons[w];
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s%s", wp.model, suffix);
    Built b;
    const char* why = "";
    if(!buildNamed(name, nullptr, b, why) || (wp.sword && b.spine.size() < 2) || (wp.heavy && b.head.empty()))
    {
        return &bd;
    }
    const glm::vec3 hand = swordHand;
    const float len = glm::length(b.tip - hand);
    if(len < 1.f)
    {
        return &bd;
    }
    const glm::vec3 a = (b.tip - hand) / len;
    const glm::vec3 y{0.f, 1.f, 0.f};
    const glm::vec3 p = glm::normalize(glm::cross(y, a));
    bd.hand = hand;
    bd.line = a;
    bd.normal = p;
    bd.length = len;
    if(wp.heavy)
    {
        glm::vec3 lo{1e30f}, hi{-1e30f};
        for(const glm::vec3& q : b.head)
        {
            const glm::vec3 d = (q - hand) / len;
            const glm::vec3 s{glm::dot(d, a), glm::dot(d, p), glm::dot(d, y)};
            lo = glm::min(lo, s);
            hi = glm::max(hi, s);
        }
        const float depth = za::max(fabsf(lo.z), fabsf(hi.z));
        lo.z = -depth;
        hi.z = depth;
        bd.head = true;
        bd.headLo = lo;
        bd.headHi = hi;
        Con_DPrintf("AD melee model: %s: head %.3f..%.3f along its line, %.3f..%.3f across, %.3f deep (of %.1f units)\n",
            name, lo.x, hi.x, lo.y, hi.y, depth, len);
        return &bd;
    }
    // The spine as (along, off, side) shares, from the hand (on the line) out; sampled by its place along the line.
    za::Vector<glm::vec3> s;
    s.pushBack(glm::vec3{0.f});
    for(const glm::vec3& q : b.spine)
    {
        const glm::vec3 d = (q - hand) / len;
        if(glm::dot(d, a) > s.back().x)
        {
            s.pushBack(glm::vec3{glm::dot(d, a), glm::dot(d, p), glm::dot(d, y)});
        }
    }
    float most = 0.f;
    size_t seg = 0;
    for(int k = 0; k < bendSamples; k++)
    {
        const float f = static_cast<float>(k) / (bendSamples - 1);
        while(seg + 2 < s.size() && s[seg + 1].x < f)
        {
            seg++;
        }
        const glm::vec3 &u = s[seg], &v = s[za::min(seg + 1, s.size() - 1)];
        const float t = v.x > u.x ? za::max(0.f, za::min(1.f, (f - u.x) / (v.x - u.x))) : 0.f;
        bd.off[k] = u.y + (v.y - u.y) * t;
        bd.side[k] = u.z + (v.z - u.z) * t;
        most = za::max(most, fabsf(bd.off[k]));
    }
    bd.curved = most * len > 0.5f; // (half a unit off its line anywhere: the straight swords are not)
    Con_DPrintf("AD melee model: %s: blade bent %.2f units off its line at most\n", name, most * len);
    return &bd;
}

[[nodiscard]] const Bend* bendOfKind(int kind)
{
    return kind >= 1 && kind <= static_cast<int>(weaponCount) ? bendOf(static_cast<size_t>(kind - 1)) : nullptr;
}

} // namespace

glm::vec2 bend(int kind, float f)
{
    const Bend* b = bendOfKind(kind);
    if(!b || !b->curved)
    {
        return glm::vec2{0.f};
    }
    const float x = za::max(0.f, za::min(1.f, f)) * (bendSamples - 1);
    const int i = za::min(static_cast<int>(x), bendSamples - 2);
    const float t = x - static_cast<float>(i);
    return {b->off[i] + (b->off[i + 1] - b->off[i]) * t, b->side[i] + (b->side[i + 1] - b->side[i]) * t};
}

bool head(int kind, glm::vec3& lo, glm::vec3& hi)
{
    const Bend* b = bendOfKind(kind);
    if(!b || !b->head)
    {
        return false;
    }
    lo = b->headLo;
    hi = b->headHi;
    return true;
}

bool spineAt(const char* model, float f, glm::vec3& out, glm::vec3& line)
{
    for(size_t w = 0; w < weaponCount; w++)
    {
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%s", weapons[w].model, suffix);
        if(strcmp(name, model) != 0)
        {
            continue;
        }
        const Bend* b = bendOf(w);
        if(!b->length || b->head)
        {
            return false;
        }
        const glm::vec2 k = bend(static_cast<int>(w) + 1, f);
        line = b->hand + b->line * (f * b->length);
        out = line + (b->normal * k.x + glm::vec3{0.f, 1.f, 0.f} * k.y) * b->length;
        return true;
    }
    return false;
}

byte* derivedFile(const char* name, unsigned int* path_id)
{
    const char* at = strstr(name, suffix);
    char base[MAX_QPATH];
    if((!at || at[strlen(suffix)] != '\0') && !flailHeadOf(name, base, sizeof(base)))
    {
        return nullptr;
    }
    Built b;
    const char* why = "";
    if(!buildNamed(name, path_id, b, why))
    {
        if(flailHeadOf(name, base, sizeof(base)))
        {
            Con_DPrintf("AD melee model: %s: %s: none\n", name, why); // (the flail's head not drawn: vr_flail.cpp)
            return nullptr;
        }
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

// vr_admelee_models [write]: each model's stats (write: its made file into the game folder, admelee_<n>.mdl, to look
// at in a model viewer or a script).
void info_f()
{
    const bool write = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "write");
    for(const Weapon& w : weapons)
    {
        const int index = static_cast<int>(&w - weapons);
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%s", w.model, suffix);
        Built b;
        const char* why = "";
        if(!buildNamed(name, nullptr, b, why))
        {
            Con_Printf("%s: %s\n", name, why);
            continue;
        }
        Con_Printf("%s: %d triangles, %d vertices, %d open edges, %d edges of 3+ triangles, %s, box %.2f %.2f %.2f .. "
                   "%.2f %.2f %.2f, tip %.4f %.4f %.4f\n",
            name, b.tris, b.verts, b.openEdges, b.overEdges, b.volume < 0.f ? "outward" : "INSIDE OUT", b.lo.x, b.lo.y,
            b.lo.z, b.hi.x, b.hi.y, b.hi.z, b.tip.x, b.tip.y, b.tip.z);
        if(write)
        {
            char out[MAX_QPATH];
            q_snprintf(out, sizeof(out), "admelee_%d.mdl", index);
            COM_WriteFile(out, b.file.data(), static_cast<int>(b.file.size()));
        }
    }
}

} // namespace qvr::admelee
