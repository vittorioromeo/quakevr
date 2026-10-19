// vr_adgunart.cpp -- see vr_adguns.hpp: the AD soldiers' guns as variants of our burst rifle (v_gruntgun.mdl), each
// with its own add-ons and skin, made as the model loads.
//
// The rifle's triangles, skin coordinates and frame 0 are kept as they are (its grip, frame and scale_origin: slot 21's
// weapon settings hold every variant); the add-ons are built in the same model space (+x the barrel, towards the muzzle
// at x 33.1; +z up; +y the gun's left), never past the rifle's packing corner (x -29.9, y -6.17, z -4.6: the corner the
// settings scale about), never past its muzzle. The skin is the rifle's 256 x 260 with 160 rows below for the add-ons
// (and, for the grenade launcher, our v_rock.mdl's skin at half size): the rifle's own texels recoloured per gun (their
// light kept), decals painted where its sides face (each texel's place on the model found through its triangle), the
// add-ons' surfaces painted as they are laid out.

#include "vr_adguns.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::adguns
{
namespace
{

constexpr const char* baseModel = "progs/v_gruntgun.mdl";
constexpr const char* launcherModel = "progs/v_rock.mdl"; // our grenade launcher (its barrel and drum)
constexpr size_t mdlHeader = 84;
constexpr int extraRows = 160;
constexpr float pi = 3.14159265f;
const glm::vec3 corner{-29.9f, -6.170675f, -4.6f}; // v_gruntgun.mdl's scale_origin
constexpr float muzzleX = 33.1f;                   // the add-ons stop short of the muzzle (slot 21's MuzzleAnchor x 33.15)

// The fuelled guns' canister under the receiver, ahead of the grip; the hose's inlet under the receiver between the grip
// (x -3.4 .. 2.6) and the stock (back from x -14), its pipe's end and the way the hose leaves it (down and back); the
// pyro's pilot light's tip.
const glm::vec3 canisterBack{5.f, 0.f, 3.4f}, canisterFront{12.f, 0.f, 3.4f};
constexpr float canisterRadius = 1.9f;
const glm::vec3 inletBoss{-11.5f, 0.f, 4.25f}; // (Vittorio, 2026-10-19: back near the stock; the underside z 4.4 there)
const glm::vec3 inletAt{-12.4f, 0.f, 2.25f};
const glm::vec3 inletDir{-0.42f, 0.f, -0.91f};
const glm::vec3 pilotAt{31.2f, 0.f, 5.6f};

// Each variant's firing feel (vr_adguns.hpp FireFeel), in WID order. Slot 21's recoil (1, 0.12 s; a kick tops out at 2.5)
// and flash (0.06 s) scaled:
// the launchers kick hard and long and blast smoke and sparks (the rocket's back-blast out of its exhaust bell, x -5.6
// z 17.5), the shotgun kicks and flashes wide, the plasma guns kick little, their flashes and lights their colour.
constexpr const char* flashBase = "progs/vr_muzzleflash.mdl";
constexpr const char* plasmaFlash = "progs/vr_muzzleflash.mdl#adf2";
constexpr const char* centurionFlash = "progs/vr_muzzleflash.mdl#adf5";
const glm::vec3 plasmaColour{0.4f, 0.62f, 1.f}, centurionColour{0.3f, 0.9f, 1.f};
const FireFeel fireFeels[variantCount] = {
    {2.5f, 2.5f, 1.9f, 1.6f, nullptr, glm::vec3{1.f, 0.55f, 0.25f}, 1.5f, FireFeel::Rocket, glm::vec3{1.f, 0.6f, 0.2f}, true,
        glm::vec3{-5.6f, 0.f, 17.5f}, glm::vec3{-1.f, 0.f, 0.f}},
    {2.f, 2.f, 1.35f, 1.3f, nullptr, glm::vec3{1.f, 0.6f, 0.3f}, 1.2f, FireFeel::Grenade, glm::vec3{1.f, 0.7f, 0.35f}},
    {0.8f, 1.f, 0.9f, 1.f, plasmaFlash, plasmaColour, 1.f, FireFeel::Electric, plasmaColour},
    {1.8f, 1.6f, 1.5f, 1.1f, nullptr, glm::vec3{1.f, 0.8f, 0.55f}, 1.2f, FireFeel::Shotgun, glm::vec3{1.f, 0.75f, 0.4f}},
    {1.f, 1.1f, 1.f, 1.f, centurionFlash, centurionColour, 1.1f, FireFeel::Electric, centurionColour}, // (the eliminator's: the centurion's)
    {1.f, 1.1f, 1.f, 1.f, centurionFlash, centurionColour, 1.1f, FireFeel::Electric, centurionColour},
    {},
    {},
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
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
}

void putf(za::Vector<byte>& out, float v)
{
    const float le = LittleFloat(v);
    out.emplaceBackRange(reinterpret_cast<const byte*>(&le), 4);
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

// An alias model's file read: its header's numbers, its first skin, where its vertices, triangles and frame 0 are.
struct Mdl
{
    const byte* src{nullptr};
    size_t size{0};
    glm::vec3 scale{1.f}, origin{0.f};
    int skinW{0}, skinH{0}, numVerts{0}, numTris{0}, flags{0};
    float sizeField{0.f};
    const byte* skin{nullptr};
    size_t stAt{0}, triAt{0}, poseAt{0};

    [[nodiscard]] glm::vec3 pos(int v) const
    {
        const byte* p = src + poseAt + static_cast<size_t>(v) * 4;
        return glm::vec3{p[0] * scale.x + origin.x, p[1] * scale.y + origin.y, p[2] * scale.z + origin.z};
    }
    [[nodiscard]] glm::vec3 normal(int v) const
    {
        const byte n = src[poseAt + static_cast<size_t>(v) * 4 + 3];
        return n < NUMVERTEXNORMALS ? glm::vec3{r_avertexnormals[n][0], r_avertexnormals[n][1], r_avertexnormals[n][2]}
                                    : glm::vec3{0.f, 0.f, 1.f};
    }
    [[nodiscard]] int tri(int t, int k) const { return rd(src, triAt + static_cast<size_t>(t) * 16 + static_cast<size_t>(k) * 4); }
    [[nodiscard]] int st(int v, int k) const { return rd(src, stAt + static_cast<size_t>(v) * 12 + static_cast<size_t>(k) * 4); }
};

[[nodiscard]] bool readMdl(const byte* src, size_t size, Mdl& m, const char*& why)
{
    if(size < mdlHeader || rd(src, 0) != IDPOLYHEADER || rd(src, 4) != ALIAS_VERSION)
    {
        why = "not an alias model";
        return false;
    }
    m.src = src;
    m.size = size;
    m.scale = {rdf(src, 8), rdf(src, 12), rdf(src, 16)};
    m.origin = {rdf(src, 20), rdf(src, 24), rdf(src, 28)};
    const int numSkins = rd(src, 48);
    m.skinW = rd(src, 52);
    m.skinH = rd(src, 56);
    m.numVerts = rd(src, 60);
    m.numTris = rd(src, 64);
    const int numFrames = rd(src, 68);
    m.flags = rd(src, 76);
    m.sizeField = rdf(src, 80);
    if(numSkins < 1 || m.skinW <= 0 || m.skinH <= 0 || m.numVerts < 3 || m.numTris < 1 || numFrames < 1)
    {
        why = "an empty model";
        return false;
    }
    const size_t skinBytes = static_cast<size_t>(m.skinW) * static_cast<size_t>(m.skinH);
    size_t at = mdlHeader;
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 8 > size)
        {
            why = "truncated (skins)";
            return false;
        }
        const byte* picture;
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
        if(s == 0)
        {
            m.skin = picture;
        }
    }
    m.stAt = at;
    m.triAt = at + static_cast<size_t>(m.numVerts) * 12;
    at = m.triAt + static_cast<size_t>(m.numTris) * 16;
    // Frame 0 (a single frame or a group's first).
    if(at + 4 > size)
    {
        why = "truncated (frames)";
        return false;
    }
    at += rd(src, at) == 0 ? 4 : 4 + 4 + 8 + static_cast<size_t>(za::max(rd(src, at + 4), 1)) * 4;
    m.poseAt = at + 24;
    if(m.poseAt + static_cast<size_t>(m.numVerts) * 4 > size || m.skin + skinBytes > src + size)
    {
        why = "truncated (vertices)";
        return false;
    }
    return true;
}

// The mesh made: vertices (model units) with their texels, triangles.
struct Vert
{
    glm::vec3 p{0.f}, n{0.f, 0.f, 1.f};
    int s{0}, t{0}, seam{0};
};
struct Tri
{
    int front{1};
    int v[3]{0, 0, 0};
    bool inner{false}; // an inner wall's (facing a part's axis: meshStats)
};
struct MeshStats
{
    int tris{0}, open{0}, doubled{0}, inward{0}, inner{0};
    bool verbose{false}; // (asked: each open or doubled edge printed)
};
struct Rect
{
    int x{0}, y{0}, w{1}, h{1};
};

// A texel's paint: its colour (0..1), lit by the model's light or fullbright (glowing), or nothing (left as it is).
struct Paint
{
    bool on{false};
    glm::vec3 rgb{0.f};
    bool bright{false};
};

[[nodiscard]] Paint paint(const glm::vec3& rgb, bool bright = false)
{
    return Paint{true, rgb, bright};
}

[[nodiscard]] float hash2(int x, int y)
{
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xffffu) / 65535.f; // 0..1
}

[[nodiscard]] float lum(const glm::vec3& c)
{
    return 0.3f * c.r + 0.59f * c.g + 0.11f * c.b;
}

[[nodiscard]] glm::vec3 palette(int i)
{
    const unsigned c = d_8to24table[i & 255];
    return glm::vec3{static_cast<float>(c & 255u), static_cast<float>((c >> 8) & 255u), static_cast<float>((c >> 16) & 255u)} / 255.f;
}

