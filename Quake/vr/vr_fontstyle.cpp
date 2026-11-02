#include "vr_alloccount.h"
// vr_fontstyle.cpp -- the 2D font restyled (Advanced VR Options > HUD and Menus > Menu, "Fonts"): the console font
// (conchars), Quake's text box pieces (gfx/box_*.lmp) and the main menu's lettering (vr_bigfont.cpp) upscaled 4x when
// loaded (vr_font_upscale: nearest, Scale4x or 4xBR) and drawn smooth (linear, mipmapped), with an outline and a drop
// shadow round the letters (vr_font_outline*, vr_font_shadow*). Made again at once as any of these changes, and
// after a game change (Draw_LoadPics). Classic with no outline and no shadow (as shipped): nothing is made, Quake's
// own textures are drawn as before.
//
// The outline and shadow are baked into the font's texture, not drawn as extra passes: every 8 x 8 glyph has a cell
// with a margin of 6 font pixels round it, and a letter with them is drawn as a quad that much bigger round the same
// place, so the text's layout (advance, line height) does not change. One quad a letter; the effects are made at the
// 4x size with a smooth disc and a Gaussian blur, which a draw-time pass could only approximate with many quads a
// letter. Only the letters get them (32-127, 160-255, the arrows 13 and 141, the brackets and gold digits 16-28 and
// 144-156), never the box, texture or slider pieces, which join up with their neighbours.
//
// The upscalers are those of the comparison sheets (ROUND23.md, "Font upscaling"): Scale4x (AdvMAME4x, EPX twice)
// and Hyllian's 4xBR (FFmpeg vf_xbr's level-2 rules). Each cell is upscaled on its own, padded 2 source pixels: with
// transparency, or with the pieces it is drawn against for the tiling ones (the slider 128-130, the lines 29-31 and
// 157-159, the text box), so they join without a notch. The textured cells of conchars' rows 0 and 8 stay nearest.

#include "vr_fontstyle.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"

#include <math.h>
#include <string.h>

extern "C" byte char_texture_data[256 * 10 * 10]; // gl_draw.c: conchars, 16 x 16 cells of 10 x 10 (8 x 8 and a border)

namespace
{

using namespace qvr;

// ---- images: premultiplied RGBA, 0..255 (the scalers' colour distances are in those units) ----

struct Image
{
    int w{0}, h{0};
    za::Vector<glm::vec4> px;

