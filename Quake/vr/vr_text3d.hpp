// vr_text3d.hpp -- text drawn in the world with the console font (world texts, weapon ammo
// counters, floating damage numbers, the wrist gadget's log), depth-tested in the scene pass: the
// solid ones with the opaque entities, the blended ones (floating texts, the log) after the
// translucent pass, over the sky.

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

// The blended texts, in each eye's scene after the translucent pass (VR_DrawSceneTranslucent).
void drawTranslucent();

} // namespace qvr::text3d
