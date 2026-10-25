#include "vr_alloccount.h"
// vr_skinrelief.cpp -- the relief of the weapons Quake VR makes from other models as they load (AD's upgraded weapons
// "<model>#vr", our burst rifle's variants "#adv<n>", AD's guns "#gun<n>", AD's monsters' melee weapons "#melee"): a
// normal map with heights for parallax mapping, made here from the skin, since none can be baked for them
// (bake_normals.py bakes our own models' files; AD's files, and anything made from them, are never shipped). In the
// style of our view models' baked maps (normaltiles.py's VIEW_MODEL, normaldetail.weapon_heights and
// normaltiles.paint_heights, ported): the dark seams painted into the skin grooved, its small painted rivets and studs
// raised, its wood and leather grained along their streaks; twice the skin's size (four times a skin under 256 wide);
// the heights in its alpha as normalbake.finish_heights makes them (the broad forms out, the 95th percentile the
// surface, AUTHORED_DEPTH units at alpha 0, deeper relief compressed towards HEIGHT_CAP), from the model's own units per
// skin texel. Drawn as a baked map is (vr_normalmap_authored, vr_parallax_authored; TexMgr_AuthoredHeights raises its
// islands' rims). The shapes (bevels) stay the model's own smooth vertex normals. Our burst rifle's variants take the
// rifle's baked map where their skin is the rifle's (its rows; vr_adgunart.cpp keeps its coordinates). Kept on disk with
// the made skins' maps (vr_normalmap_cache). vr_weapon_relief 0: the map made from the skin's colours, as before.

#include "vr_diskcache.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_jobs.hpp"