// The colours of the colours in the palette: the nearest of the lit ones (0..223) or the fullbright ones (224..254; one
// too far from it: the lit one), remembered for the build (15-bit colour).
class Quantizer
{
public:
    Quantizer() : memo_(65536, static_cast<short>(-1)) {}

    [[nodiscard]] byte operator()(const glm::vec3& c, bool bright)
    {
        const glm::vec3 k = glm::clamp(c, glm::vec3{0.f}, glm::vec3{1.f});
        const int key = (static_cast<int>(k.r * 31.f + 0.5f) << 10) | (static_cast<int>(k.g * 31.f + 0.5f) << 5) |
                        static_cast<int>(k.b * 31.f + 0.5f) | (bright ? 32768 : 0);
        short& m = memo_[static_cast<za::SizeT>(key)];
        if(m < 0)
        {
            m = static_cast<short>(search(k, bright));
        }
        return static_cast<byte>(m);
    }

private:
    [[nodiscard]] static int search(const glm::vec3& c, bool bright)
    {
        const auto best = [&](int from, int to, float& dist) {
            int pick = from;
            dist = 1e9f;
            for(int i = from; i <= to; i++)
            {
                const glm::vec3 d = palette(i) - c;
                const float e = d.r * d.r * 0.3f + d.g * d.g * 0.59f + d.b * d.b * 0.11f;
                if(e < dist)
                {
                    dist = e;
                    pick = i;
                }
            }
            return pick;
        };
        float dLit, dBright;
        const int lit = best(0, 223, dLit);
        if(!bright)
        {
            return lit;
        }
        const int glow = best(224, 254, dBright);
        return dBright < 0.03f ? glow : lit;
    }

    za::Vector<short> memo_;
};

// The fullbright colour (224..254) nearest `c`, however far (a flash's colours glow).
[[nodiscard]] byte nearestBright(const glm::vec3& c)
{
    int pick = 254;
    float dist = 1e9f;
    for(int i = 224; i <= 254; i++)
    {
        const glm::vec3 d = palette(i) - c;
        const float e = d.r * d.r * 0.3f + d.g * d.g * 0.59f + d.b * d.b * 0.11f;
        if(e < dist)
        {
            dist = e;
            pick = i;
        }
    }
    return static_cast<byte>(pick);
}

// What is made: the mesh, the skin, the atlas of the add-ons' rectangles in the rows below the rifle's.
struct Build
{
    za::Vector<Vert> verts;
    za::Vector<Tri> tris;
    int baseTris{0};
    int skinW{0}, skinH{0}, baseH{0};
    za::Vector<byte> skin;
    Quantizer quantize;
    float winding{1.f}; // the rifle's: its triangles' corners' order (+1: a, b, c turn about the outward normal)
    int atlasX{0}, atlasY{0}, atlasRow{0};
    bool inner{false}; // the triangles made now are an inner wall's (Tri::inner)

    [[nodiscard]] bool alloc(int w, int h, Rect& r)
    {
        w = za::min(w, skinW);
        if(atlasX + w > skinW)
        {
            atlasX = 0;
            atlasY += atlasRow;
            atlasRow = 0;
        }
        if(atlasY + h > skinH)
        {
            r = Rect{0, skinH - 1, 1, 1}; // (out of room: one texel; never with these guns)
            return false;
        }
        r = Rect{atlasX, atlasY, w, h};
        atlasX += w;
        atlasRow = za::max(atlasRow, h);
        return true;
    }

    void set(int x, int y, const Paint& p, float shade)
    {
        if(x < 0 || y < 0 || x >= skinW || y >= skinH || !p.on)
        {
            return;
        }
        skin[static_cast<za::SizeT>(y * skinW + x)] = quantize(p.bright ? p.rgb : p.rgb * shade, p.bright);
    }

    int vertex(const glm::vec3& p, const glm::vec3& n, float s, float t)
    {
        Vert v;
        v.p = p;
        v.n = glm::length(n) > 1e-6f ? glm::normalize(n) : glm::vec3{0.f, 0.f, 1.f};
        v.s = za::clamp(static_cast<int>(s + 0.5f), 0, skinW - 1);
        v.t = za::clamp(static_cast<int>(t + 0.5f), 0, skinH - 1);
        verts.pushBack(v);
        return static_cast<int>(verts.size()) - 1;
    }

    // A triangle facing `out` (outward), its corners in the rifle's order.
    void tri(int a, int b, int c, const glm::vec3& out)
    {
        const glm::vec3& pa = verts[static_cast<za::SizeT>(a)].p;
        const glm::vec3 g = glm::cross(verts[static_cast<za::SizeT>(b)].p - pa, verts[static_cast<za::SizeT>(c)].p - pa);
        Tri t;
        const bool flip = glm::dot(g, out) * winding < 0.f;
        t.v[0] = a;
        t.v[1] = flip ? c : b;
        t.v[2] = flip ? b : c;
        t.inner = inner;
        tris.pushBack(t);
    }
};

// The rect's texels painted by `fn(u, v)` (u across, v down the rect, in the units given for its width and height), each
// shaded a little (wear).
template <typename F>
void paintRect(Build& b, const Rect& r, float uLen, float vLen, F&& fn)
{
    for(int y = 0; y < r.h; y++)
    {
        for(int x = 0; x < r.w; x++)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(r.w) * uLen;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(r.h) * vLen;
            b.set(r.x + x, r.y + y, fn(u, v), 0.9f + 0.18f * hash2(r.x + x, r.y + y));
        }
    }
}

