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
// Kept on rand(): purely visual or audible picks (particles, decals, the water sounds' and knocks' variants), randmap.

#include "vr_srvrandom.hpp"
#include "vr_engine.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include <stdlib.h>

namespace qvr::srvrandom
{
namespace
{
za::FastNonCryptoRng rng{1u};
za::U64 currentSeed = 1u;
bool nextMapArmed = false;
za::U64 nextMapSeed = 0u;

// The seed sv_random_seed names (its string: a 64-bit seed exactly, beyond a float's 24 bits), 0 when it is 0 or unset.
[[nodiscard]] za::U64 cvarSeed() { return static_cast<za::U64>(strtoull(sv_random_seed.string, nullptr, 0)); }

// vr_test_crand <n>: n numbers drawn from the C library's rand() now, as the client's effects draw them (tests: the
// server's stream must not move).
void testCrand_f()
{
    const int n = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 1000;
    unsigned sum = 0;
    for(int i = 0; i < n; i++)
    {
        sum += static_cast<unsigned>(rand());
    }
    Con_Printf("vr_test_crand: %d drawn (sum %u)\n", n, sum);
}

void info_f()
{
    Con_Printf("server random: seed %llu (sv_random_seed %s%s)\n", static_cast<unsigned long long>(currentSeed),
        sv_random_seed.string, nextMapArmed ? "; the next map's armed" : "");
}
} // namespace

void seedNow(za::U64 seed)
{
    currentSeed = seed;
    rng = za::FastNonCryptoRng{seed};
}

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
    Cmd_AddCommand("vr_test_crand", testCrand_f);
    Cmd_AddCommand("sv_random_info", info_f);
}
} // namespace qvr::srvrandom

// SV_SpawnServer, before anything spawns: the stream seeded for this map (see above).
extern "C" void VR_ServerRandomMapLoad(void)
{
    using namespace qvr::srvrandom;
    za::U64 seed = 0u;
    if(nextMapArmed)
    {
        seed = nextMapSeed;
        nextMapArmed = false;
    }
    else if((seed = cvarSeed()) == 0u)
    {
        // The clock's, kept to 32 bits so sv_random_seed can name it again (developer 1 prints it).
        const za::U64 t = static_cast<za::U64>(za::Clock::nowNanoseconds());
        seed = ((t ^ (t >> 32)) & 0xFFFFFFFFull) | 1u;
    }
    seedNow(seed);
    Con_DPrintf("server random seed %llu\n", static_cast<unsigned long long>(seed));
}

// The server's rand(): 0..0x7fff, as the C library's (PF_random, sv_move.c).
extern "C" int VR_ServerRandom(void) { return static_cast<int>(qvr::srvrandom::rng.next() >> 49); }