#include <initializer_list>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace
{

constexpr float linesDepth = 0.8f;   // a painted seam's groove, texels deep (weapon_heights' lines_depth)
constexpr float dotsHeight = 0.9f;   // a painted rivet's dome, texels high (paint_heights)
constexpr float grainDepth = 0.55f;  // the wood's and leather's grain (VIEW_MODEL's "grain")
constexpr float authoredDepth = 0.4f; // normalbake.AUTHORED_DEPTH: alpha 0 this many model units under the surface
constexpr float heightCap = 0.12f;   // normalbake.HEIGHT_CAP
constexpr float heightBroad = 0.35f; // normalbake.HEIGHT_BROAD: broader relief left to the normals (model units)
constexpr float defaultUnits = 0.25f; // model units a skin texel, when the model's aren't known (a weapon's typical)
constexpr float maxHeightUnits = 0.3f; // the heights' units a texel at most (makeMap)
constexpr int cacheVersion = 1;      // bump with any change that makes another map from the same skin (and cacheBuild)
constexpr const char* cacheBuild = "5e1ef00000000001"; // the disk cache's build for these maps (vr_normalmap_cache)
constexpr const char* rifleModel = "progs/v_gruntgun.mdl#adv"; // our burst rifle's variants (vr_adgunart.cpp)
constexpr const char* rifleMap = "progs/v_gruntgun.mdl_0_norm"; // the rifle's baked map (bake_normals.py)

// The model being loaded's units per skin texel (VR_SkinReliefBegin), and each map's, for a reload (vid_restart).
float pendingUnits;
struct MapUnits
{
    const gltexture_t* glt;
    float units;
};
MapUnits mapUnits[64];
int mapUnitsNext;

// The passes over a map's rows shared out on the game's pool (each row written by one chunk: the same numbers as on one
// thread); small images stay on the calling thread.
qvr::jobs::Site reliefSite{"weapon reliefs made"};

template <typename F>
void forRows(int count, int texels, F&& f)
{
    qvr::jobs::parallelFor(reliefSite, (za::SizeT)(count > 0 ? count : 0), 8,
        [&f](za::SizeT begin, za::SizeT end) {
            for(za::SizeT i = begin; i < end; i++)
            {
                f((int)i);
            }
        },
        texels >= 64 * 64);
}

// A float buffer on the heap, freed with its scope.
struct Floats
{
    float* p;
    explicit Floats(size_t n) : p(static_cast<float*>(VR_HeapMalloc(n * sizeof(float)))) {}
    ~Floats() { VR_HeapFree(p); }
    Floats(const Floats&) = delete;
    Floats& operator=(const Floats&) = delete;
    float& operator[](size_t i) { return p[i]; }
    float operator[](size_t i) const { return p[i]; }
};

float smooth01(float x)
{
    x = x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
    return x * x * (3.f - 2.f * x);
}

float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// A Gaussian blur (separable, the edges clamped), normalised within `cov` (nothing outside bleeds in): normalbake.blur.
void blur(const float* in, float* out, const float* cov, int w, int h, float sigma)
{
    const size_t n = (size_t)w * h;
    if(sigma <= 0.f)
    {
        memcpy(out, in, n * sizeof(float));
        return;
    }
    const int r = (int)ceilf(sigma * 3.f);
    Floats k((size_t)(2 * r + 1)), a(n * 2), b(n * 2);
    float sum = 0.f;
    for(int i = -r; i <= r; i++)
    {
        k[(size_t)(i + r)] = expf(-0.5f * (i / sigma) * (i / sigma));
        sum += k[(size_t)(i + r)];
    }
    for(int i = 0; i <= 2 * r; i++)
    {
        k[(size_t)i] /= sum;
    }
    for(size_t i = 0; i < n; i++)
    {
        a[i] = in[i] * cov[i];
        a[n + i] = cov[i];
    }
    for(int c = 0; c < 2; c++) // along the rows, then down the columns
    {
        const float* src = a.p + c * n;
        float* mid = b.p + c * n;
        const float* kk = k.p;
        forRows(h, w * h, [&](int y) {
            for(int x = 0; x < w; x++)
            {
                float s = 0.f;
                for(int i = -r; i <= r; i++)
                {
                    const int xx = x + i < 0 ? 0 : (x + i >= w ? w - 1 : x + i);
                    s += kk[i + r] * src[(size_t)y * w + xx];
                }
                mid[(size_t)y * w + x] = s;
            }
        });
        float* dst = a.p + c * n;
        forRows(h, w * h, [&](int y) {
            for(int x = 0; x < w; x++)
            {
                float s = 0.f;
                for(int i = -r; i <= r; i++)
                {
                    const int yy = y + i < 0 ? 0 : (y + i >= h ? h - 1 : y + i);
                    s += kk[i + r] * mid[(size_t)yy * w + x];
                }
                dst[(size_t)y * w + x] = s;
            }
        });
    }
    for(size_t i = 0; i < n; i++)
    {
        out[i] = a[i] / (a[n + i] > 1e-6f ? a[n + i] : 1e-6f);
    }
}

float at(const float* img, int w, int h, int x, int y)
{
    x = x < 0 ? 0 : (x >= w ? w - 1 : x);
    y = y < 0 ? 0 : (y >= h ? h - 1 : y);
    return img[(size_t)y * w + x];
}

// Bilinear, at texel coordinates (x, y: a texel's middle at its index), the edges clamped.
float bilinear(const float* img, int w, int h, float x, float y)
{
    const int x0 = (int)floorf(x), y0 = (int)floorf(y);
    const float fx = x - x0, fy = y - y0;
    return (at(img, w, h, x0, y0) * (1.f - fx) + at(img, w, h, x0 + 1, y0) * fx) * (1.f - fy) +
           (at(img, w, h, x0, y0 + 1) * (1.f - fx) + at(img, w, h, x0 + 1, y0 + 1) * fx) * fy;
}

// The brightness's curvatures (the Hessian's eigenvalues, l1 the larger) at a texel.
void curvatures(const float* L, int w, int h, int x, int y, float& l1, float& l2)
{
    const float c = at(L, w, h, x, y);
    const float dxx = at(L, w, h, x + 1, y) - 2.f * c + at(L, w, h, x - 1, y);
    const float dyy = at(L, w, h, x, y + 1) - 2.f * c + at(L, w, h, x, y - 1);
    const float dxy = (at(L, w, h, x + 1, y + 1) - at(L, w, h, x - 1, y + 1) - at(L, w, h, x + 1, y - 1) +
                          at(L, w, h, x - 1, y - 1)) * 0.25f;
    const float tr = dxx + dyy, det = dxx * dyy - dxy * dxy;
    const float disc = sqrtf(fmaxf(tr * tr * 0.25f - det, 0.f));
    l1 = tr * 0.5f + disc;
    l2 = tr * 0.5f - disc;
}

// The 8-connected pieces of v > 0.05, each weighed by `keep(width, height)` of its extent (texels end to end): v
// scaled by its piece's weight (normalbake.labels and the loops after it).
template <typename Keep>
void keepPieces(float* v, int w, int h, Keep&& keep)
{
    const size_t n = (size_t)w * h;
    int* parent = static_cast<int*>(VR_HeapMalloc(n * sizeof(int)));
    int* box = static_cast<int*>(VR_HeapMalloc(n * 4 * sizeof(int)));
    auto find = [&](int i) {
        while(parent[i] != i)
        {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    for(size_t i = 0; i < n; i++)
    {
        parent[i] = v[i] > 0.05f ? (int)i : -1;
    }
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const int i = y * w + x;
            if(parent[i] < 0)
            {
                continue;
            }
            const int nb[4][2] = {{x - 1, y}, {x - 1, y - 1}, {x, y - 1}, {x + 1, y - 1}};
            for(const auto& q : nb)
            {
                if(q[0] < 0 || q[0] >= w || q[1] < 0)
                {
                    continue;
                }
                const int j = q[1] * w + q[0];
                if(parent[j] < 0)
                {
                    continue;
                }
                const int a = find(i), b = find(j);
                if(a != b)
                {
                    parent[a < b ? b : a] = a < b ? a : b;
                }
            }
        }
    }
    for(size_t i = 0; i < n; i++)
    {
        box[i * 4 + 0] = INT_MAX;
        box[i * 4 + 1] = INT_MAX;
        box[i * 4 + 2] = INT_MIN;
        box[i * 4 + 3] = INT_MIN;
    }
    for(size_t i = 0; i < n; i++)
    {
        if(parent[i] < 0)
        {
            continue;
        }
        const int r = find((int)i), x = (int)(i % w), y = (int)(i / w);
        int* b = box + (size_t)r * 4;
        b[0] = x < b[0] ? x : b[0];
        b[1] = y < b[1] ? y : b[1];
        b[2] = x > b[2] ? x : b[2];
        b[3] = y > b[3] ? y : b[3];
    }
    for(size_t i = 0; i < n; i++)
    {
        if(parent[i] >= 0)
        {
            const int* b = box + (size_t)find((int)i) * 4;
            v[i] *= keep(b[2] - b[0], b[3] - b[1]);
        }
        else
        {
            v[i] = 0.f;
        }
    }
    VR_HeapFree(box);
    VR_HeapFree(parent);
}

// The skin's heights in texels (paint_heights): -linesDepth on its painted seams (less on wood), +dotsHeight on its
// painted rivets, the grain on its wood and leather; spread 6 texels out of the islands (normalbake.dilate).
void paintHeights(const byte* rgba, const float* cov, int w, int h, float* out)
{
    const size_t n = (size_t)w * h;
    Floats lum(n), lum2(n), mu(n), sd(n), L(n), tmp(n), lines(n), dots(n), wood(n);
    for(size_t i = 0; i < n; i++)
    {
        lum[i] = (rgba[i * 4] * 0.3f + rgba[i * 4 + 1] * 0.59f + rgba[i * 4 + 2] * 0.11f) / 255.f;
        lum2[i] = lum[i] * lum[i];
    }
    // the local speckle: the brightness's spread over about 3 texels
    blur(lum.p, mu.p, cov, w, h, 3.f);
    blur(lum2.p, sd.p, cov, w, h, 3.f);
    for(size_t i = 0; i < n; i++)
    {
        sd[i] = sqrtf(fmaxf(sd[i] - mu[i] * mu[i], 1e-6f));
    }

    // seams (normalbake.painted_lines, lo 0.12, hi 0.4, 6 to 16 texels long): narrow valleys of the brightness, longer
    // than wide, deep against the speckle
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = 0.f;
    }
    for(const float s : {0.8f, 1.4f})
    {
        blur(lum.p, L.p, cov, w, h, s);
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                float l1, l2;
                curvatures(L.p, w, h, x, y, l1, l2);
                const float valley = fmaxf(l1, 0.f) * s * s;
                const float elong = clampf((l1 - fabsf(l2)) / fmaxf(l1, 1e-9f), 0.f, 1.f);
                const size_t i = (size_t)y * w + x;
                tmp[i] = fmaxf(tmp[i], valley * elong / (sd[i] + 0.02f));
            }
        }
    }
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = smooth01((tmp[i] - 0.12f) / 0.28f) * cov[i];
    }
    keepPieces(tmp.p, w, h, [](int dx, int dy) { return smooth01((sqrtf((float)(dx * dx + dy * dy)) - 6.f) / 10.f); });
    blur(tmp.p, lines.p, cov, w, h, 0.5f);

    // rivets and studs (normaltiles.painted_dots): bright peaks a texel or two across, round, over the speckle
    blur(lum.p, L.p, cov, w, h, 0.8f);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            float l1, l2;
            curvatures(L.p, w, h, x, y, l1, l2);
            const size_t i = (size_t)y * w + x;
            const float peak = l1 < 0.f ? -l1 : 0.f;
            tmp[i] = smooth01((peak / (sd[i] + 0.02f) - 0.25f) / 0.35f) * cov[i];
        }
    }
    keepPieces(tmp.p, w, h, [](int dx, int dy) { return (dx > dy ? dx : dy) + 1 <= 3.5f ? 1.f : 0.f; });
    blur(tmp.p, dots.p, cov, w, h, 0.6f);

    // wood and leather (normaldetail.wood_mask): warm browns, saturated enough
    for(size_t i = 0; i < n; i++)
    {
        const float r = rgba[i * 4] / 255.f, g = rgba[i * 4 + 1] / 255.f, b = rgba[i * 4 + 2] / 255.f;
        const float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
        float hue = 0.f;
        if(d > 1e-6f)
        {
            hue = mx == r ? fmodf((g - b) / d + 6.f, 6.f) : (mx == g ? (b - r) / d + 2.f : (r - g) / d + 4.f);
        }
        hue *= 60.f;
        const float sat = mx > 1e-6f ? d / mx : 0.f;
        tmp[i] = smooth01((sat - 0.28f) / 0.12f) * smooth01((hue - 18.f) / 5.f) * smooth01((42.f - hue) / 6.f) *
                 smooth01((mx - 0.06f) / 0.05f);
    }
    blur(tmp.p, wood.p, cov, w, h, 1.2f);

    // its grain (normaldetail.grain, the streaks; not the checkering): the fine paint averaged along the way it streaks
    Floats hp(n), gx(n), gy(n), jxx(n), jxy(n), jyy(n), streak(n);
    blur(lum.p, hp.p, cov, w, h, 0.7f);
    blur(lum.p, tmp.p, cov, w, h, 2.5f);
    for(size_t i = 0; i < n; i++)
    {
        hp[i] -= tmp[i];
    }
    blur(lum.p, L.p, cov, w, h, 0.8f);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const size_t i = (size_t)y * w + x;
            gx[i] = (at(L.p, w, h, x + 1, y) - at(L.p, w, h, x - 1, y)) * 0.5f;
            gy[i] = (at(L.p, w, h, x, y + 1) - at(L.p, w, h, x, y - 1)) * 0.5f;
        }
    }
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = gx[i] * gx[i];
    }
    blur(tmp.p, jxx.p, cov, w, h, 3.f);
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = gx[i] * gy[i];
    }
    blur(tmp.p, jxy.p, cov, w, h, 3.f);
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = gy[i] * gy[i];
    }
    blur(tmp.p, jyy.p, cov, w, h, 3.f);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const size_t i = (size_t)y * w + x;
            if(wood[i] < 1e-3f)
            {
                streak[i] = 0.f;
                continue;
            }
            const float ang = 0.5f * atan2f(2.f * jxy[i], jxx[i] - jyy[i]); // across the streaks
            const float dx = -sinf(ang), dy = cosf(ang);                     // along them
            float acc = 0.f;
            for(int k = -4; k <= 4; k++)
            {
                acc += bilinear(hp.p, w, h, x + k * dx, y + k * dy);
            }
            streak[i] = acc / 9.f;
        }
    }
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = streak[i] * streak[i];
    }
    blur(tmp.p, L.p, cov, w, h, 4.f);
    for(size_t i = 0; i < n; i++)
    {
        const float tr = jxx[i] + jyy[i];
        const float coh = sqrtf((jxx[i] - jyy[i]) * (jxx[i] - jyy[i]) + 4.f * jxy[i] * jxy[i]) / fmaxf(tr, 1e-9f);
        const float streaks = clampf(streak[i] / (sqrtf(L[i]) + 0.004f), -2.f, 2.f) * 0.5f;
        const float along = clampf((coh - 0.25f) / 0.25f, 0.f, 1.f);
        out[i] = -linesDepth * lines[i] * (1.f - 0.6f * wood[i]) + dotsHeight * dots[i] +
                 streaks * along * grainDepth * wood[i];
    }

    // out of the islands, ring by ring: the mean of the covered neighbours (the map's bilinear taps at a seam)
    for(size_t i = 0; i < n; i++)
    {
        tmp[i] = cov[i] > 0.5f ? 1.f : 0.f;
    }
    for(int ring = 0; ring < 6; ring++)
    {
        bool grew = false;
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                const size_t i = (size_t)y * w + x;
                if(tmp[i] > 0.f)
                {
                    continue;
                }
                float acc = 0.f, cnt = 0.f;
                for(int dy = -1; dy <= 1; dy++)
                {
                    for(int dx = -1; dx <= 1; dx++)
                    {
                        const int xx = x + dx, yy = y + dy;
                        if((dx || dy) && xx >= 0 && xx < w && yy >= 0 && yy < h && tmp[(size_t)yy * w + xx] == 1.f)
                        {
                            acc += out[(size_t)yy * w + xx];
                            cnt += 1.f;
                        }
                    }
                }
                if(cnt > 0.f)
                {
                    out[i] = acc / cnt;
                    tmp[i] = 0.5f; // grown this ring (counted from the next)
                    grew = true;
                }
            }
        }
        for(size_t i = 0; i < n; i++)
        {
            tmp[i] = tmp[i] > 0.f ? 1.f : 0.f;
        }
        if(!grew)
        {
            break;
        }
    }
}

