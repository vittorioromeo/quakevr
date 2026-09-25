// vr_text3d.hpp -- text drawn in the world with the console font (world texts, weapon ammo
// counters), depth-tested in the scene pass.

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

} // namespace qvr::text3d