    void reset(int width, int height)
    {
        w = width;
        h = height;
        px.clear();
        px.resize(static_cast<za::SizeT>(w) * h, glm::vec4{0.f});
    }
    [[nodiscard]] glm::vec4& at(int x, int y) { return px[static_cast<za::SizeT>(y) * w + x]; }
    [[nodiscard]] const glm::vec4& at(int x, int y) const { return px[static_cast<za::SizeT>(y) * w + x]; }
    [[nodiscard]] const glm::vec4& clamped(int x, int y) const
    {
        return at(glm::clamp(x, 0, w - 1), glm::clamp(y, 0, h - 1));
    }
};

[[nodiscard]] glm::vec4 fromPalette(const unsigned int* table, byte index)
{
    const byte* c = reinterpret_cast<const byte*>(&table[index]);
    const float a = c[3];
    return {c[0] * a / 255.f, c[1] * a / 255.f, c[2] * a / 255.f, a};
}

enum Upscale
{
    Classic,
    NearestSmooth,
    Scale4x,
    Xbr4x
};

// ---- the scalers: in (w, h) -> out (4w, 4h); edges replicate (callers pad) ----

void nearest4(const Image& in, Image& out)
{
    out.reset(in.w * 4, in.h * 4);
    for(int y = 0; y < out.h; y++)
    {
        for(int x = 0; x < out.w; x++)
        {
            out.at(x, y) = in.at(x / 4, y / 4);
        }
    }
}

void scale2x(const Image& in, Image& out)
{
    out.reset(in.w * 2, in.h * 2);
    for(int y = 0; y < in.h; y++)
    {
        for(int x = 0; x < in.w; x++)
        {
            const glm::vec4& E = in.at(x, y);
            const glm::vec4& B = in.clamped(x, y - 1);
            const glm::vec4& H = in.clamped(x, y + 1);
            const glm::vec4& D = in.clamped(x - 1, y);
            const glm::vec4& F = in.clamped(x + 1, y);
            out.at(2 * x, 2 * y) = D == B && B != F && D != H ? D : E;
            out.at(2 * x + 1, 2 * y) = B == F && B != D && F != H ? F : E;
            out.at(2 * x, 2 * y + 1) = D == H && D != B && H != F ? D : E;
            out.at(2 * x + 1, 2 * y + 1) = H == F && D != H && B != F ? F : E;
        }
    }
}

struct Scratch
{
    Image half, yuva;                 // Scale4x's 2x step; 4xBR's colours in YUV + alpha
    Image padded, scaled, cell;       // a cell padded, upscaled, then with its effects
    za::Vector<float> cover, shadow, blurTmp; // the outline's coverage; the shadow's alpha
};
Scratch scratch;

void scale4x(const Image& in, Image& out)
{
    scale2x(in, scratch.half);
    scale2x(scratch.half, out);
}

[[nodiscard]] float xbrDiff(const glm::vec4& p, const glm::vec4& q)
{
    const glm::vec4 d = glm::abs(p - q);
    return 48.f * d.x + 7.f * d.y + 6.f * d.z + 48.f * d.w;
}

void xbr4(const Image& in, Image& out)
{
    Image& yuva = scratch.yuva;
    yuva.reset(in.w, in.h);
    for(za::SizeT i = 0; i < in.px.size(); i++)
    {
        const glm::vec4& c = in.px[i];
        yuva.px[i] = {0.299f * c.r + 0.587f * c.g + 0.114f * c.b, -0.169f * c.r - 0.331f * c.g + 0.5f * c.b,
            0.5f * c.r - 0.419f * c.g - 0.081f * c.b, c.a};
    }
    out.reset(in.w * 4, in.h * 4);
    for(int y = 0; y < in.h; y++)
    {
        for(int x = 0; x < in.w; x++)
        {
            glm::vec4 block[4][4];
            for(auto& row : block)
            {
                for(glm::vec4& b : row)
                {
                    b = in.at(x, y);
                }
            }
            // The bottom-right corner's rules, the source and the block turned a quarter (counter-clockwise) for each
            // of the four corners: an offset (dx, dy) of the turned frame is (-dy, dx) of the one before; a block
            // texel (r, c) is (c, 3 - r).
            for(int k = 0; k < 4; k++)
            {
                const auto turn = [k](int& dx, int& dy) {
                    for(int i = 0; i < k; i++)
                    {
                        const int t = dx;
                        dx = -dy;
                        dy = t;
                    }
                };
                const auto P = [&](int dx, int dy) -> const glm::vec4& {
                    turn(dx, dy);
                    return in.clamped(x + dx, y + dy);
                };
                const auto Y = [&](int dx, int dy) -> const glm::vec4& {
                    turn(dx, dy);
                    return yuva.clamped(x + dx, y + dy);
                };
                const auto N = [&](int r, int c) -> glm::vec4& {
                    for(int i = 0; i < k; i++)
                    {
                        const int t = r;
                        r = c;
                        c = 3 - t;
                    }
                    return block[r][c];
                };
                const glm::vec4 &E = P(0, 0), &F = P(1, 0), &H = P(0, 1), &G = P(-1, 1), &C = P(1, -1), &D = P(-1, 0),
                                &B = P(0, -1);
                if(E == H || E == F)
                {
                    continue;
                }
                const glm::vec4 &e_ = Y(0, 0), &f_ = Y(1, 0), &h_ = Y(0, 1), &i_ = Y(1, 1), &g_ = Y(-1, 1),
                                &c_ = Y(1, -1), &d_ = Y(-1, 0), &b_ = Y(0, -1), &h5 = Y(0, 2), &i5 = Y(1, 2),
                                &f4 = Y(2, 0), &i4 = Y(2, 1);
                const auto df = xbrDiff;
                const auto eq = [](const glm::vec4& a, const glm::vec4& b) { return xbrDiff(a, b) < 155.f; };
                const float e = df(e_, c_) + df(e_, g_) + df(i_, h5) + df(i_, f4) + 4.f * df(h_, f_);
                const float i = df(h_, d_) + df(h_, i5) + df(f_, i4) + df(f_, b_) + 4.f * df(e_, i_);
                if(e > i)
                {
                    continue;
                }
                const glm::vec4 px = df(e_, f_) <= df(e_, h_) ? F : H;
                const auto blend = [&](int r, int c, float a) {
                    glm::vec4& n = N(r, c);
                    n += (px - n) * a;
                };
                const auto set = [&](int r, int c, const glm::vec4& v) { N(r, c) = v; };
                const bool strong =
                    e < i && ((!eq(f_, b_) && !eq(f_, c_)) || (!eq(h_, d_) && !eq(h_, g_)) ||
                                 (eq(e_, i_) && ((!eq(f_, f4) && !eq(f_, i4)) || (!eq(h_, h5) && !eq(h_, i5)))) ||
                                 eq(e_, g_) || eq(e_, c_));
                if(!strong)
                {
                    blend(3, 3, 0.5f);
                    continue;
                }
                const float ke = df(f_, g_), ki = df(h_, c_);
                const bool left = 2.f * ke <= ki && E != G && D != G;
                const bool up = ke >= 2.f * ki && E != C && B != C;
                if(left && up)
                {
                    blend(3, 1, 0.75f);
                    blend(3, 0, 0.25f);
                    set(3, 3, px);
                    set(3, 2, px);
                    set(2, 3, px);
                    const glm::vec4 n12 = N(3, 0), n13 = N(3, 1);
                    set(2, 2, n12);
                    set(0, 3, n12);
                    set(1, 3, n13);
                }
                else if(left)
                {
                    blend(2, 3, 0.75f);
                    blend(3, 1, 0.75f);
                    blend(2, 2, 0.25f);
                    blend(3, 0, 0.25f);
                    set(3, 2, px);
                    set(3, 3, px);
                }
                else if(up)
                {
                    blend(3, 2, 0.75f);
                    blend(1, 3, 0.75f);
                    blend(2, 2, 0.25f);
                    blend(0, 3, 0.25f);
                    set(2, 3, px);
                    set(3, 3, px);
                }
                else
                {
                    blend(2, 3, 0.5f);
                    blend(3, 2, 0.5f);
                    set(3, 3, px);
                }
            }
            for(int r = 0; r < 4; r++)
            {
                for(int c = 0; c < 4; c++)
                {
                    out.at(4 * x + c, 4 * y + r) = block[r][c];
                }
            }
        }
    }
}

void upscale(int mode, const Image& in, Image& out)
{
    switch(mode)
    {
        case Scale4x: scale4x(in, out); break;
        case Xbr4x: xbr4(in, out); break;
        default: nearest4(in, out); break;
    }
}

constexpr int pad = 2; // source pixels round a cell as it is upscaled (4xBR reaches 2 pixels)

// A cell (`src`'s w x h from x0, y0) padded `pad` pixels (transparent, or the columns/rows of the pieces it is drawn
// against: their images, each the cell's size along that side) and upscaled: (w + 4) * 4 square, the cell's own
// pixels at (8, 8).
struct Neighbours
{
    const Image *left{nullptr}, *right{nullptr}, *up{nullptr}, *down{nullptr};
};

void upscalePadded(int mode, const Image& src, int x0, int y0, int w, int h, const Neighbours& n, Image& out)
{
    Image& p = scratch.padded;
    p.reset(w + 2 * pad, h + 2 * pad);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            p.at(x + pad, y + pad) = src.at(x0 + x, y0 + y);
        }
    }
    for(int i = 0; i < pad; i++)
    {
        for(int y = 0; y < h; y++)
        {
            if(n.left && n.left->h == h)
            {
                p.at(i, y + pad) = n.left->at(n.left->w - pad + i, y);
            }
            if(n.right && n.right->h == h)
            {
                p.at(w + pad + i, y + pad) = n.right->at(i, y);
            }
        }
        for(int x = 0; x < w; x++)
        {
            if(n.up && n.up->w == w)
            {
                p.at(x + pad, i) = n.up->at(x, n.up->h - pad + i);
            }
            if(n.down && n.down->w == w)
            {
                p.at(x + pad, h + pad + i) = n.down->at(x, i);
            }
        }
    }
    upscale(mode, p, out);
}

