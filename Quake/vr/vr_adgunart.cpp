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

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
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

// The fuelled guns' canister under the receiver, ahead of the grip, and the hose's inlet at its back end.
const glm::vec3 canisterBack{5.f, 0.f, 3.4f}, canisterFront{12.f, 0.f, 3.4f};
constexpr float canisterRadius = 1.9f;
const glm::vec3 inletAt{3.2f, 0.f, 2.4f};
const glm::vec3 inletDir{-0.45f, 0.f, -0.89f};

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
    for(int k = 0; k < sides; k++)
    {
        const int a0 = first + k, a1 = first + k + 1, c0 = first + sides + 1 + k, c1 = first + sides + 2 + k;
        const float ang = 2.f * pi * (static_cast<float>(k) + 0.5f) / static_cast<float>(sides);
        const glm::vec3 out = (e1 * cosf(ang) + e2 * sinf(ang)) * flip;
        b.tri(a0, a1, c1, out);
        b.tri(a0, c1, c0, out);
    }
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
// trigger, the sights), scaled by `k` and moved by `offset`, over the rifle; its skin pasted at half size (lifted a
// little: its dark greys read black) into the rows from `skinRow` (its texels' coordinates halved: its seam's half
// width with them).
void graftLauncher(Build& b, const Mdl& g, float k, const glm::vec3& offset, int skinRow)
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
        return count[r] >= 8 && lo[r].x >= -4.5f && hi[r].z >= 6.f;
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
                out.p = g.pos(v[c]) * k + offset;
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
            // The launch tube over the rifle, on two mounts; its rear an exhaust's bell, dark and sooty inside; a warhead's
            // nose in its mouth.
            const float z = 17.3f, r = 2.4f;
            const Rect tubeR = tubeRect(b, r, 33.f, 1.5f);
            paintRect(b, tubeR, 2.f * pi * r, 33.f, [](float u, float v) {
                if(v > 28.f && v < 29.6f)
                {
                    return stripes(u, v, 0.7f);
                }
                const float circ = 2.f * pi * 2.4f, side = fractf((u + 2.6f) / (circ * 0.5f)) * circ * 0.5f;
                if(side < 5.2f && v > 4.f && v < 26.f)
                {
                    return rockets(v, side, 5.2f, olive);
                }
                return paint(olive);
            });
            tube(b, glm::vec3{-6.f, 0.f, z}, glm::vec3{27.f, 0.f, z}, r, r, 12, tubeR, false, false);
            Rect inner;
            (void)b.alloc(16, 8, inner);
            paintRect(b, inner, 1.f, 1.f, [](float, float v) { return paint(black * (1.f + v)); });
            tube(b, glm::vec3{-6.f, 0.f, z}, glm::vec3{27.f, 0.f, z}, r * 0.86f, r * 0.86f, 12, inner, false, false, true);
            Rect bell;
            (void)b.alloc(32, 8, bell);
            paintRect(b, bell, 1.f, 1.f, [](float u, float v) { return paint(glm::mix(darkSteel, copper * 0.6f, v) * (0.8f + 0.4f * fractf(u * 6.f))); });
            tube(b, glm::vec3{-6.f, 0.f, z}, glm::vec3{-10.5f, 0.f, z}, r * 1.02f, r * 1.45f, 12, bell, false, false);
            tube(b, glm::vec3{-6.f, 0.f, z}, glm::vec3{-10.5f, 0.f, z}, r * 0.86f, r * 1.35f, 12, inner, false, false, true);
            Rect ring;
            (void)b.alloc(32, 4, ring);
            paintRect(b, ring, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            tube(b, glm::vec3{26.f, 0.f, z}, glm::vec3{28.f, 0.f, z}, r * 1.12f, r * 1.12f, 12, ring, false, false);
            tube(b, glm::vec3{-6.6f, 0.f, z}, glm::vec3{-5.f, 0.f, z}, r * 1.12f, r * 1.12f, 12, ring, false, false);
            Rect nose;
            (void)b.alloc(16, 6, nose);
            paintRect(b, nose, 1.f, 1.f, [](float, float v) { return paint(v < 0.5f ? red : red * 0.7f); });
            tube(b, glm::vec3{24.5f, 0.f, z}, glm::vec3{27.6f, 0.f, z}, r * 0.8f, 0.25f, 10, nose, true, true);
            Rect mount;
            (void)b.alloc(8, 8, mount);
            paintRect(b, mount, 1.f, 1.f, [](float, float) { return paint(darkSteel); });
            box(b, glm::vec3{0.f, 0.f, 14.6f}, glm::vec3{1.6f, 1.1f, 1.f}, mount);
            box(b, glm::vec3{19.f, 0.f, 14.6f}, glm::vec3{1.6f, 1.1f, 1.f}, mount);
            break;
        }
        case Grenade:
        {
            recolour(b, Theme{steel, 0.35f});
            decalSides(b, m, glm::vec3{12.5f, lo.y, 11.5f}, glm::vec3{23.5f, hi.y, 13.f}, [](float u, float v) { return stripes(u, v, 0.7f); });
            if(launcher)
            {
                // Its barrel and drum (x -4 .. 31.4, z -0.8 .. 9.6) laid along the top of the rifle at 0.8: x 1.8 .. 30.1,
                // z 14 .. 22.3.
                const int row = b.atlasY;
                b.atlasY += launcher->skinH / 2 + 1;
                graftLauncher(b, *launcher, 0.8f, glm::vec3{5.f, 0.f, 14.6f}, row);
            }
            break;
        }
        case Plasma:
        case Eliminator:
        case Centurion:
        {
            const glm::vec3 tint = variant == Plasma ? glm::vec3{0.32f, 0.5f, 1.f}
                                   : variant == Eliminator ? glm::vec3{0.45f, 0.38f, 1.f}
                                                           : glm::vec3{0.3f, 0.72f, 1.f};
            recolour(b, Theme{tint, 0.85f});
            const glm::vec3 ground = tint * 0.32f;
            if(variant == Eliminator)
            {
                // The eliminator's: a bank of glowing cells (bars in a grid), not lightning.
                decalSides(b, m, glm::vec3{12.5f, lo.y, 3.f}, glm::vec3{23.5f, hi.y, 13.5f}, [ground](float u, float v) {
                    const float cu = fractf(u / 1.6f), cv = fractf(v / 3.4f);
                    if(cu > 0.25f && cu < 0.75f && cv > 0.15f && cv < 0.85f)
                    {
                        return paint(cu > 0.4f && cu < 0.6f ? glm::vec3{0.9f, 0.85f, 1.f} : glm::vec3{0.6f, 0.5f, 1.f}, true);
                    }
                    return paint(ground);
                });
            }
            else
            {
                decalSides(b, m, glm::vec3{12.5f, lo.y, 3.f}, glm::vec3{23.5f, hi.y, 13.5f}, [ground, variant](float u, float v) {
                    return lightning(u, v, 10.5f, variant == Centurion ? 3 : 2, ground, static_cast<float>(variant));
                });
            }
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
            else if(variant == Eliminator)
            {
                // Coils round the barrel, a glowing tube under it.
                for(int k = 0; k < 3; k++)
                {
                    const float x = 30.6f + 0.85f * static_cast<float>(k);
                    tube(b, glm::vec3{x, 0.f, 8.1f}, glm::vec3{x + 0.55f, 0.f, 8.1f}, 1.7f, 1.7f, 12, capR, true, true);
                }
                tube(b, glm::vec3{24.f, 0.f, 5.5f}, glm::vec3{31.5f, 0.f, 5.5f}, 0.8f, 0.8f, 10, glassR, true, true);
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
                    tube(b, glm::vec3{x, 0.f, 14.f}, glm::vec3{x, 0.f, 17.f}, 0.75f, 0.75f, 8, glassR, false, false);
                    tube(b, glm::vec3{x, 0.f, 17.f}, glm::vec3{x, 0.f, 18.2f}, 0.75f, 0.15f, 8, glassR, false, true);
                    tube(b, glm::vec3{x, 0.f, 13.9f}, glm::vec3{x, 0.f, 14.8f}, 1.05f, 1.05f, 8, capR, false, true);
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
            tube(b, glm::vec3{24.f, 0.f, 5.6f}, glm::vec3{32.4f, 0.f, 5.6f}, 0.95f, 0.95f, 10, magR, false, true);
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
            tube(b, inletAt, canisterBack + glm::vec3{0.8f, 0.f, -1.f}, 0.55f, 0.55f, 8, inR, true, false);
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
                // The nozzle's flared bell round the barrel's end; a pilot light under it.
                tube(b, glm::vec3{28.5f, 0.f, 8.1f}, glm::vec3{muzzleX - 0.1f, 0.f, 8.1f}, 1.3f, 2.1f, 12, nozR, false, false);
                tube(b, glm::vec3{28.5f, 0.f, 8.1f}, glm::vec3{muzzleX - 0.1f, 0.f, 8.1f}, 1.1f, 1.85f, 12, inR, false, false, true);
                tube(b, glm::vec3{26.f, 0.f, 5.6f}, glm::vec3{31.f, 0.f, 5.6f}, 0.5f, 0.5f, 8, inR, true, false);
                Rect flame;
                (void)b.alloc(16, 8, flame);
                paintRect(b, flame, 1.f, 1.f, [](float, float v) { return paint(v < 0.5f ? glm::vec3{0.5f, 0.7f, 1.f} : flameYellow, true); });
                tube(b, glm::vec3{31.f, 0.f, 5.6f}, glm::vec3{32.6f, 0.f, 5.6f}, 0.4f, 0.05f, 8, flame, false, true);
            }
            else
            {
                // A wide flat spray head on the barrel's end, its slot dark.
                box(b, glm::vec3{31.f, 0.f, 8.1f}, glm::vec3{1.9f, 2.2f, 0.85f}, nozR);
                tube(b, glm::vec3{27.5f, 0.f, 8.1f}, glm::vec3{29.2f, 0.f, 8.1f}, 1.25f, 1.25f, 10, nozR, false, false);
            }
            break;
        }
        default: break;
    }
}

