// vr_permdecals.hpp -- the permanent decals (vr_permdecals): blood, scorches and chips baked for good into a layer of
// the map's own faces, so that they never go and cost the same however many there are.
//
// Each lit face of the map (the world's and its brush models': doors, lifts, plats) has a grid of pages over it, in its
// own plane, vr_permdecal_density units a texel (read at the map's load), 30 x 30 texels a page. A page takes a slot of
// an atlas (vr_permdecal_atlas: 32 x 32 texels a slot, its page's and a texel of its neighbours' round it for the
// filtering; 4 mip levels) the first time a mark lands on it; when the atlas is full, the page baked least recently
// gives its slot up. A mark that has settled (vr_decals.cpp: shown, spread and darkened) is drawn once into each page
// under it on the GPU, multiplied in as the live marks multiply what is under them, and leaves the live ring: the live
// cap (vr_decal_max) no longer limits what stays. The world's shader reads the layer before the texture's light
// (QVR_PERMDECAL_FUNCTIONS, vr_glsl.h): a page table lookup and one filtered read a pixel, whatever the number of
// marks. A brush model's faces are laid out in its own space, so a mark on a door moves with it (baked at once: the
// live marks are the static world's). Not on sky, liquids or fences, nor on the boxes made in the game (vr_brushes.cpp:
// their marks stay live) or the models of their own files (b_*.bsp items: one model for every box).
//
// What was baked is kept as a list of stamps (each mark in its model's space), for the saves to bake again.

#pragma once

#include <glm/glm.hpp>

namespace qvr::permdecals
{

// A mark to bake: on model `model` (0 the world, n its brush model *n), in that model's space.
struct Stamp
{
    glm::vec3 centre;
    glm::vec3 u, v;   // half its length and width, along the surface
    glm::vec3 normal; // the surface's it was laid on
    float depth;      // how far from its plane a face takes it
    float dark;       // its colour's share (1 - how much it darkened)
    int cell;         // its cell in the decals' atlas (vr_decals.cpp)
    int model;
};

// The layer on (vr_permdecals) and the map laid out for it.
[[nodiscard]] bool on();

// Stamp `s` (in the world's space, its model ignored) baked on the static world's faces under it; how many faces.
int bakeWorld(const Stamp& s);
// ... on the faces of the brush models in the map that it lies on (doors, lifts, plats: each in its own space).
int bakeEntities(const Stamp& s);

// Once a frame, before the world draws (R_SetupView's first view): what was baked drawn into the atlas; `decalAtlas`
// the marks' (vr_decals.cpp).
void flush(unsigned decalAtlas);

// R_DrawBrushModels_Real: the atlas (unit 18) and the page table (buffer 10) for the world's shader.
void bind();

// All gone (a new map, vr_cheats' clean up).
void clear();

// vr_permdecal_info: the layer's pages, atlas and stamps.
void info_f();

} // namespace qvr::permdecals
