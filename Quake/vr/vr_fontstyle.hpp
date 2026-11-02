// vr_fontstyle.hpp -- the 2D font restyled (vr_fontstyle.cpp): the console font, Quake's text box pieces and the main
// menu's lettering upscaled 4x and drawn smooth, with an outline and a drop shadow round the letters (Advanced VR
// Options > HUD and Menus > Menu, "Fonts"). gl_draw.c and menu.c use it through vr_api.h (VR_FontStyle_*).

#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"

struct gltexture_s;
struct cvar_s;

namespace qvr::fontstyle
{

// The kinds of text, each with its own style (upscaling, outline, shadow) and settings: the menus and everything
// else drawn on the 2D canvas (Menu > Fonts); the wrist gadget's screen, hologram and log; the weapons' ammo screens;
// the world's texts (map boards, floating texts, tips, tools' hints). Screens > the fonts' pages.
enum class Surface : int
{
    Menu,
    Gadget,
    Ammo,
    World,
    Count
};

// A surface's settings (vr_font_* for the menu, vr_font_gadget_*, vr_font_ammo_*, vr_font_world_*).
struct SurfaceCvars
{
    cvar_s* upscale;
    cvar_s* outline;
    cvar_s* outlineRgb[3];
    cvar_s* shadowOpacity;
    cvar_s* shadowGrow;
    cvar_s* shadowSpread;
    cvar_s* shadowX;
    cvar_s* shadowY;
    cvar_s* shadowRgb[3];
};
[[nodiscard]] const SurfaceCvars& cvars(Surface s);

// While one lives, the text drawn (the console font, gfx::fontTexture and fontGlyph) is in surface `s`'s style.
class Use
{
public:
    explicit Use(Surface s);
    ~Use();
    Use(const Use&) = delete;
    Use& operator=(const Use&) = delete;

private:
    Surface previous;
};

// The lightest grey (palette index 1..15) in conchars' white half (characters 0..127): the wrist gadget's screens
// stretch the greys so that it becomes white (15). 0: none.
[[nodiscard]] int consoleGreyTop();

// The restyled font brightened that way (the wrist gadget's screens and hologram: gadget::useBrightFont), the same
// upscaling, outline and shadow; made when first asked after a change. Null: not restyled (the gadget's own copy).
[[nodiscard]] gltexture_s* brightChars();

// The cvars' callbacks and the test command (VR_Init).
void init();

// Bumped each time the style changes (a setting, a game change): a strip made by restyleStrip is made again.
[[nodiscard]] int generation();

// Anything restyled (not Classic without outline or shadow). Off: every picture is drawn as Quake's.
[[nodiscard]] bool active();

// A strip of letters restyled (vr_bigfont.cpp's): its texture's pic (Draw_SubPic: in the strip's own units, a source
// pixel each, the texture 4x), each letter's first column in it, and the margin round every letter (source pixels:
// the outline's and the shadow's room; 0 without them). A letter is drawn `margin` further left and up, `2 * margin`
// wider and taller, at the same place as before.
struct Strip
{
    za::Vector<za::U8> pic;     // Draw_PicBytes () bytes
    za::Vector<za::U32> rgba;  // its texels (reloads of the texture read them)
    za::Vector<int> column;    // per letter given; -1: not given (left out)
    int width{0}, height{0};   // in source pixels
    int margin{0};
};

// Restyle the letters of an 8-bit strip (`width` x `height`, 255 transparent, the game's palette): letter i spans
// columns x[i] .. x[i] + w[i] (x[i] < 0: left out). Each is upscaled on its own and gets the outline and shadow.
void restyleStrip(const za::U8* data, int width, int height, const int* x, const int* w, int count, const char* name,
    Strip& out);

} // namespace qvr::fontstyle