// The map (scale times the skin's size, RGBA) from the skin `rgba` (w x h), its islands `cov` and the model's units
// per skin texel.
void makeMap(const byte* rgba, const float* cov, int w, int h, int scale, float units, byte* map)
{
    const size_t n = (size_t)w * h;
    Floats ht(n), hu(n), broad(n);
    paintHeights(rgba, cov, w, h, ht.p);

    // the heights for parallax mapping (finish_heights), at the skin's size: in model units, the broad forms out, the
    // top (95th percentile of the islands) the surface. A coarse skin (AD's monsters' weapons: about a unit a texel)
    // as deep in texels as our view models' (0.1 to 0.3 units a texel), not 4 times deeper: its every texel would
    // reach HEIGHT_CAP, the whole surface pitted, its paint swimming.
    const float hunits = fminf(units, maxHeightUnits);
    for(size_t i = 0; i < n; i++)
    {
        hu[i] = ht[i] * hunits;
    }
    blur(hu.p, broad.p, cov, w, h, heightBroad / hunits);
    int histogram[1024] = {}, counted = 0;
    constexpr float lo = -1.f, hi = 1.f; // model units: the relief's range (a few tenths)
    for(size_t i = 0; i < n; i++)
    {
        hu[i] -= broad[i];
        if(cov[i] > 0.5f)
        {
            histogram[(int)clampf((hu[i] - lo) / (hi - lo) * 1023.f, 0.f, 1023.f)]++;
            counted++;
        }
    }
    int b = 1023;
    for(int above = 0; b > 0 && above + histogram[b] <= counted / 20; b--)
    {
        above += histogram[b];
    }
    const float top = lo + (b + 0.5f) / 1023.f * (hi - lo);
    bool relief = false;
    for(size_t i = 0; i < n; i++)
    {
        const float d = fmaxf(top - hu[i], 0.f);
        hu[i] = cov[i] > 0.5f ? clampf(1.f - heightCap * tanhf(d / heightCap) / authoredDepth, 0.f, 1.f) : 1.f;
        relief = relief || hu[i] < 254.5f / 255.f;
    }

    // up to the map's size: the normals from the heights' slopes (texels a texel; green up the image), the alpha
    const int W = w * scale, H = h * scale;
    const float step = 1.f / scale;
    forRows(H, W * H, [&](int Y) {
        for(int X = 0; X < W; X++)
        {
            const float sx = (X + 0.5f) * step - 0.5f, sy = (Y + 0.5f) * step - 0.5f;
            const float ddx = (bilinear(ht.p, w, h, sx + step, sy) - bilinear(ht.p, w, h, sx - step, sy)) / (2.f * step);
            const float ddy = (bilinear(ht.p, w, h, sx, sy + step) - bilinear(ht.p, w, h, sx, sy - step)) / (2.f * step);
            const float nx = -ddx, ny = ddy, len = 1.f / sqrtf(nx * nx + ny * ny + 1.f);
            byte* o = map + ((size_t)Y * W + X) * 4;
            o[0] = (byte)clampf((nx * len * 0.5f + 0.5f) * 255.f + 0.5f, 0.f, 255.f);
            o[1] = (byte)clampf((ny * len * 0.5f + 0.5f) * 255.f + 0.5f, 0.f, 255.f);
            o[2] = (byte)clampf((len * 0.5f + 0.5f) * 255.f + 0.5f, 0.f, 255.f);
            o[3] = relief ? (byte)clampf(bilinear(hu.p, w, h, sx, sy) * 255.f + 0.5f, 0.f, 255.f) : 255;
        }
    });
}