// A frame across a line's direction.
void across(const glm::vec3& d, glm::vec3& e1, glm::vec3& e2)
{
    const glm::vec3 ref = za::abs(d.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    e1 = glm::normalize(glm::cross(ref, d));
    e2 = glm::cross(d, e1);
}

// A tube from a (radius ra) to b (rb), `sides` round, its surface the rect (u round it, v along it), closed at the ends
// asked for (flat caps in the rect's first and last rows). `inside`: its inner wall (facing the axis: a bore, a bell's
// inside).
void tube(Build& b, const glm::vec3& a, const glm::vec3& c, float ra, float rc, int sides, const Rect& r, bool capA, bool capC,
    bool inside = false)
{
    const glm::vec3 d = glm::normalize(c - a);
    const float len = glm::length(c - a);
    glm::vec3 e1, e2;
    across(d, e1, e2);
    const float slope = (ra - rc) / za::max(len, 1e-3f);
    const float flip = inside ? -1.f : 1.f;
    const int first = static_cast<int>(b.verts.size());
    for(int end = 0; end < 2; end++)
    {
        const glm::vec3 o = end ? c : a;
        const float rr = end ? rc : ra;
        for(int k = 0; k <= sides; k++)
        {
            const float ang = 2.f * pi * static_cast<float>(k) / static_cast<float>(sides);
            const glm::vec3 radial = e1 * cosf(ang) + e2 * sinf(ang);
            b.vertex(o + radial * rr, (radial + d * slope) * flip, static_cast<float>(r.x) + static_cast<float>(r.w - 1) * static_cast<float>(k) / static_cast<float>(sides),
                static_cast<float>(r.y) + (end ? static_cast<float>(r.h - 1) : 0.f));
        }
    }
    b.inner = inside;
    for(int k = 0; k < sides; k++)
    {
        const int a0 = first + k, a1 = first + k + 1, c0 = first + sides + 1 + k, c1 = first + sides + 2 + k;
        const float ang = 2.f * pi * (static_cast<float>(k) + 0.5f) / static_cast<float>(sides);
        const glm::vec3 out = (e1 * cosf(ang) + e2 * sinf(ang)) * flip;
        b.tri(a0, a1, c1, out);
        b.tri(a0, c1, c0, out);
    }
    b.inner = false;
    for(int end = 0; end < 2; end++)
    {
        if(!(end ? capC : capA))
        {
            continue;
        }
        const glm::vec3 o = end ? c : a;
        const float rr = end ? rc : ra;
        const glm::vec3 out = end ? d : -d;
        const float t = static_cast<float>(r.y) + (end ? static_cast<float>(r.h - 1) : 0.f);
        const int mid = b.vertex(o, out, static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f, t);
        const int ring = static_cast<int>(b.verts.size());
        for(int k = 0; k <= sides; k++)
        {
            const float ang = 2.f * pi * static_cast<float>(k) / static_cast<float>(sides);
            b.vertex(o + (e1 * cosf(ang) + e2 * sinf(ang)) * rr, out,
                static_cast<float>(r.x) + static_cast<float>(r.w - 1) * static_cast<float>(k) / static_cast<float>(sides), t);
        }
        for(int k = 0; k < sides; k++)
        {
            b.tri(mid, ring + k, ring + k + 1, out);
        }
    }
}

// A part turned about the line from `a` along `d` (a unit direction): its profile's corners (s along the line, r out
// from it) joined in order, each segment a band round it (a disc's fan where r is 0) painted with its first corner's rect
// (u round it, v along the segment). Each faces the profile's left: a profile from the line out, along and back in to
// it is a closed shell, outside and in (a hollow tube's outer wall, lips and bore all facing out of its metal: seen from
// anywhere, inside or out, never through).
struct Turn
{
    float s{0.f}, r{0.f};
    const Rect* rect{nullptr};
};
void lathe(Build& b, const glm::vec3& a, const glm::vec3& d, const Turn* profile, int n, int sides)
{
    glm::vec3 e1, e2;
    across(d, e1, e2);
    for(int i = 0; i + 1 < n; i++)
    {
        const Turn& p0 = profile[i];
        const Turn& p1 = profile[i + 1];
        const float ds = p1.s - p0.s, dr = p1.r - p0.r, len = sqrtf(ds * ds + dr * dr);
        if(len < 1e-4f || !p0.rect)
        {
            continue;
        }
        const Rect& r = *p0.rect;
        const float ns = -dr / len, nr = ds / len; // (the profile's left)
        const int first = static_cast<int>(b.verts.size());
        for(int end = 0; end < 2; end++)
        {
            const Turn& p = end ? p1 : p0;
            for(int k = 0; k <= sides; k++)
            {
                const float ang = 2.f * pi * static_cast<float>(k) / static_cast<float>(sides);
                const glm::vec3 radial = e1 * cosf(ang) + e2 * sinf(ang);
                b.vertex(a + d * p.s + radial * p.r, d * ns + radial * nr,
                    static_cast<float>(r.x) + static_cast<float>(r.w - 1) * static_cast<float>(k) / static_cast<float>(sides),
                    static_cast<float>(r.y) + (end ? static_cast<float>(r.h - 1) : 0.f));
            }
        }
        b.inner = nr < -0.5f;
        for(int k = 0; k < sides; k++)
        {
            const int a0 = first + k, a1 = first + k + 1, c0 = first + sides + 1 + k, c1 = first + sides + 2 + k;
            const float ang = 2.f * pi * (static_cast<float>(k) + 0.5f) / static_cast<float>(sides);
            const glm::vec3 out = d * ns + (e1 * cosf(ang) + e2 * sinf(ang)) * nr;
            if(p0.r > 1e-4f)
            {
                b.tri(a0, a1, c1, out);
            }
            if(p1.r > 1e-4f)
            {
                b.tri(a0, c1, c0, out);
            }
        }
        b.inner = false;
    }
}

// The rifle's own triangles wholly inside the cylinder round the line y 0, z `z` from x `x0` to `x1` (radius `r`)
// dropped: its barrel inside a launcher's barrel (not seen down its bore).
void dropRifleInside(Build& b, float x0, float x1, float z, float r)
{
    za::Vector<Tri> kept;
    int base = 0;
    for(za::SizeT i = 0; i < b.tris.size(); i++)
    {
        const Tri& t = b.tris[i];
        bool inside = static_cast<int>(i) < b.baseTris;
        for(int c = 0; c < 3 && inside; c++)
        {
            const glm::vec3& p = b.verts[static_cast<za::SizeT>(t.v[c])].p;
            inside = p.x >= x0 && p.x <= x1 && p.y * p.y + (p.z - z) * (p.z - z) <= r * r;
        }
        if(!inside)
        {
            kept.pushBack(t);
            base += static_cast<int>(i) < b.baseTris ? 1 : 0;
        }
    }
    b.tris = static_cast<za::Vector<Tri>&&>(kept);
    b.baseTris = base;
}

// A box (centre, half sizes), every face the whole rect.
void box(Build& b, const glm::vec3& c, const glm::vec3& h, const Rect& r)
{
    for(int axis = 0; axis < 3; axis++)
    {
        for(int sign = -1; sign <= 1; sign += 2)
        {
            glm::vec3 n{0.f};
            n[axis] = static_cast<float>(sign);
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            int idx[4];
            for(int k = 0; k < 4; k++)
            {
                glm::vec3 p = c + n * h[axis];
                const float su = (k == 1 || k == 2) ? 1.f : -1.f, sv = k >= 2 ? 1.f : -1.f;
                p[u] += su * h[u];
                p[v] += sv * h[v];
                idx[k] = b.vertex(p, n, static_cast<float>(r.x) + (su > 0.f ? static_cast<float>(r.w - 1) : 0.f),
                    static_cast<float>(r.y) + (sv > 0.f ? static_cast<float>(r.h - 1) : 0.f));
            }
            b.tri(idx[0], idx[1], idx[2], n);
            b.tri(idx[0], idx[2], idx[3], n);
        }
    }
}

// A tube's rect: about 2 texels a unit round and along it (its circumference, its length), at least 8 x 4.
[[nodiscard]] Rect tubeRect(Build& b, float radius, float length, float density = 2.f)
{
    Rect r;
    (void)b.alloc(za::max(8, static_cast<int>(2.f * pi * radius * density)), za::max(4, static_cast<int>(length * density)), r);
    return r;
}

// ---- Colours and patterns -------------------------------------------------------------------------------------------

const glm::vec3 black{0.07f, 0.07f, 0.065f};
const glm::vec3 hazard{0.86f, 0.68f, 0.08f};
const glm::vec3 white{0.82f, 0.8f, 0.74f};
const glm::vec3 red{0.66f, 0.1f, 0.06f};
const glm::vec3 olive{0.24f, 0.27f, 0.12f};
const glm::vec3 steel{0.42f, 0.44f, 0.47f};
const glm::vec3 darkSteel{0.17f, 0.18f, 0.2f};
const glm::vec3 copper{0.66f, 0.36f, 0.16f};
const glm::vec3 brass{0.72f, 0.56f, 0.22f};
const glm::vec3 wood{0.36f, 0.2f, 0.1f};
const glm::vec3 electric{0.55f, 0.75f, 1.f};
const glm::vec3 glass{0.3f, 0.55f, 1.f};
const glm::vec3 flameYellow{1.f, 0.86f, 0.32f};
const glm::vec3 flameOrange{1.f, 0.48f, 0.08f};
const glm::vec3 poison{0.36f, 0.8f, 0.18f};
const glm::vec3 poisonDark{0.12f, 0.3f, 0.08f};

[[nodiscard]] float fractf(float x)
{
    return x - floorf(x);
}

// Diagonal hazard stripes, `w` units wide.
[[nodiscard]] Paint stripes(float u, float v, float w)
{
    return paint(fractf((u + v) / (2.f * w)) < 0.5f ? hazard : black);
}

// Lightning: `bolts` zigzags across the band (v 0..h), bright cores, a glow round them; else `ground`.
[[nodiscard]] Paint lightning(float u, float v, float h, int bolts, const glm::vec3& ground, float seed)
{
    float best = 1e9f;
    for(int k = 0; k < bolts; k++)
    {
        const float mid = h * (static_cast<float>(k) + 0.5f) / static_cast<float>(bolts);
        // A zigzag (a triangle wave with a jitter a segment).
        const float seg = 1.6f;
        const float i = floorf(u / seg + seed + static_cast<float>(k) * 3.7f);
        const float f = fractf(u / seg + seed + static_cast<float>(k) * 3.7f);
        const float y0 = (hash2(static_cast<int>(i), k) - 0.5f) * h * 0.55f;
        const float y1 = (hash2(static_cast<int>(i) + 1, k) - 0.5f) * h * 0.55f;
        best = za::min(best, za::abs(v - (mid + y0 + (y1 - y0) * f)));
    }
    if(best < 0.22f)
    {
        return paint(glm::vec3{0.85f, 0.95f, 1.f}, true);
    }
    if(best < 0.6f)
    {
        return paint(electric * 0.85f, true);
    }
    return paint(ground);
}

// Flames rising from the band's bottom (v down the band: h its bottom), tongues of fire's colours; else `ground`.
[[nodiscard]] Paint flames(float u, float v, float h, const glm::vec3& ground)
{
    const float up = h - v; // from the bottom
    const float tongue = h * (0.45f + 0.25f * sinf(u * 1.9f) + 0.2f * sinf(u * 3.3f + 1.3f) + 0.1f * sinf(u * 7.1f));
    if(up > tongue)
    {
        return paint(ground);
    }
    const float k = up / za::max(tongue, 1e-3f);
    if(k < 0.4f)
    {
        return paint(flameYellow, true);
    }
    if(k < 0.8f)
    {
        return paint(flameOrange, true);
    }
    return paint(red);
}

// Poison: bubbles (rings) over the ground, a few drips.
[[nodiscard]] Paint bubbles(float u, float v, const glm::vec3& ground)
{
    const float cell = 4.4f;
    const int cx = static_cast<int>(floorf(u / cell)), cy = static_cast<int>(floorf(v / cell));
    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -1; dx <= 1; dx++)
        {
            const int x = cx + dx, y = cy + dy;
            const float bx = (static_cast<float>(x) + 0.2f + 0.6f * hash2(x, y)) * cell;
            const float by = (static_cast<float>(y) + 0.2f + 0.6f * hash2(y, x + 7)) * cell;
            const float rad = 1.1f + 0.9f * hash2(x + 3, y + 11);
            const float dist = glm::length(glm::vec2{u - bx, v - by});
            if(dist < rad)
            {
                return paint(dist > rad - 0.55f ? poison * 1.3f : (dist < rad * 0.4f ? poison * 0.85f : poisonDark * 2.2f));
            }
        }
    }
    return paint(ground);
}

// Rocket icons in a row of cells `cell` units long (the nose towards +u), on the ground.
[[nodiscard]] Paint rockets(float u, float v, float h, const glm::vec3& ground)
{
    const float cell = h * 1.9f;
    const float cu = fractf(u / cell), cv = v / h;
    const float half = za::abs(cv - 0.5f);
    if(cu > 0.22f && cu < 0.72f && half < 0.11f)
    {
        return paint(cu < 0.33f ? red : white);
    }
    if(cu >= 0.72f && cu < 0.9f && half < 0.11f * (0.9f - cu) / 0.18f)
    {
        return paint(red);
    }
    if(cu > 0.2f && cu < 0.34f && half >= 0.11f && half < 0.11f + 0.14f * (0.34f - cu) / 0.14f)
    {
        return paint(darkSteel);
    }
    if(cu > 0.1f && cu <= 0.22f && half < 0.06f)
    {
        return paint(flameOrange, true);
    }
    return paint(ground);
}

// Shells standing side by side (red, brass heads at the band's top).
[[nodiscard]] Paint shells(float u, float v, float h)
{
    const float cu = fractf(u / 1.1f);
    if(cu < 0.15f || cu > 0.85f)
    {
        return paint(darkSteel);
    }
    return paint(v < h * 0.28f ? brass : red * (0.8f + 0.4f * za::abs(cu - 0.5f)));
}

