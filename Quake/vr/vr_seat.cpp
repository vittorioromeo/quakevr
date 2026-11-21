// vr_seat.cpp -- splitscreen seats (vr_seat.hpp; docs/vr-port/SPLITSCREEN.md): the seats, the current one, the
// `seat` command, and vr_seat_test (two seats' state kept apart).
#include "vr_seat.hpp"

#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <SDL.h>

namespace qvr::seat
{

namespace
{

// The seats themselves (each seat's state is its own; this is their owner), which one is current and how many play.
struct Seats
{
    Seat all[maxSeats] = {{0}, {1}, {2}, {3}};
    Seat* cur = &all[0];
    int inPlay = 1;
} seats; // seat-shared: the seats' owner

// seat <1-4> <command>: runs a command as that seat (its bindings: "seat 2 bind YBUTTON +jump"; its buttons)
void seat_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("seat <1-%d> <command> : runs a command as that seat (seat 1: the keyboard and mouse)\n", maxSeats);
        return;
    }
    const int n = Q_atoi(Cmd_Argv(1));
    if(n < 1 || n > maxSeats)
    {
        Con_Printf("seat: %d isn't a seat (1-%d)\n", n, maxSeats);
        return;
    }
    // the command line after the seat's number, as typed
    const char* rest = Cmd_Args();
    while(*rest == ' ' || *rest == '\t')
    {
        rest++;
    }
    while(*rest && *rest != ' ' && *rest != '\t')
    {
        rest++;
    }
    const Scope in(at(n - 1));
    Cmd_ExecuteString(rest, src_command);
}

// vr_seat_test's state across its two steps (the queued binds run between them), and +seattest's log
// seat-shared: vr_seat_test's own
struct SeatTest
{
    testkit::Tally tally;
    struct Entry
    {
        int seat, sign, key;
    } log[32];
    int logged = 0;
    keydest_t keyDest = key_game;
    char saved[3][256]{};
    bool had[3]{};
} test; // seat-shared: vr_seat_test's own

constexpr int testKeys[3] = {K_YBUTTON, K_XBUTTON, K_F11};

// +seattest / -seattest <key>: what ran, as which seat
void seattest_f(const int sign)
{
    if(test.logged < static_cast<int>(sizeof(test.log) / sizeof(test.log[0])))
    {
        test.log[test.logged++] = {CL_SeatIndex(), sign, Q_atoi(Cmd_Argv(1))};
    }
}

void seattestDown_f()
{
    seattest_f(1);
}

void seattestUp_f()
{
    seattest_f(-1);
}

bool check(const bool ok, const char* what)
{
    if(test.tally.check(ok, "seat_test", what))
    {
        Con_DPrintf("seat_test: PASS %s\n", what);
    }
    return ok;
}

bool held(const kbutton_t& b)
{
    return (b.state & 1) != 0;
}

void run(const char* text)
{
    Cmd_ExecuteString(text, src_command);
}

// Phase 1: in seat 1 (a Scope), the +commands and the impulse land in seat 1's buttons and none of seat 0's, and the
// reverse; the menu's release (CL_ReleaseMoveButtons) lets go of the current seat's only; scopes nest.
void testButtons()
{
    Seat& s0 = at(0);
    Seat& s1 = at(1);
    const int impulse0 = s0.input.impulse;

    check(&current() == &s0 && CL_SeatInput() == &s0.input, "seat 0 is current outside a seat loop");
    check(&s0.input != &s1.input && s1.index == 1, "seats 0 and 1 have their own input");
    {
        const Scope in1(s1);
        check(&current() == &s1 && CL_SeatInput() == &s1.input, "a scope makes seat 1 current");
        run("+forward 200");
        run("+attack 201");
        run("impulse 7");
        check(held(s1.input.forward) && !held(s0.input.forward), "+forward in seat 1: seat 1's held, seat 0's not");
        check(held(s1.input.attack) && !held(s0.input.attack), "+attack in seat 1: seat 1's held, seat 0's not");
        check(s1.input.impulse == 7 && s0.input.impulse == impulse0, "impulse 7 in seat 1: seat 0's unchanged");
        check(CL_KeyState(&s1.input.forward) == 0.5f && CL_KeyState(&s0.input.forward) == 0.f,
              "CL_KeyState: seat 1 pressed this frame, seat 0 up");
        {
            const Scope in2(at(2));
            check(&current() == &at(2), "a nested scope makes seat 2 current");
        }
        check(&current() == &s1, "the nested scope gives seat 1 back");
    }
    check(&current() == &s0, "the scope gives seat 0 back");
    run("+back 202");
    check(held(s0.input.back) && !held(s1.input.back), "+back in seat 0: seat 0's held, seat 1's not");
    check(held(s1.input.forward), "seat 1's +forward still held from seat 0");
    {
        const Scope in1(s1);
        check(CL_MoveButtonsHeld() == 1, "seat 1 holds one movement button (its own count)");
        CL_ReleaseMoveButtons();
        check(!held(s1.input.forward) && held(s0.input.back), "the release in seat 1: seat 1's let go, seat 0's held");
        run("-attack 201");
        check(!held(s1.input.attack), "-attack in seat 1 lets go of seat 1's");
        s1.input.impulse = 0;
    }
    run("-back 202");
    check(!held(s0.input.back) && CL_MoveButtonsHeld() == 0, "-back in seat 0: nothing held");
}