// ---- the style ----

struct Style
{
    int upscale{Classic};
    float outline{0.f}, shadowOpacity{0.f}, shadowSize{0.f}, shadowSpread{0.f};
    glm::vec3 outlineColour{0.f}, shadowColour{0.f};

    [[nodiscard]] bool effects() const { return outline > 0.f || shadowOpacity > 0.f; }
    [[nodiscard]] bool active() const { return upscale != Classic || effects(); }
    [[nodiscard]] bool smooth() const { return upscale != Classic; }
};

[[nodiscard]] Style readStyle()
{
    Style s;
    s.upscale = glm::clamp(static_cast<int>(vr_font_upscale.value), 0, 3);
    s.outline = glm::clamp(vr_font_outline.value, 0.f, 2.f);
    s.outlineColour = glm::clamp(glm::vec3{vr_font_outline_r.value, vr_font_outline_g.value, vr_font_outline_b.value},
        glm::vec3{0.f}, glm::vec3{1.f});
    s.shadowOpacity = glm::clamp(vr_font_shadow_opacity.value, 0.f, 1.f);
    s.shadowSize = glm::clamp(vr_font_shadow_size.value, 0.f, 2.f);
    s.shadowSpread = glm::clamp(vr_font_shadow_spread.value, 0.f, 2.f);
    s.shadowColour = glm::clamp(glm::vec3{vr_font_shadow_r.value, vr_font_shadow_g.value, vr_font_shadow_b.value},
        glm::vec3{0.f}, glm::vec3{1.f});
    return s;
}

constexpr int margin = 6;           // source pixels round a letter for its effects: outline 2 + shadow 2 + spread 2
constexpr int cell4 = (8 + 2 * margin) * 4; // a conchars cell in the texture: 80 x 80
constexpr int atlas4 = 16 * cell4;  // the texture: 1280 x 1280

