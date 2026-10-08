#include "vr_alloccount.h"
// vr_menubrand.cpp -- the menus in Quake VR's colours:
//
// - The banner: Quake VR: Unleashed's vertical logo (quakevr/gfx/vr/menu_banner.png, made by
//   Misc/quakevr/make_menu_banner.py) where Quake's vertical plaque (gfx/qplaque.lmp) was, as tall (144 menu pixels
//   from its top), centred a little left of the plaque's column so that the main menu's cursor stays clear of it
//   (menu.c, M_DrawPlaque). With the VR menu style the plaque is left out (the panel's rows reach its column), so the
//   banner stands in the column under the corner's buttons instead, on every menu, the same size on all. The image is mipmapped and filtered smoothly, and only its
//   opaque rectangle is drawn (the image found from its alpha). Without the image: Quake's plaque, as before.
// - The version label (vr_menu_version): "Quake VR: Unleashed - v0.9" over "by Vittorio Romeo" over a "Support on
//   Ko-fi" link (Ko-fi's cup before it), small, right-aligned in a box like the status box's (as far from the corner,
//   its lines as far inside it) in the canvas's bottom right corner of every menu (VR and flat), the version from the
//   repository's VERSION file (VR_Version: MAJOR.MINOR while PATCH is 0, else the whole), a dev build's "-dev" fainter.
//   Drawn before the page (what opens over it hides it), and only where nothing the menu draws reaches under it
//   (qvr::menu::contentRightBelow) nor the status box comes down to it. The link lights up under the laser or the
//   desktop mouse; a press (trigger, click) opens https://ko-fi.com/vittorioromeovee in the desktop's browser
//   (SDL_OpenURL) once, and the link says so for a few seconds (vr_menu_link_dryrun: only printed).
// - The colours (vr_menu_recolor): while the menus draw, the gui shader turns their browns, tans, oranges and
//   yellows towards a blood red (gl_shaders.h, MenuRecolor; gl_draw.c, Draw_SetMenuRecolor), a true hue change in
//   Oklab: the lightness kept, the chroma kept but for vr_menu_recolor_saturation, greys, blues and greens left
//   alone. Only what M_Draw draws: not the game, the HUD or the console under the menu; and not the banner, which
//   is red already.

#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hue.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menupaint.hpp"
#include "vr_menuui.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cbrt.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Pow.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

extern "C" int M_TextLeft(void); // menu.c
extern "C" float m_mousex, m_mousey; // menu.c: the menus' mouse, in menu coordinates

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

// An image drawn in the menus: the banner, and Ko-fi's logo (the version box's link).
struct Banner
{
    const char* path;             // png, tga or jpg
    alignas(16) za::U8 pic[64]{}; // its qpic_t (Draw_LoadImagePic: Draw_PicBytes () bytes)
    int bounds[4]{};              // the image's opaque rectangle (texels): left, top, right, bottom
    int state{0};                 // 0: not loaded yet, 1: loaded, -1: no image
};

Banner banner{bannerImage};
Banner kofiLogo{"gfx/vr/kofi_symbol"}; // Ko-fi's cup, unaltered (Ko-fi's brand assets; the installer's Assets/kofi_symbol.png)

// Where the banner was last drawn (menu x and y; nothing: x1 < x0), for menu_vr pos.
struct BannerPlace
{
    float x0{0.f}, x1{-1.f}, y0{0.f}, y1{-1.f};
};
BannerPlace bannerPlace;

[[nodiscard]] qpic_t* imagePic(Banner& b)
{
    if(b.state == 0)
    {
        b.state = -1;
        if(Draw_PicBytes() > sizeof(b.pic))
        {
            Con_Warning("VR: the menus' %s pic needs %d bytes\n", b.path, static_cast<int>(Draw_PicBytes()));
        }
        else if(!Draw_LoadImagePic(reinterpret_cast<qpic_t*>(b.pic), b.path, b.bounds))
        {
            Con_DPrintf("VR: no %s image%s\n", b.path, &b == &banner ? ": Quake's plaque in the menus" : "");
        }
        else if(b.bounds[2] > b.bounds[0] && b.bounds[3] > b.bounds[1])
        {
            b.state = 1;
        }
    }
    return b.state > 0 ? reinterpret_cast<qpic_t*>(b.pic) : nullptr;
}

