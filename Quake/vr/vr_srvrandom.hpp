// vr_srvrandom.hpp -- the server's own random numbers (QuakeC's random(), the monsters' movetogoal turns), apart from
// the C library's rand() the client's effects draw from. See vr_srvrandom.cpp.
#pragma once
#include "Zancle/Base/IntTypes.hpp"

namespace qvr::srvrandom
{
void seedNow(za::U64 seed);          // the stream restarted from this seed now (tests: vr_bench_seed, motion takes, vr_hull)
void seedNextMap(za::U64 seed);      // the next map load's seed, once (before sv_random_seed): a test's "seed; map"
[[nodiscard]] za::U64 mapSeed();     // the seed this map's stream started from (sv_random_info)
[[nodiscard]] za::U64 derivedSeed(za::U64 salt); // another server stream's seed (explosion debris): fixed with sv_random_seed
void registerCommands();          // vr_test_crand, sv_random_info
}