// The outline and the shadow round what `img` holds (4x pixels; transparent margins wide enough for them).
void applyEffects(const Style& st, Image& img)
{
    const int n = img.w * img.h;
    if(st.outline > 0.f)
    {
        const float R = st.outline * 4.f;
        const int reach = static_cast<int>(ceilf(R + 0.5f));
        za::Vector<float>& cover = scratch.cover;
        cover.clear();
        cover.resize(static_cast<za::SizeT>(n), 0.f);
        for(int y = 0; y < img.h; y++)
        {
            for(int x = 0; x < img.w; x++)
            {
                const float a = img.at(x, y).a / 255.f;
                if(a <= 0.f)
                {
                    continue;
                }
                for(int dy = -reach; dy <= reach; dy++)
                {
                    for(int dx = -reach; dx <= reach; dx++)
                    {
                        const int tx = x + dx, ty = y + dy;
                        if(tx < 0 || ty < 0 || tx >= img.w || ty >= img.h)
                        {
                            continue;
                        }
                        const float c = glm::clamp(R + 0.5f - sqrtf(static_cast<float>(dx * dx + dy * dy)), 0.f, 1.f) * a;
                        float& o = cover[static_cast<za::SizeT>(ty) * img.w + tx];
                        o = c > o ? c : o;
                    }
                }
            }
        }
        const glm::vec3 rgb = st.outlineColour * 255.f;
        for(int i = 0; i < n; i++)
        {
            glm::vec4& p = img.px[static_cast<za::SizeT>(i)];
            const float o = cover[static_cast<za::SizeT>(i)];
            p += glm::vec4{rgb * o, 255.f * o} * (1.f - p.a / 255.f);
        }
    }
    if(st.shadowOpacity > 0.f)
    {
        const int off = static_cast<int>(lroundf(st.shadowSize * 4.f));
        za::Vector<float>& s = scratch.shadow;
        za::Vector<float>& t = scratch.blurTmp;
        s.clear();
        s.resize(static_cast<za::SizeT>(n), 0.f);
        t.clear();
        t.resize(static_cast<za::SizeT>(n), 0.f);
        for(int y = off; y < img.h; y++)
        {
            for(int x = off; x < img.w; x++)
            {
                s[static_cast<za::SizeT>(y) * img.w + x] = img.at(x - off, y - off).a / 255.f;
            }
        }
        const float r = st.shadowSpread * 4.f;
        if(r >= 0.5f)
        {
            const int k = static_cast<int>(ceilf(r));
            const float sigma = r * 0.5f;
            za::Array<float, 2 * 8 + 1> wt{};
            float sum = 0.f;
            for(int i = -k; i <= k; i++)
            {
                wt[static_cast<za::SizeT>(i + k)] = expf(-static_cast<float>(i * i) / (2.f * sigma * sigma));
                sum += wt[static_cast<za::SizeT>(i + k)];
            }
            for(int pass = 0; pass < 2; pass++)
            {
                const za::Vector<float>& from = pass == 0 ? s : t;
                za::Vector<float>& to = pass == 0 ? t : s;
                for(int y = 0; y < img.h; y++)
                {
                    for(int x = 0; x < img.w; x++)
                    {
                        float acc = 0.f;
                        for(int i = -k; i <= k; i++)
                        {
                            const int sx = pass == 0 ? x + i : x, sy = pass == 0 ? y : y + i;
                            if(sx >= 0 && sy >= 0 && sx < img.w && sy < img.h)
                            {
                                acc += from[static_cast<za::SizeT>(sy) * img.w + sx] * wt[static_cast<za::SizeT>(i + k)];
                            }
                        }
                        to[static_cast<za::SizeT>(y) * img.w + x] = acc / sum;
                    }
                }
            }
        }
        const glm::vec3 rgb = st.shadowColour * 255.f;
        for(int i = 0; i < n; i++)
        {
            glm::vec4& p = img.px[static_cast<za::SizeT>(i)];
            const float a = s[static_cast<za::SizeT>(i)] * st.shadowOpacity;
            p += glm::vec4{rgb * a, 255.f * a} * (1.f - p.a / 255.f);
        }
    }
}

// `img` (premultiplied) into `out` (RGBA8, straight alpha) at (ox, oy), a row `stride` texels.
void store(const Image& img, za::U32* out, int stride, int ox, int oy)
{
    for(int y = 0; y < img.h; y++)
    {
        for(int x = 0; x < img.w; x++)
        {
            const glm::vec4& p = img.at(x, y);
            const float a = glm::clamp(p.a, 0.f, 255.f);
            const glm::vec3 rgb = a > 0.f ? glm::clamp(glm::vec3{p} * (255.f / a), 0.f, 255.f) : glm::vec3{0.f};
            out[static_cast<za::SizeT>(oy + y) * stride + ox + x] = static_cast<za::U32>(rgb.r + 0.5f) |
                static_cast<za::U32>(rgb.g + 0.5f) << 8 | static_cast<za::U32>(rgb.b + 0.5f) << 16 |
                static_cast<za::U32>(a + 0.5f) << 24;
        }
    }
}

