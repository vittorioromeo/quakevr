// vr_permdecals.hpp -- the permanent decals (vr_permdecals): blood, scorches and chips baked for good into a layer of
// the map's own faces, so that they never go and cost the same however many there are.
//
// Each lit face of the map (the world's and its brush models': doors, lifts, plats) has a grid of pages over it, in its
// own plane, vr_permdecal_density units a texel (read at the map's load), 30 x 30 texels a page. A page takes a slot of
// an atlas (vr_permdecal_atlas: 32 x 32 texels a slot, its page's and a texel of its neighbours' round it for the
// filtering; 4 mip levels) the first time a mark lands on it; when the atlas is full it grows a layer or more (a 2D
// array texture, up to vr_permdecal_atlas_max MB), and only full at that does the page baked least recently give its
// slot up. A mark that has settled (vr_decals.cpp: shown, spread and darkened) is drawn once into each page
// under it on the GPU, multiplied in as the live marks multiply what is under them, and leaves the live ring: the live
// cap (vr_decal_max) no longer limits what stays. The world's shader reads the layer before the texture's light
// (QVR_PERMDECAL_FUNCTIONS, vr_glsl.h): a page table lookup and one filtered read a pixel, whatever the number of
// marks. A brush model's faces are laid out in its own space, so a mark on a door moves with it (baked at once: the
// live marks are the static world's). Not on sky, liquids or fences, nor on the boxes made in the game (vr_brushes.cpp:
// their marks stay live) or the models of their own files (b_*.bsp items: one model for every box).
//
// What was baked is kept as a list of stamps (each mark in its model's space), for the saves to bake again.

#pragma once

#include "Zancle/Container/Vector.hpp"

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

// The layer on (vr_permdecals, with vr_decals: off hides what was baked) and the map laid out for it.
[[nodiscard]] bool on();
// The layer kept but not shown: vr_permdecals 0 (vr_decals on, the map laid out). The rule (PERM_DECALS.md, "On and
// off"): every mark goes into the stamps, on or off. On, they are all shown, baked. Off, the newest vr_decal_max of them
// are shown live, as without the layer (vr_decals.cpp: showRecord at the switch; the marks the ring lets go of kept,
// not baked: record); on again, those are dropped from the ring and what was kept while off baked (rebake).
// vr_decals 0 hides both.
[[nodiscard]] bool kept();
void record(const Stamp& s);
bool rebake();
// The newest `max` stamps on the static world (model 0), the oldest first, into `out`.
void newestWorld(int max, za::Vector<Stamp>& out);
// Pages the bakes since the last flush draw into (bakeSettled's budget: vr_permdecal_bake_max).
[[nodiscard]] int pendingPages();

// Stamp `s` (in the world's space, its model ignored) baked on the static world's faces under it; how many faces.
int bakeWorld(const Stamp& s);
// ... on the faces of the brush models in the map that it lies on (doors, lifts, plats: each in its own space).
// A mark that shows later (`due`, on cl.time: the gore's flying blood) waits, laid in the model's space as it is now,
// and is baked when it shows (bakeDue), as the world's live marks show then.
int bakeEntities(const Stamp& s, double due = 0.0);
// Once a frame (vr_decals.cpp's bakeSettled, the layer on): the brush models' marks due by now baked; how many.
int bakeDue();
// The brush models' marks not shown yet appended to `out` (the saves: baked at once after the load).
void waitingStamps(za::Vector<Stamp>& out);

// Once a frame, before the world draws (R_SetupView's first view): what was baked drawn into the atlas; `decalAtlas`
// the marks' (vr_decals.cpp).
void flush(unsigned decalAtlas);

// R_DrawBrushModels_Real: the atlas (unit 18) and the page table (buffer 10) for the world's shader.
void bind();

// All gone (a new map, vr_cheats' clean up).
void clear();

// vr_permdecal_info: the layer's pages, atlas and stamps.
void info_f();

// Saves: the stamps, then `live` (the live ring's marks not among them, settled), as the save's comment lines
// (VR_SaveDecalLines; valid until the next call); a save's read before its load (VR_ReadSaveInfo), baked again once its
// map has loaded (restore, once a frame: 1 if it baked them, 2 if it kept them, the layer off: kept).
[[nodiscard]] const char* saveLines(const za::Vector<Stamp>& live);
void readSaveLines(const char* text);
int restore();

} // namespace qvr::permdecals