// ---- The rifle's own surface: recoloured, decals where its sides face ------------------------------------------------

// A gun's colours: the rifle's light kept, its colour taken `amount` of the way towards `tint`'s.
struct Theme
{
    glm::vec3 tint{1.f};
    float amount{0.f};
};

void recolour(Build& b, const Theme& theme)
{
    if(theme.amount <= 0.f)
    {
        return;
    }
    byte remap[256];
    const glm::vec3 t = theme.tint / za::max(lum(theme.tint), 0.05f);
    for(int i = 0; i < 256; i++)
    {
        if(i >= 224)
        {
            remap[i] = static_cast<byte>(i);
            continue;
        }
        const glm::vec3 c = palette(i);
        remap[i] = b.quantize(glm::mix(c, t * lum(c), theme.amount), false);
    }
    for(int i = 0; i < b.skinW * b.baseH; i++)
    {
        byte& px = b.skin[static_cast<za::SizeT>(i)];
        px = remap[px];
    }
}

// A decal on the rifle where its surface faces `facing` (+y its left side, -y its right, +z its top; within 60
// degrees), inside the box [lo, hi]: `fn(u, v)` with u along x, v down (from hi.z) for the sides, across (y) for the top.
// Each texel's place found through the triangles using it (barycentric); the rifle's light under it kept a little.
template <typename F>
void decal(Build& b, const Mdl& m, const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& facing, F&& fn)
{
    for(int t = 0; t < b.baseTris; t++)
    {
        const Tri& tr = b.tris[static_cast<za::SizeT>(t)];
        const Vert* v[3] = {&b.verts[static_cast<za::SizeT>(tr.v[0])], &b.verts[static_cast<za::SizeT>(tr.v[1])],
            &b.verts[static_cast<za::SizeT>(tr.v[2])]};
        const glm::vec3 tlo = glm::min(v[0]->p, glm::min(v[1]->p, v[2]->p)), thi = glm::max(v[0]->p, glm::max(v[1]->p, v[2]->p));
        if(glm::any(glm::lessThan(thi, lo)) || glm::any(glm::greaterThan(tlo, hi)))
        {
            continue;
        }
        glm::vec2 q[3];
        for(int k = 0; k < 3; k++)
        {
            q[k] = glm::vec2{static_cast<float>(v[k]->s + (v[k]->seam && !tr.front ? m.skinW / 2 : 0)), static_cast<float>(v[k]->t)};
        }
        const float area = (q[1].x - q[0].x) * (q[2].y - q[0].y) - (q[2].x - q[0].x) * (q[1].y - q[0].y);
        if(za::abs(area) < 1e-4f)
        {
            continue;
        }
        const int x0 = za::max(0, static_cast<int>(floorf(za::min(q[0].x, za::min(q[1].x, q[2].x)))));
        const int x1 = za::min(b.skinW - 1, static_cast<int>(ceilf(za::max(q[0].x, za::max(q[1].x, q[2].x)))));
        const int y0 = za::max(0, static_cast<int>(floorf(za::min(q[0].y, za::min(q[1].y, q[2].y)))));
        const int y1 = za::min(b.baseH - 1, static_cast<int>(ceilf(za::max(q[0].y, za::max(q[1].y, q[2].y)))));
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                const glm::vec2 c{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                const float w1 = ((c.x - q[0].x) * (q[2].y - q[0].y) - (q[2].x - q[0].x) * (c.y - q[0].y)) / area;
                const float w2 = ((q[1].x - q[0].x) * (c.y - q[0].y) - (c.x - q[0].x) * (q[1].y - q[0].y)) / area;
                const float w0 = 1.f - w1 - w2;
                if(w0 < -0.02f || w1 < -0.02f || w2 < -0.02f)
                {
                    continue;
                }
                const glm::vec3 p = v[0]->p * w0 + v[1]->p * w1 + v[2]->p * w2;
                const glm::vec3 n = glm::normalize(v[0]->n * w0 + v[1]->n * w1 + v[2]->n * w2 + glm::vec3{1e-6f});
                if(glm::any(glm::lessThan(p, lo)) || glm::any(glm::greaterThan(p, hi)) || glm::dot(n, facing) < 0.5f)
                {
                    continue;
                }
                const float u = p.x - lo.x;
                const float vv = facing.z > 0.5f ? p.y - lo.y : hi.z - p.z;
                const float under = lum(palette(b.skin[static_cast<za::SizeT>(y * b.skinW + x)]));
                b.set(x, y, fn(u, vv), za::clamp(0.75f + 0.9f * (under - 0.3f), 0.6f, 1.2f));
            }
        }
    }
}

// Both sides alike.
template <typename F>
void decalSides(Build& b, const Mdl& m, const glm::vec3& lo, const glm::vec3& hi, F&& fn)
{
    decal(b, m, lo, hi, glm::vec3{0.f, 1.f, 0.f}, fn);
    decal(b, m, lo, hi, glm::vec3{0.f, -1.f, 0.f}, fn);
}

// ---- The grenade launcher's barrel and drum, from our v_rock.mdl -----------------------------------------------------

// Its parts (connected pieces of its mesh) that make its barrel and drum (the body and its six bolts: not the grip, the
// trigger, the sights), the triangles of them whose corners `want(p)` keeps (its model's place), each corner put where
// `place(p)` says, over the rifle; its skin pasted at half size (lifted a little: its dark greys read black) into the
// rows from `skinRow` (its texels' coordinates halved: its seam's half width with them).
template <typename W, typename P>
void graftLauncher(Build& b, const Mdl& g, W&& want, P&& place, int skinRow)
{
    const int halfW = g.skinW / 2, halfH = g.skinH / 2;
    for(int y = 0; y < halfH && skinRow + y < b.skinH; y++)
    {
        for(int x = 0; x < halfW && x < b.skinW; x++)
        {
            // (the top-left texel of each 2 x 2: an indexed skin is not averaged)
            const byte px = g.skin[static_cast<size_t>(y * 2) * static_cast<size_t>(g.skinW) + static_cast<size_t>(x * 2)];
            b.skin[static_cast<za::SizeT>((skinRow + y) * b.skinW + x)] = px >= 224 ? px : b.quantize(palette(px) * 1.45f, false);
        }
    }
    // Its pieces: union-find over the triangles.
    za::Vector<int> parent(static_cast<za::SizeT>(g.numVerts), 0);
    for(int v = 0; v < g.numVerts; v++)
    {
        parent[static_cast<za::SizeT>(v)] = v;
    }
    const auto root = [&](int v) {
        while(parent[static_cast<za::SizeT>(v)] != v)
        {
            parent[static_cast<za::SizeT>(v)] = parent[static_cast<za::SizeT>(parent[static_cast<za::SizeT>(v)])];
            v = parent[static_cast<za::SizeT>(v)];
        }
        return v;
    };
    // (vertices at the same place are one: a piece's skin seams split its vertices)
    for(int v = 0; v < g.numVerts; v++)
    {
        const byte* pv = g.src + g.poseAt + static_cast<size_t>(v) * 4;
        for(int w = 0; w < v; w++)
        {
            const byte* pw = g.src + g.poseAt + static_cast<size_t>(w) * 4;
            if(pv[0] == pw[0] && pv[1] == pw[1] && pv[2] == pw[2])
            {
                parent[static_cast<za::SizeT>(root(v))] = root(w);
                break;
            }
        }
    }
    for(int t = 0; t < g.numTris; t++)
    {
        const int a = g.tri(t, 1), c = g.tri(t, 2), d = g.tri(t, 3);
        if(a < 0 || c < 0 || d < 0 || a >= g.numVerts || c >= g.numVerts || d >= g.numVerts)
        {
            continue;
        }
        parent[static_cast<za::SizeT>(root(a))] = root(c);
        parent[static_cast<za::SizeT>(root(c))] = root(d);
    }
    za::Vector<glm::vec3> lo(static_cast<za::SizeT>(g.numVerts), glm::vec3{1e9f}), hi(static_cast<za::SizeT>(g.numVerts), glm::vec3{-1e9f});
    za::Vector<int> count(static_cast<za::SizeT>(g.numVerts), 0);
    for(int v = 0; v < g.numVerts; v++)
    {
        const za::SizeT r = static_cast<za::SizeT>(root(v));
        lo[r] = glm::min(lo[r], g.pos(v));
        hi[r] = glm::max(hi[r], g.pos(v));
        count[r]++;
    }
    const auto kept = [&](int v) {
        const za::SizeT r = static_cast<za::SizeT>(root(v));
        return count[r] >= 8 && lo[r].x >= -4.5f && hi[r].z >= 6.f && want(g.pos(v));
    };
    za::Vector<int> map(static_cast<za::SizeT>(g.numVerts), -1);
    for(int t = 0; t < g.numTris; t++)
    {
        int v[3];
        bool all = true;
        for(int c = 0; c < 3; c++)
        {
            v[c] = g.tri(t, c + 1);
            all = all && v[c] >= 0 && v[c] < g.numVerts && kept(v[c]);
        }
        if(!all)
        {
            continue;
        }
        Tri tr;
        tr.front = g.tri(t, 0);
        for(int c = 0; c < 3; c++)
        {
            int& m = map[static_cast<za::SizeT>(v[c])];
            if(m < 0)
            {
                Vert out;
                out.p = place(g.pos(v[c]));
                out.n = g.normal(v[c]);
                out.seam = g.st(v[c], 0) != 0 ? 1 : 0;
                out.s = za::clamp(g.st(v[c], 1) / 2, 0, b.skinW - 1);
                out.t = za::clamp(skinRow + g.st(v[c], 2) / 2, 0, b.skinH - 1);
                b.verts.pushBack(out);
                m = static_cast<int>(b.verts.size()) - 1;
            }
            tr.v[c] = m;
        }
        b.tris.pushBack(tr);
    }
}

