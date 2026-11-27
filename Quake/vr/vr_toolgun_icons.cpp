// vr_toolgun_icons.cpp -- thumbnails of models for the toolgun's panel (vr_toolgun_icons.hpp): each model's file read
// again (COM_LoadMallocFile: the loaded model keeps no triangles nor 8-bit skins), its triangles drawn by a small
// software rasteriser (orthographic, z-buffered, 2x2 supersampled) into a picture the 2D pass draws (gfx::Picture).
//   .mdl  the first frame (a group's first), the skin asked for (a group's first), the seam's back half as Quake maps it;
//   .bsp  version 29: its first model's faces, each with its texture (texinfo), skies and triggers left out.
// Lit flat per triangle from over the viewer's shoulder (both sides alike: the files' windings aren't trusted), the
// palette's fullbright colours unlit.

#include "vr_toolgun_icons.hpp"

#include "vr_alloccount.h"
#include "vr_attach.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_api.h"

#include <string.h>

using namespace qvr;

namespace
{

constexpr int iconSize = 96;  // the picture's pixels across and down
constexpr int superSample = 2; // drawn this many times larger each way, then averaged
constexpr int maxIcons = 512;

enum class IconState : int
{
    Waiting,
    Made,
    Failed
};

struct Icon
{
    char model[64]{};
    int skin{0};
    bool side{false};
    int attach{0}; // the attachment drawn on it (attach::Kind; 0 none)
    IconState state{IconState::Waiting};
    gfx::Picture picture;
};

// Every thumbnail asked for (the main thread: the panel's layout asks, the 2D pass makes them).
struct IconSet
{
    za::InPlaceVector<Icon, maxIcons> icons;
    double seconds{0.0}; // making them, all told
    int made{0};
};
IconSet iconSet;

// A triangle in model space, its texture coordinates in texels of texture `texture`.
struct Corner
{
    glm::vec3 p{0.f};
    glm::vec2 st{0.f};
};

struct Triangle
{
    Corner c[3];
    int texture{0};
};

struct Texture
{
    const byte* pixels{nullptr}; // 8-bit, the palette's
    int width{0};
    int height{0};
    bool plain{false}; // none: white, lit (a posed body: drawMesh)
};

// The rasteriser's buffers (one icon at a time; the 2D pass: the main thread).
struct IconScratch
{
    za::Vector<Triangle> triangles;
    za::Vector<Texture> textures;
    za::Vector<uint32_t> color; // the supersampled picture, RGBA
    za::Vector<float> depth;
    za::Vector<uint32_t> out; // the picture
    auto members() { return mem::list(triangles, textures, color, depth, out); }
};
mem::Scratch<IconScratch> scratch{"toolgun icons"};

// A file's bytes read at an offset, bounds-checked (false: past its end).
struct Reader
{
    const byte* data{nullptr};
    int size{0};

    [[nodiscard]] bool has(int ofs, int bytes) const { return ofs >= 0 && bytes >= 0 && ofs <= size - bytes; }

    [[nodiscard]] int i32(int ofs) const
    {
        int v = 0;
        ZA_MEMCPY(&v, data + ofs, 4);
        return LittleLong(v);
    }

    [[nodiscard]] int u16(int ofs) const { return data[ofs] | (data[ofs + 1] << 8); }

    [[nodiscard]] int s16(int ofs) const { return static_cast<short>(u16(ofs)); }

