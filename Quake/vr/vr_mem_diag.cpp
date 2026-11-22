// vr_mem_diag.cpp -- vr_resets: the registered resets (vr_mem.hpp) listed, traced as they run, and checked
// (Misc/quakevr/resets_test.sh: every one exactly once an event, in the declared order, at a map, a changelevel, a
// loaded game and a disconnect).
//
//   vr_resets                      each event's resets, in the order they run
//   vr_resets trace <0|1>          print each reset as it runs ("reset <event> <system> (<phase>)")
//   vr_resets record               keep each event and reset run from now on (vr_resets check takes them)
//   vr_resets check <scene> <server> <client>
//                                  the events run since the last record or check: <server> server world resets and
//                                  <client> client state resets, each running every registered reset of its event once,
//                                  in the list's order
//   vr_resets test                 the registry itself: the test's own resets, made out of order, run in order
//   vr_resets verdict              "vr_resets: N passed, M failed" for the checks and tests since the last verdict

#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <cstdlib>

namespace qvr::mem
{

namespace
{

// The checks' tally until vr_resets verdict, and the order vr_resets test's own resets ran in (main thread).
struct ResetsTest
{
    testkit::Tally tally;
    za::InPlaceVector<char, 32> ran; // each test reset's letter, as it ran
};
ResetsTest resetsTest;

void check(const bool ok, const char* what)
{
    resetsTest.tally.check(ok);
    Con_Printf("vr_resets: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void list()
{
    for(int e = 0; e < static_cast<int>(ResetEvent::Count); e++)
    {
        const ResetEvent event = static_cast<ResetEvent>(e);
        if(event == ResetEvent::Test)
        {
            continue;
        }
        Con_Printf("%s: %d resets, in this order\n", name(event), resetCount(event));
        for(const Reset* r = Reset::first(); r; r = r->next())
        {
            if(r->event() == event)
            {
                Con_Printf("  %-9s %s\n", name(r->phase()), r->system());
            }
        }
    }
}

// One event's run, as recorded: every registered reset of its event once, in the list's order.
[[nodiscard]] bool ranInOrder(const za::Vector<ResetCall>& calls, za::SizeT begin, za::SizeT end, const ResetEvent event)
{
    za::SizeT i = begin;
    for(const Reset* r = Reset::first(); r; r = r->next())
    {
        if(r->event() != event)
        {
            continue;
        }
        if(i == end || calls[i].reset != r)
        {
            return false;
        }
        i++;
    }
    return i == end;
}

void checkRecorded(const char* scene, const int wantServer, const int wantClient)
{
    const za::Vector<ResetCall> calls = takeRecordedResets();
    int runs[static_cast<int>(ResetEvent::Count)] = {};
    int wrong[static_cast<int>(ResetEvent::Count)] = {};
    for(za::SizeT i = 0; i < calls.size();)
    {
        const ResetEvent event = calls[i].event;
        za::SizeT end = i + 1;
        while(end < calls.size() && calls[end].reset)
        {
            end++;
        }
        runs[static_cast<int>(event)]++;
        wrong[static_cast<int>(event)] += ranInOrder(calls, i + 1, end, event) ? 0 : 1;
        i = end;
    }
    const struct
    {
        ResetEvent event;
        int want;
    } events[] = {{ResetEvent::ServerWorld, wantServer}, {ResetEvent::ClientState, wantClient}};
    for(const auto& [event, want] : events)
    {
        const int n = runs[static_cast<int>(event)];
        const int bad = wrong[static_cast<int>(event)];
        check(n == want, va("%s: %s resets ran %d times (%d expected)", scene, name(event), n, want));
        check(bad == 0, va("%s: each %s run ran its %d resets once each, in order (%d runs not)", scene, name(event),
                            resetCount(event), bad));
    }
}

void test()
{
    // The real lists: in order (event, phase, name), no two of one event, phase and name (their order would be the
    // initialisers'), and the order the resets rely on (vr_mem.hpp's phases).
    bool sorted = true;
    bool unique = true;
    for(const Reset* r = Reset::first(); r && r->next(); r = r->next())
    {
        const Reset* n = r->next();
        const int byName = ZA_STRCMP(r->system(), n->system());
        sorted = sorted && (r->event() < n->event() || (r->event() == n->event() && (r->phase() < n->phase() ||
                                                                                       (r->phase() == n->phase() && byName <= 0))));
        unique = unique && !(r->event() == n->event() && r->phase() == n->phase() && byName == 0);
    }
    check(sorted, "the list is in order: event, phase, system's name");
    check(unique, "no two resets of one event and phase share a name");
    check(resetCount(ResetEvent::ServerWorld) > 0 && resetCount(ResetEvent::ClientState) > 0, "both events have resets");
    const Reset* firstServer = nullptr;
    const Reset* lastServer = nullptr;
    for(const Reset* r = Reset::first(); r; r = r->next())
    {
        if(r->event() == ResetEvent::ServerWorld)
        {
            firstServer = firstServer ? firstServer : r;
            lastServer = r;
        }
    }
    check(firstServer && !ZA_STRCMP(firstServer->system(), "box3d") && firstServer->phase() == ResetPhase::World,
        "the server's first reset is Box3D's world (phase world)");
    check(lastServer && !ZA_STRCMP(lastServer->system(), "scenario") && lastServer->phase() == ResetPhase::Settings,
        "the server's last reset is the test scene's settings put back (phase settings)");

    // The registry: the test's own resets, made out of their order, run in it (phase, then name), each once a run; made
    // here and gone at the scope's end.
    resetsTest.ran.clear();
    {
        Reset c{"c", ResetEvent::Test, ResetPhase::State, [] { resetsTest.ran.pushBack('c'); }};
        Reset last{"a", ResetEvent::Test, ResetPhase::Settings, [] { resetsTest.ran.pushBack('S'); }};
        Reset a{"a", ResetEvent::Test, ResetPhase::State, [] { resetsTest.ran.pushBack('a'); }};
        Reset world{"z", ResetEvent::Test, ResetPhase::World, [] { resetsTest.ran.pushBack('W'); }};
        Reset b{"b", ResetEvent::Test, ResetPhase::State, [] { resetsTest.ran.pushBack('b'); }};
        check(resetCount(ResetEvent::Test) == 5, "the test's 5 resets registered");
        resetAll(ResetEvent::Test);
        resetsTest.ran.pushBack('|');
        resetAll(ResetEvent::Test);
    }
    resetsTest.ran.pushBack('\0');
    const char* ran = resetsTest.ran.data();
    check(!ZA_STRCMP(ran, "WabcS|WabcS"),
        va("two runs of the test's event: each reset once a run, world, the state's by name, settings (ran \"%s\")", ran));
    check(resetCount(ResetEvent::Test) == 0, "the test's resets gone with their scope");
}

void resets_f()
{
    const int argc = Cmd_Argc();
    const char* what = argc > 1 ? Cmd_Argv(1) : "";
    if(!what[0])
    {
        list();
    }
    else if(!ZA_STRCMP(what, "trace") && argc > 2)
    {
        traceResets(atoi(Cmd_Argv(2)) != 0);
    }
    else if(!ZA_STRCMP(what, "record"))
    {
        (void)takeRecordedResets();
        recordResets(true);
    }
    else if(!ZA_STRCMP(what, "check") && argc > 4)
    {
        checkRecorded(Cmd_Argv(2), atoi(Cmd_Argv(3)), atoi(Cmd_Argv(4)));
    }
    else if(!ZA_STRCMP(what, "test"))
    {
        test();
    }
    else if(!ZA_STRCMP(what, "verdict"))
    {
        resetsTest.tally.verdict("vr_resets");
        resetsTest.tally.reset();
    }
    else
    {
        Con_Printf("vr_resets [trace <0|1> | record | check <scene> <server runs> <client runs> | test | verdict]\n");
    }
}

} // namespace

void registerDiagCommands()
{
    Cmd_AddCommand("vr_resets", resets_f);
}

} // namespace qvr::mem
