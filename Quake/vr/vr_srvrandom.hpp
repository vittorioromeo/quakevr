// vr_srvrandom.hpp -- the server's own random numbers (QuakeC's random(), the monsters' movetogoal turns), apart from
// the C library's rand() the client's effects draw from. See vr_srvrandom.cpp.
#pragma once
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

namespace qvr::srvrandom
{
void seedNow(za::U64 seed);          // the stream restarted from this seed now (tests: vr_bench_seed, motion takes, vr_hull)
void seedNextMap(za::U64 seed);      // the next map load's seed, once (before sv_random_seed): a test's "seed; map"
[[nodiscard]] za::U64 derivedSeed(za::U64 salt); // another server stream's seed (explosion debris): fixed with sv_random_seed
void registerCommands();          // sv_random_info, vr_map_seed_info

// The map's content seed (vr_fresh_map_seeds, vr_map_seed): mixed into the per-map rolls that were the map's name's
// alone (the crates' and barrels' layout, the rocks and bricks, the armed replacements' loadouts, the grunts' armour).
// 0: none (the map's name alone, as before). Chosen at each new map load (a new game, a changelevel, a restart); a
// loaded game has its own (QC's vr_map_content_seed, a saved global), restored with its globals. Off in a test session
// (vr_backend mock) and a test's seeded load (seedNextMap), unless vr_map_seed asks: their content stays the same.
void chooseMapSeed(bool fromSave);   // VR_OnSpawnServerBeforeLoad: this map's, before anything spawns
void restoreMapSeed(za::U32 seed);   // VR_OnLoadGame: the saved game's
[[nodiscard]] za::U32 mapSeed();     // 0..0xFFFFFF (QC's floats hold it exactly)
// This map's server stream seeded from the clock (sv_random_seed 0, no test's seed): its AI, gibs and ragdolls differ
// run to run (vr_physics_hash says so: a determinism check needs sv_random_seed or vr_bench_seed).
[[nodiscard]] bool clockSeeded();

// The C++ gameplay draws (his rule: gameplay randomness comes from the server's seed, so multiplayer can sync it):
// each its own stream, derived from the server's (derivedSeed) and restarted with it (seedNow: every map load), so a
// draw more or less of QuakeC's random() or of another stream leaves it as it was. Cosmetic client effects keep their
// own local streams.
enum class Stream : unsigned char
{
    Legsweep, // a tough monster group keeping its feet (Monster Groups > Knockdown Chance)
    Count
};
[[nodiscard]] float draw01(Stream s); // [0, 1)
}
