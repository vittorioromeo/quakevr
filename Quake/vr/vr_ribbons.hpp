// vr_ribbons.hpp -- thin trails drawn as ribbons in the scene (ROUND23.md, "Persistent muzzle wisps"): a trail is a line
// of points, each with its half width and colour; it is drawn as a strip turned to face the eye, smoothed between its
// points (Catmull-Rom), its alpha falling off across it to nothing at its edges (soft, blurry lines: no texture). A
// ribbon thinner than a pixel or two on screen is drawn that wide with its alpha lowered as much ("phone-wire" AA: no
// shimmering or breaking up into dots far off). Ribbons are appended to one triangle list (the caller's buffer, reused:
// nothing allocated a frame) and drawn in one call. Users: the guns' muzzle wisps (vr_muzzlewisps.cpp).

#pragma once

#include "vr_gfx.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>

namespace qvr::ribbons
{

// A point of a ribbon: where, its half width (units) and its colour (rgb, alpha: straight, not premultiplied).
struct Point
{
    glm::vec3 pos{0.f};
    float halfWidth{0.f};
    glm::vec4 color{1.f};
};

// The view the ribbons are made for (made again each view: each eye's own).
struct View
{
    glm::vec3 eye{0.f};
    float pixelsPerUnit{0.f}; // on screen, one unit across at one unit from the eye
    float minPixels{1.5f};    // the narrowest a ribbon is drawn, in pixels
    float nearFade{6.f};      // units from the eye a ribbon fades out within (from a third of it)
};

// The scene view being drawn (gfx::sceneCamera, the projection and viewport).
[[nodiscard]] View sceneView(float minPixels);

// A ribbon through `points` (two at least), each piece between two points split into `smoothing` (1-8) along a
// Catmull-Rom curve, appended to `out` as triangles. Returns the vertices added.
int append(za::Vector<gfx::Vertex>& out, const View& view, za::Span<const Point> points, int smoothing);

// All the ribbons appended, in one draw: depth tested, not written, alpha blended (or added: `additive`).
void draw(za::Span<const gfx::Vertex> triangles, bool additive);

} // namespace qvr::ribbons
