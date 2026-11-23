// vr_seatplay.cpp -- splitscreen phase 6 (docs/vr-port/SPLITSCREEN.md): seats 1-3 joining and leaving the local game.
//
// A seat joins with a pad (seat_join <pad>; phase 10's join screen will do it): it comes into play driven by that pad,
// and once the local game is in play (a server running, seat 0 signed on) it connects over its own loopback pair
// (net_loop.c) and takes a free server slot as a network player does, under its own name ("<name> 2"...): coop's level
// start things, deathmatch's spawn; coming back by name after leaving (vr_coopsave.cpp's leavers) gets its things back.
// A seat leaving (seat_leave <n>) disconnects as a network player does, the game running on. A seat in play but out of
// the game (a new map started, a game loaded: the server dropped everyone) joins the next local game by itself; with
// no free slot (single player, a full server) it is told so and leaves. Seats 1-3 run headless until phase 7.
//
// vr_seat_play_test: seat 2 joins a running game by a mock pad, moves by its stick, is seen by seat 1 as a player,
// leaves, and joins again with its things (coop); in deathmatch it spawns, moves and leaves. Verdict
// "seat_play_test: N passed, M failed".
#include "vr_seat.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"

#include <SDL.h>

namespace qvr::seat
{

namespace
{

// The seats' "out of a game" notices printed once each (a seat waiting with no free slot is told once).
// seat-shared: the joins' own notices, by seat
struct Joins
{
    bool toldFull[maxSeats] = {};
} joins;

[[nodiscard]] bool localGameInPlay()
{
    const clientseat_t* n0 = CL_SeatNetIfMade(0);
    return sv.active && n0 && n0->cstatic.state == ca_connected && n0->cstatic.signon == SIGNONS &&
           !n0->cstatic.demoplayback;
}

// The server slot seat s's loopback has (-1: none).
[[nodiscard]] int slotOf(const int s)
{
    if(!sv.active || !svs.clients)
    {
        return -1;
    }
    for(int i = 0; i < svs.maxclients; i++)
    {
        if(svs.clients[i].netconnection && NET_QSocketLocalSeat(svs.clients[i].netconnection) == s)
        {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] int freeSlots()
{
    int n = 0;
    for(int i = 0; sv.active && svs.clients && i < svs.maxclients; i++)
    {
        n += svs.clients[i].active ? 0 : 1;
    }
    return n;
}

// The first seat 1-3 not in play (-1: all play).
[[nodiscard]] int freeSeat()
{
    for(int s = 1; s < maxSeats; s++)
    {
        if(!plays(s))
        {
            return s;
        }
    }
    return -1;
}

// Seat s (1-3) into play, joined by pad k: it connects in the next frames (CL_SeatFrame). Returns false (printed) when
// it can't: no seat left, the pad already a seat's, no local game with room (needGame: the join screen's seats come in
// before the game, which they join when it starts).
bool join(const int s, const int pad, const bool needGame = true)
{
    if(s < 1 || s >= maxSeats || plays(s))
    {
        Con_Printf("seat_join: no seat free (seats 2-%d all play)\n", maxSeats);
        return false;
    }
    for(int i = 1; i < maxSeats; i++)
    {
        if(plays(i) && padOf(i) == pad)
        {
            Con_Printf("seat_join: pad %d already plays as seat %d\n", pad, i + 1);
            return false;
        }
    }
    if(!needGame && (!sv.active || svs.maxclients <= 1))
    {
        // (no local multiplayer game: the seat waits for the next one)
    }
    else if(!sv.active || svs.maxclients <= 1)
    {
        Con_Printf("seat_join: splitscreen needs a local multiplayer game (coop or deathmatch, maxplayers 2 or more)\n");
        return false;
    }
    else if(freeSlots() < 1)
    {
        Con_Printf("seat_join: the game is full (maxplayers %d)\n", svs.maxclients);
        return false;
    }
    setPlays(s, true, pad);
    IN_PadSetUnseated(pad, false);
    joins.toldFull[s] = false;
    Con_Printf("seat %d joins (pad %d)\n", s + 1, pad);
    return true;
}

void leave(const int s)
{
    if(s < 1 || s >= maxSeats || !plays(s))
    {
        return;
    }
    {
        const Scope in(at(s));
        CL_SeatDisconnect();
    }
    // split11: its pad drives no seat now (not seat 1's as its spare): its Start or A joins again (IN_PadDropIn)
    const int pad = padOf(s);
    IN_PadSetUnseated(pad, true);
    setPlays(s, false, -1);
    Con_Printf("seat %d leaves\n", s + 1);
    if(pad >= 0 && sv.active && svs.maxclients > 1)
    {
        Con_Printf("Controller %d: Start or A joins the game again\n", pad + 1);
    }
}

// seat_join <pad>: the pad's player joins the local game as the next free seat (seats 2-4; seat 1 is the keyboard and
// mouse). Tests and the Debug menu until the join screen (phase 10).
void join_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("seat_join <pad 0-3> : that controller's player joins the game as the next free seat (2-%d)\n",
            maxSeats);
        return;
    }
    join(freeSeat(), Q_atoi(Cmd_Argv(1)));
}

// seat_leave <n>: seat n (2-4) leaves the game; with no number the last seat that plays.
void leave_f()
{
    int s = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) - 1 : -1;
    for(int i = maxSeats - 1; s < 0 && i > 0; i--)
    {
        s = plays(i) ? i : -1;
    }
    if(s < 1 || s >= maxSeats || !plays(s))
    {
        Con_Printf("seat_leave <2-%d> : that seat leaves the game (no seat of that number plays)\n", maxSeats);
        return;
    }
    leave(s);
}

// seat_mockpad <pad> <0|1> [lx ly rx ry]: a pad without a controller (the tests', and a way to try a seat without one):
// on or off, and its sticks.
void mockPad_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("seat_mockpad <pad 0-3> <0|1> [lx ly rx ry] : a pretend controller (tests), its sticks -1..1\n");
        return;
    }
    const int k = Q_atoi(Cmd_Argv(1));
    if(Q_atoi(Cmd_Argv(2)) != 0 && Cmd_Argc() < 4)
    {
        IN_MockPad(k, true);
    }
    else if(Q_atoi(Cmd_Argv(2)) == 0)
    {
        IN_MockPad(k, false);
    }
    if(Cmd_Argc() >= 7)
    {
        IN_MockPadAxes(k, Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)));
    }
}

