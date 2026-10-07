#include "vr_alloccount.h"
// vr_menubrand.cpp -- the menus in Quake VR's colours:
//
// - The banner: Quake VR: Unleashed's vertical logo (quakevr/gfx/vr/menu_banner.png, made by
//   Misc/quakevr/make_menu_banner.py) where Quake's vertical plaque (gfx/qplaque.lmp) was, as tall (144 menu pixels
//   from its top), centred a little left of the plaque's column so that the main menu's cursor stays clear of it
//   (menu.c, M_DrawPlaque). With the VR menu style the plaque is left out (the panel's rows reach its column), so the
//   banner stands in the column under the corner's buttons instead, on every menu, the same size on all. The image is mipmapped and filtered smoothly, and only its
//   opaque rectangle is drawn (the image found from its alpha). Without the image: Quake's plaque, as before.
// - The version label (vr_menu_version): "Quake VR: Unleashed - v0.9" over "by Vittorio Romeo", small, right-aligned
//   in the canvas's bottom right corner of every menu (VR and flat), the version from the repository's VERSION file
//   (VR_Version: MAJOR.MINOR while PATCH is 0, else the whole), a dev build's "-dev" fainter. Drawn before the page (what
//   opens over it hides it), and only where nothing the menu draws reaches under it (qvr::menu::contentRightBelow) nor
//   the status box comes down to it.
// - The colours (vr_menu_recolor): while the menus draw, the gui shader turns their browns, tans, oranges and
//   yellows towards a blood red (gl_shaders.h, MenuRecolor; gl_draw.c, Draw_SetMenuRecolor), a true hue change in
//   Oklab: the lightness kept, the chroma kept but for vr_menu_recolor_saturation, greys, blues and greens left
//   alone. Only what M_Draw draws: not the game, the HUD or the console under the menu; and not the banner, which
//   is red already.

#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hue.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cbrt.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Pow.hpp"

extern "C" int M_TextLeft(void); // menu.c

namespace
{

constexpr const char* bannerImage = "gfx/vr/menu_banner"; // png, tga or jpg
constexpr float plaqueHeight = 144.f;                     // gfx/qplaque.lmp's height (menu pixels)
constexpr float plaqueCentre = 14.f;     // the banner's middle from the plaque's left (its column 32 wide: 16 would put
                                         // a 48 wide banner's right edge on the main menu's cursor, at x 54)
constexpr float columnHeight = 216.f;    // the banner's height there (true pixels: the plaque's at the shipped row spacing,
                                         // 1.5), the same on every menu (their canvases stretch y by different amounts)
constexpr float columnGap = 8.f;         // under the buttons, and above the panel's bottom (true pixels)
constexpr float columnMinHeight = 40.f;  // the least banner worth drawing there (true pixels)
constexpr float flatGap = 8.f;           // a flat screen's banner: at least this clear left of the menu's text (menu x)

struct Banner
{
    alignas(16) za::U8 pic[64]{}; // its qpic_t (Draw_LoadImagePic: Draw_PicBytes () bytes)
    int bounds[4]{};              // the image's opaque rectangle (texels): left, top, right, bottom
    int state{0};                 // 0: not loaded yet, 1: loaded, -1: no image
};

Banner banner;

// Where the banner was last drawn (menu x and y; nothing: x1 < x0), for menu_vr pos.
struct BannerPlace
{
    float x0{0.f}, x1{-1.f}, y0{0.f}, y1{-1.f};
};
BannerPlace bannerPlace;

[[nodiscard]] qpic_t* bannerPic()
{
    if(banner.state == 0)
    {
        banner.state = -1;
        if(Draw_PicBytes() > sizeof(banner.pic))
        {
            Con_Warning("VR: the menu banner's pic needs %d bytes\n", static_cast<int>(Draw_PicBytes()));
        }
        else if(!Draw_LoadImagePic(reinterpret_cast<qpic_t*>(banner.pic), bannerImage, banner.bounds))
        {
            Con_DPrintf("VR: no %s image: Quake's plaque in the menus\n", bannerImage);
        }
        else if(banner.bounds[2] > banner.bounds[0] && banner.bounds[3] > banner.bounds[1])
        {
            banner.state = 1;
        }
    }
    return banner.state > 0 ? reinterpret_cast<qpic_t*>(banner.pic) : nullptr;
}

// The logo's width over its height.
[[nodiscard]] float bannerAspect()
{
    return static_cast<float>(banner.bounds[2] - banner.bounds[0]) / static_cast<float>(banner.bounds[3] - banner.bounds[1]);
}

// How much more the menu canvas stretches y than x (gl_draw.c, Draw_KeepMenuGlyphSize: pictures keep their size).
[[nodiscard]] float glyphStretch()
{
    const drawtransform_t& t = glcanvas.transform;
    if(glcanvas.type != CANVAS_MENU)
    {
        return 1.f;
    }
    const float k = -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth);
    return k < 1.001f ? 1.f : k;
}