[[nodiscard]] qpic_t* bannerPic()
{
    return imagePic(banner);
}

// An image's width over its height (its opaque rectangle's).
[[nodiscard]] float imageAspect(const Banner& b)
{
    return static_cast<float>(b.bounds[2] - b.bounds[0]) / static_cast<float>(b.bounds[3] - b.bounds[1]);
}

[[nodiscard]] float bannerAspect()
{
    return imageAspect(banner);
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

// An image (its opaque rectangle) from (x, top) (menu pixels) `width` across and `height` true pixels down (as wide as
// they are across), in its own colours (not the menus' red), faded as the menu's canvas colour says.
void drawImage(const Banner& b, qpic_t* pic, float x, float top, float width, float height)
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
    Draw_SubPic(x, top - height * (1.f - 1.f / k) * 0.5f, width, height, pic, static_cast<float>(b.bounds[0]) / w,
        static_cast<float>(b.bounds[1]) / h, static_cast<float>(b.bounds[2] - b.bounds[0]) / w,
        static_cast<float>(b.bounds[3] - b.bounds[1]) / h, rgb, alpha);
    Draw_SetMenuRecolor(recolor);
}

void drawBanner(qpic_t* pic, float x, float top, float width, float height)
{
    drawImage(banner, pic, x, top, width, height);
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
constexpr const char* linkText = "Support on Ko-fi";
constexpr const char* linkUrl = "https://ko-fi.com/vittorioromeovee";
constexpr float versionCorner = 4.f; // the box from the canvas's right and bottom edges (true pixels; the status box's)
constexpr float versionPad = 4.f;    // its lines from its edges (true pixels; the status box's: vr_menuui.cpp, StatusMetrics)
constexpr float versionGap = 8.f;    // clear of what the menu draws beside it (menu x) and of the status box (true pixels)
constexpr float versionGapAbove = 4.f; // and of what it draws above it (true pixels; a VR page's help box ends 4 above its bottom)
constexpr double linkFeedback = 3.0;   // seconds the link says it opened the page
constexpr double linkRepeat = 1.5;     // seconds a second press does nothing (a double click, both triggers)

// The label as last placed, for menu_vr pos and the link's presses.
struct VersionLabel
{
    char text[64]{};
    float x0{0.f}, x1{-1.f}, y0{0.f}, y1{-1.f}; // the box (menu x and y)
    float lx0{0.f}, lx1{-1.f}, ly0{0.f}, ly1{-1.f}; // what the link takes (menu x and y): its row, to the canvas's corner
    float lxc{0.f}, lyc{0.f};                       // the link's middle (vr_mock_laser kofi)
    float contentRight{0.f};                        // what the menu draws right to under it (and versionGapAbove above)
    float statusBottom{0.f};
    const char* leftOut{"no menu drawn yet"}; // why it was not drawn (nullptr: drawn)
    int menu{m_none};                         // the menu it was last drawn over
    int frame{-10};                           // and when (host_framecount)
    bool hot{false};                          // the link under the laser or the mouse
    double pressed{-100.0};                   // when the link was last pressed (realtime)
    int opened{0};                            // the times it opened the page (or, in tests, printed it)
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

// `text` from x on the row whose middle is ym (menu y), `size` across, in the menus' tan (turned red with them), or
// white.
float drawSmall(float x, float ym, float size, const char* text, bool white = false)
{
    for(const char* c = text; *c; c++, x += size)
    {
        Draw_CharacterEx(x, ym - size * 0.5f, size, size, white ? *c : *c | 128);
    }
    return x;
}

// What the link says: "Support on Ko-fi", or for a while after a press, where the page opened (the headset's
// player does not see the desktop's browser).
[[nodiscard]] const char* linkShown()
{
    if(realtime - versionLabel.pressed < linkFeedback)
    {
        return qvr::vrActive() ? "Opened on your desktop" : "Opened in your browser";
    }
    return linkText;
}

// Where the box goes (menu x and y), from the menu canvas's transform (as the menus lay out, whether or not it is the
// one set): versionCorner from the canvas's right and bottom edges, its lines versionPad inside it, right-aligned:
// the title, the author, and a little lower the link (Ko-fi's cup and its text).
struct VersionBox
{
    float x0, x1, y0, y1;     // the box
    float right, bottom;      // the canvas's corner
    float size, step, k;
    float textRight;          // the lines' right end
    float titleY, authorY, linkY; // the rows' middles
    float linkHalf;           // the link's row: half its height, its highlight's (true pixels)
    float iconHeight;         // Ko-fi's cup (true pixels)
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
    b.right = right;
    b.bottom = bottom;
    b.size = headset ? 7.f : 5.f;
    b.step = headset ? 9.f : 7.f;
    b.k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth)); // (as glyphStretch)
    const float linkGap = headset ? 3.f : 2.f; // the link's row a little apart from the lines above it (true pixels)
    b.linkHalf = b.size * 0.5f + 2.f;
    b.iconHeight = b.size + 3.f; // (within the highlight)
    char version[40];
    versionShown(version, sizeof(version));
    b.dev = VR_VersionIsDev() != 0;
    q_snprintf(title, titleSize, "%s%s", versionTitle, version);
    b.titleWidth = b.size * static_cast<float>(strlen(title) + (b.dev ? strlen(versionDevMark) : 0));
    b.authorWidth = b.size * static_cast<float>(strlen(versionAuthor));
    // (The link as wide as its longest text, so that a press does not change the box.)
    const float iconWidth = b.iconHeight * 1.25f + b.size * 0.5f;
    const float linkWidth = iconWidth + b.size * static_cast<float>(strlen("Opened on your desktop"));
    b.x1 = right - versionCorner;
    b.textRight = b.x1 - versionPad;
    b.x0 = b.textRight - za::fmax(za::fmax(b.titleWidth, b.authorWidth), linkWidth) - versionPad;
    b.y1 = bottom - versionCorner / b.k;
    const float height = versionPad + 2.f * b.step + linkGap + b.size + versionPad; // true pixels
    b.y0 = b.y1 - height / b.k;
    b.titleY = b.y0 + (versionPad + b.size * 0.5f) / b.k;
    b.authorY = b.y0 + (versionPad + b.step + b.size * 0.5f) / b.k;
    b.linkY = b.y0 + (versionPad + 2.f * b.step + linkGap + b.size * 0.5f) / b.k;
    return b;
}