// Phase 2, at once: bindings per seat (bind as seat 2 through `seat`; a copy of seat 0's is the seat's own); the
// `seat` command runs as its seat.
void testBindings()
{
    seatkeys_t* k0 = Key_SeatOf(0);
    seatkeys_t* k1 = Key_SeatOf(1);
    seatkeys_t* k2 = Key_SeatOf(2);
    check(Key_Seat() == k0 && k0 != k1, "seat 0's keys are the current ones; seats have their own");
    for(int i = 0; i < 3; i++)
    {
        const char* b = k0->bindings[testKeys[i]];
        test.had[i] = b != nullptr;
        q_strlcpy(test.saved[i], b ? b : "", sizeof(test.saved[i]));
    }
    run("seat 2 bind YBUTTON \"+seattest\"");
    check(k1->bindings[K_YBUTTON] && !strcmp(k1->bindings[K_YBUTTON], "+seattest") &&
              (test.had[0] ? !strcmp(k0->bindings[K_YBUTTON], test.saved[0]) : !k0->bindings[K_YBUTTON]),
          "seat 2 bind YBUTTON: seat 1's binding set, seat 0's as it was");
    run("bind YBUTTON \"+seattest\"");
    run("bind XBUTTON \"+seattest\"");
    run("bind F11 \"+seattest\"");
    run("seat 2 bind F11 \"+seattest\"");
    check(!k1->bindings[K_XBUTTON], "bind XBUTTON as seat 1 (index 0): seat 2's XBUTTON still unbound");

    Key_SeatCopyBindings(2, 0);
    check(Key_SeatHasBindings(2) && k2->bindings[K_XBUTTON] && k2->bindings[K_XBUTTON] != k0->bindings[K_XBUTTON] &&
              !strcmp(k2->bindings[K_XBUTTON], k0->bindings[K_XBUTTON]),
          "a copy of seat 0's bindings: the same text, seat 2's own strings");
    run("seat 3 unbind XBUTTON");
    check(!k2->bindings[K_XBUTTON] && k0->bindings[K_XBUTTON], "unbind as seat 3: seat 0's binding kept");
    run("seat 3 unbindall");
    check(!Key_SeatHasBindings(2) && k0->bindings[K_XBUTTON], "unbindall as seat 3: seat 3 has none, seat 0's kept");

    test.logged = 0;
    run("seat 2 +seattest 999");
    check(test.logged == 1 && test.log[0].seat == 1 && test.log[0].key == 999 && &current() == &at(0),
          "seat 2 +seattest: ran as seat index 1, seat 0 current after");
    test.logged = 0;
}