// The logo from (x, top) (menu pixels) `width` across and `height` true pixels down (as wide as they are across),
// in its own colours (not the menus' red), faded as the menu's canvas colour says.
void drawBanner(qpic_t* pic, float x, float top, float width, float height)
{
    const uint32_t c = glcanvas.colorstack[glcanvas.colorstacktop];
    const float rgb[3] = {static_cast<float>(c & 0xff) / 255.f, static_cast<float>((c >> 8) & 0xff) / 255.f,
        static_cast<float>((c >> 16) & 0xff) / 255.f};
    const float alpha = static_cast<float>((c >> 24) & 0xff) / 255.f;
    const float w = static_cast<float>(pic->width);
    const float h = static_cast<float>(pic->height);
    const float k = glyphStretch();
    const qboolean recolor = Draw_SetMenuRecolor(false);
    // (Draw_SubPic moves a picture's y and shrinks its height by the stretch: give it what lands on top..height.)
    Draw_SubPic(x, top - height * (1.f - 1.f / k) * 0.5f, width, height, pic, static_cast<float>(banner.bounds[0]) / w,
        static_cast<float>(banner.bounds[1]) / h, static_cast<float>(banner.bounds[2] - banner.bounds[0]) / w,
        static_cast<float>(banner.bounds[3] - banner.bounds[1]) / h, rgb, alpha);
    Draw_SetMenuRecolor(recolor);
}

// Oklab's hue (radians) of an sRGB colour (gamma 2.2, as the gui shader takes it).
[[nodiscard]] float oklabHue(const glm::vec3& srgb)
{
    const glm::vec3 c{za::pow(srgb.x, 2.2f), za::pow(srgb.y, 2.2f), za::pow(srgb.z, 2.2f)};
    const float l = za::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = za::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = za::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    const float a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    const float b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    return za::atan2(b, a);
}

} // namespace

// Quake's plaque's place, its top left at (x, y): the banner as tall, centred on plaqueCentre.
extern "C" int VR_MenuDrawBanner(int x, int y)
{
    qpic_t* pic = bannerPic();
    if(!pic)
    {
        return 0;
    }
    const float k = glyphStretch();
    float height = plaqueHeight * k;
    float width = height * bannerAspect();
    float left = static_cast<float>(x) + plaqueCentre - width * 0.5f;
    // Clear left of the menu's text, as the headset's column is (menu::contentLeft): the VR pages' long labels and help,
    // and Ironwail's lists, reach into the plaque's column. As far left as the canvas goes; narrower (and shorter) where
    // even that is too near; left out where too little of it would be left.
    const float text = m_state == m_vr ? qvr::menu::contentLeft() : static_cast<float>(M_TextLeft());
    const float right = za::fmin(left + width, text - flatGap);
    if(right < left + width)
    {
        left = za::fmax(right - width, glcanvas.left + 2.f);
        width = right - left;
        height = width / bannerAspect();
        if(height < columnMinHeight)
        {
            bannerPlace = {};
            return 1; // (no plaque either)
        }
    }
    drawBanner(pic, left, static_cast<float>(y), width, height);
    bannerPlace = {left, left + width, static_cast<float>(y), static_cast<float>(y) + height / k};
    return 1;
}

void qvr::menuui::bannerRect(float& x0, float& x1, float& y0, float& y1)
{
    x0 = bannerPlace.x0;
    x1 = bannerPlace.x1;
    y0 = bannerPlace.y0;
    y1 = bannerPlace.y1;
}

