// vr_text3d.hpp -- text drawn in the world with the console font (world texts, weapon ammo
// counters, floating damage numbers, the wrist gadget's log), depth-tested in the scene pass: the
// solid ones with the opaque entities, the blended ones (floating texts) after the translucent
// pass, over the sky; the log over the eye's final image, as UI (drawOverlay). The ammo screens
// and the wrist gadget's glow softly round their edges (vr_screen_glow), added in that pass too;
// the ammo screens are small CRTs like the gadget's (vr_weapon_screen_crt: their images drawn in
// the 2D pass, shown a frame later). The maps' text boards (world texts) are CRTs too
// (vr_worldtext_crt, in vr_worldtext_hue): each board's text in an image of its own, redrawn only when
// it changes, in a bezel with a soft glow.

#pragma once

#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>



namespace qvr::screenstyle
{
struct Style;
}

namespace qvr::text3d
{

enum class Align : int
{
    Left = 0,
    Centre = 1,
    Right = 2
};

// Queued for this frame's scene: `pos` is the text block's centre, `angles` face it (Quake
// angles), each character is 8 * scale units. Lines are separated by '\n'. With `screen`, the
// text sits on a small screen (a bezel box with a lit face, in the wrist gadget's colours), as on
// the weapons' ammo counters. `tint` (a screen's): its text in this colour (0..1, times the screen's brightness)
// instead of the screen's (an ammo screen's warning colours: vr_ammo_color_warn); its face stays the screen's.
// `style` (a screen's): its weapon's look (vr_screenstyle.hpp: rounded glass, size, offset; null the default). Kept by
// pointer till the frame is drawn: the table's entries.
// `glyphs` (a screen's): its text alone resized and moved on the glass (weapons.txt's wpntxt_font, wpntxt_tx and
// wpntxt_ty); the glass, its box and its glow stay as `scale` and the style make them. `size` times the font's size;
// `shift` the text's centre moved right and up, in shares of the glass's width and height. Shown through the CRT's
// image (vr_weapon_screen_crt), text past the glass is cut off at its edge; drawn plain (CRT off), it spills.
// `restSize` and `restBright`: the lines after the first, their size times the first's and their brightness (1 the
// text's, 0 the face's: an ammo screen's total under its magazine, vr_ammo_total_scale and _bright). The block of lines
// is what is centred and fitted to a housing's glass (a glass too small for it: the whole block smaller).
struct Glyphs
{
    float size{1.f};
    glm::vec2 shift{0.f};
    float restSize{1.f};
    float restBright{1.f};
};
void queue(za::StringView text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale,
    bool screen = false, const glm::vec3* tint = nullptr, const screenstyle::Style* style = nullptr,
    const Glyphs& glyphs = {});

// A filled bar in an overlay text (the profiler's panel): on line `line` (0 the first), from character `column`, `cells`
// characters long (fractions too), over a dim track `track` characters long.
struct OverlayBar
{
    int line;
    float column;
    float cells;
    float track;
    glm::vec4 color;
};

// Queued likewise, but drawn over the eye's image with the wrist log (drawOverlay: not depth tested,
// white on a dark backing), centred: a notice that nothing in the scene may hide (the motion recorder's).
// Each line is centred: lines of one length line up as a table. `bars` are drawn over the backing (of opacity
// `backing`), under the text.
void queueOverlay(za::StringView text, const glm::vec3& pos, const glm::vec3& angles, float scale,
    za::Span<const OverlayBar> bars = {}, float backing = 0.55f);

// A tip's screen over the eye's image (vr_tips.cpp; drawOverlay: not depth tested): `text` on a CRT screen as the map
// boards' (vr_worldtext_crt: its own image, the phosphor colour, the face, the bezel and the soft glow), centred on
// `pos` in the plane of `right` and `up`, its characters `charSize` units, faded by `alpha`. With `lineTo`, a cable of
// the same screen (bezel, face, glow) from inside the screen (under it: no seam) to that point, as thick on screen all
// along (`widthPerUnit` its face's half width for each unit from `eye`, the head's middle), ending in a round spot.
void queueOverlayScreen(za::StringView text, const glm::vec3& pos, const glm::vec3& right, const glm::vec3& up,
    float charSize, float alpha, const glm::vec3& eye, const glm::vec3* lineTo = nullptr, float widthPerUnit = 0.f);

// Once per frame, after the eyes (and the flat view) are drawn.
void clear();

// At the end of the 2D pass (from gadget::renderScreen), after the eyes: the queued screens' faces
// and texts into their images, shown through the CRT shader in the next frame's eyes
// (vr_weapon_screen_crt); and the map boards' whose text changed (vr_worldtext_crt).
void renderScreens();

// The blended texts, in each eye's scene after the translucent pass (VR_DrawSceneTranslucent).
void drawTranslucent();

// The wrist gadget's log (UI: not in the scene's post-processing, the underwater wobble), over the eye's final image
// with the HUD panel (vr_stereo.cpp); not depth tested (nothing is between it and the eye but the other hand).
void drawOverlay();

// Texts queued this frame and map text boards held (vr_memstats).
void counts(int& queuedTexts, int& boardCount);

// The ammo screens' images drawn so far, and the screens shown through another's image (the same text) so far.
void screenStats(int& draws, int& shared);

// Its command (VR_Init): vr_fontstyle_live_test, a change to a font's style (Screens > World Text / Ammo Screen / Wrist
// Gadget Font) drawing that surface's cached text images again once, on the next frame.
void init();

} // namespace qvr::text3d