// The grafted launcher barrel's bottom (its vertices from `firstVert`), its keel left off: a flat quad across its sides'
// lower edges (the lowest of its corners within its sides, |y| under 3.45: not the wider collar's), from the back of
// them to the front on each side.
void closeLauncherBottom(Build& b, za::SizeT firstVert)
{
    float zb = 1e9f;
    for(za::SizeT i = firstVert; i < b.verts.size(); i++)
    {
        if(za::abs(b.verts[i].p.y) < 3.45f)
        {
            zb = za::min(zb, b.verts[i].p.z);
        }
    }
    int corner[2][2] = {{-1, -1}, {-1, -1}}; // [side y- / y+][back / front]
    for(za::SizeT i = firstVert; i < b.verts.size(); i++)
    {
        const glm::vec3& p = b.verts[i].p;
        if(za::abs(p.z - zb) > 0.05f || za::abs(p.y) >= 3.45f || za::abs(p.y) < 2.5f)
        {
            continue;
        }
        int* c = corner[p.y > 0.f ? 1 : 0];
        if(c[0] < 0 || p.x < b.verts[static_cast<za::SizeT>(c[0])].p.x)
        {
            c[0] = static_cast<int>(i);
        }
        if(c[1] < 0 || p.x > b.verts[static_cast<za::SizeT>(c[1])].p.x)
        {
            c[1] = static_cast<int>(i);
        }
    }
    if(corner[0][0] < 0 || corner[1][0] < 0)
    {
        return;
    }
    // (painted the launcher's blue steel, its skin's lighter band down the middle as its keel had)
    Rect plate;
    (void)b.alloc(8, 4, plate);
    paintRect(b, plate, 1.f, 1.f, [](float u, float) { return paint(glm::vec3{0.34f, 0.38f, 0.6f} * (za::abs(u - 0.5f) < 0.2f ? 1.3f : 1.f)); });
    int q[4];
    const int order[4][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
    for(int k = 0; k < 4; k++)
    {
        Vert v = b.verts[static_cast<za::SizeT>(corner[order[k][0]][order[k][1]])];
        v.n = glm::vec3{0.f, 0.f, -1.f};
        v.s = plate.x + (order[k][0] ? plate.w - 1 : 0); // (u across it, y- to y+)
        v.t = plate.y + (order[k][1] ? plate.h - 1 : 0);
        v.seam = 0;
        b.verts.pushBack(v);
        q[k] = static_cast<int>(b.verts.size()) - 1;
    }
    b.tri(q[0], q[1], q[2], glm::vec3{0.f, 0.f, -1.f});
    b.tri(q[0], q[2], q[3], glm::vec3{0.f, 0.f, -1.f});
}

// ---- The variants ----------------------------------------------------------------------------------------------------

// Each gun's add-ons and paint. The rifle (model units): the stock x -29..-14, the receiver x -8..12 (its top z 14.4),
// a deep magazine box x 12..24 (y -5.3..5.4, z 1.8..14.4), the barrel x 24..33 round z 8, the grip under x 0 (z -4.2).
void dress(Build& b, const Mdl& m, int variant, const Mdl* launcher)
{
    const glm::vec3 lo{-30.f, -7.f, -5.f}, hi{40.f, 7.f, 20.f};
    switch(variant)
    {
        case Rocket:
        {
            recolour(b, Theme{olive, 0.75f});
            decalSides(b, m, glm::vec3{12.5f, lo.y, 6.f}, glm::vec3{23.5f, hi.y, 12.5f},
                [](float u, float v) { return rockets(u, v, 6.5f, olive * 0.8f); });
            decalSides(b, m, glm::vec3{-8.f, lo.y, 11.f}, glm::vec3{-4.f, hi.y, 14.5f}, [](float u, float v) { return stripes(u, v, 0.8f); });
            // The muzzle a wide launch tube round the barrel (Vittorio, 2026-10-18: "the rockets come out of it"), dark
            // inside, a warhead's red nose waiting in its mouth, a steel ring at the lip.
            const float mz = 8.08f, mr = 3.45f, m0 = 23.5f, m1 = muzzleX - 0.1f;
            const Rect tubeR = tubeRect(b, mr, m1 - m0, 1.5f);
            paintRect(b, tubeR, 2.f * pi * mr, m1 - m0, [](float u, float v) {
                if(v > 6.4f && v < 7.6f)
                {
                    return stripes(u, v, 0.7f);
                }
                const float circ = 2.f * pi * 3.45f, side = fractf((u + 3.3f) / (circ * 0.5f)) * circ * 0.5f;
                if(side < 4.4f && v > 0.6f && v < 6.f)
                {
                    return rockets(v, side, 4.4f, olive);
                }
                return paint(olive * 1.35f);
            });
            Rect inner;
            (void)b.alloc(16, 8, inner);
            paintRect(b, inner, 1.f, 1.f, [](float, float v) { return paint(black * (1.f + v)); });
            Rect ring;
            (void)b.alloc(32, 4, ring);
            paintRect(b, ring, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            // One hollow tube (Vittorio, 2026-10-19: no rifle barrel inside it, nor see-through at its lip): its back, the
            // back ring, the tube, the lip's ring and the lip, the bore back to its floor (each facing out of its metal).
            dropRifleInside(b, m0, muzzleX + 1.f, mz, mr);
            const float ml = m1 - m0;
            const Turn launch[] = {{0.f, 0.f, &ring}, {0.f, mr * 1.08f, &ring}, {0.8f, mr * 1.08f, &ring}, {0.8f, mr, &tubeR},
                {ml - 1.f, mr, &ring}, {ml - 1.f, mr * 1.1f, &ring}, {ml, mr * 1.1f, &ring}, {ml, mr * 0.84f, &inner},
                {1.f, mr * 0.84f, &inner}, {1.f, 0.f, nullptr}};
            lathe(b, glm::vec3{m0, 0.f, mz}, glm::vec3{1.f, 0.f, 0.f}, launch, static_cast<int>(sizeof(launch) / sizeof(launch[0])), 14);
            Rect nose;
            (void)b.alloc(16, 6, nose);
            paintRect(b, nose, 1.f, 1.f, [](float, float v) { return paint(v < 0.5f ? red : red * 0.7f); });
            tube(b, glm::vec3{m1 - 4.2f, 0.f, mz}, glm::vec3{m1 - 1.4f, 0.f, mz}, mr * 0.76f, 0.3f, 12, nose, true, true);
            // The exhaust: a short pipe rising out of the gun's top near the muzzle, then straight back over the receiver,
            // a flared bell at its rear (sooty inside) where the launch gases leave.
            const float er = 1.75f, ez = 17.5f, bend = 14.f, rear = -1.5f;
            Rect pipe;
            (void)b.alloc(24, 32, pipe);
            paintRect(b, pipe, 1.f, 1.f, [](float u, float v) { return paint(glm::mix(darkSteel, copper * 0.55f, v * v) * (0.85f + 0.3f * fractf(u * 5.f))); });
            tube(b, glm::vec3{21.5f, 0.f, 12.4f}, glm::vec3{bend, 0.f, ez}, er, er, 12, pipe, true, true); // (its ends in the gun, the collar)
            Rect bell, soot;
            (void)b.alloc(32, 8, bell);
            paintRect(b, bell, 1.f, 1.f, [](float u, float v) { return paint(glm::mix(copper * 0.55f, darkSteel * 0.7f, v) * (0.8f + 0.4f * fractf(u * 6.f))); });
            (void)b.alloc(24, 16, soot);
            paintRect(b, soot, 1.f, 1.f, [](float u, float v) { return paint(glm::mix(copper * 0.55f, darkSteel * 1.1f, v) * (0.75f + 0.35f * fractf(u * 6.f))); });
            // Back from the collar at the bend (along -x): the collar, the pipe, the ring, the flared bell and its lip, then
            // its inside (Vittorio, 2026-10-19: an empty pipe, walled inside and out): the bell's, the pipe's sooty bore,
            // closed at the collar.
            const float el = bend + 0.9f - rear, bellAt = er * 1.02f + er * 0.58f * 0.6f / 4.f;
            const Turn exhaust[] = {{0.f, 0.f, &ring}, {0.f, er * 1.18f, &ring}, {1.8f, er * 1.18f, &ring}, {1.8f, er, &pipe},
                {el - 0.4f, er, &ring}, {el - 0.4f, er * 1.15f, &ring}, {el + 0.6f, er * 1.15f, &ring}, {el + 0.6f, bellAt, &bell},
                {el + 4.f, er * 1.6f, &ring}, {el + 4.f, er * 1.48f, &soot}, {el - 3.f, er * 0.8f, &soot}, {1.8f, er * 0.8f, &soot},
                {1.8f, 0.f, nullptr}};
            lathe(b, glm::vec3{bend + 0.9f, 0.f, ez}, glm::vec3{-1.f, 0.f, 0.f}, exhaust,
                static_cast<int>(sizeof(exhaust) / sizeof(exhaust[0])), 12);
            Rect mount;
            (void)b.alloc(8, 8, mount);
            paintRect(b, mount, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            box(b, glm::vec3{5.f, 0.f, 15.f}, glm::vec3{1.4f, 0.9f, 0.9f}, mount);
            break;
        }
        case Grenade:
        {
            recolour(b, Theme{steel, 0.35f});
            decalSides(b, m, glm::vec3{12.5f, lo.y, 11.5f}, glm::vec3{23.5f, hi.y, 13.f}, [](float u, float v) { return stripes(u, v, 0.7f); });
            const float mz = 8.08f;
            if(launcher)
            {
                // The muzzle our grenade launcher's (Vittorio, 2026-10-18): its hexagonal barrel from the drum's face to its
                // mouth (x 18.3 .. 31.4, its bore round z 5.92; not the drum behind it), shortened to x 23 .. 33 and at
                // 0.92 across, where the rifle's barrel was.
                const int row = b.atlasY;
                b.atlasY += launcher->skinH / 2 + 1;
                // Not its keel under the barrel (below its z 2: cut short at the drum and squeezed, it was a twisted, gaping
                // wedge under the muzzle; Vittorio, 2026-10-19): the barrel's bottom closed flat across its sides' lower
                // edges instead, and the rifle's barrel inside it gone (only the launcher's muzzle).
                const float sx = (muzzleX - 0.1f - 23.f) / (31.4f - 18.3f);
                const za::SizeT firstVert = b.verts.size();
                graftLauncher(b, *launcher, [](const glm::vec3& p) { return p.x >= 18.f && p.z >= 2.f; },
                    [sx, mz](const glm::vec3& p) { return glm::vec3{muzzleX - 0.1f - (31.4f - p.x) * sx, p.y * 0.92f, mz + (p.z - 5.92f) * 0.92f}; },
                    row);
                dropRifleInside(b, 23.f, muzzleX + 1.f, mz, 3.3f);
                closeLauncherBottom(b, firstVert);
            }
            // The magazine on top: an open-sided steel box of grenades lying across it, a chute feeding the gun from its
            // front.
            Rect frame, nade, chute;
            (void)b.alloc(16, 16, frame);
            paintRect(b, frame, 1.f, 1.f, [](float u, float v) { return paint(darkSteel * (0.9f + 0.25f * fractf((u + v) * 3.f))); });
            const float bx0 = -3.6f, bx1 = 10.6f, by = 2.9f, bz0 = 14.4f, bz1 = 19.8f, cx = 0.5f * (bx0 + bx1), hx = 0.5f * (bx1 - bx0);
            box(b, glm::vec3{cx, 0.f, bz0 + 0.4f}, glm::vec3{hx, by, 0.4f}, frame);
            box(b, glm::vec3{cx, 0.f, bz1 - 0.3f}, glm::vec3{hx, by, 0.3f}, frame);
            box(b, glm::vec3{bx0 + 0.3f, 0.f, 0.5f * (bz0 + bz1)}, glm::vec3{0.3f, by, 0.5f * (bz1 - bz0)}, frame);
            box(b, glm::vec3{bx1 - 0.3f, 0.f, 0.5f * (bz0 + bz1)}, glm::vec3{0.3f, by, 0.5f * (bz1 - bz0)}, frame);
            for(const float s : {-1.f, 1.f})
            {
                box(b, glm::vec3{cx, s * (by - 0.1f), 17.1f}, glm::vec3{hx, 0.14f, 0.3f}, frame);
            }
            (void)b.alloc(32, 16, nade);
            paintRect(b, nade, 1.f, 1.f, [](float u, float v) {
                if(v < 0.1f || v > 0.9f)
                {
                    return paint(brass * 0.8f);
                }
                if(v > 0.44f && v < 0.56f)
                {
                    return paint(red);
                }
                return paint(fractf(u * 6.f) < 0.15f || fractf(v * 5.f) < 0.15f ? poisonDark * 0.7f : glm::vec3{0.2f, 0.25f, 0.13f});
            });
            for(int k = 0; k < 4; k++)
            {
                const float x = -1.5f + 3.3f * static_cast<float>(k);
                tube(b, glm::vec3{x, -2.45f, 16.9f}, glm::vec3{x, 2.45f, 16.9f}, 1.5f, 1.5f, 10, nade, false, false);
                tube(b, glm::vec3{x, -2.45f, 16.9f}, glm::vec3{x, -2.75f, 16.9f}, 1.5f, 0.9f, 10, nade, false, true);
                tube(b, glm::vec3{x, 2.45f, 16.9f}, glm::vec3{x, 2.75f, 16.9f}, 1.5f, 0.9f, 10, nade, false, true);
            }
            (void)b.alloc(16, 16, chute);
            paintRect(b, chute, 1.f, 1.f, [](float u, float) { return paint(fractf(u * 4.f) < 0.5f ? darkSteel : steel * 0.7f); });
            tube(b, glm::vec3{bx1 - 0.4f, 0.f, 16.2f}, glm::vec3{14.f, 0.f, 13.6f}, 1.5f, 1.4f, 10, chute, true, true);
            break;
        }
        case Plasma:
        case Centurion:
        {
            const glm::vec3 tint = variant == Plasma ? glm::vec3{0.32f, 0.5f, 1.f} : glm::vec3{0.3f, 0.72f, 1.f};
            recolour(b, Theme{tint, 0.85f});
            const glm::vec3 ground = tint * 0.32f;
            decalSides(b, m, glm::vec3{12.5f, lo.y, 3.f}, glm::vec3{23.5f, hi.y, 13.5f}, [ground, variant](float u, float v) {
                return lightning(u, v, 10.5f, variant == Centurion ? 3 : 2, ground, static_cast<float>(variant));
            });
            Rect glassR, capR;
            (void)b.alloc(32, 24, glassR);
            paintRect(b, glassR, 6.f, 18.f, [variant](float u, float v) {
                // A cathode tube's glass: its glow, a filament down its middle, sparks.
                const float f = za::abs(u - 3.f);
                if(f < 0.5f)
                {
                    return paint(glm::vec3{0.9f, 0.97f, 1.f}, true);
                }
                return lightning(v, u, 6.f, 1, glass * (1.1f - 0.08f * f), static_cast<float>(variant) + 0.5f);
            });
            (void)b.alloc(24, 6, capR);
            paintRect(b, capR, 1.f, 1.f, [](float u, float) { return paint(fractf(u * 6.f) < 0.5f ? copper : copper * 0.6f); });
            if(variant == Plasma)
            {
                // Two cathode tubes along the barrel's sides, in copper rings.
                for(int s = -1; s <= 1; s += 2)
                {
                    const float y = 3.9f * static_cast<float>(s);
                    tube(b, glm::vec3{26.f, y, 9.6f}, glm::vec3{32.f, y, 9.6f}, 0.95f, 0.95f, 10, glassR, true, true);
                    tube(b, glm::vec3{25.5f, y, 9.6f}, glm::vec3{26.5f, y, 9.6f}, 1.25f, 1.25f, 10, capR, true, true);
                    tube(b, glm::vec3{31.6f, y, 9.6f}, glm::vec3{32.6f, y, 9.6f}, 1.25f, 1.25f, 10, capR, true, true);
                }
            }
            else
            {
                // The centurion's: a capacitor drum of glass on top, banded; two cathode tubes standing on the box.
                tube(b, glm::vec3{-2.f, 0.f, 16.6f}, glm::vec3{16.f, 0.f, 16.6f}, 1.9f, 1.9f, 12, glassR, true, true);
                for(const float x : {-2.4f, 7.f, 16.4f})
                {
                    tube(b, glm::vec3{x - 0.6f, 0.f, 16.6f}, glm::vec3{x + 0.6f, 0.f, 16.6f}, 2.2f, 2.2f, 12, capR, true, true);
                }
                for(const float x : {19.5f, 22.5f})
                {
                    tube(b, glm::vec3{x, 0.f, 14.f}, glm::vec3{x, 0.f, 17.f}, 0.75f, 0.75f, 8, glassR, true, false);
                    tube(b, glm::vec3{x, 0.f, 17.f}, glm::vec3{x, 0.f, 18.2f}, 0.75f, 0.15f, 8, glassR, false, true);
                    tube(b, glm::vec3{x, 0.f, 13.9f}, glm::vec3{x, 0.f, 14.8f}, 1.05f, 1.05f, 8, capR, true, true);
                }
            }
            break;
        }
        case Defender:
        {
            recolour(b, Theme{glm::vec3{0.36f, 0.37f, 0.42f}, 0.65f});
            // A magazine tube under the barrel, a ribbed wooden pump on it; shell carriers on the receiver's sides, rows
            // of shells painted on the magazine box's.
            Rect magR, pumpR, carrier;
            (void)b.alloc(16, 16, magR);
            paintRect(b, magR, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            tube(b, glm::vec3{24.f, 0.f, 5.6f}, glm::vec3{32.4f, 0.f, 5.6f}, 0.95f, 0.95f, 10, magR, true, true);
            (void)b.alloc(32, 16, pumpR);
            paintRect(b, pumpR, 1.f, 8.f, [](float, float v) { return paint(fractf(v) < 0.3f ? wood * 0.8f : wood * 1.45f); });
            box(b, glm::vec3{27.5f, 0.f, 5.3f}, glm::vec3{2.5f, 1.55f, 1.35f}, pumpR);
            (void)b.alloc(48, 12, carrier);
            paintRect(b, carrier, 8.f, 3.4f, [](float u, float v) { return shells(u, v, 3.4f); });
            box(b, glm::vec3{-3.f, -5.25f, 10.6f}, glm::vec3{4.f, 0.35f, 1.7f}, carrier);
            box(b, glm::vec3{-3.f, 5.25f, 10.6f}, glm::vec3{4.f, 0.35f, 1.7f}, carrier);
            decalSides(b, m, glm::vec3{12.5f, lo.y, 4.f}, glm::vec3{23.5f, hi.y, 13.f}, [](float u, float v) {
                const float row = fractf(v / 4.5f) * 4.5f;
                return row < 3.4f ? shells(u, row, 3.4f) : paint(darkSteel);
            });
            break;
        }
        case Pyro:
        case Fumigator:
        {
            const bool pyro = variant == Pyro;
            recolour(b, pyro ? Theme{glm::vec3{1.f, 0.38f, 0.14f}, 0.78f} : Theme{glm::vec3{0.38f, 0.9f, 0.26f}, 0.8f});
            const glm::vec3 ground = pyro ? glm::vec3{0.4f, 0.12f, 0.05f} : poisonDark;
            if(pyro)
            {
                decalSides(b, m, glm::vec3{12.5f, lo.y, 2.f}, glm::vec3{23.5f, hi.y, 13.5f}, [ground](float u, float v) { return flames(u, v, 11.5f, ground); });
            }
            else
            {
                decalSides(b, m, glm::vec3{12.5f, lo.y, 4.f}, glm::vec3{23.5f, hi.y, 13.5f}, [ground](float u, float v) { return bubbles(u, v, ground); });
            }
            decalSides(b, m, glm::vec3{12.5f, lo.y, 1.5f}, glm::vec3{23.5f, hi.y, 4.f}, [](float u, float v) { return stripes(u, v, 0.6f); });
            decalSides(b, m, glm::vec3{-8.f, lo.y, 11.f}, glm::vec3{-4.f, hi.y, 14.5f}, [](float u, float v) { return stripes(u, v, 0.6f); });
            // The fuel canister under the receiver (hazard bands, its contents' sign), the hose's inlet at its back.
            Rect canR, nozR, inR;
            (void)b.alloc(48, 16, canR);
            paintRect(b, canR, 2.f * pi * canisterRadius, 7.f, [pyro, ground](float u, float v) {
                if(v < 1.f || v > 6.f)
                {
                    return stripes(u, v, 0.55f);
                }
                return pyro ? flames(u, v - 1.f, 5.f, ground * 1.4f) : bubbles(u, v, ground * 1.5f);
            });
            tube(b, canisterBack, canisterFront, canisterRadius, canisterRadius, 12, canR, true, true);
            (void)b.alloc(16, 8, inR);
            paintRect(b, inR, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            // The hose's inlet: a boss under the receiver between the grip and the stock, a short pipe out of it, down and
            // back (Vittorio, 2026-10-18: not from the trigger; 2026-10-19: further back, almost at the stock).
            box(b, inletBoss, glm::vec3{1.4f, 1.15f, 0.7f}, inR);
            tube(b, inletBoss + glm::vec3{0.f, 0.f, 0.2f}, inletAt, 0.62f, 0.55f, 8, inR, true, true);
            (void)b.alloc(32, 10, nozR);
            paintRect(b, nozR, 1.f, 1.f, [pyro](float u, float v) {
                if(v > 0.85f)
                {
                    return paint(pyro ? flameOrange * 0.6f : poisonDark, false);
                }
                return paint(fractf(u * 8.f) < 0.5f ? darkSteel : steel);
            });
            if(pyro)
            {
                // The nozzle's flared bell round the barrel's end; a pilot light's pipe under it (its flame particles out of
                // its tip: vr_adtank.cpp, pilotAt).
                // (Each hollow, closed: its back, the bell, its lip, its inside to a floor; the pilot's pipe the same.)
                const float nl = muzzleX - 0.1f - 28.5f, pl = pilotAt.x - 0.2f - 26.f;
                const Turn nozzle[] = {{0.f, 0.f, &nozR}, {0.f, 1.3f, &nozR}, {nl, 2.1f, &nozR}, {nl, 1.85f, &inR},
                    {0.3f, 1.1f + 0.75f * 0.3f / nl, &inR}, {0.3f, 0.f, nullptr}};
                lathe(b, glm::vec3{28.5f, 0.f, 8.1f}, glm::vec3{1.f, 0.f, 0.f}, nozzle, static_cast<int>(sizeof(nozzle) / sizeof(nozzle[0])), 12);
                const Turn pilot[] = {{0.f, 0.f, &inR}, {0.f, 0.5f, &inR}, {pl, 0.5f, &inR}, {pl, 0.3f, &inR}, {pl - 0.6f, 0.3f, &inR},
                    {pl - 0.6f, 0.f, nullptr}};
                lathe(b, glm::vec3{26.f, 0.f, pilotAt.z}, glm::vec3{1.f, 0.f, 0.f}, pilot, static_cast<int>(sizeof(pilot) / sizeof(pilot[0])), 8);
            }
            else
            {
                // A wide flat spray head on the barrel's end, its slot dark.
                box(b, glm::vec3{31.f, 0.f, 8.1f}, glm::vec3{1.9f, 2.2f, 0.85f}, nozR);
                tube(b, glm::vec3{27.5f, 0.f, 8.1f}, glm::vec3{29.2f, 0.f, 8.1f}, 1.25f, 1.25f, 10, nozR, true, true);
            }
            break;
        }
        default: break;
    }
}

// The add-ons' mesh checked (vr_adgun_variants): their corners welded by place (a 1/64 unit grid), each edge's
// triangles counted. `open`: edges one triangle has (a hole, a part's open end: see-through from where its far side is
// culled); `doubled`: edges two triangles run the same way (a neighbour wound the other way: one of them faces in);
// `inward`: triangles whose corners' normals point against their face (wound the wrong way for their shading);
// `inner`: triangles facing a part's axis (bores, bells' insides: the inner walls).
void meshStats(const Build& b, MeshStats& s, bool verbose)
{
    s = MeshStats{};
    s.verbose = verbose;
    struct Edge
    {
        int64_t a{0}, c{0};
    };
    za::Vector<Edge> edges;
    const auto key = [&](int v) {
        const glm::vec3 p = b.verts[static_cast<za::SizeT>(v)].p * 64.f;
        const int64_t x = static_cast<int64_t>(lroundf(p.x)) + (1 << 20), y = static_cast<int64_t>(lroundf(p.y)) + (1 << 20),
                      z = static_cast<int64_t>(lroundf(p.z)) + (1 << 20);
        return (x << 42) | (y << 21) | z;
    };
    for(za::SizeT i = static_cast<za::SizeT>(b.baseTris); i < b.tris.size(); i++)
    {
        const Tri& t = b.tris[i];
        s.tris++;
        const glm::vec3& pa = b.verts[static_cast<za::SizeT>(t.v[0])].p;
        const glm::vec3 g = glm::cross(b.verts[static_cast<za::SizeT>(t.v[1])].p - pa, b.verts[static_cast<za::SizeT>(t.v[2])].p - pa) * b.winding;
        const glm::vec3 n = b.verts[static_cast<za::SizeT>(t.v[0])].n + b.verts[static_cast<za::SizeT>(t.v[1])].n + b.verts[static_cast<za::SizeT>(t.v[2])].n;
        s.inward += glm::dot(g, n) < 0.f ? 1 : 0;
        if(verbose && glm::dot(g, n) < 0.f)
        {
            const glm::vec3 c = (pa + b.verts[static_cast<za::SizeT>(t.v[1])].p + b.verts[static_cast<za::SizeT>(t.v[2])].p) / 3.f;
            Con_Printf("  wound inward: triangle at %.2f %.2f %.2f\n", c.x, c.y, c.z);
        }
        s.inner += t.inner ? 1 : 0;
        for(int k = 0; k < 3; k++)
        {
            const int64_t ka = key(t.v[k]), kc = key(t.v[(k + 1) % 3]);
            if(ka != kc)
            {
                edges.pushBack(Edge{ka, kc});
            }
        }
    }
    // Each edge, its way: the same way twice is doubled; one with no partner either way is open.
    za::quickSort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) { return x.a != y.a ? x.a < y.a : x.c < y.c; });
    const auto count = [&](int64_t a, int64_t c) {
        // (binary search: the first at or past (a, c))
        za::SizeT lo = 0, hi = edges.size();
        while(lo < hi)
        {
            const za::SizeT mid = (lo + hi) / 2;
            const Edge& e = edges[mid];
            if(e.a < a || (e.a == a && e.c < c))
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid;
            }
        }
        int n = 0;
        while(lo < edges.size() && edges[lo].a == a && edges[lo].c == c)
        {
            n++;
            lo++;
        }
        return n;
    };
    const auto at = [](int64_t k) {
        const auto axis = [](int64_t v) { return static_cast<float>(v - (1 << 20)) / 64.f; };
        return glm::vec3{axis((k >> 42) & 0x1fffff), axis((k >> 21) & 0x1fffff), axis(k & 0x1fffff)};
    };
    for(za::SizeT i = 0; i < edges.size(); i++)
    {
        const Edge& e = edges[i];
        const bool doubled = i > 0 && edges[i - 1].a == e.a && edges[i - 1].c == e.c;
        const bool open = !doubled && count(e.c, e.a) == 0 && count(e.a, e.c) == 1;
        s.doubled += doubled ? 1 : 0;
        s.open += open ? 1 : 0;
        if(verbose && (open || doubled))
        {
            const glm::vec3 p = at(e.a), q = at(e.c);
            Con_Printf("  %s edge %.2f %.2f %.2f .. %.2f %.2f %.2f\n", open ? "open" : "doubled", p.x, p.y, p.z, q.x, q.y, q.z);
        }
    }
}

[[nodiscard]] bool makeVariant(int variant, const byte* src, size_t size, const byte* rockSrc, size_t rockSize, Built& out,
    const char*& why, MeshStats* stats)
{
    Mdl m;
    if(!readMdl(src, size, m, why))
    {
        return false;
    }
    Mdl rock;
    const char* rockWhy = "";
    const bool haveRock = rockSrc && readMdl(rockSrc, rockSize, rock, rockWhy) && rock.skinW / 2 <= m.skinW;

    Build b;
    b.skinW = m.skinW;
    b.baseH = m.skinH;
    b.skinH = m.skinH + extraRows;
    b.skin.resize(static_cast<za::SizeT>(b.skinW * b.skinH), static_cast<byte>(0));
    memcpy(b.skin.data(), m.skin, static_cast<size_t>(m.skinW) * static_cast<size_t>(m.skinH));
    b.atlasY = m.skinH;

    // The rifle as it is.
    for(int v = 0; v < m.numVerts; v++)
    {
        Vert out;
        out.p = m.pos(v);
        out.n = m.normal(v);
        out.seam = m.st(v, 0) != 0 ? 1 : 0;
        out.s = m.st(v, 1);
        out.t = m.st(v, 2);
        b.verts.pushBack(out);
    }
    float sense = 0.f;
    for(int t = 0; t < m.numTris; t++)
    {
        Tri tr;
        tr.front = m.tri(t, 0);
        bool ok = true;
        for(int c = 0; c < 3; c++)
        {
            tr.v[c] = m.tri(t, c + 1);
            ok = ok && tr.v[c] >= 0 && tr.v[c] < m.numVerts;
        }
        if(!ok)
        {
            continue;
        }
        const glm::vec3& a = b.verts[static_cast<za::SizeT>(tr.v[0])].p;
        const glm::vec3 g = glm::cross(b.verts[static_cast<za::SizeT>(tr.v[1])].p - a, b.verts[static_cast<za::SizeT>(tr.v[2])].p - a);
        sense += glm::dot(g, b.verts[static_cast<za::SizeT>(tr.v[0])].n + b.verts[static_cast<za::SizeT>(tr.v[1])].n +
                                 b.verts[static_cast<za::SizeT>(tr.v[2])].n);
        b.tris.pushBack(tr);
    }
    b.baseTris = static_cast<int>(b.tris.size());
    b.winding = sense >= 0.f ? 1.f : -1.f;

    dress(b, m, variant, haveRock ? &rock : nullptr);
    if(stats)
    {
        meshStats(b, *stats, stats->verbose);
    }

    // Packed from the rifle's corner (its settings' scale_origin) whenever no add-on goes past it.
    glm::vec3 lo = corner, hi{-1e30f};
    float radius = 0.f;
    for(const Vert& v : b.verts)
    {
        lo = glm::min(lo, v.p);
        hi = glm::max(hi, v.p);
        radius = za::max(radius, glm::length(v.p));
    }
    out.cornerKept = lo == corner;
    out.lo = lo;
    out.hi = hi;
    out.muzzle = glm::vec3{33.1463f, 0.0159f, 8.0776f}; // (slot 21's: the rifle's own)
    const glm::vec3 pack = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});

    za::Vector<byte>& f = out.file;
    f.clear();
    put(f, IDPOLYHEADER);
    put(f, ALIAS_VERSION);
    putf(f, pack.x);
    putf(f, pack.y);
    putf(f, pack.z);
    putf(f, lo.x);
    putf(f, lo.y);
    putf(f, lo.z);
    putf(f, radius);
    putf(f, 0.f);
    putf(f, 0.f);
    putf(f, 0.f);
    put(f, 1); // one skin
    put(f, b.skinW);
    put(f, b.skinH);
    put(f, static_cast<int>(b.verts.size()));
    put(f, static_cast<int>(b.tris.size()));
    put(f, 1); // one frame
    put(f, 0);
    put(f, m.flags);
    putf(f, m.sizeField);
    put(f, 0); // a single skin
    f.emplaceBackRange(b.skin.data(), b.skin.size());
    for(const Vert& v : b.verts)
    {
        put(f, v.seam ? ALIAS_ONSEAM : 0);
        put(f, v.s);
        put(f, v.t);
    }
    for(const Tri& t : b.tris)
    {
        put(f, t.front);
        put(f, t.v[0]);
        put(f, t.v[1]);
        put(f, t.v[2]);
    }
    put(f, 0); // a single frame
    byte bmin[4] = {255, 255, 255, 0}, bmax[4] = {0, 0, 0, 0};
    za::Vector<byte> packed;
    for(const Vert& v : b.verts)
    {
        byte q[4];
        for(int c = 0; c < 3; c++)
        {
            q[c] = static_cast<byte>(za::clamp(static_cast<int>((v.p[c] - lo[c]) / pack[c] + 0.5f), 0, 255));
            bmin[c] = za::min(bmin[c], q[c]);
            bmax[c] = za::max(bmax[c], q[c]);
        }
        q[3] = nearestNormal(v.n);
        packed.emplaceBackRange(q, 4);
    }
    f.emplaceBackRange(bmin, 4);
    f.emplaceBackRange(bmax, 4);
    char frameName[16] = "gun";
    f.emplaceBackRange(reinterpret_cast<const byte*>(frameName), 16);
    f.emplaceBackRange(packed.data(), packed.size());
    out.verts = static_cast<int>(b.verts.size());
    out.tris = static_cast<int>(b.tris.size());
    return true;
}