// Whether the label is over this menu now (drawn this frame or the last).
[[nodiscard]] bool labelShown()
{
    const VersionLabel& v = versionLabel;
    return !v.leftOut && v.menu == m_state && key_dest == key_menu && host_framecount - v.frame <= 2;
}

// The page in the desktop's browser (SDL_OpenURL), or only its address printed: vr_menu_link_dryrun, and the kit's test
// runs (QVR_TEST_HIDDEN), never open a browser.
void openLink(const char* url)
{
    VersionLabel& v = versionLabel;
    v.opened++;
    const bool dry = qvr::vr_menu_link_dryrun.value || getenv("QVR_TEST_HIDDEN");
    Con_Printf("menu link: opening %s (%d)%s\n", url, v.opened, dry ? " (dry run: no browser)" : "");
    if(!dry && SDL_OpenURL(url) != 0)
    {
        Con_Printf("menu link: the browser could not be opened (%s)\n", SDL_GetError());
    }
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

bool qvr::menuui::versionLinkAt(float x, float y)
{
    const VersionLabel& v = versionLabel;
    return labelShown() && x >= v.lx0 && x <= v.lx1 && y >= v.ly0 && y <= v.ly1;
}

bool qvr::menuui::versionLinkSpot(float& x, float& y)
{
    x = versionLabel.lxc;
    y = versionLabel.lyc;
    return labelShown();
}

bool qvr::menuui::versionLinkPress()
{
    VersionLabel& v = versionLabel;
    if(!versionLinkAt(m_mousex, m_mousey) || !pointerOn(false))
    {
        return false;
    }
    if(realtime - v.pressed < linkRepeat)
    {
        return true; // (taken: the page is opening already)
    }
    v.pressed = realtime;
    S_LocalSound("misc/menu2.wav");
    openLink(linkUrl);
    return true;
}

extern "C" void VR_MenuDrawVersion()
{
    VersionLabel& v = versionLabel;
    v.x1 = v.x0 - 1.f;
    v.hot = false;
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
    const float size = b.size, k = b.k;
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
    v.menu = m_state;
    v.frame = host_framecount;
    // The link takes its row as wide as the box, and on to the canvas's corner.
    v.lx0 = b.x0;
    v.lx1 = b.right;
    v.ly0 = b.linkY - b.linkHalf / k;
    v.ly1 = b.bottom;
    v.lxc = (b.x0 + b.x1) * 0.5f;
    v.lyc = b.linkY;
    v.hot = qvr::menuui::versionLinkAt(m_mousex, m_mousey) && qvr::menuui::pointerOn(true);

    // A box like the status box (vr_menuui.cpp, VR_MenuDrawStatus).
    const qvr::menupaint::Painter p;
    namespace colors = qvr::menupaint::colors;
    const float half = (b.y1 - b.y0) * k * 0.5f;
    const float yc = (b.y0 + b.y1) * 0.5f;
    p.rounded(b.x0, b.x1, yc, half, 3.f, colors::boxBorder);
    p.rounded(b.x0 + 1.f, b.x1 - 1.f, yc, half - 1.f, 2.f, colors::boxFill);

    pushFaded(0.85f);
    const float devX = drawSmall(b.textRight - b.titleWidth, b.titleY, size, v.text);
    GL_PopCanvasColor();
    if(b.dev)
    {
        pushFaded(0.45f); // (subtle: a build not made for a release)
        drawSmall(devX, b.titleY, size, versionDevMark);
        GL_PopCanvasColor();
    }
    pushFaded(0.6f);
    drawSmall(b.textRight - b.authorWidth, b.authorY, size, versionAuthor);
    GL_PopCanvasColor();

    // The link: lit as the corner's buttons under the laser or the mouse, white for a while once pressed.
    const bool pressed = realtime - v.pressed < linkFeedback;
    if(v.hot)
    {
        p.rounded(b.x0 + 2.f, b.x1 - 2.f, b.linkY, b.linkHalf, 2.f, colors::highlightEdge);
        p.rounded(b.x0 + 3.f, b.x1 - 3.f, b.linkY, b.linkHalf - 1.f, 1.5f, colors::buttonHover);
    }
    const char* text = linkShown();
    const float textX = b.textRight - size * static_cast<float>(strlen(text));
    drawSmall(textX, b.linkY, size, text, v.hot || pressed);
    if(qpic_t* pic = imagePic(kofiLogo))
    {
        const float height = b.iconHeight;
        const float width = height * imageAspect(kofiLogo);
        drawImage(kofiLogo, pic, textX - size * 0.5f - width, b.linkY - height * 0.5f / k, width, height);
    }
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
    Con_Printf("menu_vr pos: version link \"%s\" at %.1f %.1f, takes x %.0f..%.0f, y %.1f..%.1f%s, opened %d\n", linkShown(),
        v.lxc, v.lyc, v.lx0, v.lx1, v.ly0, v.ly1, v.hot ? " (lit)" : "", v.opened);
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