[[nodiscard]] bool makeVariant(int variant, const byte* src, size_t size, const byte* rockSrc, size_t rockSize, Built& out,
    const char*& why)
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

[[nodiscard]] bool buildVariant(int variant, unsigned int* path_id, Built& out, const char*& why)
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
    const bool ok = makeVariant(variant, src, size, rock, rockSize, out, why);
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
    return d[0] >= '0' && d[0] < '0' + variantCount && d[1] == 0 ? d[0] - '0' : -1;
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

void variants_f()
{
    static constexpr const char* names[variantCount] = {"rocket", "grenade", "plasma", "defender", "eliminator", "centurion",
        "pyro", "fumigator"};
    for(int v = 0; v < variantCount; v++)
    {
        Built b;
        const char* why = "";
        const double t0 = Sys_DoubleTime();
        if(!buildVariant(v, nullptr, b, why))
        {
            Con_Printf("adv%d %s: %s\n", v, names[v], why);
            continue;
        }
        Con_Printf("adv%d %s: %d triangles, %d vertices, box %.2f %.2f %.2f .. %.2f %.2f %.2f%s, %.1f ms\n", v, names[v], b.tris,
            b.verts, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z, b.cornerKept ? "" : " (its corner moved: past the settings')",
            (Sys_DoubleTime() - t0) * 1000.0);
    }
}

} // namespace qvr::adguns