[[nodiscard]] bool buildVariant(int variant, unsigned int* path_id, Built& out, const char*& why, MeshStats* stats = nullptr)
{
    unsigned int pathId = 0;
    byte* src = COM_LoadMallocFile(VR_ModelFile(baseModel), &pathId);
    if(!src)
    {
        why = "v_gruntgun.mdl not found";
        return false;
    }
    const size_t size = static_cast<size_t>(com_filesize);
    byte* rock = nullptr;
    size_t rockSize = 0;
    if(variant == Grenade)
    {
        unsigned int rockPath = 0;
        rock = COM_LoadMallocFile(VR_ModelFile(launcherModel), &rockPath);
        rockSize = rock ? static_cast<size_t>(com_filesize) : 0;
    }
    const bool ok = makeVariant(variant, src, size, rock, rockSize, out, why, stats);
    VR_HeapFree(src);
    if(rock)
    {
        VR_HeapFree(rock);
    }
    if(path_id)
    {
        *path_id = pathId;
    }
    return ok;
}

} // namespace

int variantOf(const char* name)
{
    const char* at = name ? strstr(name, variantSuffix) : nullptr;
    if(!at)
    {
        return -1;
    }
    const char* d = at + strlen(variantSuffix);
    const int v = d[0] >= '0' && d[0] < '0' + variantCount && d[1] == 0 ? d[0] - '0' : -1;
    return v == EliminatorLegacy ? Centurion : v; // (an older save's eliminator's gun: the centurion's, its twin)
}

