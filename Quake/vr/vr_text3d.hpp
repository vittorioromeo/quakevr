// vr_text3d.hpp -- text drawn in the world with the console font (world texts, weapon ammo
// counters), depth-tested in the scene pass.

#pragma once

#include "vr_engine.hpp"

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
// angles), each character is 8 * scale units. Lines are separated by '\n'.
void queue(std::string_view text, const glm::vec3& pos, const glm::vec3& angles, Align align, float scale);

// Once per frame, after the eyes (and the flat view) are drawn.
void clear();

} // namespace qvr::text3d
