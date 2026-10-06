#pragma once

// vr_relight_vis.hpp -- see-through liquids for id's maps in the in-game relight (Graphics > Relighting, See-Through
// Liquids; vr_relight_seethrough): VisPatch's water-vised visibility put into the relit map, as Misc/quakevr/vis_maps.py
// does for relight_maps.py (a port of its read_vis_file, leaf_shape and vispatch).
//
// Quake's 1996 maps were vised with liquids as walls: from above the water nothing below it is in the potentially
// visible set, so engines keep their water opaque (Ironwail's Mod_CheckWaterVis). VisPatch's data files hold a
// water-vised visibility lump and leaf lump for every map of id1, hipnotic and rogue (id1.vis, hipnotic.vis,
// rogue.vis, or <game>/vispatch.dat; the "vispatch data" 1.0 files of https://sourceforge.net/projects/vispatch/files/,
// unpacked). They are not shipped with Quake VR: the game only reads them where they are put (dataDirs below). A map
// is patched only if the patch's leaves are the map's own (contents, bounds and faces): a patch for another version of
// a map is refused.

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::relight::vis
{

// A map's water-vised lumps (visibility, 4, and leaves, 10, whose visibility offsets point into it).
struct Patch
{
    za::Vector<unsigned char> visibility;
    za::Vector<unsigned char> leafs;
    za::String file; // the data file it came from
};

// The folder holding `game`'s data file (<dir>/<game>.vis or <dir>/<game>/vispatch.dat), looked for in:
// vr_relight_vispatch_dir alone when set (testing); else tools/vispatch in each game folder (the installer's
// <QVR>/quakevr/tools/vispatch, beside relight_maps.py), then QUAKEVR_VISPATCH (relight_maps.py's too). The data
// file's path, "" if none.
[[nodiscard]] za::String dataFile(const char* game);

// Whether any of id1's, hipnotic's or rogue's data files is found (the page's row is usable).
[[nodiscard]] bool available();

// Where the data is looked for, for the console and the page's help ("a; b; c").
[[nodiscard]] za::String placesText();

// The patch for `game`'s map `map` ("e1m1"; false: no data file for the game, or no entry for the map).
[[nodiscard]] bool find(const char* game, const char* map, Patch& out);

// Whether `patch` is for this .bsp (BSP29, the same leaves but for what vis computes: vis_maps.vispatch's check).
[[nodiscard]] bool fits(const unsigned char* bsp, za::SizeT size, const Patch& patch);

// Which liquids the .bsp is vised for as see-through, and which it has (vis_maps.water_vis, Ironwail's
// Mod_CheckWaterVis): bits 1 water, 2 teleporters, 4 slime, 8 lava.
struct Liquids
{
    int seeThrough{0};
    int found{0};
};
[[nodiscard]] Liquids liquids(const unsigned char* bsp, za::SizeT size);

// "water, tele see-through; slime opaque" (vis_maps.describe's words).
[[nodiscard]] za::String describe(const Liquids& l);

} // namespace qvr::relight::vis
