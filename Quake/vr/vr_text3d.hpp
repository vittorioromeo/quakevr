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

#include <glm/glm.hpp>

#include <string_view>

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
// the weapons' ammo counters.
void queue(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale,
    bool screen = false);

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

} // namespace qvr::text3d
