// vr_hosterror.cpp -- Host_Error and Host_EndGame jump (longjmp) from wherever they are called back to _Host_Frame,
// over C++ frames whose destructors never run and over begin/end pairs whose end never comes. VR_OnHostError, called
// before the jump, ends the frame-scoped state such a jump leaves open, in this order:
//   - the cvars::Override scopes still alive: their values put back (named in the console: one held across a call
//     that can run QuakeC breaks CODE_STYLE.md's rule, "Threads");
//   - slow motion's per-entity clock (VR_PhysicsEntityBegin without its End: the world's frame back);
//   - compatibility mode's shot point (VR_BeforePlayerPostThink without VR_AfterPlayerPostThink);
//   - the profiler's open scopes on the main thread (and its "quakec" scope's flag).
// Handled where they are, as before: Tracy's zones (VR_TracyAbort, after the jump), the start-up log's steps
// (startlog::frameEnd), a model load's lookups being recorded (vr_modelkeep.cpp), the temporary lists
// (Temp_FreeToMark) and net_message (NET_RestoreMessage) in host.c. What is not put back: locks (none may be held
// across QuakeC: CODE_STYLE.md) and the C++ locals' memory (a leak, the size of what the scopes held).
//
// vr_hosterror_test: a QuakeC error (PR_RunError) raised inside such a pair, opened from C++ as SV_Physics opens it,
// then the state checked after the jump and in the next map's frames.

#include "vr_hosterror.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_cvars_util.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_timescale.hpp"
#include "Zancle/Base/Strcmp.hpp"