// Phase 2, the devices: with two seats in play, pad 0 drives seat 1 and pad 1 seat 0 (the keyboard player's pad);
// each pad's buttons are its seat's keys, the keyboard's (a real SDL event, handled with seat 1 current) seat 0's;
// each pad's stick moves its seat only, the mouse only seat 0; back to one seat, pad 0 lets go in seat 1 and drives
// seat 0, pad 1 none. Their binds are queued (Key_Event): they run before step 2, which reads +seattest's log.
void testDevices()
{
    seatkeys_t* k0 = Key_SeatOf(0);
    seatkeys_t* k1 = Key_SeatOf(1);
    const int queued0 = Cbuf_Pending();
    test.keyDest = key_dest;
    key_dest = key_game; // (the binds run in the game: restored in step 2)

    setInPlay(2);
    IN_MockPad(0, true);
    IN_MockPad(1, true);
    check(IN_PadSeat(0) == 1 && IN_PadSeat(1) == 0 && IN_PadSeat(2) == -1, "two seats: pad 0 drives seat 1, pad 1 seat 0");

    IN_MockPadButton(0, SDL_CONTROLLER_BUTTON_Y, true);
    IN_Commands();
    check(k1->down[K_YBUTTON] && !k0->down[K_YBUTTON], "pad 0's Y: seat 1's key held, seat 0's not");
    IN_MockPadButton(1, SDL_CONTROLLER_BUTTON_X, true);
    IN_Commands();
    check(k0->down[K_XBUTTON] && !k1->down[K_XBUTTON], "pad 1's X: seat 0's key held, seat 1's not");
    {
        const Scope in1(at(1));
        IN_MockKeyboardKey(K_F11, true);
        check(&current() == &at(1), "the keyboard's event leaves seat 1 current");
    }
    check(k0->down[K_F11] && !k1->down[K_F11], "the keyboard's F11 (seat 1 current): seat 0's key held, seat 1's not");

    // the sticks: pad 0's left stick forward, pad 1's at rest; the mouse moved
    IN_MockPadAxes(0, 0.f, -1.f, 0.f, 0.f);
    IN_Commands();
    k0->mouse_dx = 5;
    usercmd_t cmd0{}, cmd1{};
    {
        const Scope in1(at(1));
        IN_JoyMove(&cmd1);
        IN_MouseMove(&cmd1);
    }
    IN_JoyMove(&cmd0);
    check(cmd1.forwardmove > 0.f && cmd0.forwardmove == 0.f, "pad 0's stick moves seat 1 forward, seat 0 not");
    check(k0->mouse_dx == 5 && k1->mouse_dx == 0, "seat 1's move leaves the mouse's motion to seat 0");
    k0->mouse_dx = 0;
    IN_MockPadAxes(0, 0.f, 0.f, 0.f, 0.f);

    setInPlay(1);
    IN_Commands();
    check(IN_PadSeat(0) == 0 && IN_PadSeat(1) == -1, "one seat: pad 0 drives seat 0 (as before), pad 1 none");
    check(!k1->down[K_YBUTTON] && k0->down[K_YBUTTON] && !k0->down[K_XBUTTON],
          "one seat: pad 0's Y let go in seat 1, held in seat 0; pad 1's X let go in seat 0");

    IN_MockPadButton(0, SDL_CONTROLLER_BUTTON_Y, false);
    IN_MockPadButton(1, SDL_CONTROLLER_BUTTON_X, false);
    IN_Commands();
    IN_MockKeyboardKey(K_F11, false);
    check(!k0->down[K_YBUTTON] && !k0->down[K_F11], "let go: nothing held in seat 0");
    {
        const Scope in1(at(1));
        Cbuf_AddText("+seattest 300\n"); // the queued text ends as seat 1's: step 2 must still run as seat 0
    }
    IN_MockPad(0, false);
    IN_MockPad(1, false);

    // step 2 after the queued binds (vr_mock_key's way: the tail moved to run next)
    const int queued = Cbuf_Pending() - queued0;
    Cbuf_InsertText("vr_seat_test 2");
    Cbuf_TailToFront(queued);
}

