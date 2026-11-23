// vr_seat.cpp -- splitscreen seats (vr_seat.hpp; docs/vr-port/SPLITSCREEN.md): the seats, the current one, the
// `seat` command, and vr_seat_test (two seats' state kept apart).
#include "vr_seat.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/GetArraySize.hpp"

#include "vr_avatar.hpp"
#include "vr_engine.hpp"
#include "vr_flatholster.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_grip.hpp"
#include "vr_hands.hpp"
#include "vr_handsource.hpp"
#include "vr_netsnap.hpp"
#include "vr_particles.hpp"
#include "vr_predict.hpp"
#include "vr_testkit.hpp"
#include "vr_throw.hpp"
#include "vr_visor.hpp"


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
    unsigned playing = 1u;                      // the seats in play, a bit each (seat 0's always)
    int pad[maxSeats] = {-1, -1, -1, -1};       // the pad driving each seat 1-3 (-1: none; phase 6's joins)
    clientseat_t net0{};                        // seat 0's client: the game's (CL_Init's, Host_Init's), as before
    za::UniquePtr<clientseat_t> nets[maxSeats]; // seats 1-3's, made on first use (~3 MB each: the entity tables)
} seats; // seat-shared: the seats' owner

// Makes a seat current: its input, keys, view... and its client (cl_seat: client.h's cl, cls and entity tables).
// Phase 7: the body posed for the seat leaving parked in its part, the next seat's brought in (vr_avatar.cpp's globals
// are the current seat's body; one seat: never switched, as before).
void makeCurrent(Seat& s)
{
    if(seats.cur != &s)
    {
        avatar::seatSwitched(*seats.cur, s);
    }
    seats.cur = &s;
    cl_seat = &netOf(s);
}

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
    check(k1->bindings[K_YBUTTON] && !ZA_STRCMP(k1->bindings[K_YBUTTON], "+seattest") &&
              (test.had[0] ? !ZA_STRCMP(k0->bindings[K_YBUTTON], test.saved[0]) : !k0->bindings[K_YBUTTON]),
          "seat 2 bind YBUTTON: seat 1's binding set, seat 0's as it was");
    run("bind YBUTTON \"+seattest\"");
    run("bind XBUTTON \"+seattest\"");
    run("bind F11 \"+seattest\"");
    run("seat 2 bind F11 \"+seattest\"");
    check(!k1->bindings[K_XBUTTON], "bind XBUTTON as seat 1 (index 0): seat 2's XBUTTON still unbound");

    Key_SeatCopyBindings(2, 0);
    check(Key_SeatHasBindings(2) && k2->bindings[K_XBUTTON] && k2->bindings[K_XBUTTON] != k0->bindings[K_XBUTTON] &&
              !ZA_STRCMP(k2->bindings[K_XBUTTON], k0->bindings[K_XBUTTON]),
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

// Phase 3: the view and the HUD. As seat 1 (a Scope): a centre print, +showscores, v_cshift, a hit on the visor and
// the lean command land in seat 1's and leave seat 0's as they were, and the reverse for seat 0's; each seat's window
// area is the whole window (no layout yet); a fresh seat's view starts as Quake's (the contents tint). Every seat's
// state is put back after.
void testViewHud()
{
    Seat& s0 = at(0);
    Seat& s1 = at(1);
    const seatview_t view0 = s0.view, view1 = s1.view;
    const seathud_t hud0 = s0.hud, hud1 = s1.hud;

    check(V_SeatView() == &s0.view && SCR_SeatHud() == &s0.hud && V_SeatViewOf(1) == &s1.view &&
              SCR_SeatHudOf(1) == &s1.hud && &s0.view != &s1.view,
          "the view and HUD accessors: seat 0's current, each seat its own");
    check(at(3).view.contents.destcolor[0] == 130 && at(3).view.contents.destcolor[2] == 50 && at(3).view.contents.percent == 0,
          "a fresh seat's contents tint is Quake's (130 80 50, 0)");
    {
        const Scope in1(s1);
        SCR_CenterPrint("seat two\nsays hello");
        check(ZA_STRCMP(s1.hud.centerstring, "seat two\nsays hello") == 0 && s1.hud.center_lines == 2 &&
                  ZA_STRCMP(s0.hud.centerstring, hud0.centerstring) == 0 && s0.hud.center_lines == hud0.center_lines,
              "a centre print in seat 1: seat 1's text and lines, seat 0's unchanged");
        check(s1.hud.centertime_off > 0.f && s0.hud.centertime_off == hud0.centertime_off, "its time is seat 1's alone");
        SCR_ClearCenterString();
        check(s1.hud.centertime_off == 0.f, "the clear in seat 1 clears seat 1's");
        run("+showscores");
        check(s1.hud.showscores && !s0.hud.showscores, "+showscores in seat 1: seat 1's scores shown, seat 0's not");
        run("-showscores");
        run("v_cshift 10 20 30 40");
        check(s1.view.contents.destcolor[1] == 20 && s1.view.contents.percent == 40 &&
                  s0.view.contents.destcolor[1] == view0.contents.destcolor[1] && s0.view.contents.percent == view0.contents.percent,
              "v_cshift in seat 1: seat 1's contents tint, seat 0's unchanged");
        const int pending0 = [&] {
            const Scope back0(s0);
            return visor::damagePending();
        }();
        const int pending1 = visor::damagePending();
        visor::onDamage(3, 4);
        const int after0 = [&] {
            const Scope back0(s0);
            return visor::damagePending();
        }();
        check(visor::damagePending() == pending1 + 7 && after0 == pending0, "a hit on seat 1's visor: seat 0's has none");
        run("+flatleanright");
        check(flatmove::leanCommand() == 1, "+flatleanright in seat 1: seat 1 leans");
    }
    check(flatmove::leanCommand() == 0, "seat 0 does not lean with seat 1");
    run("+flatleanleft");
    check(flatmove::leanCommand() == -1 && [&] {
        const Scope in1(s1);
        return flatmove::leanCommand() == 1;
    }(), "+flatleanleft in seat 0: seat 0's, seat 1's still right");
    run("-flatleanleft");
    {
        const Scope in1(s1);
        run("-flatleanright");
        V_ResetEffects();
        check(s1.view.contents.percent == 0 && s0.view.contents.percent == view0.contents.percent,
              "V_ResetEffects in seat 1 resets seat 1's view only");
    }
    vrect_t r0{}, r1{};
    SCR_SeatRect(0, &r0);
    SCR_SeatRect(1, &r1);
    check(r0.x == 0 && r0.y == 0 && r1.x == r0.x && r1.y == r0.y && r1.width == r0.width && r1.height == r0.height,
          "every seat's window area is the whole window (layouts undecided)");
    // (seat 1's visor keeps its 7 pending: its frame runs only for a seat in play)
    s0.view = view0;
    s1.view = view1;
    s0.hud = hud0;
    s1.hud = hud1;
}

// Phase 4a: flat play. A fresh seat's flat play starts as the mode's defaults; as seat 1 (a Scope) +flatcrouch,
// +flatholsters and the ironsight's base field of view land in seat 1's and leave seat 0's as they were; seat 1
// letting go leaves seat 0's held; each seat its own parts, made only where its code ran. Seat 0's left as it was.
void testFlat()
{
    Seat& s0 = at(0);
    Seat& s1 = at(1);
    const auto slot = [](Seat& s, const Part p) { return s.parts[static_cast<int>(p)].get(); };
    const float fov0 = flatsight::hipFov();
    const bool crouch0 = flatplay::crouchCommand(), holster0 = flatholster::holdCommand();
    {
        const Scope in2(at(2));
        check(!flatplay::crouchCommand() && !flatholster::holdCommand() && flatsight::hipFov() == 90.f &&
                  flatplay::crouchRatio() == 0.f && flatplay::proneRatio() == 0.f,
              "a fresh seat's flat play: no crouch or holster command, the sight's base FOV 90, standing");
    }
    {
        const Scope in1(s1);
        run("+flatcrouch");
        run("+flatholsters");
        (void)flatsight::fov(55.f);
        check(flatplay::crouchCommand() && flatholster::holdCommand() && flatsight::hipFov() == 55.f,
              "as seat 1: +flatcrouch, +flatholsters and the sight's base FOV are seat 1's");
    }
    check(flatplay::crouchCommand() == crouch0 && flatholster::holdCommand() == holster0 && flatsight::hipFov() == fov0,
          "seat 0's crouch and holster commands and its sight unchanged by seat 1's");
    run("+flatcrouch");
    {
        const Scope in1(s1);
        run("-flatcrouch");
        run("-flatholsters");
        check(!flatplay::crouchCommand() && !flatholster::holdCommand(), "seat 1 lets go of its own");
    }
    check(flatplay::crouchCommand(), "seat 0's +flatcrouch still held after seat 1's -flatcrouch");
    run("-flatcrouch");
    check(flatplay::crouchCommand() == crouch0, "seat 0 lets go of its own");
    bool apart = true;
    for(const Part p : {Part::FlatPlay, Part::FlatHolster, Part::FlatSight})
    {
        apart = apart && slot(s0, p) != nullptr && slot(s1, p) != nullptr && slot(s0, p) != slot(s1, p);
    }
    bool none3 = true;
    for(int p = static_cast<int>(Part::FlatPlay); p < static_cast<int>(Part::Count); ++p)
    {
        // (the parked body excepted: any switch to and from the seat parks it, avatar::seatSwitched)
        none3 = none3 && (slot(at(3), static_cast<Part>(p)) == nullptr || static_cast<Part>(p) == Part::AvatarSelf);
    }
    check(apart, "flat play, the holsters and the sight: each seat's own part");
    check(none3, "seat 3 has none of flat play's parts made (no code of it ran as seat 3)");
}

// Phase 4b: the hands. As seat 1 (a Scope) the play space's turn (the rig), a hand's source this frame, its measured
// frame (the grip's) and a throw's motion sample land in seat 1's and leave seat 0's as they were; each seat its own
// parts, made only where its code ran.
void testHands()
{
    Seat& s0 = at(0);
    Seat& s1 = at(1);
    const auto slot = [](Seat& s, const Part p) { return s.parts[static_cast<int>(p)].get(); };
    constexpr int hand = 1;
    const float yaw0 = hands::playSpaceYaw();
    const handsource::Source source0 = handsource::source(hand);
    const float radius0 = grip::ownFrame(hand).channelRadius;
    const double thrown0 = throwing::latestTime(hand);
    {
        const Scope in1(s1);
        check(hands::playSpaceYaw() == 0.f && handsource::source(hand) == handsource::Source::Tracked &&
                  grip::ownFrame(hand).channelRadius == 0.f && throwing::latestTime(hand) == 0.0,
              "a fresh seat's hands: no turn, tracked, no measured frame, no throw's samples");
        hands::setPlaySpaceYaw(10.f);
        handsource::mark(hand, handsource::Source::Action, "seat test");
        grip::HandFrame f = grip::ownFrame(hand);
        f.channelRadius = radius0 + 7.f;
        grip::setHandFrame(hand, f);
        throwing::sample(hand, thrown0 + 12345.0, throwing::Motion{}, throwing::Motion{});
        check(hands::playSpaceYaw() == 10.f && handsource::source(hand) == handsource::Source::Action &&
                  grip::ownFrame(hand).channelRadius == radius0 + 7.f && throwing::latestTime(hand) == thrown0 + 12345.0,
              "as seat 1: the play space's turn, the hand's source, its frame and a throw's sample are seat 1's");
    }
    check(hands::playSpaceYaw() == yaw0 && handsource::source(hand) == source0 &&
              grip::ownFrame(hand).channelRadius == radius0 && throwing::latestTime(hand) == thrown0,
          "seat 0's turn, hand source, frame and throw samples unchanged by seat 1's");
    bool apart = true;
    for(const Part p : {Part::Hands, Part::HandSource, Part::Grip, Part::Throw})
    {
        apart = apart && slot(s0, p) != nullptr && slot(s1, p) != nullptr && slot(s0, p) != slot(s1, p);
    }
    bool none3 = true;
    for(int p = static_cast<int>(Part::Hands); p < static_cast<int>(Part::Count); ++p)
    {
        // (the parked body excepted: any switch to and from the seat parks it, avatar::seatSwitched)
        none3 = none3 && (slot(at(3), static_cast<Part>(p)) == nullptr || static_cast<Part>(p) == Part::AvatarSelf);
    }
    check(apart, "the rig, the hands' sources, frames and throws: each seat's own part");
    check(none3, "seat 3 has none of the hands' parts made (no code of them ran as seat 3)");
}

// Phase 5: each seat its own client. cl, cls and the entity tables follow the current seat (cl_seat: a scope, C's
// CL_SeatSet); a fresh seat's is disconnected, its message buffer its own, its client world empty; what seat 1 writes
// leaves seat 0's as it was; the VR client's parts (the prediction's totals, the snapshots' interpolation) each seat's.
void testNet()
{
    Seat& s0 = at(0);
    Seat& s1 = at(1);
    const auto slot = [](Seat& s, const Part p) { return s.parts[static_cast<int>(p)].get(); };
    clientseat_t* const n0 = &netOf(s0);
    check(cl_seat == n0 && &cl == &n0->cstate && &cls == &n0->cstatic, "seat 0's client is the current one");
    const cactive_t state0 = cls.state;
    const int msg0 = cls.message.cursize;
    const double time0 = cl.time;
    const int vis0 = cl_numvisedicts;
    const float radius0 = cl_dlights[0].radius;
    entity_t* const ents0 = cl_entities;
    za::U64 n = 0;
    double sum = 0.0;
    predict::correctionTotals(n, sum);
    (void)netsnap::interpActive();
    {
        const Scope in1(s1);
        clientseat_t* const n1 = &netOf(s1);
        check(cl_seat == n1 && n1 != n0 && &cl == &n1->cstate && &cl_dlights[0] == &n1->dlights[0],
              "a scope makes seat 1's client current");
        check(cls.state == ca_disconnected && cls.demonum == -1 && cls.message.data == n1->messagebuf &&
                  cls.message.maxsize == static_cast<int>(sizeof(n1->messagebuf)) && !cls.netcon && cls.signon == 0,
              "a fresh seat's client: disconnected, out of the demo loop, its own message buffer");
        check(cl_entities == nullptr && cl_numvisedicts == 0 && cl.time == 0.0 && cl.worldmodel == nullptr,
              "a fresh seat's client world: empty");
        cl.time = time0 + 123.0;
        cl_numvisedicts = vis0 + 5;
        cl_dlights[0].radius = radius0 + 9.f;
        MSG_WriteByte(&cls.message, clc_nop);
        za::U64 n1c = 1;
        double s1c = 1.0;
        predict::correctionTotals(n1c, s1c);
        check(n1c == 0 && s1c == 0.0 && !netsnap::interpActive(),
              "seat 1's prediction totals and interpolation: its own, empty");
        const int was = CL_SeatSet(0);
        check(cl_seat == n0 && cl.time == time0, "CL_SeatSet(0) makes seat 0's client current (C's way)");
        CL_SeatSet(was);
        check(cl_seat == n1 && cl.time == time0 + 123.0 && cls.message.cursize == 1, "... and seat 1's again after");
        cl.time = 0.0; // (seat 1's left fresh: the test runs again the same)
        cl_numvisedicts = 0;
        cl_dlights[0].radius = 0.f;
        SZ_Clear(&cls.message);
    }
    check(cl_seat == n0 && cls.state == state0 && cls.message.cursize == msg0 && cl.time == time0 &&
              cl_numvisedicts == vis0 && cl_dlights[0].radius == radius0 && cl_entities == ents0,
          "seat 0's client unchanged by seat 1's writes (state, message, time, visible entities, lights, entities)");
    bool apart = true;
    for(const Part p : {Part::PredictTotals, Part::NetSnapInterp})
    {
        apart = apart && slot(s0, p) != nullptr && slot(s1, p) != nullptr && slot(s0, p) != slot(s1, p);
    }
    check(apart, "the prediction's totals and the snapshots' interpolation: each seat's own part");
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
    testViewHud();
    testFlat();
    testHands();
    testNet();
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
    int n = 0;
    for(int i = 0; i < maxSeats; i++)
    {
        n += plays(i) ? 1 : 0;
    }
    return n;
}

void setInPlay(const int n)
{
    const int k = n < 1 ? 1 : n > maxSeats ? maxSeats : n;
    seats.playing = (1u << k) - 1u;
    for(int i = 0; i < maxSeats; i++)
    {
        seats.pad[i] = i > 0 && i < k ? i - 1 : -1;
    }
}

bool plays(const int i)
{
    return i >= 0 && i < maxSeats && (seats.playing & (1u << i)) != 0;
}

int padOf(const int i)
{
    return i > 0 && i < maxSeats ? seats.pad[i] : -1;
}

void setPlays(const int i, const bool on, const int pad)
{
    if(i <= 0 || i >= maxSeats)
    {
        return; // (seat 0, the keyboard and mouse, always plays)
    }
    seats.playing = on ? seats.playing | (1u << i) : seats.playing & ~(1u << i);
    seats.pad[i] = on ? pad : -1;
    closeMenu(i); // (phase 9: a seat comes in or goes out with its menu shut)
}

clientseat_t* netIfMade(const int i)
{
    return i == 0 ? &seats.net0 : i > 0 && i < maxSeats ? seats.all[i].net : nullptr;
}

clientseat_t& netOf(Seat& s)
{
    if(!s.net)
    {
        if(s.index == 0)
        {
            s.net = &seats.net0;
        }
        else
        {
            seats.nets[s.index] = za::makeUnique<clientseat_t>();
            s.net = seats.nets[s.index].get();
            CL_SeatNetReady(s.net);
        }
    }
    return *s.net;
}

Scope::Scope(Seat& s) : prev_(seats.cur)
{
    makeCurrent(s);
}

Scope::~Scope()
{
    makeCurrent(*prev_);
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

extern "C" seatview_t* V_SeatView()
{
    return &qvr::seat::current().view;
}

extern "C" seatview_t* V_SeatViewOf(const int seat)
{
    return &qvr::seat::at(seat).view;
}

extern "C" seathud_t* SCR_SeatHud()
{
    return &qvr::seat::current().hud;
}

extern "C" seathud_t* SCR_SeatHudOf(const int seat)
{
    return &qvr::seat::at(seat).hud;
}

extern "C" int CL_SeatIndex()
{
    return qvr::seat::current().index;
}

extern "C" int CL_SeatSet(const int seat)
{
    const int was = CL_SeatIndex();
    qvr::seat::makeCurrent(qvr::seat::at(seat));
    return was;
}

extern "C" clientseat_t* CL_SeatNetOf(const int seat)
{
    return &qvr::seat::netOf(qvr::seat::at(seat));
}

// The current seat's client (client.h's cl, cls, entity tables): seat 0's until a scope moves it (constant-initialized:
// the game's C code reads it from its first frame).
extern "C" clientseat_t* cl_seat = &qvr::seat::seats.net0; // seat-shared: which seat's client is current

extern "C" int CL_SeatCount()
{
    return qvr::seat::maxSeats;
}

extern "C" int CL_SeatsInPlay()
{
    return qvr::seat::inPlay();
}

extern "C" int CL_SeatPlays(const int seat)
{
    return qvr::seat::plays(seat) ? 1 : 0;
}

extern "C" int CL_SeatPad(const int seat)
{
    return qvr::seat::padOf(seat);
}

extern "C" qboolean CL_SeatHeadless()
{
    return qvr::seat::current().index != 0;
}

extern "C" qboolean CL_SeatDrawn()
{
    return qvr::seat::current().index == 0 || SCR_SeatsDrawn() > 1;
}

namespace
{
// The events the seats fed to the shared effects this frame (CL_SeatEffectFirst; with tiles drawn only).
struct EffectFed
{
    int kind = -1, key = 0, seat = 0, frame = -1;
    int org[3] = {};
};
constexpr int effectsRemembered = 512;
struct EffectsFed
{
    EffectFed ring[effectsRemembered];
    int next = 0;
} effectsFed; // seat-shared: the shared effects' feed, across the seats
} // namespace

extern "C" qboolean CL_SeatEffectFirst(const int kind, const int key, const float* org)
{
    if(!CL_SeatDrawn())
    {
        return false;
    }
    if(SCR_SeatsDrawn() <= 1)
    {
        return true;
    }
    const int seat = qvr::seat::current().index;
    int at[3] = {};
    for(int i = 0; org && i < 3; i++)
    {
        at[i] = static_cast<int>(org[i] < 0.f ? org[i] - 0.5f : org[i] + 0.5f);
    }
    for(const EffectFed& e : effectsFed.ring)
    {
        if(e.frame == host_framecount && e.kind == kind && e.key == key && e.seat != seat && e.org[0] == at[0] &&
            e.org[1] == at[1] && e.org[2] == at[2])
        {
            return false;
        }
    }
    EffectFed& slot = effectsFed.ring[effectsFed.next];
    effectsFed.next = (effectsFed.next + 1) % effectsRemembered;
    slot = EffectFed{kind, key, seat, host_framecount, {at[0], at[1], at[2]}};
    return true;
}

namespace
{
// The server events (their bytes) the drawn seats fed this frame, with their lengths (phase 8: CL_SeatFxBegin looks
// ahead for one of them).
struct EventFed
{
    int frame = -1, seat = 0, length = 0;
    unsigned int hash = 0;
};
constexpr int eventsRemembered = 256;
struct EventsFed
{
    EventFed ring[eventsRemembered];
    int next = 0;
    int repeat = 0; // the parse under way: a repeat's length (CL_SeatFxRepeat)
} eventsFed; // seat-shared: the events' feed, across the seats

// The bytes [from, to) of the message (FNV-1a): the event, the same in every seat's client that reads it.
[[nodiscard]] unsigned int eventHash(const int from, const int to)
{
    unsigned int h = 2166136261u;
    for(int i = from; i < to; i++)
    {
        h = (h ^ net_message.data[i]) * 16777619u;
    }
    return h;
}
} // namespace

extern "C" void CL_SeatFxBegin(seatfxmark_t* mark, const int readcount)
{
    mark->on = SCR_SeatsDrawn() > 1 && CL_SeatDrawn() ? 1 : 0;
    mark->readcount = readcount;
    mark->vrparticles = mark->on ? qvr::particles::liveCount() : 0;
    mark->particles = mark->on ? R_ParticleCount() : 0;
    mark->prevrepeat = eventsFed.repeat;
    mark->repeat = 0;
    if(mark->on && readcount >= 0)
    {
        // A repeat: the message goes on with the bytes of an event another drawn seat fed this frame (the parse reads as
        // many: the same bytes, the same fields).
        const int seat = qvr::seat::current().index;
        for(const EventFed& e : eventsFed.ring)
        {
            if(e.frame == host_framecount && e.seat != seat && e.length > 0 && readcount + e.length <= net_message.cursize &&
                eventHash(readcount, readcount + e.length) == e.hash)
            {
                mark->repeat = e.length;
                break;
            }
        }
    }
    eventsFed.repeat = mark->repeat;
}

extern "C" int CL_SeatFxRepeat()
{
    return eventsFed.repeat;
}

extern "C" void CL_SeatFxEnd(const seatfxmark_t* mark)
{
    eventsFed.repeat = mark->prevrepeat;
    if(!mark->on || mark->readcount < 0 || msg_readcount <= mark->readcount || msg_readcount > net_message.cursize)
    {
        return;
    }
    const unsigned int h = eventHash(mark->readcount, msg_readcount);
    if(!CL_SeatEffectFirst(SEATFX_VR, static_cast<int>(h), nullptr))
    {
        qvr::particles::truncate(mark->vrparticles);
        R_ParticleTruncate(mark->particles);
    }
    else if(!mark->repeat)
    {
        EventFed& slot = eventsFed.ring[eventsFed.next];
        eventsFed.next = (eventsFed.next + 1) % eventsRemembered;
        slot = EventFed{host_framecount, qvr::seat::current().index, msg_readcount - mark->readcount, h};
    }
}

extern "C" int CL_SeatListeners(vec3_t org[MAX_SPLITSEATS], vec3_t right[MAX_SPLITSEATS], int ent[MAX_SPLITSEATS])
{
    if(SCR_SeatsDrawn() <= 1)
    {
        return 0;
    }
    int n = 0;
    for(int s = 0; s < qvr::seat::maxSeats && n < MAX_SPLITSEATS; s++)
    {
        const clientseat_t* c = qvr::seat::plays(s) ? qvr::seat::netIfMade(s) : nullptr;
        const seatview_t* v = V_SeatViewOf(s);
        if(!c || c->cstatic.signon != SIGNONS || v->drawnframes <= 0)
        {
            continue;
        }
        VectorCopy(v->drawnorg, org[n]);
        VectorCopy(v->drawnright, right[n]);
        ent[n] = c->cstate.viewentity;
        n++;
    }
    return n;
}

int qvr::seat::seatOfPlayer(const int ent)
{
    if(ent <= 0 || SCR_SeatsDrawn() <= 1)
    {
        return -1;
    }
    for(int s = 0; s < maxSeats; s++)
    {
        const clientseat_t* c = plays(s) ? netIfMade(s) : nullptr;
        if(c && c->cstatic.signon == SIGNONS && c->cstate.viewentity == ent)
        {
            return s;
        }
    }
    return -1;
}

extern "C" clientseat_t* CL_SeatNetIfMade(const int seat)
{
    return qvr::seat::netIfMade(seat);
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

namespace
{
// split11: a controller seat's own layout for flat play, whose own keys (vr_flat_key_*: the holster menu, reload, swap,
// crouch, ...) are the keyboard's and the mouse's (vr_seat.hpp, PadAction). The gamepad keys are the VR controllers'
// too (quakevr/vr_bindings.cfg: the grips on the shoulders, the main stick's up and down swimming): a button seat 1's
// copy has unbound or as the VR controllers' (or default.cfg's) binding, `stock`, takes the flat command; a button he
// bound himself keeps his.
constexpr qvr::seat::PadAction padDefaults[] = {
    {K_RTRIGGER, "+attack", nullptr, "Fire (right)"},              // (the left mouse button)
    {K_LTRIGGER, "+offhandattack", "+jump", "Left hand"},          // (the right mouse button; default.cfg's +jump)
    {K_ABUTTON, "+jump", nullptr, "Jump"},
    {K_BBUTTON, "+flatcrouch", "impulse 10", "Crouch"},            // (Ctrl)
    {K_XBUTTON, "impulse 55", "+reloadoff", "Reload"},             // (R)
    {K_YBUTTON, "impulse 48", "impulse 12", "Swap hands"},         // (3)
    {K_RSHOULDER, "impulse 10", "+grabmain", "Next weapon"},       // (the wheel; a flat hand is always closed)
    {K_LSHOULDER, "impulse 12", "+graboff", "Prev weapon"},
    {K_DPAD_UP, "+flatholsters", "+moveup", "Holster menu"},       // (mouse 3: the weapon wheel; the look stick points)
    {K_DPAD_DOWN, "impulse 47", "+movedown", "Drop"},              // (H)
    {K_DPAD_LEFT, "impulse 46", nullptr, "Force grab"},            // (mouse 4)
    {K_DPAD_RIGHT, "vr_flashlight_toggle", nullptr, "Flashlight"}, // (F)
    {K_LTHUMB, "+speed", nullptr, "Run"},                          // (Shift)
    {K_RTHUMB, "vr_flat_punch main", "+reloadmain", "Punch"},      // (V)
    // (Back is Tab, the scores, and Start the seat's menu: Ironwail maps them, no binding)
};

// The current seat's bindings: the controller layout where a button is unbound or stock.
void applyPadDefaults()
{
    for(const qvr::seat::PadAction& d : padDefaults)
    {
        const char* b = Key_Seat()->bindings[d.key];
        if(!b || !b[0] || (d.stock && !ZA_STRCMP(b, d.stock)))
        {
            Key_SetBinding(d.key, d.command);
        }
    }
}
} // namespace

const qvr::seat::PadAction* qvr::seat::padLayout(int& count)
{
    count = static_cast<int>(za::getArraySize(padDefaults));
    return padDefaults;
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
    if(to > 0)
    {
        applyPadDefaults(); // (a controller's seat: split11's layout where seat 1's leaves a button free)
    }
    CL_SeatSet(prev);
}
