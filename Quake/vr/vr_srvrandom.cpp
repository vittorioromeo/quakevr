// vr_srvrandom.cpp -- the server's own random numbers.
//
// QuakeC's random() (PF_random) and the monsters' movetogoal turns (sv_move.c) drew from the C library's rand(), which
// the client's effects share (Quake's particles, dynamic lights' flicker, decals, a beam's srand each frame) and which
// Host_Frame stirs once a frame. So how many effects a frame drew, and how many frames ran, changed the game: two runs
// of the same AI scene went apart as soon as one drew a blood mark the other didn't. The server has a stream of its own
// now (Zancle's FastNonCryptoRng, Xoroshiro128++), the client keeps rand(). The numbers are rand()'s range (0..0x7fff,
// fifteen bits) and PF_random turns them into floats as before (sv_gameplayfix_random: never exactly 0 or 1), so the
// game plays the same; only which numbers come out differs.
//
// The stream is seeded at every map load (SV_SpawnServer: a new map, a changelevel, a loaded game): from sv_random_seed
// when it isn't 0 (tests: the same AI every run, whatever the client draws or how fast it renders), else from the clock
// (normal play: a new sequence each load, the seed shown with developer 1 so a run can be replayed with it). A test that
// seeds before its "map" (vr_bench_seed, a motion take's map load) arms seedNextMap, which wins over sv_random_seed once.
//
// Saved games: the stream's state isn't saved; a loaded game is a map load, so it is reseeded as above (with a fixed
// seed, every load of a save plays on the same; with 0, differently each time, as Quake's rand() did). Saves stay the
// format other engines read.
//
// The C++ gameplay draws have streams of their own derived from it (Stream: the legsweep's knockdown chance), restarted
// with it. Purely visual or audible picks keep local streams of their own (decals, the water sounds' and knocks'
// variants, Quake's particle colours: no longer the C library's rand()); randmap keeps rand().

#include "vr_srvrandom.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_api.h"
#include <stdlib.h>