// Transparent texels take their opaque neighbours' colour, so that smooth sampling has no dark fringe.
void edgeFix(za::U32* data, int w, int h)
{
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            za::U32& t = data[static_cast<za::SizeT>(y) * w + x];
            if(t >> 24)
            {
                continue;
            }
            int r = 0, g = 0, b = 0, count = 0;
            for(int dy = -1; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx++)
                {
                    const int nx = x + dx, ny = y + dy;
                    if(nx < 0 || ny < 0 || nx >= w || ny >= h)
                    {
                        continue;
                    }
                    const za::U32 v = data[static_cast<za::SizeT>(ny) * w + nx];
                    if(v >> 24)
                    {
                        r += static_cast<int>(v & 255);
                        g += static_cast<int>(v >> 8 & 255);
                        b += static_cast<int>(v >> 16 & 255);
                        count++;
                    }
                }
            }
            if(count)
            {
                t = static_cast<za::U32>(r / count) | static_cast<za::U32>(g / count) << 8 |
                    static_cast<za::U32>(b / count) << 16;
            }
        }
    }
}

// ---- conchars ----

enum class CellKind
{
    Letter,  // upscaled, with the outline and shadow
    Plain,   // upscaled (the slider's cursor)
    Texture, // nearest 4x (the textured box pieces of rows 0 and 8)
    Tile     // upscaled against its neighbours (slider, lines)
};

[[nodiscard]] CellKind cellKind(int c)
{
    if(c == 13 || c == 141 || (c >= 16 && c <= 28) || (c >= 144 && c <= 156) || (c >= 32 && c <= 127) || c >= 160)
    {
        return CellKind::Letter;
    }
    if((c >= 128 && c <= 130) || (c >= 29 && c <= 31) || (c >= 157 && c <= 159))
    {
        return CellKind::Tile;
    }
    return c == 131 ? CellKind::Plain : CellKind::Texture;
}

// The tiling pieces' middle (what they are drawn against): the slider's 129, the lines' 30 and 158; -1: none.
[[nodiscard]] int tileLeft(int c)
{
    return c == 129 || c == 130 ? 129 : c == 30 || c == 31 ? 30 : c == 158 || c == 159 ? 158 : -1;
}
[[nodiscard]] int tileRight(int c)
{
    return c == 128 || c == 129 ? 129 : c == 29 || c == 30 ? 30 : c == 157 || c == 158 ? 158 : -1;
}

struct BoxPiece
{
    const char* name;
    int left, right, up, down; // the pieces it is drawn against (indexes here; -1: transparent)
};
// M_DrawTextBox's pieces: the top, middle rows and bottom tile across, the sides down.
constexpr BoxPiece boxPieces[] = {
    {"gfx/box_tl.lmp", -1, 1, -1, 3},
    {"gfx/box_tm.lmp", 1, 1, -1, 4},
    {"gfx/box_tr.lmp", 1, -1, -1, 6},
    {"gfx/box_ml.lmp", -1, 4, 3, 3},
    {"gfx/box_mm.lmp", 4, 4, 1, 5},
    {"gfx/box_mm2.lmp", 5, 5, 5, 5},
    {"gfx/box_mr.lmp", 4, -1, 6, 6},
    {"gfx/box_bl.lmp", -1, 8, 3, -1},
    {"gfx/box_bm.lmp", 8, 8, 5, -1},
    {"gfx/box_br.lmp", 8, -1, 6, -1},
};
constexpr int boxCount = static_cast<int>(sizeof(boxPieces) / sizeof(boxPieces[0]));

struct BoxPic
{
    Image src;                // as loaded (premultiplied)
    za::Vector<za::U32> rgba; // the texture's texels
    za::Vector<za::U8> pic;   // its qpic_t (Draw_PicBytes)
    bool made{false};
};

struct State
{
    bool ready{false};          // conchars loaded (Draw_LoadPics): textures can be made
    int generation{0};
    Style style;
    Image chars;                // conchars as 16 x 16 cells of 8 x 8 (premultiplied)
    za::Vector<za::U32> atlas;  // the restyled font, atlas4 square (reloads of the texture read it)
    gltexture_t* texture{nullptr};
    gltexture_t* source{nullptr}; // char_texture it was made from (another, the wrist gadget's: drawn as it is)
    za::Array<BoxPic, boxCount> box{};
};
State state;

void freeTexture(gltexture_t*& t)
{
    if(t)
    {
        TexMgr_FreeTexture(t);
        t = nullptr;
    }
}

[[nodiscard]] unsigned textureFlags(bool smooth)
{
    return TEXPREF_ALPHA | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_CLAMP | TEXPREF_UNCOMPRESSED |
        (smooth ? TEXPREF_LINEAR | TEXPREF_MIPMAP : TEXPREF_NEAREST);
}

