// vr_seat.cpp -- splitscreen seats (vr_seat.hpp; docs/vr-port/SPLITSCREEN.md): the seats, the current one, and
// vr_seat_test (two seats' state kept apart).
#include "vr_seat.hpp"

#include "vr_engine.hpp"

namespace qvr::seat
{

namespace
{

// seat-shared: the seats themselves (each seat's state is its own; this is their owner) and which one is current
struct Seats
{
    Seat all[maxSeats] = {{0}, {1}, {2}, {3}};
    Seat* cur = &all[0];
} seats;

// vr_seat_test: in seat 1 (a Scope), the +commands and the impulse land in seat 1's buttons and none of seat 0's, and
// the reverse; the menu's release (CL_ReleaseMoveButtons) lets go of the current seat's only; scopes nest. Everything
// it presses it lets go of. Verdict: "seat_test: N passed, M failed".
void test_f()
{
    int passed = 0;
    int failed = 0;
    const auto check = [&](const bool ok, const char* what) {
        ok ? ++passed : ++failed;
        Con_Printf("seat_test: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    const auto run = [](const char* text) { Cmd_ExecuteString(text, src_command); };
    const auto held = [](const kbutton_t& b) { return (b.state & 1) != 0; };
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
    Con_Printf("seat_test: %d passed, %d failed\n", passed, failed);
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
    return 1;
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
    Cmd_AddCommand("vr_seat_test", test_f);
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