    [[nodiscard]] float f32(int ofs) const
    {
        float v = 0.f;
        ZA_MEMCPY(&v, data + ofs, 4);
        return LittleFloat(v);
    }
};

// An .mdl's first frame with skin `skin` (Quake's format: modelgen.h).
[[nodiscard]] bool readMdl(const Reader& r, int skin, za::Vector<Triangle>& tris, za::Vector<Texture>& textures)
{
    constexpr int headerSize = 84;
    constexpr int maxGroup = 1024; // the most skins or frames a group may have (more: a bad file)
    if(!r.has(0, headerSize) || r.i32(0) != IDPOLYHEADER)
    {
        return false;
    }
    const glm::vec3 scale{r.f32(8), r.f32(12), r.f32(16)};
    const glm::vec3 origin{r.f32(20), r.f32(24), r.f32(28)};
    const int numSkins = r.i32(48), skinWidth = r.i32(52), skinHeight = r.i32(56);
    const int numVerts = r.i32(60), numTris = r.i32(64), numFrames = r.i32(68);
    if(numSkins < 1 || skinWidth < 1 || skinHeight < 1 || numVerts < 3 || numTris < 1 || numFrames < 1 ||
        skinWidth > 4096 || skinHeight > 4096 || numVerts > 65536 || numTris > 131072)
    {
        return false;
    }
    const int skinBytes = skinWidth * skinHeight;
    const int wanted = za::clamp(skin, 0, numSkins - 1);
    int ofs = headerSize;
    Texture tex;
    for(int i = 0; i < numSkins; i++)
    {
        if(!r.has(ofs, 4))
        {
            return false;
        }
        const int type = r.i32(ofs);
        ofs += 4;
        int count = 1;
        if(type != 0)
        {
            if(!r.has(ofs, 4))
            {
                return false;
            }
            count = r.i32(ofs);
            if(count < 1 || count > maxGroup) // (a bad file's count: no int overflow below)
            {
                return false;
            }
            ofs += 4 + count * 4; // (the group's times)
        }
        if(count > r.size / skinBytes || !r.has(ofs, skinBytes * count))
        {
            return false;
        }
        if(i == wanted)
        {
            tex = {r.data + ofs, skinWidth, skinHeight}; // (a group's first)
        }
        ofs += skinBytes * count;
    }
    textures.pushBack(tex);

    const int stOfs = ofs;
    const int triOfs = stOfs + numVerts * 12;
    int frameOfs = triOfs + numTris * 16;
    if(!r.has(stOfs, numVerts * 12 + numTris * 16) || !r.has(frameOfs, 4))
    {
        return false;
    }
    // The first frame: a single one (type 0: its box, its name, its vertices), or a group's first.
    const int frameType = r.i32(frameOfs);
    frameOfs += 4;
    if(frameType != 0)
    {
        if(!r.has(frameOfs, 4))
        {
            return false;
        }
        const int count = r.i32(frameOfs);
        if(count < 0 || count > maxGroup)
        {
            return false;
        }
        frameOfs += 4 + 8 + count * 4; // (its count, its box, its times)
    }
    const int vertOfs = frameOfs + 8 + 16;
    if(!r.has(vertOfs, numVerts * 4))
    {
        return false;
    }
    for(int t = 0; t < numTris; t++)
    {
        const int o = triOfs + t * 16;
        const bool front = r.i32(o) != 0;
        Triangle tri;
        for(int k = 0; k < 3; k++)
        {
            const int v = r.i32(o + 4 + k * 4);
            if(v < 0 || v >= numVerts)
            {
                return false;
            }
            const byte* pv = r.data + vertOfs + v * 4;
            tri.c[k].p = glm::vec3{pv[0], pv[1], pv[2]} * scale + origin;
            const int so = stOfs + v * 12;
            float s = static_cast<float>(r.i32(so + 4));
            if(!front && r.i32(so) != 0)
            {
                s += skinWidth * 0.5f; // (the seam's back half)
            }
            tri.c[k].st = glm::vec2{s + 0.5f, static_cast<float>(r.i32(so + 8)) + 0.5f};
        }
        tris.pushBack(tri);
    }
    return true;
}

// A version 29 .bsp's first model (bspfile.h).
[[nodiscard]] bool readBsp(const Reader& r, za::Vector<Triangle>& tris, za::Vector<Texture>& textures)
{
    if(!r.has(0, 4 + 15 * 8) || r.i32(0) != BSPVERSION)
    {
        return false;
    }
    const auto lumpOfs = [&](int l) { return r.i32(4 + l * 8); };
    const auto lumpLen = [&](int l) { return r.i32(8 + l * 8); };
    for(int l = 0; l < 15; l++)
    {
        if(!r.has(lumpOfs(l), lumpLen(l)))
        {
            return false;
        }
    }
    constexpr int lumpTextures = 2, lumpVertexes = 3, lumpTexinfo = 6, lumpFaces = 7, lumpEdges = 12, lumpSurfedges = 13,
                  lumpModels = 14;
    if(lumpLen(lumpModels) < 64)
    {
        return false;
    }
    const int firstFace = r.i32(lumpOfs(lumpModels) + 56), numFaces = r.i32(lumpOfs(lumpModels) + 60);
    const int faceCount = lumpLen(lumpFaces) / 20, texinfoCount = lumpLen(lumpTexinfo) / 40;
    const int edgeCount = lumpLen(lumpEdges) / 4, surfedgeCount = lumpLen(lumpSurfedges) / 4;
    const int vertexCount = lumpLen(lumpVertexes) / 12;
    // (Every count and offset read from the file is bounded before it is added or multiplied: no int overflow.)
    if(firstFace < 0 || numFaces < 1 || numFaces > faceCount - firstFace)
    {
        return false;
    }

    // The textures, by miptex number (a missing one: none, its faces left out).
    const int texOfs = lumpOfs(lumpTextures);
    const int numMiptex = lumpLen(lumpTextures) >= 4 ? r.i32(texOfs) : 0;
    if(numMiptex < 0 || numMiptex > lumpLen(lumpTextures) / 4 || !r.has(texOfs, 4 + numMiptex * 4))
    {
        return false;
    }
    textures.clear();
    for(int i = 0; i < numMiptex; i++)
    {
        const int d = r.i32(texOfs + 4 + i * 4);
        Texture tex;
        const int m = d > 0 && d < lumpLen(lumpTextures) ? texOfs + d : -1;
        if(m > 0 && r.has(m, 40))
        {
            char name[17]{};
            ZA_MEMCPY(name, r.data + m, 16);
            const int w = r.i32(m + 16), h = r.i32(m + 20), pixels = r.i32(m + 24);
            const bool hidden = !q_strncasecmp(name, "sky", 3) || !q_strncasecmp(name, "trigger", 7) || !q_strncasecmp(name, "clip", 4);
            if(!hidden && w > 0 && h > 0 && w <= 1024 && h <= 1024 && pixels > 0 && pixels <= r.size - m &&
                r.has(m + pixels, w * h))
            {
                tex = {r.data + m + pixels, w, h};
            }
        }
        textures.pushBack(tex);
    }

    for(int f = firstFace; f < firstFace + numFaces; f++)
    {
        const int o = lumpOfs(lumpFaces) + f * 20;
        const int firstEdge = r.i32(o + 4), numEdges = r.s16(o + 8), ti = r.s16(o + 10);
        if(numEdges < 3 || firstEdge < 0 || firstEdge > surfedgeCount - numEdges || ti < 0 || ti >= texinfoCount)
        {
            continue;
        }
        const int tio = lumpOfs(lumpTexinfo) + ti * 40;
        const int miptex = r.i32(tio + 32);
        if(miptex < 0 || miptex >= numMiptex || !textures[miptex].pixels)
        {
            continue;
        }
        const glm::vec4 sv{r.f32(tio), r.f32(tio + 4), r.f32(tio + 8), r.f32(tio + 12)};
        const glm::vec4 tv{r.f32(tio + 16), r.f32(tio + 20), r.f32(tio + 24), r.f32(tio + 28)};
        const auto corner = [&](int i, Corner& c) {
            const int se = r.i32(lumpOfs(lumpSurfedges) + (firstEdge + i) * 4);
            const int e = se >= 0 ? se : se == INT_MIN ? edgeCount : -se;
            if(e >= edgeCount)
            {
                return false;
            }
            const int v = r.u16(lumpOfs(lumpEdges) + e * 4 + (se >= 0 ? 0 : 2));
            if(v >= vertexCount)
            {
                return false;
            }
            const int vo = lumpOfs(lumpVertexes) + v * 12;
            c.p = glm::vec3{r.f32(vo), r.f32(vo + 4), r.f32(vo + 8)};
            c.st = glm::vec2{glm::dot(c.p, glm::vec3{sv}) + sv.w, glm::dot(c.p, glm::vec3{tv}) + tv.w};
            return true;
        };
        // A fan from its first corner.
        Corner first, prev;
        if(!corner(0, first) || !corner(1, prev))
        {
            continue;
        }
        for(int i = 2; i < numEdges; i++)
        {
            Corner next;
            if(!corner(i, next))
            {
                break;
            }
            Triangle tri;
            tri.c[0] = first;
            tri.c[1] = prev;
            tri.c[2] = next;
            tri.texture = miptex;
            tris.pushBack(tri);
            prev = next;
        }
    }
    return !tris.empty();
}

[[nodiscard]] uint32_t texel(const Texture& t, const glm::vec2& st, float shade)
{
    const int s = static_cast<int>(za::floor(st.x)) % t.width, u = static_cast<int>(za::floor(st.y)) % t.height;
    const byte index = t.pixels[(u < 0 ? u + t.height : u) * t.width + (s < 0 ? s + t.width : s)];
    const uint32_t c = d_8to24table[index];
    const float k = index >= 224 ? 1.f : shade; // (the fullbright colours)
    const auto channel = [&](int shift) { return static_cast<uint32_t>(za::min(255.f, static_cast<float>((c >> shift) & 0xff) * k)); };
    return channel(0) | (channel(8) << 8) | (channel(16) << 16) | 0xff000000u;
}

// The triangles drawn into `out` (width x height, RGBA, rows top first), framed to fit, seen from along `toViewer`; the
// framing (the picture's axes, middle and scale) into `framing` when asked.
void rasterize(const za::Vector<Triangle>& tris, const za::Vector<Texture>& textures, const glm::vec3& toViewer,
    za::Vector<uint32_t>& out, int width = iconSize, int height = iconSize, toolgun::icons::Framing* framing = nullptr)
{
    const int rw = width * superSample, rh = height * superSample;
    const glm::vec3 f = -glm::normalize(toViewer); // the view's way
    const glm::vec3 right = glm::normalize(glm::cross(f, glm::vec3{0.f, 0.f, 1.f}));
    const glm::vec3 up = glm::cross(right, f);
    const glm::vec3 light = glm::normalize(-f + up * 0.9f - right * 0.4f);

    glm::vec2 lo{1e30f}, hi{-1e30f};
    for(const Triangle& t : tris)
    {
        for(const Corner& c : t.c)
        {
            const glm::vec2 q{glm::dot(c.p, right), glm::dot(c.p, up)};
            lo = glm::min(lo, q);
            hi = glm::max(hi, q);
        }
    }
    const glm::vec2 mid = (lo + hi) * 0.5f;
    const float scale = za::min(static_cast<float>(rw) * 0.9f / za::max(hi.x - lo.x, 1e-3f),
        static_cast<float>(rh) * 0.9f / za::max(hi.y - lo.y, 1e-3f));
    if(framing)
    {
        *framing = {right, up, mid, scale / static_cast<float>(superSample), glm::vec2{static_cast<float>(width), static_cast<float>(height)}};
    }

    za::Vector<uint32_t>& color = scratch.color;
    za::Vector<float>& depth = scratch.depth;
    color.clear();
    depth.clear();
    color.resize(static_cast<za::SizeT>(rw * rh), 0u);
    depth.resize(static_cast<za::SizeT>(rw * rh), 1e30f);

    for(const Triangle& t : tris)
    {
        const Texture& tex = textures[t.texture];
        if(!tex.pixels && !tex.plain)
        {
            continue;
        }
        glm::vec3 s[3]; // screen x, y (down), depth
        for(int k = 0; k < 3; k++)
        {
            const glm::vec3& p = t.c[k].p;
            s[k] = glm::vec3{(glm::dot(p, right) - mid.x) * scale + static_cast<float>(rw) * 0.5f,
                static_cast<float>(rh) * 0.5f - (glm::dot(p, up) - mid.y) * scale, glm::dot(p, f)};
        }
        const float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
        if(za::fabs(area) < 1e-6f)
        {
            continue;
        }
        const glm::vec3 n = glm::cross(t.c[1].p - t.c[0].p, t.c[2].p - t.c[0].p);
        const float nl = glm::length(n);
        const float shade = 0.45f + 0.75f * (nl > 0.f ? za::fabs(glm::dot(n / nl, light)) : 0.5f);
        const int x0 = za::max(0, static_cast<int>(za::floor(za::min(s[0].x, s[1].x, s[2].x))));
        const int x1 = za::min(rw - 1, static_cast<int>(za::floor(za::max(s[0].x, s[1].x, s[2].x))));
        const int y0 = za::max(0, static_cast<int>(za::floor(za::min(s[0].y, s[1].y, s[2].y))));
        const int y1 = za::min(rh - 1, static_cast<int>(za::floor(za::max(s[0].y, s[1].y, s[2].y))));
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                const float px = x + 0.5f, py = y + 0.5f;
                // Barycentric weights (either winding).
                const float w0 = ((s[1].x - px) * (s[2].y - py) - (s[2].x - px) * (s[1].y - py)) / area;
                const float w1 = ((s[2].x - px) * (s[0].y - py) - (s[0].x - px) * (s[2].y - py)) / area;
                const float w2 = 1.f - w0 - w1;
                if(w0 < 0.f || w1 < 0.f || w2 < 0.f)
                {
                    continue;
                }
                const float z = w0 * s[0].z + w1 * s[1].z + w2 * s[2].z;
                float& d = depth[y * rw + x];
                if(z >= d)
                {
                    continue;
                }
                d = z;
                const glm::vec2 st = t.c[0].st * w0 + t.c[1].st * w1 + t.c[2].st * w2;
                if(tex.plain)
                {
                    const uint32_t g = static_cast<uint32_t>(za::clamp(shade * 0.8f, 0.f, 1.f) * 255.f);
                    color[y * rw + x] = g | (g << 8) | (g << 16) | 0xff000000u;
                    continue;
                }
                color[y * rw + x] = texel(tex, st, shade);
            }
        }
    }

    // Averaged down: the colour of the covered samples, the alpha their share.
    out.clear();
    out.resize(static_cast<za::SizeT>(width * height), 0u);
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            uint32_t sum[3]{}, covered = 0;
            for(int j = 0; j < superSample; j++)
            {
                for(int i = 0; i < superSample; i++)
                {
                    const uint32_t c = color[(y * superSample + j) * rw + x * superSample + i];
                    if(c >> 24)
                    {
                        covered++;
                        sum[0] += c & 0xff;
                        sum[1] += (c >> 8) & 0xff;
                        sum[2] += (c >> 16) & 0xff;
                    }
                }
            }
            if(covered)
            {
                out[y * width + x] = (sum[0] / covered) | ((sum[1] / covered) << 8) | ((sum[2] / covered) << 16) |
                                        ((covered * 255 / (superSample * superSample)) << 24);
            }
        }
    }
}