// seat_status: the seats in play, each one's pad, connection and server slot.
void status_f()
{
    for(int s = 0; s < maxSeats; s++)
    {
        const clientseat_t* n = CL_SeatNetIfMade(s);
        char name[64];
        CL_SeatName(s, name, sizeof(name));
        Con_Printf("seat %d: %s%s pad %d, %s, signon %d, slot %d (%s)\n", s + 1, plays(s) ? "plays" : "out",
            s == 0 ? " (keyboard and mouse)" : "", padOf(s), n && n->cstatic.state == ca_connected ? "connected" : "not connected",
            n ? n->cstatic.signon : 0, slotOf(s), name);
    }
}

// ---- vr_seat_play_test ----------------------------------------------------------------------------------------------

enum class Step
{
    Off,
    WaitGame,   // seat 1 (index 0) in the game
    WaitJoin,   // seat 2 signed on
    Moving,     // seat 2's stick forward: its player walks
    WaitSeen,   // seat 1's client has seat 2's player where the server has him
    WaitLeft,   // seat 2 left: its slot free
    WaitRejoin, // seat 2 back: its slot and things
    // split12 (coop): one player reaches the level exit (the crash of 2026-11-23: seat 2's client on the freed world)
    WaitIntermission, // seat 2's player put in the level's trigger_changelevel: both seats' clients at the intermission
    WaitNewMap,       // seat 2's A held: the next map, both seats signed on in it; no seat's client on the old world
    PlayNewMap,       // both play the new map a second
};