// The map's relief within the islands, for the log (as scratch statistics of the baked maps): the mean tilt
// (degrees), the share tilted over 5 degrees, the mean and 10th percentile of the alpha.
void reliefStats(const byte* map, int W, int H, const byte* mask, int mw, int mh, float stats[4])
{
    double tilt = 0.0, alpha = 0.0;
    int over = 0, count = 0, alphas[256] = {};
    for(int Y = 0; Y < H; Y++)
    {
        for(int X = 0; X < W; X++)
        {
            if(mask && !mask[(size_t)(Y * mh / H) * mw + X * mw / W])
            {
                continue;
            }
            const byte* o = map + ((size_t)Y * W + X) * 4;
            const float z = clampf(o[2] / 127.5f - 1.f, -1.f, 1.f), t = acosf(z) * 57.29578f;
            tilt += t;
            over += t > 5.f;
            alpha += o[3];
            alphas[o[3]]++;
            count++;
        }
    }
    int p10 = 0;
    for(int seen = 0; p10 < 255 && seen + alphas[p10] < count / 10; p10++)
    {
        seen += alphas[p10];
    }
    stats[0] = count ? (float)(tilt / count) : 0.f;
    stats[1] = count ? 100.f * over / count : 0.f;
    stats[2] = count ? (float)(alpha / count) : 255.f;
    stats[3] = (float)p10;
}