// The icon's attachment (wheelui: the holster menu's and the weapon wheel's thumbnails) added to the gun's triangles: its
// model's first frame placed on the gun's model space by the gun's mount (attach::mount / onGun: the one placement the
// drawn attachment and the server's points use). Returns the attachment's file (its skin's texels are read from it while
// the icon is drawn: freed after), nullptr when it does not fit the gun or can't be read.
[[nodiscard]] byte* addAttachment(const Icon& icon, za::Vector<Triangle>& tris, za::Vector<Texture>& textures)
{
    const attach::KindInfo* kind = attach::info(icon.attach);
    const attach::Mount m = attach::mount(weapons::slotForName(icon.model), icon.attach);
    if(!kind || !m.fits)
    {
        return nullptr;
    }
    byte* data = COM_LoadMallocFile(kind->model, nullptr);
    if(!data)
    {
        return nullptr;
    }
    const za::SizeT first = tris.size();
    const int texture = static_cast<int>(textures.size());
    if(!readMdl(Reader{data, static_cast<int>(com_filesize)}, 0, tris, textures))
    {
        tris.resize(first);
        textures.resize(static_cast<za::SizeT>(texture));
        VR_HeapFree(data);
        return nullptr;
    }
    for(za::SizeT i = first; i < tris.size(); i++)
    {
        tris[i].texture = texture;
        for(Corner& c : tris[i].c)
        {
            c.p = attach::onGun(m, c.p);
        }
    }
    return data;
}