// seat-shared: vr_seat_play_test's own state machine
struct PlayTest
{
    testkit::Tally tally;
    Step step = Step::Off;
    int frames = 0; // in this step
    int slot = -1;  // seat 2's server slot
    vec3_t start0{}, start1{}; // seat 1's and seat 2's players where the walk started
    bool dm = false;
    char fromMap[64]{};  // split12: the map the exit was taken on
    int staleWorld = 0;  // frames a seat's client had a world model that is not the server's (a freed one)
    int staleSeat = -1;  // the first such seat
} playTest;

constexpr int pad = 0;           // the test's mock pad
constexpr int stepFrames = 1440; // a step's give-up (20 s of game time): a FAIL "timed out"

bool check(const bool ok, const char* what)
{
    if(playTest.tally.check(ok, "seat_play_test", what))
    {
        Con_Printf("seat_play_test: PASS %s\n", what);
    }
    return ok;
}

[[nodiscard]] edict_t* playerOf(const int slot)
{
    return slot >= 0 && slot < svs.maxclients && svs.clients[slot].active ? svs.clients[slot].edict : nullptr;
}

[[nodiscard]] float flatDistance(const vec3_t a, const vec3_t b)
{
    return za::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
}

void finish()
{
    IN_MockPadAxes(pad, 0.f, 0.f, 0.f, 0.f);
    leave(1);
    IN_MockPad(pad, false);
    playTest.step = Step::Off;
    playTest.tally.verdict("seat_play_test");
}

void go(const Step s)
{
    playTest.step = s;
    playTest.frames = 0;
}

[[nodiscard]] bool signedOn(const int s)
{
    const clientseat_t* n = CL_SeatNetIfMade(s);
    return n && n->cstatic.state == ca_connected && n->cstatic.signon == SIGNONS;
}

// The checks once seat 2 is in (first join and rejoin): its own slot (not the host's), its name, its client's player.
void checkJoined(const char* when)
{
    const int slot = slotOf(1);
    char name[64];
    CL_SeatName(1, name, sizeof(name));
    check(slot > 0 && slotOf(0) == 0, va("%s: seat 2 in a slot of its own (%d), seat 1 in slot 0 (%d)", when, slot, slotOf(0)));
    check(slot >= 0 && !ZA_STRCMP(svs.clients[slot].name, name), va("%s: under its own name (\"%s\")", when, slot >= 0 ? svs.clients[slot].name : ""));
    const clientseat_t* n1 = CL_SeatNetIfMade(1);
    check(n1 && n1->cstate.viewentity == slot + 1, va("%s: seat 2's client views its own player (entity %d)", when, n1 ? n1->cstate.viewentity : -1));
    {
        const Scope in(at(1));
        check(progs::hostSlot() == slot, va("%s: seat 2's own slot is its seat's (progs::hostSlot %d)", when, progs::hostSlot()));
    }
    check(progs::hostSlot() == 0, "seat 1's own slot is still 0");
    check(inPlay() == 2 && !Host_SeatsMaySave("s0") && Host_SeatsMaySave("autosave/coop~e1m1"),
        "two seats: a save refused, the level-start coop autosave allowed");
}