[[nodiscard]] int scaleOf(int width)
{
    return width < 256 ? 4 : 2; // normaltiles.bake: VIEW_MODEL's 2, at least 4 times a small skin
}

[[nodiscard]] bool isRifleVariant(const gltexture_t* glt)
{
    return !strncmp(glt->name, rifleModel, strlen(rifleModel));
}

// Our burst rifle's variants: the rifle's own baked map over the rows of their skin that are the rifle's.
void rifleRows(byte* map, int W, int H)
{
    int fw, fh;
    enum srcformat fmt;
    const int mark = Hunk_LowMark();
    byte* img = Image_LoadImage(rifleMap, &fw, &fh, &fmt);
    if(img && fmt == SRC_RGBA && fw == W && fh <= H)
    {
        memcpy(map, img, (size_t)fw * fh * 4);
    }
    Hunk_FreeToLowMark(mark);
}

} // namespace

/*
=================
VR_SkinReliefWanted -- whether an alias model's skins get their relief made here (a weapon made from another model's
file: its name has #vr, #adv, #gun or #melee; vr_weapon_relief)
=================
*/
extern "C" int VR_SkinReliefWanted(const char* model)
{
    if(qvr::vr_weapon_relief.value == 0.f || !model)
    {
        return 0;
    }
    const char* tag = strchr(model, '#');
    return tag && (!strncmp(tag, "#vr", 3) || !strncmp(tag, "#adv", 4) || !strncmp(tag, "#gun", 4) || !strncmp(tag, "#melee", 6));
}