// The VR menu style: the banner in the corner buttons' column (or, where they are only icons, the panel's margin left
// of the menus), just under the buttons, columnHeight tall (less where the panel ends sooner or the column is
// narrower).
extern "C" void VR_MenuDrawBannerColumn()
{
    if(!qvr::menuui::active() || glcanvas.type != CANVAS_MENU)
    {
        return;
    }
    qpic_t* pic = bannerPic();
    if(!pic)
    {
        return;
    }
    const float k = glyphStretch();
    float x0 = qvr::menuui::toolbarLeft();
    float x1 = qvr::menuui::toolbarRight();
    if(x1 - x0 < columnMinHeight)
    {
        x0 = glcanvas.left + 4.f;
        x1 = qvr::menuui::toolbarLimit(); // (as near the menu as the column may come)
    }
    const float top = qvr::menuui::toolbarBottom() + columnGap / k;
    float height = za::fmin(columnHeight, (glcanvas.bottom - top) * k - columnGap); // true pixels
    float width = height * bannerAspect();
    if(width > x1 - x0)
    {
        width = x1 - x0;
        height = width / bannerAspect();
    }
    if(height < columnMinHeight)
    {
        return;
    }
    drawBanner(pic, (x0 + x1 - width) * 0.5f, top, width, height);
}

namespace
{

constexpr const char* versionTitle = "Quake VR: Unleashed - ";
constexpr const char* versionAuthor = "by Vittorio Romeo";
constexpr const char* versionDevMark = "-dev";
constexpr float versionCorner = 4.f; // from the canvas's right and bottom edges (true pixels; the status box's)
constexpr float versionGap = 8.f;    // clear of what the menu draws beside it (menu x) and of the status box (true pixels)
constexpr float versionGapAbove = 4.f; // and of what it draws above it (true pixels; a VR page's help box ends 4 above its bottom)

// The label as last placed, for menu_vr pos.
struct VersionLabel
{
    char text[64]{};
    float x0{0.f}, x1{-1.f}, y0{0.f}, y1{-1.f}; // menu x and y
    float contentRight{0.f};                    // what the menu draws right to under it (and versionGapAbove above)
    float statusBottom{0.f};
    const char* leftOut{"no menu drawn yet"}; // why it was not drawn (nullptr: drawn)
};
VersionLabel versionLabel;

// "v" and the version: MAJOR.MINOR while PATCH is 0 ("v0.9"), else the whole ("v0.9.1", "v1.0.0-beta.1").
void versionShown(char* out, size_t size)
{
    const char* v = VR_Version();
    int major = 0, minor = 0, patch = 0, end = 0;
    if(sscanf(v, "%d.%d.%d%n", &major, &minor, &patch, &end) == 3 && patch == 0 && v[end] == '\0')
    {
        q_snprintf(out, size, "v%d.%d", major, minor);
    }
    else
    {
        q_snprintf(out, size, "v%s", v);
    }
}

// The canvas's colour (the menus' fade), its alpha times `alpha`, for what is drawn next.
void pushFaded(float alpha)
{
    const uint32_t c = glcanvas.colorstack[glcanvas.colorstacktop];
    GL_PushCanvasColor(static_cast<float>(c & 0xff) / 255.f, static_cast<float>((c >> 8) & 0xff) / 255.f,
        static_cast<float>((c >> 16) & 0xff) / 255.f, static_cast<float>((c >> 24) & 0xff) / 255.f * alpha);
}

// `text` from x on the row whose middle is ym (menu y), `size` across, in the menus' tan (turned red with them).
float drawSmall(float x, float ym, float size, const char* text)
{
    for(const char* c = text; *c; c++, x += size)
    {
        Draw_CharacterEx(x, ym - size * 0.5f, size, size, *c | 128);
    }
    return x;
}

// Where the label goes (menu x and y), from the menu canvas's transform (as the menus lay out, whether or not it is the
// one set): right-aligned versionCorner from the canvas's right and bottom edges, the title over the author.
struct VersionBox
{
    float x0, x1, y0, y1;
    float size, step, k;
    float titleWidth, authorWidth;
    bool dev;
};
[[nodiscard]] VersionBox versionBox(char* title, size_t titleSize)
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float left, top, right, bottom;
    Draw_GetTransformBounds(&t, &left, &top, &right, &bottom);
    // The status box's size: nearer the corner buttons' 8 in the headset, small on a flat screen.
    const bool headset = qvr::menuui::active();
    VersionBox b;
    b.size = headset ? 7.f : 5.f;
    b.step = headset ? 9.f : 7.f;
    b.k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth)); // (as glyphStretch)
    char version[40];
    versionShown(version, sizeof(version));
    b.dev = VR_VersionIsDev() != 0;
    q_snprintf(title, titleSize, "%s%s", versionTitle, version);
    b.titleWidth = b.size * static_cast<float>(strlen(title) + (b.dev ? strlen(versionDevMark) : 0));
    b.authorWidth = b.size * static_cast<float>(strlen(versionAuthor));
    b.x1 = right - versionCorner;
    b.x0 = b.x1 - za::fmax(b.titleWidth, b.authorWidth);
    b.y1 = bottom - versionCorner / b.k;
    b.y0 = b.y1 - (b.step + b.size) / b.k;
    return b;
}

} // namespace