void testFrame()
{
    PlayTest& t = playTest;
    t.frames++;
    if(t.frames > stepFrames)
    {
        check(false, va("timed out (step %d)", static_cast<int>(t.step)));
        finish();
        return;
    }
    switch(t.step)
    {
        case Step::Off: return;
        case Step::WaitGame:
            if(!localGameInPlay())
            {
                return;
            }
            t.dm = deathmatch.value != 0.f;
            check(inPlay() == 1 && Host_SeatsMaySave("s0"), "one seat: saving as before");
            IN_MockPad(pad, true);
            check(IN_PadSeat(pad) == 0, "one seat: the pad drives seat 1 (as before)");
            check(join(1, pad), "seat 2 joins the running game by pad 0");
            check(IN_PadSeat(pad) == 1, "pad 0 drives seat 2");
            go(Step::WaitJoin);
            return;
        case Step::WaitJoin:
        {
            if(!signedOn(1))
            {
                return;
            }
            checkJoined("joined");
            t.slot = slotOf(1);
            edict_t* p0 = playerOf(0);
            edict_t* p1 = playerOf(t.slot);
            if(!check(p0 && p1 && p1->v.health > 0.f, "seat 2's player spawned"))
            {
                finish();
                return;
            }
            check(flatDistance(p0->v.origin, p1->v.origin) > 1.f, va("seat 2 spawned apart from seat 1 (%.0f units)",
                static_cast<double>(flatDistance(p0->v.origin, p1->v.origin))));
            if(!t.dm)
            {
                p1->v.health = 70.f; // his things, for his return (MP_PLAN.md B15)
                p1->v.ammo_nails = 50.f;
            }
            VectorCopy(p0->v.origin, t.start0);
            VectorCopy(p1->v.origin, t.start1);
            IN_MockPadAxes(pad, 0.f, -1.f, 0.f, 0.f); // the left stick forward
            go(Step::Moving);
            return;
        }
        case Step::Moving:
        {
            edict_t* p0 = playerOf(0);
            edict_t* p1 = playerOf(t.slot);
            if(!p0 || !p1)
            {
                check(false, "the players stay in the game while seat 2 walks");
                finish();
                return;
            }
            if(flatDistance(p1->v.origin, t.start1) < 64.f)
            {
                return;
            }
            IN_MockPadAxes(pad, 0.f, 0.f, 0.f, 0.f);
            check(true, va("seat 2 walks by its pad's stick (%.0f units in %d frames)",
                static_cast<double>(flatDistance(p1->v.origin, t.start1)), t.frames));
            check(flatDistance(p0->v.origin, t.start0) < 1.f, va("seat 1 stays where it was (%.1f units)",
                static_cast<double>(flatDistance(p0->v.origin, t.start0))));
            // (deathmatch spawns them out of each other's sight, and the server sends only what a player may see: seat
            // 2's player is put beside seat 1's, on the first free side, for the "seen" checks)
            if(flatDistance(p0->v.origin, p1->v.origin) > 128.f)
            {
                const progs::ServerVm vm; // (the server's world: SV_Move, SV_LinkEdict)
                constexpr float sides[4][2] = {{56.f, 0.f}, {-56.f, 0.f}, {0.f, 56.f}, {0.f, -56.f}};
                for(const auto& side : sides)
                {
                    vec3_t to = {p0->v.origin[0] + side[0], p0->v.origin[1] + side[1], p0->v.origin[2]};
                    const trace_t tr = SV_Move(p0->v.origin, p1->v.mins, p1->v.maxs, to, MOVE_NORMAL, p0);
                    if(tr.fraction == 1.f && !tr.startsolid && !tr.allsolid)
                    {
                        VectorCopy(to, p1->v.origin);
                        SV_LinkEdict(p1, false);
                        break;
                    }
                }
            }
            go(Step::WaitSeen);
            return;
        }
        case Step::WaitSeen:
        {
            // seat 1's client world has seat 2's player where the server has him, and seat 2's has seat 1's
            const clientseat_t* n0 = CL_SeatNetIfMade(0);
            const clientseat_t* n1 = CL_SeatNetIfMade(1);
            edict_t* p0 = playerOf(0);
            edict_t* p1 = playerOf(t.slot);
            if(!n0 || !n1 || !p0 || !p1 || t.slot + 1 >= n0->max_edicts || n1->max_edicts < 2)
            {
                return;
            }
            const entity_t& seen1 = n0->entities[t.slot + 1];
            const entity_t& seen0 = n1->entities[1];
            const float d1 = flatDistance(seen1.msg_origins[0], p1->v.origin);
            const float d0 = flatDistance(seen0.msg_origins[0], p0->v.origin);
            if((!seen1.model || d1 > 2.f || !seen0.model || d0 > 2.f) && t.frames < 72)
            {
                return;
            }
            check(seen1.model && d1 <= 2.f, va("seat 1 sees seat 2's player where he is (%.1f units)", static_cast<double>(d1)));
            check(seen0.model && d0 <= 2.f, va("seat 2 sees seat 1's player where he is (%.1f units)", static_cast<double>(d0)));
            leave(1);
            // (split11: the pad that left drives no seat while the game runs, not seat 1's as its spare: its Start or A joins)
            check(!plays(1) && IN_PadSeat(pad) < 0, "seat 2 left: out of play, its pad drives no seat (its Start or A joins again)");
            go(Step::WaitLeft);
            return;
        }
        case Step::WaitLeft:
            if(svs.clients[t.slot].active)
            {
                return;
            }
            check(true, va("seat 2's slot %d freed", t.slot));
            check(signedOn(0) && playerOf(0), "seat 1 plays on");
            check(join(1, pad), "seat 2 joins again");
            go(Step::WaitRejoin);
            return;
        case Step::WaitRejoin:
        {
            if(!signedOn(1))
            {
                return;
            }
            checkJoined("back");
            edict_t* p1 = playerOf(slotOf(1));
            if(!t.dm)
            {
                check(slotOf(1) == t.slot, va("back in his old slot (%d)", slotOf(1)));
                check(p1 && p1->v.health == 70.f && p1->v.ammo_nails == 50.f,
                    va("back with his things (health %.0f, nails %.0f)", p1 ? static_cast<double>(p1->v.health) : -1.0,
                        p1 ? static_cast<double>(p1->v.ammo_nails) : -1.0));
            }
            else
            {
                check(p1 && p1->v.health > 0.f, "deathmatch: back, spawned again");
                finish();
                return;
            }
            // split12: seat 2's player into the level's exit
            edict_t* exit = nullptr;
            {
                const progs::ServerVm vm;
                for(int i = 1; i < qcvm->num_edicts && !exit; i++)
                {
                    edict_t* e = EDICT_NUM(i);
                    if(!e->free && !ZA_STRCMP(PR_GetString(e->v.classname), "trigger_changelevel"))
                    {
                        exit = e;
                    }
                }
                if(exit && p1)
                {
                    for(int k = 0; k < 3; k++)
                    {
                        p1->v.origin[k] = (exit->v.absmin[k] + exit->v.absmax[k]) * 0.5f;
                    }
                    SV_LinkEdict(p1, false);
                }
            }
            if(!check(exit && p1, "the level's exit found, seat 2's player put in it"))
            {
                finish();
                return;
            }
            q_strlcpy(t.fromMap, sv.name, sizeof(t.fromMap));
            go(Step::WaitIntermission);
            return;
        }
        case Step::WaitIntermission:
        {
            const clientseat_t* n0 = CL_SeatNetIfMade(0);
            const clientseat_t* n1 = CL_SeatNetIfMade(1);
            if(!n0 || !n1 || !n0->cstate.intermission || !n1->cstate.intermission)
            {
                return;
            }
            check(true, va("both seats at the intermission (%d frames)", t.frames));
            IN_MockPadButton(pad, SDL_CONTROLLER_BUTTON_A, true); // (held: the exit waits 2 s, then takes a press)
            go(Step::WaitNewMap);
            return;
        }
        case Step::WaitNewMap:
        case Step::PlayNewMap:
        {
            // Every frame from the exit on: each seat's client has no world or the server's (the old one was freed)
            for(int s = 0; s < 2; s++)
            {
                const clientseat_t* n = CL_SeatNetIfMade(s);
                if(n && n->cstate.worldmodel && (!sv.active || n->cstate.worldmodel != sv.worldmodel))
                {
                    t.staleWorld++;
                    t.staleSeat = t.staleSeat < 0 ? s : t.staleSeat;
                }
            }
            if(t.step == Step::WaitNewMap)
            {
                if(!sv.active || !ZA_STRCMP(sv.name, t.fromMap) || !signedOn(0) || !signedOn(1))
                {
                    return;
                }
                IN_MockPadButton(pad, SDL_CONTROLLER_BUTTON_A, false);
                check(true, va("the next map (%s after %s): both seats signed on in it", sv.name, t.fromMap));
                go(Step::PlayNewMap);
                return;
            }
            if(t.frames < 72)
            {
                return;
            }
            edict_t* p0 = playerOf(0);
            edict_t* p1 = playerOf(slotOf(1));
            check(p0 && p1 && p0->v.health > 0.f && p1->v.health > 0.f && slotOf(1) > 0,
                va("both play the new map (seat 2 in slot %d)", slotOf(1)));
            check(t.staleWorld == 0, va("no seat's client on the old (freed) world after the exit (%d frames, seat %d)",
                t.staleWorld, t.staleSeat + 1));
            finish();
            return;
        }
    }
}