// The atlas of conchars restyled (state.atlas) for `st`, from state.chars.
void buildAtlas(const Style& st)
{
    state.atlas.clear();
    state.atlas.resize(static_cast<za::SizeT>(atlas4) * atlas4, 0u);
    za::Array<Image, 3> mids{}; // 129, 30, 158 (the tiles' middles), on their own
    const auto cellImage = [](int c, Image& out) {
        out.reset(8, 8);
        for(int y = 0; y < 8; y++)
        {
            for(int x = 0; x < 8; x++)
            {
                out.at(x, y) = state.chars.at((c & 15) * 8 + x, (c >> 4) * 8 + y);
            }
        }
    };
    cellImage(129, mids[0]);
    cellImage(30, mids[1]);
    cellImage(158, mids[2]);
    const auto mid = [&](int c) -> const Image* {
        return c == 129 ? &mids[0] : c == 30 ? &mids[1] : c == 158 ? &mids[2] : nullptr;
    };
    for(int c = 0; c < 256; c++)
    {
        const CellKind kind = cellKind(c);
        Neighbours n;
        if(kind == CellKind::Tile)
        {
            n.left = mid(tileLeft(c));
            n.right = mid(tileRight(c));
        }
        const int mode = kind == CellKind::Texture || st.upscale == Classic ? NearestSmooth : st.upscale;
        upscalePadded(mode, state.chars, (c & 15) * 8, (c >> 4) * 8, 8, 8, n, scratch.scaled);
        // Into the cell: the glyph at the margin; a tile's padding too (its neighbours' texels, for smooth sampling).
        Image& cell = scratch.cell;
        cell.reset(cell4, cell4);
        const int keep = kind == CellKind::Tile ? pad * 4 : 0;
        for(int y = -keep; y < 32 + keep; y++)
        {
            for(int x = -keep; x < 32 + keep; x++)
            {
                cell.at(margin * 4 + x, margin * 4 + y) = scratch.scaled.at(pad * 4 + x, pad * 4 + y);
            }
        }
        if(kind == CellKind::Letter && st.effects())
        {
            applyEffects(st, cell);
        }
        store(cell, state.atlas.data(), atlas4, (c & 15) * cell4, (c >> 4) * cell4);
    }
    edgeFix(state.atlas.data(), atlas4, atlas4);
}

void loadBoxSources()
{
    for(int i = 0; i < boxCount; i++)
    {
        BoxPic& b = state.box[static_cast<za::SizeT>(i)];
        b.src.reset(0, 0);
        byte* const file = COM_LoadMallocFile(boxPieces[i].name, nullptr);
        if(!file)
        {
            continue;
        }
        int w, h;
        memcpy(&w, file, 4);
        memcpy(&h, file + 4, 4);
        w = LittleLong(w);
        h = LittleLong(h);
        if(w > 0 && h > 0 && w <= 256 && h <= 256 && 8 + w * h <= com_filesize)
        {
            b.src.reset(w, h);
            for(int t = 0; t < w * h; t++)
            {
                b.src.px[static_cast<za::SizeT>(t)] = fromPalette(d_8to24table, file[8 + t]);
            }
        }
        VR_HeapFree(file);
    }
}

void buildBox(const Style& st)
{
    for(int i = 0; i < boxCount; i++)
    {
        BoxPic& b = state.box[static_cast<za::SizeT>(i)];
        if(!b.src.w)
        {
            b.made = false;
            continue;
        }
        const auto piece = [](int j) -> const Image* {
            return j >= 0 && state.box[static_cast<za::SizeT>(j)].src.w ? &state.box[static_cast<za::SizeT>(j)].src : nullptr;
        };
        const BoxPiece& p = boxPieces[i];
        const Neighbours n{piece(p.left), piece(p.right), piece(p.up), piece(p.down)};
        upscalePadded(st.upscale == Classic ? NearestSmooth : st.upscale, b.src, 0, 0, b.src.w, b.src.h, n, scratch.scaled);
        Image& core = scratch.cell;
        core.reset(b.src.w * 4, b.src.h * 4);
        for(int y = 0; y < core.h; y++)
        {
            for(int x = 0; x < core.w; x++)
            {
                core.at(x, y) = scratch.scaled.at(pad * 4 + x, pad * 4 + y);
            }
        }
        b.rgba.clear();
        b.rgba.resize(static_cast<za::SizeT>(core.w) * core.h, 0u);
        store(core, b.rgba.data(), core.w, 0, 0);
        edgeFix(b.rgba.data(), core.w, core.h);
        if(b.pic.empty())
        {
            b.pic.resize(Draw_PicBytes(), 0);
        }
        Draw_ReplacePicRGBA(reinterpret_cast<qpic_t*>(b.pic.data()), va("vr_fontstyle:%s", p.name), b.src.w, b.src.h,
            core.w, core.h, b.rgba.data(), st.smooth());
        b.made = true;
    }
}

// Everything made again for the cvars' style (or freed: Classic without effects).
void rebuild()
{
    state.generation++;
    state.style = readStyle();
    if(!state.ready)
    {
        return;
    }
    freeTexture(state.texture);
    if(!state.style.active())
    {
        state.atlas = {};
        return;
    }
    buildAtlas(state.style);
    state.texture = TexMgr_LoadImage(nullptr, "vr_fontstyle_conchars", atlas4, atlas4, SRC_RGBA,
        reinterpret_cast<byte*>(state.atlas.data()), "", reinterpret_cast<src_offset_t>(state.atlas.data()),
        textureFlags(state.style.smooth()));
    buildBox(state.style);
}

void onStyleChanged(cvar_t*)
{
    rebuild();
}

// ---- vr_fontstyle_test: each mode's texture made at its size, the tiling pieces joined without a notch ----