/*
=================
VR_SkinReliefBegin -- the model being loaded's model units per skin texel (the median over its skin's area, from its
first frame: `frames` points at the file's frames), for the heights of the reliefs made next; 0 after them
=================
*/
extern "C" void VR_SkinReliefBegin(const void* hdr, const void* stverts, const void* triangles, const void* frames)
{
    pendingUnits = 0.f;
    if(!hdr)
    {
        return;
    }
    const aliashdr_t* pheader = static_cast<const aliashdr_t*>(hdr);
    const stvert_t* verts = static_cast<const stvert_t*>(stverts);
    const dtriangle_t* tris = static_cast<const dtriangle_t*>(triangles);
    const byte* p = static_cast<const byte*>(frames);
    int type;
    memcpy(&type, p, 4);
    p += 4;
    if(LittleLong(type) != ALIAS_SINGLE)
    {
        int count;
        memcpy(&count, p, 4);
        p += sizeof(daliasgroup_t) + (size_t)LittleLong(count) * sizeof(daliasinterval_t);
    }
    const trivertx_t* v = reinterpret_cast<const trivertx_t*>(p + sizeof(daliasframe_t));
    const int numtris = pheader->numtris, w = pheader->skinwidth;
    float* units = static_cast<float*>(VR_HeapMalloc((size_t)(numtris > 0 ? numtris : 1) * 2 * sizeof(float)));
    int count = 0;
    float total = 0.f;
    for(int i = 0; i < numtris; i++)
    {
        float pos[3][3], uv[3][2];
        bool ok = true;
        for(int j = 0; j < 3; j++)
        {
            const int k = tris[i].vertindex[j];
            if(k < 0 || k >= pheader->numverts)
            {
                ok = false;
                break;
            }
            for(int c = 0; c < 3; c++)
            {
                pos[j][c] = v[k].v[c] * pheader->scale[c] + pheader->scale_origin[c];
            }
            uv[j][0] = (float)verts[k].s + (!tris[i].facesfront && verts[k].onseam ? w / 2 : 0);
            uv[j][1] = (float)verts[k].t;
        }
        if(!ok)
        {
            continue;
        }
        float e1[3], e2[3];
        for(int c = 0; c < 3; c++)
        {
            e1[c] = pos[1][c] - pos[0][c];
            e2[c] = pos[2][c] - pos[0][c];
        }
        const float cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
        const float area = 0.5f * sqrtf(cx * cx + cy * cy + cz * cz);
        const float uvArea = 0.5f * fabsf((uv[1][0] - uv[0][0]) * (uv[2][1] - uv[0][1]) - (uv[2][0] - uv[0][0]) * (uv[1][1] - uv[0][1]));
        if(area <= 1e-6f || uvArea <= 1e-3f)
        {
            continue;
        }
        units[count * 2] = sqrtf(area / uvArea);
        units[count * 2 + 1] = uvArea;
        total += uvArea;
        count++;
    }
    if(count)
    {
        qsort(units, (size_t)count, 2 * sizeof(float), [](const void* a, const void* b) {
            const float x = *static_cast<const float*>(a), y = *static_cast<const float*>(b);
            return (x > y) - (x < y);
        });
        float seen = 0.f;
        int i = 0;
        for(; i < count - 1 && seen + units[i * 2 + 1] < total * 0.5f; i++)
        {
            seen += units[i * 2 + 1];
        }
        pendingUnits = units[i * 2];
    }
    VR_HeapFree(units);
}