bool qvr::menuui::versionLabelClearance(float& x, float& y)
{
    if(!vr_menu_version.value)
    {
        return false;
    }
    char title[64];
    const VersionBox b = versionBox(title, sizeof(title));
    x = b.x0 - versionGap;
    y = b.y0 - versionGapAbove / b.k;
    return true;
}

extern "C" void VR_MenuDrawVersion()
{
    VersionLabel& v = versionLabel;
    v.x1 = v.x0 - 1.f;
    if(!qvr::vr_menu_version.value)
    {
        v.leftOut = "vr_menu_version 0";
        return;
    }
    if(glcanvas.type != CANVAS_MENU)
    {
        v.leftOut = "not the menu canvas";
        return;
    }
    const VersionBox b = versionBox(v.text, sizeof(v.text));
    const float size = b.size, step = b.step, k = b.k;
    v.x0 = b.x0;
    v.x1 = b.x1;
    v.y0 = b.y0;
    v.y1 = b.y1;
    v.contentRight = qvr::menu::contentRightBelow(v.y0 - versionGapAbove / k);
    v.statusBottom = qvr::menuui::statusBottom(v.x1);
    if(v.x0 < v.contentRight + versionGap)
    {
        v.leftOut = "the menu reaches under it";
        return;
    }
    if(v.y0 < v.statusBottom + versionGap / k)
    {
        v.leftOut = "the status box reaches down to it";
        return;
    }
    v.leftOut = nullptr;

    const float title = v.y0 + size * 0.5f / k;
    const float author = v.y0 + (step + size * 0.5f) / k;
    pushFaded(0.85f);
    const float devX = drawSmall(v.x1 - b.titleWidth, title, size, v.text);
    GL_PopCanvasColor();
    if(b.dev)
    {
        pushFaded(0.45f); // (subtle: a build not made for a release)
        drawSmall(devX, title, size, versionDevMark);
        GL_PopCanvasColor();
    }
    pushFaded(0.6f);
    drawSmall(v.x1 - b.authorWidth, author, size, versionAuthor);
    GL_PopCanvasColor();
}

void qvr::menuui::printVersionLabel()
{
    const VersionLabel& v = versionLabel;
    if(v.leftOut)
    {
        Con_Printf("menu_vr pos: version label \"%s%s\" not drawn (%s; x %.0f..%.0f, y %.1f..%.1f, menu to x %.0f, status to y %.1f)\n",
            v.text, VR_VersionIsDev() ? versionDevMark : "", v.leftOut, v.x0, v.x1, v.y0, v.y1, v.contentRight, v.statusBottom);
        return;
    }
    Con_Printf("menu_vr pos: version label \"%s%s\" x %.0f..%.0f, y %.1f..%.1f (menu to x %.0f, status to y %.1f; build %s)\n",
        v.text, VR_VersionIsDev() ? versionDevMark : "", v.x0, v.x1, v.y0, v.y1, v.contentRight, v.statusBottom, VR_BuildVersion());
}

extern "C" void VR_MenuRecolor(float* params)
{
    const float strength = qvr::vr_menu_recolor.value ? za::clamp(qvr::vr_menu_recolor_strength.value, 0.f, 1.f) : 0.f;
    if(strength <= 0.f)
    {
        params[0] = params[1] = params[2] = params[3] = 0.f;
        return;
    }
    params[0] = strength;
    params[1] = oklabHue(qvr::hsv(qvr::hue::of(qvr::vr_menu_recolor_hue), 1.f, 1.f));
    params[2] = za::clamp(qvr::vr_menu_recolor_saturation.value, 0.5f, 3.f);
    params[3] = 0.f;
}