// vr_seat_play_test: run in a local coop or deathmatch game (maxplayers 2 or more; -flatscreenmode), or right before
// its map command: the test waits for the game.
void playTest_f()
{
    playTest = PlayTest{};
    go(Step::WaitGame);
    Con_Printf("seat_play_test: started\n");
}

} // namespace

int joinPad(const int pad)
{
    const int s = freeSeat();
    return join(s, pad, false) ? s : -1;
}

void leaveSeat(const int s)
{
    leave(s);
}

int seatOfPad(const int pad)
{
    for(int s = 1; s < maxSeats; s++)
    {
        if(plays(s) && padOf(s) == pad)
        {
            return s;
        }
    }
    return -1;
}

void registerPlayCommands()
{
    Cmd_AddCommand("seat_join", join_f);
    Cmd_AddCommand("seat_leave", leave_f);
    Cmd_AddCommand("seat_mockpad", mockPad_f);
    Cmd_AddCommand("seat_status", status_f);
    Cmd_AddCommand("vr_seat_play_test", playTest_f);
}

} // namespace qvr::seat

// split11: a pad driving no seat (it left its seat, or a spare) pressed Start or A in a running local multiplayer game
// (in_sdl.c's IN_PadDropIn): it joins as the next free seat at once and connects (CL_SeatFrame), no menu.
extern "C" void VR_SeatDropIn(const int pad)
{
    using namespace qvr::seat;
    if(VR_IsActive() || seatOfPad(pad) > 0)
    {
        return; // (splitscreen is flatscreen play's; a pad in a seat already drives it)
    }
    const int s = joinPad(pad);
    if(s > 0)
    {
        Con_Printf("Controller %d joins as seat %d\n", pad + 1, s + 1);
    }
}