void testCommand()
{
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) { tally.check(ok, "fontstyle test", what); };
    if(!state.ready)
    {
        check(false, "conchars not loaded");
        tally.verdict("fontstyle test");
        return;
    }
    const Style saved = state.style;
    for(int mode = 0; mode < 4; mode++)
    {
        for(int fx = 0; fx < 2; fx++)
        {
            Style st;
            st.upscale = mode;
            st.outline = fx ? 1.f : 0.f;
            st.shadowOpacity = fx ? 0.8f : 0.f;
            st.shadowSize = 1.f;
            st.shadowSpread = 0.5f;
            st.shadowColour = glm::vec3{0.f};
            st.outlineColour = glm::vec3{0.f};
            if(!st.active())
            {
                continue;
            }
            buildAtlas(st);
            check(state.atlas.size() == static_cast<za::SizeT>(atlas4) * atlas4, va("mode %d fx %d: atlas size", mode, fx));
            // 'A' (65) has texels; with effects, its margin has some (the outline), else none.
            const auto alphaIn = [](int c, int x0, int y0, int x1, int y1) {
                int n = 0;
                for(int y = y0; y < y1; y++)
                {
                    for(int x = x0; x < x1; x++)
                    {
                        n += (state.atlas[static_cast<za::SizeT>((c >> 4) * cell4 + y) * atlas4 + (c & 15) * cell4 + x] >> 24) > 0;
                    }
                }
                return n;
            };
            check(alphaIn(65, margin * 4, margin * 4, margin * 4 + 32, margin * 4 + 32) > 0, va("mode %d fx %d: A drawn", mode, fx));
            const int outside =
                alphaIn(65, 0, 0, cell4, cell4) - alphaIn(65, margin * 4, margin * 4, margin * 4 + 32, margin * 4 + 32);
            check(fx ? outside > 0 : outside == 0, va("mode %d fx %d: effects round A only when on", mode, fx));
            check(alphaIn(4, 0, 0, cell4, margin * 4 - 1) == 0, va("mode %d fx %d: no effects on a box cell", mode, fx));
            // The tiling pieces (slider 128 129 130, lines 29 30 31 and 157 158 159; L M R) as drawn against the
            // middle: each end piece beside two middles, a middle between two, against upscaling that row of three at
            // once: equal texel for texel.
            constexpr int groups[3][3] = {{128, 129, 130}, {29, 30, 31}, {157, 158, 159}};
            for(const auto& g : groups)
            {
                const int rows[3][3] = {{g[0], g[1], g[1]}, {g[1], g[1], g[1]}, {g[1], g[1], g[2]}};
                for(int which = 0; which < 3; which++)
                {
                    const int* const row = rows[which];
                    Image strip;
                    strip.reset(24, 8);
                    for(int i = 0; i < 3; i++)
                    {
                        for(int y = 0; y < 8; y++)
                        {
                            for(int x = 0; x < 8; x++)
                            {
                                strip.at(i * 8 + x, y) = state.chars.at((row[i] & 15) * 8 + x, (row[i] >> 4) * 8 + y);
                            }
                        }
                    }
                    Image whole;
                    upscalePadded(mode == Classic ? NearestSmooth : mode, strip, 0, 0, 24, 8, {}, whole);
                    za::Vector<za::U32> ref(static_cast<za::SizeT>(whole.w) * whole.h, 0u);
                    store(whole, ref.data(), whole.w, 0, 0);
                    const int c = row[which];
                    int differ = 0;
                    for(int y = 0; y < 32; y++)
                    {
                        for(int x = 0; x < 32; x++)
                        {
                            const za::U32 a = state.atlas[static_cast<za::SizeT>((c >> 4) * cell4 + margin * 4 + y) * atlas4 +
                                (c & 15) * cell4 + margin * 4 + x];
                            const za::U32 b = ref[static_cast<za::SizeT>(pad * 4 + y) * whole.w + pad * 4 + which * 32 + x];
                            differ += (a >> 24) != (b >> 24) || ((a >> 24) && a != b);
                        }
                    }
                    check(differ == 0, va("mode %d fx %d: piece %d joins (%d texels differ)", mode, fx, c, differ));
                }
            }
        }
    }
    state.style = saved;
    rebuild(); // the cvars' own again
    tally.verdict("fontstyle test");
}

// vr_fontstyle_sample (Menu > Fonts, Debug > Tools): a centre message and a console line (the notify lines in game),
// white and gold, to judge the style over the game.
void sampleCommand()
{
    char gold[64];
    q_strlcpy(gold, "Gold: 0123456789 You got the Nailgun!", sizeof(gold));
    for(char* c = gold; *c; c++)
    {
        *c = static_cast<char>(*c | 0x80);
    }
    SCR_CenterPrintFor("The Quick Brown Fox Jumps Over\nthe Lazy Dog: 0123456789 !?\nYou need the silver key", 6.f);
    Con_Printf("Sample: You got the shells (25) and 100 health\n%s\n", gold);
}

} // namespace