// step 2: the binds ran as the seats that pressed them
void testQueued()
{
    struct Want
    {
        int seat, sign, key;
    };
    constexpr Want want[] = {{1, 1, K_YBUTTON}, {0, 1, K_XBUTTON}, {0, 1, K_F11}, {1, -1, K_YBUTTON}, {0, 1, K_YBUTTON},
                             {0, -1, K_XBUTTON}, {0, -1, K_YBUTTON}, {0, -1, K_F11}, {1, 1, 300}};
    constexpr int wanted = static_cast<int>(sizeof(want) / sizeof(want[0]));
    bool same = test.logged == wanted;
    for(int i = 0; same && i < wanted; i++)
    {
        same = test.log[i].seat == want[i].seat && test.log[i].sign == want[i].sign && test.log[i].key == want[i].key;
    }
    if(!same)
    {
        for(int i = 0; i < test.logged; i++)
        {
            Con_Printf("seat_test: ran %c%d as seat %d\n", test.log[i].sign > 0 ? '+' : '-', test.log[i].key,
                       test.log[i].seat);
        }
    }
    check(same, "the queued binds ran as the seats that pressed them, in order (pad 0's in seat 1 then 0)");
    check(&current() == &at(0), "step 2 runs as seat 0 after a queue ending in seat 1's");

    // restore: seat 0's three bindings, the other seats' none, key_dest
    for(int i = 0; i < 3; i++)
    {
        Key_SetBinding(testKeys[i], test.had[i] ? test.saved[i] : nullptr);
    }
    for(int s = 1; s < maxSeats; s++)
    {
        const Scope in(at(s));
        run("unbindall");
    }
    key_dest = test.keyDest;
    test.logged = 0;
    test.tally.verdict("seat_test");
}

// vr_seat_test [2]: phase 1's buttons, phase 2's bindings and devices; then (2, queued by the first) the queued binds'
// seats and the verdict "seat_test: N passed, M failed". No map needed.
void test_f()
{
    if(Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) == 2)
    {
        testQueued();
        return;
    }
    test.tally.reset();
    testButtons();
    testBindings();
    testDevices();
}

} // namespace

Seat& current()
{
    return *seats.cur;
}

Seat& at(const int i)
{
    return seats.all[i < 0 ? 0 : i >= maxSeats ? maxSeats - 1 : i];
}

int inPlay()
{
    return seats.inPlay;
}

void setInPlay(const int n)
{
    seats.inPlay = n < 1 ? 1 : n > maxSeats ? maxSeats : n;
}

Scope::Scope(Seat& s) : prev_(seats.cur)
{
    seats.cur = &s;
}

Scope::~Scope()
{
    seats.cur = prev_;
}

void registerCommands()
{
    Cmd_AddCommand("seat", seat_f);
    Cmd_AddCommand("vr_seat_test", test_f);
    Cmd_AddCommand("+seattest", seattestDown_f);
    Cmd_AddCommand("-seattest", seattestUp_f);
}

} // namespace qvr::seat

extern "C" seatinput_t* CL_SeatInput()
{
    return &qvr::seat::current().input;
}

extern "C" seatinput_t* CL_SeatInputOf(const int seat)
{
    return &qvr::seat::at(seat).input;
}

extern "C" int CL_SeatIndex()
{
    return qvr::seat::current().index;
}

extern "C" int CL_SeatSet(const int seat)
{
    const int was = CL_SeatIndex();
    qvr::seat::seats.cur = &qvr::seat::at(seat);
    return was;
}

extern "C" int CL_SeatCount()
{
    return qvr::seat::maxSeats;
}

extern "C" int CL_SeatsInPlay()
{
    return qvr::seat::inPlay();
}

extern "C" seatkeys_t* Key_Seat()
{
    return &qvr::seat::current().keys;
}

extern "C" seatkeys_t* Key_SeatOf(const int seat)
{
    return &qvr::seat::at(seat).keys;
}

extern "C" qboolean Key_SeatHasBindings(const int seat)
{
    for(const char* b : Key_SeatOf(seat)->bindings)
    {
        if(b)
        {
            return true;
        }
    }
    return false;
}

extern "C" void Key_SeatCopyBindings(const int to, const int from)
{
    if(Key_SeatOf(to) == Key_SeatOf(from))
    {
        return;
    }
    const int prev = CL_SeatSet(to);
    for(int k = 0; k < MAX_KEYS; k++)
    {
        Key_SetBinding(k, Key_SeatOf(from)->bindings[k]); // (Key_SetBinding: the current seat's, its own copy)
    }
    CL_SeatSet(prev);
}
