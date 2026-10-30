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
void registerCommands();          // sv_random_info

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
