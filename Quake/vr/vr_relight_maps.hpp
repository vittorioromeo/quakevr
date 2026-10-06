#pragma once

// vr_relight_maps.hpp -- the maps a relighting batch takes (vr_relight.cpp; Graphics > Relighting, Many Maps): the
// map in play, an episode (e1m1..e1m8, hip2m1..., by the map in play or picked), a game (id1, hipnotic, rogue, dopa,
// mg1, mg3: the map in play's or picked), the Map Library's installed packages, or every map there is. Found in the
// game folders' paks and maps/ folders themselves (com_searchpaths), not through the search path's lookups: a campaign
// not being played, or a package not mounted, still has its maps found, and each is read from its own file.

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::relight::maps
{

// Where one map is: its name, its game (the folder relit/ and relit_custom/ name it by: id1, a package's folder) and
// its file (a .pak and its place in it, or the .bsp).
struct Source
{
    za::String map;  // "e1m1" ("sub/x": a package's map in a subfolder)
    za::String game; // "id1"
    za::String file; // the .pak, or the loose .bsp
    za::I64 offset{0};
    za::I64 length{0}; // its bytes (the batch's estimate of the work it is)
    bool inPak{false};
};

enum class Set
{
    Map,        // the map in play
    Episode,    // its episode, or the one named ("e2": id1's)
    Game,       // its game, or the one named ("hipnotic")
    Library,    // every map of the Map Library's installed packages
    Everything, // every map of every game folder, and the Library's
};

// The set's maps (no ammo boxes' and other brush models' b_*.bsp), each game's in name order (e1m2 before e1m10),
// the games in a fixed order (id1, hipnotic, rogue, dopa, mg1, mg3, the rest, the Library's). `arg`: the episode
// ("e2", "hip1": a game's episode prefix up to its number) or game named; "" : the map in play's. `current`: the map
// in play ("" : none). False and `why` when there is none.
bool collect(Set set, const char* arg, const char* current, za::Vector<Source>& out, za::String& why);

// The map `map` as the game finds it now (the search path's answer, a campaign's isolation included). False: none.
bool locate(const char* map, Source& out);

// Its file's bytes.
bool read(const Source& source, za::Vector<unsigned char>& out);

// The episode prefix of a map's name ("e1m3": "e1m", "hip2m4": "hip2m"), or "" (start, dm1, a mapper's name).
[[nodiscard]] za::String episodePrefix(const char* map);

} // namespace qvr::relight::maps