byte* variantFile(const char* name, unsigned int* path_id)
{
    const int variant = variantOf(name);
    if(variant < 0)
    {
        return nullptr;
    }
    Built b;
    const char* why = "";
    const double t0 = Sys_DoubleTime();
    if(!buildVariant(variant, path_id, b, why))
    {
        Con_DPrintf("AD gun model: %s: %s: the burst rifle stands in\n", name, why);
        return COM_LoadMallocFile(baseModel, path_id);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(b.file.size() + 1));
    memcpy(out, b.file.data(), b.file.size());
    out[b.file.size()] = 0;
    com_filesize = static_cast<int>(b.file.size());
    Con_DPrintf("AD gun model: %s made: %d triangles, %d vertices%s (%.1f ms)\n", name, b.tris, b.verts,
        b.cornerKept ? "" : ", its corner moved", (Sys_DoubleTime() - t0) * 1000.0);
    return out;
}

bool hoseInlet(int variant, glm::vec3& at, glm::vec3& dir)
{
    if(variant != Pyro && variant != Fumigator)
    {
        return false;
    }
    at = inletAt;
    dir = glm::normalize(inletDir);
    return true;
}

bool pilotLight(int variant, glm::vec3& at)
{
    at = pilotAt;
    return variant == Pyro;
}

