// vr_toolgun_icons.hpp -- thumbnails of models for the toolgun's panel (vr_toolgun_panel.cpp: the spawn tool's grid):
// a model's file (a .mdl, or a .bsp's first model) read from the game's files and drawn by the CPU into a small picture
// (z-buffered, its skin or textures, lit from over the viewer's shoulder, 2x2 supersampled, a transparent background).
// Made once each when first asked for (a few a frame, in the 2D pass) and kept for the session (made again after a
// game directory change). Main thread.

#pragma once

#include "vr_gfx.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::toolgun::icons
{

// The thumbnail of `model` with skin `skin` (the first frame), seen from ahead and to its right, or with `side` from its
// right side (a gun's length across): nullptr while it waits to be made (makePending), or when its file can't be read.
// `attach` (attach::Kind, wheelui): that attachment drawn on it at the gun's mount (the holster menu's and the weapon
// wheel's thumbnails), none where it doesn't fit.
[[nodiscard]] const gfx::Picture* get(const char* model, int skin, bool side, int attach = 0);

// How a picture drawMesh made was framed: a point of the mesh's space at `place`'s pixels (x right, y down).
struct Framing
{
    glm::vec3 right{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec2 mid{0.f};
    float scale{1.f}; // pixels a unit
    glm::vec2 size{1.f};
    [[nodiscard]] glm::vec2 place(const glm::vec3& p) const
    {
        return {(glm::dot(p, right) - mid.x) * scale + size.x * 0.5f, size.y * 0.5f - (glm::dot(p, up) - mid.y) * scale};
    }
};
// A mesh (three indices a triangle; wheelui: the player's posed body, avatar::posedMesh) drawn by the thumbnails'
// rasteriser, untextured (grey, lit as they are), into `picture` (made `width` x `height` the first time: keep the size),
// seen along `toViewer` (the picture's up the world's up), framed to fit. False: nothing to draw.
bool drawMesh(const za::Vector<glm::vec3>& verts, const za::Vector<unsigned>& tris, const glm::vec3& toViewer, int width,
    int height, gfx::Picture& picture, Framing& framing);
// The last drawMesh's size: its triangles and picture (vr_flatholster_dump).
struct MeshStats
{
    int triangles{0};
    int width{0};
    int height{0};
};
[[nodiscard]] MeshStats lastMeshStats();

// In the 2D pass (the panel's image is drawn): up to `budget` of the thumbnails asked for made.
void makePending(int budget);

// The game directory changed: each made again from the new game's files when next asked for.
void onGameDirChanged();

// vr_toolgun_icons: how many are made, failed and waiting, and the time making them took.
void print();

} // namespace qvr::toolgun::icons