namespace qvr::srvrandom
{
namespace
{
za::FastNonCryptoRng rng{1u};
za::U64 currentSeed = 1u;
bool nextMapArmed = false;
bool seededByClock = false; // (the last map load's seed: the clock's)
za::U64 nextMapSeed = 0u;
// The gameplay streams (Stream), each from derivedSeed(streamSalt + its index).
constexpr za::U64 streamSalt = 0x5EED5EEDA11CE000ull;
za::FastNonCryptoRng streams[static_cast<int>(Stream::Count)]; // (seeded at every map load: seedNow)

// The seed sv_random_seed names (its string: a 64-bit seed exactly, beyond a float's 24 bits), 0 when it is 0 or unset.
[[nodiscard]] za::U64 cvarSeed() { return static_cast<za::U64>(strtoull(sv_random_seed.string, nullptr, 0)); }

void info_f()
{
    Con_Printf("server random: seed %llu (sv_random_seed %s%s)\n", static_cast<unsigned long long>(currentSeed),
        sv_random_seed.string, nextMapArmed ? "; the next map's armed" : "");
}

// The map's content seed (see the header): 0 none. lastLoadArmed: this load was seeded by a test (seedNextMap: a
// motion take's, vr_bench_seed's), whose content stays the map name's.
za::U32 currentMapSeed = 0u;
const char* mapSeedWhy = "no map yet";
bool lastLoadArmed = false;

[[nodiscard]] bool testSession() { return !q_strcasecmp(vr_backend.string, "mock"); }

void mapSeedInfo_f()
{
    Con_Printf("map seed: %u (%s; vr_fresh_map_seeds %s, vr_map_seed %s)\n", static_cast<unsigned>(currentMapSeed), mapSeedWhy,
        vr_fresh_map_seeds.string, vr_map_seed.string);
}
} // namespace

void chooseMapSeed(bool fromSave)
{
    const int fixed = static_cast<int>(vr_map_seed.value);
    if(fromSave)
    {
        // (a saved game's own replaces this with its globals; a save from before the seeds has none: the map's name)
        currentMapSeed = 0u;
        mapSeedWhy = "a saved game's (none: one made before the map seeds)";
    }
    else if(fixed > 0)
    {
        currentMapSeed = static_cast<za::U32>(fixed) & 0xFFFFFFu;
        mapSeedWhy = "vr_map_seed's";
    }
    else if(lastLoadArmed)
    {
        currentMapSeed = 0u;
        mapSeedWhy = "none: a test's seeded load";
    }
    else if(fixed == 0 && (vr_fresh_map_seeds.value == 0.f || testSession()))
    {
        currentMapSeed = 0u;
        mapSeedWhy = vr_fresh_map_seeds.value == 0.f ? "none: vr_fresh_map_seeds 0" : "none: a test session (vr_backend mock)";
    }
    else
    {
        // The clock's, through splitmix64, to 24 bits (never 0). Apart from the server stream's own seed.
        za::U64 z = static_cast<za::U64>(za::Clock::nowNanoseconds()) ^ 0x6D617073656564ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        currentMapSeed = static_cast<za::U32>(z % 0xFFFFFFull) + 1u;
        mapSeedWhy = "fresh";
    }
    Con_DPrintf("map seed %u (%s)\n", static_cast<unsigned>(currentMapSeed), mapSeedWhy);
}

void restoreMapSeed(za::U32 seed)
{
    currentMapSeed = seed & 0xFFFFFFu;
    mapSeedWhy = seed ? "the saved game's" : "the saved game's: none";
}

za::U32 mapSeed() { return currentMapSeed; }

void seedNow(za::U64 seed)
{
    currentSeed = seed;
    rng = za::FastNonCryptoRng{seed};
    for(int i = 0; i < static_cast<int>(Stream::Count); i++)
    {
        streams[i] = za::FastNonCryptoRng{derivedSeed(streamSalt + static_cast<za::U64>(i))};
    }
}

float draw01(Stream s) { return streams[static_cast<int>(s)].getF(0.f, 1.f); }

void seedNextMap(za::U64 seed)
{
    nextMapArmed = true;
    nextMapSeed = seed;
}


za::U64 derivedSeed(za::U64 salt)
{
    // splitmix64's finaliser: a seed unlike the server stream's own.
    za::U64 z = currentSeed ^ salt;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void registerCommands()
{
    Cmd_AddCommand("sv_random_info", info_f);
    Cmd_AddCommand("vr_map_seed_info", mapSeedInfo_f);
}
} // namespace qvr::srvrandom

// SV_SpawnServer, before anything spawns: the stream seeded for this map (see above).
extern "C" void VR_ServerRandomMapLoad(void)
{
    using namespace qvr::srvrandom;
    za::U64 seed = 0u;
    lastLoadArmed = nextMapArmed; // (the map's content seed too: chooseMapSeed)
    seededByClock = false;
    if(nextMapArmed)
    {
        seed = nextMapSeed;
        nextMapArmed = false;
    }
    else if((seed = cvarSeed()) == 0u)
    {
        seededByClock = true;
        // The clock's, kept to 32 bits so sv_random_seed can name it again (developer 1 prints it).
        const za::U64 t = static_cast<za::U64>(za::Clock::nowNanoseconds());
        seed = ((t ^ (t >> 32)) & 0xFFFFFFFFull) | 1u;
    }
    seedNow(seed);
    Con_DPrintf("server random seed %llu\n", static_cast<unsigned long long>(seed));
}

bool qvr::srvrandom::clockSeeded()
{
    return seededByClock;
}

// The server's rand(): 0..0x7fff, as the C library's (PF_random, sv_move.c).
extern "C" int VR_ServerRandom(void) { return static_cast<int>(qvr::srvrandom::rng.next() >> 49); }
