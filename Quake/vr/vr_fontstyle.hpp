// vr_fontstyle.hpp -- the 2D font restyled (vr_fontstyle.cpp): the console font, Quake's text box pieces and the main
// menu's lettering upscaled 4x and drawn smooth, with an outline and a drop shadow round the letters (Advanced VR
// Options > HUD and Menus > Menu, "Fonts"). gl_draw.c and menu.c use it through vr_api.h (VR_FontStyle_*).

#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"

namespace qvr::fontstyle
{

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
