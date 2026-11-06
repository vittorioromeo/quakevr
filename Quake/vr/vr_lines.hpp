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

// The same in the scene: hidden behind what is in front of them (depth-tested, in the translucent pass; the others are
// drawn over everything with the UI).
void sceneLine(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB);
void sceneGlow(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB);

// In the scene's translucent pass too, but over all that is drawn by then (no depth test): flatscreen play's stand-in for line,
// which has no UI pass to be drawn in (the arcs on your own arms: vr_shock.cpp).
void sceneOverLine(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB);
void sceneOverPoint(const glm::vec3& p, float size, const glm::vec4& color); // (the crosshair's dot in flatscreen play)

// A debug surface, batched with this frame's lines. In the scene (depth-tested) unless xray is requested.
void triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec4& color, bool xray = false);

// Draws the frame's queue (but the scene's) into the bound framebuffer, facing `eye`.
void drawInEye(const glm::vec3& eye);

// Draws the scene's lines (sceneLine, sceneGlow) in the scene's translucent pass, facing its camera.
void drawInScene();

// Empties the queue (once per frame, after both eyes).
void clear();

} // namespace qvr::lines