namespace qvr::hosterror
{
namespace
{

// What VR_OnHostError found open at the last error (vr_hosterror_test reads it). The main thread's.
struct Unwound
{
    int errors{0};          // VR_OnHostError's calls so far
    int overrides{0};       // cvars::Override scopes put back
    bool timescalePair{false};
    bool muzzleShift{false};
    int profileScopes{0};   // the profiler's scopes ended
    int profileLeft{0};     // ... and those still open after (0)
};
Unwound unwound;

// vr_hosterror_test's state between its steps (arm, check, pair): the main thread's.
struct Test
{
    testkit::Tally tally;
    int errorsBefore{0};
    int depthBefore{0};   // the profiler's open scopes in a console command, before the test's own
    bool pairOpened{false};
    char overridden[64]{}; // the overridden cvar's value before
    char map[64]{};
};
Test test;

constexpr const char* testName = "vr_hosterror_test";
constexpr double probeFrame = 12345.0; // (VR_ThinkFrame gives it back unless a pair is open)

void check(bool ok, const char* what)
{
    test.tally.check(ok);
    Con_Printf("%s: %s %s\n", testName, ok ? "PASS" : "FAIL", what);
}

void arm()
{
    test = Test{};
    if(cls.state == ca_dedicated || !sv.active || svs.maxclients != 1)
    {
        check(false, "needs a single-player game with a client (a dedicated server exits on a Host_Error)");
        test.tally.verdict(testName);
        return;
    }
    const progs::ServerVm vm;
    const func_t raise = progs::findFunction("VR_HostErrorTest_Raise");
    edict_t* const player = progs::playerEdict();
    if(!raise || !player)
    {
        check(false, "VR_HostErrorTest_Raise in the progs (QC/vr_hosterror_test.qc: a build with QVR_TESTS)");
        test.tally.verdict(testName);
        return;
    }
    test.errorsBefore = unwound.errors;
    test.depthBefore = profile::openScopes();
    q_strlcpy(test.overridden, vr_debug_snd.string, sizeof(test.overridden));
    q_strlcpy(test.map, sv.name, sizeof(test.map));
    Cbuf_InsertText(va("wait; %s check\n", testName)); // (left in the buffer by the jump: the next frame's)

    // As SV_Physics runs the player: its turn in the player's time, a profile scope, and (against the rule, on purpose)
    // a cvar overridden across the call into QuakeC.
    const cvars::Override knob{vr_debug_snd, vr_debug_snd.value + 1.f};
    VR_ProfileBegin("vr_hosterror_test");
    const double world = VR_PhysicsEntityBegin(player, 1);
    test.pairOpened = world > 0.0;
    Con_Printf("%s: raising a QuakeC error inside the player's turn (%s; time scale %g), %d profile scopes open\n",
        testName, test.pairOpened ? "in the player's time" : "in the world's time", VR_TimeScale(), profile::openScopes());
    progs::callQc(raise); // PR_RunError: Host_Error's longjmp; nothing after this runs
    check(false, "the QuakeC error ended the call");
    VR_PhysicsEntityEnd(player, 1, world);
    VR_ProfileEnd();
}

void checkAfter()
{
    const Unwound& u = unwound;
    Con_Printf("%s: VR_OnHostError found %d override(s), the pair %s, the muzzle shift %s, %d profile scope(s) (%d left)\n",
        testName, u.overrides, u.timescalePair ? "open" : "closed", u.muzzleShift ? "set" : "unset", u.profileScopes,
        u.profileLeft);
    check(u.errors == test.errorsBefore + 1, "the error ran VR_OnHostError once");
    check(!sv.active, "the error ended the server");
    check(u.overrides == 1, "it put back the cvars::Override the error jumped out of");
    check(!ZA_STRCMP(vr_debug_snd.string, test.overridden), "the overridden cvar has its value from before");
    check(test.pairOpened, "slow motion's pair was open at the error (vr_timescale 0.5, vr_timescale_move_realtime 1)");
    check(u.timescalePair == test.pairOpened, "it found the pair open (VR_PhysicsEntityBegin without its End)");
    check(VR_ThinkFrame(probeFrame) == probeFrame, "the pair is closed: no entity in the player's time");
    check(u.profileScopes >= test.depthBefore + 1, "it found the profiler's scopes open (the test's and its callers')");
    check(u.profileLeft == 0, "it ended them all before the jump");
    char waits[30 * 6 + 1] = ""; // (30 frames of the new map: "wait;" each, as the console has no count)
    for(int i = 0; i < 30; i++)
    {
        q_strlcat(waits, "wait;", sizeof(waits));
    }
    Cbuf_InsertText(va("map %s; %s %s pair\n", test.map, waits, testName));
}

void checkPair()
{
    check(sv.active, "the map loaded again");
    check(VR_ThinkFrame(probeFrame) == probeFrame, "the next map's frames left no pair open");
    check(profile::openScopes() == test.depthBefore, "the profiler's depth in a console command is as before the error");
    if(edict_t* const player = sv.active ? progs::playerEdict() : nullptr)
    {
        const progs::ServerVm vm;
        const double world = VR_PhysicsEntityBegin(player, 1);
        check(world > 0.0 && VR_ThinkFrame(probeFrame) == world, "the next pair opens (the player's time)");
        VR_PhysicsEntityEnd(player, 1, world);
        check(VR_ThinkFrame(probeFrame) == probeFrame, "... and closes");
    }
    test.tally.verdict(testName);
}

// vr_hosterror_test [check | pair]: with no argument, in a single-player map with slow motion on (vr_timescale 0.5,
// vr_timescale_move_realtime 1, a few frames to ease in), a QuakeC error raised from C++ inside the player's turn in
// its own time, with a profile scope and a cvars::Override open; the error queues `check` (the next frame: what
// VR_OnHostError found and put back), which loads the map again and queues `pair` (slow motion's pairs whole again).
// Ends on "vr_hosterror_test: N passed, M failed".
void test_f()
{
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(step, "check"))
    {
        checkAfter();
    }
    else if(!q_strcasecmp(step, "pair"))
    {
        checkPair();
    }
    else
    {
        arm();
    }
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_hosterror_test", test_f);
}

} // namespace qvr::hosterror

// Host_Error (Host_ReportError) and Host_EndGame, before the server's shutdown and the longjmp: what the jump would
// leave open put back (the list: this file's head).
extern "C" void VR_OnHostError(void)
{
    using namespace qvr;
    hosterror::Unwound& u = hosterror::unwound;
    ++u.errors;
    u.overrides = cvars::Override::putBackAll();
    u.timescalePair = timescale::onHostError();
    u.muzzleShift = physics::onHostError();
    u.profileScopes = profile::onHostError();
    u.profileLeft = profile::openScopes();
    Con_DPrintf("VR_OnHostError: %d cvar override(s) put back, slow motion's pair %s, the muzzle shift %s, %d profile "
                "scope(s) ended\n",
        u.overrides, u.timescalePair ? "closed" : "not open", u.muzzleShift ? "forgotten" : "not set", u.profileScopes);
}