// Once a host frame, before the seat loop (host.c): seats in play connect to the local game in play, seats whose game
// ended let go of it; then the play test's step.
extern "C" void CL_SeatFrame()
{
    using namespace qvr::seat;
    for(int s = 1; s < maxSeats; s++)
    {
        if(!plays(s))
        {
            continue;
        }
        const clientseat_t* n = CL_SeatNetIfMade(s);
        const bool connected = n && n->cstatic.state == ca_connected;
        // (the local game ended, or the server closed its side of the seat's loopback: a new map, a load)
        if(connected && (!sv.active || !n->loopserver))
        {
            const Scope in(at(s));
            CL_SeatDisconnect(); // (the local game ended: the next one is joined)
        }
        else if(!connected && localGameInPlay())
        {
            if(svs.maxclients <= 1 || freeSlots() < 1)
            {
                if(M_Splitscreen_Open())
                {
                    continue; // (the join screen up over a game with no room: its seats wait for the game it starts)
                }
                if(!joins.toldFull[s])
                {
                    Con_Printf("seat %d: no room in this game (maxplayers %d): it leaves\n", s + 1, svs.maxclients);
                }
                joins.toldFull[s] = true;
                leave(s);
                continue;
            }
            const Scope in(at(s));
            CL_SeatConnectLocal();
        }
    }
    if(playTest.step != Step::Off)
    {
        testFrame();
    }
    renderTestFrame();
    menuTestFrame();
    joinTestFrame();
}