void make(Icon& icon)
{
    const auto t0 = za::Clock::nowNanoseconds();
    icon.state = IconState::Failed;
    // AD support: a model made from another's file ("<model>#...": an AD soldier's gun, vr_adguns.cpp; the armed death
    // guard's and crossbow knight's progs/vrdguard.mdl and vrdcrossbow.mdl, no files), as it loads.
    byte* data = VR_DerivedModelFile(icon.model, nullptr);
    if(!data)
    {
        data = COM_LoadMallocFile(icon.model, nullptr);
    }
    if(data)
    {
        const Reader r{data, static_cast<int>(com_filesize)};
        za::Vector<Triangle>& tris = scratch.triangles;
        za::Vector<Texture>& textures = scratch.textures;
        tris.clear();
        textures.clear();
        const za::SizeT n = ZA_STRLEN(icon.model);
        const bool bsp = n > 4 && !q_strcasecmp(icon.model + n - 4, ".bsp");
        byte* attachData = nullptr;
        if(bsp ? readBsp(r, tris, textures) : readMdl(r, icon.skin, tris, textures))
        {
            if(icon.attach > 0)
            {
                attachData = addAttachment(icon, tris, textures);
            }
            const glm::vec3 toViewer = icon.side ? glm::vec3{0.15f, -1.f, 0.3f} : glm::vec3{1.f, -0.75f, 0.5f};
            rasterize(tris, textures, toViewer, scratch.out);
            gfx::ensurePicture(icon.picture, iconSize, iconSize, "toolgun icon");
            gfx::updatePicture(icon.picture, scratch.out.data());
            icon.state = IconState::Made;
            iconSet.made++;
        }
        if(attachData)
        {
            VR_HeapFree(attachData);
        }
        VR_HeapFree(data);
    }
    if(icon.state == IconState::Failed)
    {
        Con_DPrintf("toolgun: no thumbnail of %s (its file not read)\n", icon.model);
    }
    iconSet.seconds += za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0);
}

} // namespace

