// vr_lines.hpp -- soft lines and points in the world (crosshair laser, teleport aim), batched
// per frame and drawn in each eye as camera-facing quads (core profile has no wide lines).

#pragma once

#include <glm/glm.hpp>

namespace qvr::lines
{

// Queued for this frame; `width` and `size` are in world units.
void line(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB);
void point(const glm::vec3& p, float size, const glm::vec4& color);

// The same, added onto the scene (light: energy beams), colours not multiplied by their alpha.
void glow(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB);
void glowPoint(const glm::vec3& p, float size, const glm::vec4& color);

// Draws the frame's queue into the bound framebuffer, facing `eye`.
void drawInEye(const glm::vec3& eye);

// Empties the queue (once per frame, after both eyes).
void clear();

} // namespace qvr::lines