const FireFeel& fireFeel(int variant)
{
    return variant >= 0 && variant < variantCount ? fireFeels[variant] : fireFeels[Pyro];
}

byte* flashFile(const char* name, unsigned int* path_id)
{
    const char* at = name ? strstr(name, flashSuffix) : nullptr;
    if(!at)
    {
        return nullptr;
    }
    const char* d = at + strlen(flashSuffix);
    const int variant = d[0] >= '0' && d[0] < '0' + variantCount && d[1] == 0 ? d[0] - '0' : -1;
    byte* src = COM_LoadMallocFile(VR_ModelFile(flashBase), path_id);
    if(!src || variant < 0)
    {
        return src;
    }
    Mdl m;
    const char* why = "";
    if(!readMdl(src, static_cast<size_t>(com_filesize), m, why))
    {
        return src;
    }
    // Each texel's light kept, its colour the gun's (white at its hottest), the nearest fullbright colour.
    const glm::vec3 tint = fireFeel(variant).colour;
    byte* skin = src + (m.skin - src);
    for(int i = 0; i < m.skinW * m.skinH; i++)
    {
        const byte px = skin[i];
        if(px == 255)
        {
            continue;
        }
        const glm::vec3 c = palette(px);
        const float lum = za::clamp(c.r * 0.3f + c.g * 0.59f + c.b * 0.11f, 0.f, 1.f);
        skin[i] = nearestBright(glm::mix(tint, glm::vec3{1.f}, lum * lum * lum * 0.7f) * za::min(1.f, 0.3f + lum));
    }
    Con_DPrintf("AD gun flash: %s made\n", name);
    return src;
}

void variants_f()
{
    static constexpr const char* names[variantCount] = {"rocket", "grenade", "plasma", "defender", "eliminator", "centurion",
        "pyro", "fumigator"};
    for(int v = 0; v < variantCount; v++)
    {
        if(v == EliminatorLegacy)
        {
            continue; // (the centurion's)
        }
        Built b;
        MeshStats s;
        s.verbose = Cmd_Argc() > 1 && atoi(Cmd_Argv(1)) != 0; // (vr_adgun_variants 1: each open or doubled edge)
        const char* why = "";
        const double t0 = Sys_DoubleTime();
        if(!buildVariant(v, nullptr, b, why, &s))
        {
            Con_Printf("adv%d %s: %s\n", v, names[v], why);
            continue;
        }
        Con_Printf("adv%d %s: %d triangles, %d vertices, box %.2f %.2f %.2f .. %.2f %.2f %.2f%s, %.1f ms\n", v, names[v], b.tris,
            b.verts, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z, b.cornerKept ? "" : " (its corner moved: past the settings')",
            (Sys_DoubleTime() - t0) * 1000.0);
        // (the add-ons' mesh: meshStats; the grenade launcher's grafted barrel counted with them)
        Con_Printf("adv%d %s add-ons: %d triangles, %d open edges, %d doubled edges, %d wound inward, %d inner faces\n", v,
            names[v], s.tris, s.open, s.doubled, s.inward, s.inner);
    }
}

} // namespace qvr::adguns