namespace qvr::toolgun::icons
{

const gfx::Picture* get(const char* model, int skin, bool side, int attach)
{
    if(!model || !model[0])
    {
        return nullptr;
    }
    for(const Icon& icon : iconSet.icons)
    {
        if(icon.skin == skin && icon.side == side && icon.attach == attach && !ZA_STRCMP(icon.model, model))
        {
            return icon.state == IconState::Made ? &icon.picture : nullptr;
        }
    }
    if(iconSet.icons.size() >= maxIcons)
    {
        return nullptr;
    }
    Icon& icon = iconSet.icons.emplaceBack();
    q_strlcpy(icon.model, model, sizeof(icon.model));
    icon.skin = skin;
    icon.side = side;
    icon.attach = attach;
    return nullptr;
}

bool drawMesh(const za::Vector<glm::vec3>& verts, const za::Vector<unsigned>& tris, const glm::vec3& toViewer, int width,
    int height, gfx::Picture& picture, Framing& framing)
{
    if(verts.empty() || tris.size() < 3 || width <= 0 || height <= 0)
    {
        return false;
    }
    za::Vector<Triangle>& out = scratch.triangles;
    za::Vector<Texture>& textures = scratch.textures;
    out.clear();
    textures.clear();
    Texture plain;
    plain.plain = true;
    textures.pushBack(plain);
    for(za::SizeT t = 0; t + 2 < tris.size(); t += 3)
    {
        Triangle tri;
        for(int k = 0; k < 3; k++)
        {
            const unsigned v = tris[t + static_cast<za::SizeT>(k)];
            if(v >= verts.size())
            {
                return false;
            }
            tri.c[k].p = verts[v];
        }
        out.pushBack(tri);
    }
    rasterize(out, textures, toViewer, scratch.out, width, height, &framing);
    gfx::ensurePicture(picture, width, height, "body preview");
    gfx::updatePicture(picture, scratch.out.data());
    return true;
}

void makePending(int budget)
{
    for(Icon& icon : iconSet.icons)
    {
        if(budget <= 0)
        {
            return;
        }
        if(icon.state == IconState::Waiting)
        {
            make(icon);
            budget--;
        }
    }
}

void onGameDirChanged()
{
    for(Icon& icon : iconSet.icons)
    {
        icon.state = IconState::Waiting; // (its picture kept: drawn over)
    }
}

void print()
{
    int made = 0, failed = 0, waiting = 0;
    for(const Icon& icon : iconSet.icons)
    {
        made += icon.state == IconState::Made;
        failed += icon.state == IconState::Failed;
        waiting += icon.state == IconState::Waiting;
        if(icon.state == IconState::Failed)
        {
            Con_Printf("  failed: %s\n", icon.model);
        }
    }
    Con_Printf("toolgun icons: %d made, %d failed, %d waiting (%d made in all, %.1f ms)\n", made, failed, waiting, iconSet.made,
        iconSet.seconds * 1000.0);
}

} // namespace qvr::toolgun::icons