namespace qvr::fontstyle
{

void init()
{
    for(cvar_t* v : {&vr_font_upscale, &vr_font_outline, &vr_font_outline_r, &vr_font_outline_g, &vr_font_outline_b,
            &vr_font_shadow_opacity, &vr_font_shadow_size, &vr_font_shadow_spread, &vr_font_shadow_r, &vr_font_shadow_g,
            &vr_font_shadow_b})
    {
        Cvar_SetCallback(v, onStyleChanged);
    }
    Cmd_AddCommand("vr_fontstyle_test", testCommand);
    Cmd_AddCommand("vr_fontstyle_sample", sampleCommand);
}

int generation()
{
    return state.generation;
}

bool active()
{
    return state.ready && state.style.active();
}

void restyleStrip(const za::U8* data, int width, int height, const int* x, const int* w, int count, const char* name,
    Strip& out)
{
    const Style& st = state.style;
    out.margin = st.effects() ? margin : 0;
    out.column.clear();
    out.column.resize(static_cast<za::SizeT>(count), -1);
    Image src;
    src.reset(width, height);
    for(int t = 0; t < width * height; t++)
    {
        src.px[static_cast<za::SizeT>(t)] = fromPalette(d_8to24table, data[t]);
    }
    int at = 0;
    for(int i = 0; i < count; i++)
    {
        if(x[i] >= 0)
        {
            out.column[static_cast<za::SizeT>(i)] = at;
            at += w[i] + 2 * out.margin;
        }
    }
    out.width = glm::max(at, 1);
    out.height = height + 2 * out.margin;
    out.rgba.clear();
    out.rgba.resize(static_cast<za::SizeT>(out.width) * 4 * out.height * 4, 0u);
    for(int i = 0; i < count; i++)
    {
        if(x[i] < 0)
        {
            continue;
        }
        upscalePadded(st.upscale == Classic ? NearestSmooth : st.upscale, src, x[i], 0, w[i], height, {}, scratch.scaled);
        Image& cell = scratch.cell;
        cell.reset((w[i] + 2 * out.margin) * 4, out.height * 4);
        for(int y = 0; y < height * 4; y++)
        {
            for(int c = 0; c < w[i] * 4; c++)
            {
                cell.at(out.margin * 4 + c, out.margin * 4 + y) = scratch.scaled.at(pad * 4 + c, pad * 4 + y);
            }
        }
        if(st.effects())
        {
            applyEffects(st, cell);
        }
        store(cell, out.rgba.data(), out.width * 4, out.column[static_cast<za::SizeT>(i)] * 4, 0);
    }
    edgeFix(out.rgba.data(), out.width * 4, out.height * 4);
    if(out.pic.empty())
    {
        out.pic.resize(Draw_PicBytes(), 0);
    }
    Draw_ReplacePicRGBA(reinterpret_cast<qpic_t*>(out.pic.data()), name, out.width, out.height, out.width * 4,
        out.height * 4, out.rgba.data(), st.smooth());
}

} // namespace qvr::fontstyle

// ---- the engine's side (vr_api.h) ----

extern "C" void VR_FontStyle_Rebuild(void)
{
    // conchars as 16 x 16 cells of 8 x 8, from gl_draw.c's padded copy (the console's palette: 0 transparent)
    state.chars.reset(128, 128);
    for(int c = 0; c < 256; c++)
    {
        for(int y = 0; y < 8; y++)
        {
            for(int x = 0; x < 8; x++)
            {
                const byte v = char_texture_data[((c >> 4) * 10 + 1 + y) * 160 + (c & 15) * 10 + 1 + x];
                state.chars.at((c & 15) * 8 + x, (c >> 4) * 8 + y) = fromPalette(d_8to24table_conchars, v);
            }
        }
    }
    state.source = char_texture;
    state.ready = true;
    loadBoxSources();
    rebuild();
}

extern "C" gltexture_t* VR_FontStyle_Chars(gltexture_t* conchars)
{
    return state.texture && conchars == state.source ? state.texture : nullptr;
}

extern "C" int VR_FontStyle_CharCell(int num, float* s, float* t, float* size, float* grow)
{
    num &= 255;
    const bool fx = cellKind(num) == CellKind::Letter && state.style.effects();
    const float inset = fx ? 0.f : margin * 4.f;
    *s = ((num & 15) * cell4 + inset) / atlas4;
    *t = ((num >> 4) * cell4 + inset) / atlas4;
    *size = (cell4 - 2.f * inset) / atlas4;
    *grow = fx ? margin / 8.f : 0.f;
    return fx ? (state.style.outline > 0.f ? 3 : 1) : 0;
}

extern "C" qpic_t* VR_FontStyle_Pic(const char* name, qpic_t* pic)
{
    if(!state.texture)
    {
        return pic;
    }
    for(int i = 0; i < boxCount; i++)
    {
        if(state.box[static_cast<za::SizeT>(i)].made && !strcmp(name, boxPieces[i].name))
        {
            return reinterpret_cast<qpic_t*>(state.box[static_cast<za::SizeT>(i)].pic.data());
        }
    }
    return pic;
}
