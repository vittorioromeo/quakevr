// vr_toolgun_icons.hpp -- thumbnails of models for the toolgun's panel (vr_toolgun_panel.cpp: the spawn tool's grid):
// a model's file (a .mdl, or a .bsp's first model) read from the game's files and drawn by the CPU into a small picture
// (z-buffered, its skin or textures, lit from over the viewer's shoulder, 2x2 supersampled, a transparent background).
// Made once each when first asked for (a few a frame, in the 2D pass) and kept for the session (made again after a
// game directory change). Main thread.

#pragma once

#include "vr_gfx.hpp"

namespace qvr::toolgun::icons
{

// The thumbnail of `model` with skin `skin` (the first frame), seen from ahead and to its right, or with `side` from its
// right side (a gun's length across): nullptr while it waits to be made (makePending), or when its file can't be read.
[[nodiscard]] const gfx::Picture* get(const char* model, int skin, bool side);

// In the 2D pass (the panel's image is drawn): up to `budget` of the thumbnails asked for made.
void makePending(int budget);

// The game directory changed: each made again from the new game's files when next asked for.
void onGameDirChanged();

// vr_toolgun_icons: how many are made, failed and waiting, and the time making them took.
void print();

} // namespace qvr::toolgun::icons