extern "C" void VR_SkinReliefEnd(void)
{
    pendingUnits = 0.f;
}

/*
=================
VR_SkinRelief -- TexMgr_LoadImage32: the relief map of a skin (`data`, RGBA, glt's size; NORMALMAP_RELIEF) made, at
its scale (on the hunk; glt's size set to it), from the disk cache when it has it
=================
*/
extern "C" byte* VR_SkinRelief(gltexture_t* glt, byte* data)
{
    const double t0 = Sys_DoubleTime();
    const int w = (int)glt->width, h = (int)glt->height, scale = scaleOf(w), W = w * scale, H = h * scale;
    const size_t n = (size_t)w * h;

    // its model's units per texel: the model being loaded's, else as made before (a reload), else a weapon's typical
    float units = pendingUnits;
    for(int i = 0; units <= 0.f && i < (int)countof(mapUnits); i++)
    {
        if(mapUnits[i].glt == glt)
        {
            units = mapUnits[i].units;
        }
    }
    if(units <= 0.f)
    {
        units = defaultUnits;
    }
    else if(pendingUnits > 0.f)
    {
        mapUnits[mapUnitsNext] = {glt, units};
        mapUnitsNext = (mapUnitsNext + 1) % (int)countof(mapUnits);
    }

    int mw = 0, mh = 0;
    const byte* mask = VR_SkinIslands(&mw, &mh);
    byte* map = static_cast<byte*>(Hunk_AllocNoFill((size_t)W * H * 4));
    unsigned long long key = qvr::diskcache::fnvStart;
    const int cache = VR_NormalCacheMode();
    {
        const int params[6] = {cacheVersion, w, h, scale, mask ? mw : 0, mask ? mh : 0};
        key = qvr::diskcache::fnv(key, "skin relief", 11);
        key = qvr::diskcache::fnv(key, params, sizeof(params));
        key = qvr::diskcache::fnv(key, &units, sizeof(units));
        key = qvr::diskcache::fnv(key, data, n * 4);
        if(mask)
        {
            key = qvr::diskcache::fnv(key, mask, (size_t)mw * mh);
        }
    }
    const bool hit = cache == 1 && VR_ReliefCacheLoad(cacheBuild, key, map, W, H);
    if(!hit)
    {
        Floats cov(n);
        for(int y = 0; y < h; y++)
        {
            for(int x = 0; x < w; x++)
            {
                cov[(size_t)y * w + x] = !mask || mask[(size_t)(y * mh / h) * mw + x * mw / w] ? 1.f : 0.f;
            }
        }
        makeMap(data, cov.p, w, h, scale, units, map);
        if(cache)
        {
            VR_ReliefCacheStore(cacheBuild, key, map, W, H);
        }
    }
    if(isRifleVariant(glt))
    {
        rifleRows(map, W, H);
    }
    glt->width = (unsigned)W;
    glt->height = (unsigned)H;
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    if(developer.value)
    {
        float stats[4];
        reliefStats(map, W, H, mask, mw, mh, stats);
        Con_DPrintf("skinrelief: %s %d x %d (%.3f units a texel) %s in %.1f ms: tilt mean %.2f, %.1f%% over 5, alpha mean "
                    "%.1f p10 %.0f\n",
            glt->name, W, H, units, hit ? "read" : "made", ms, stats[0], stats[1], stats[2], stats[3]);
    }
    return map;
}
