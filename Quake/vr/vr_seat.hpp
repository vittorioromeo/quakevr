// vr_seat.hpp -- splitscreen seats (docs/vr-port/SPLITSCREEN.md, "Design"): one Seat per local player, up to four,
// owning that player's client state, one struct per subsystem. What a player has his own of moves out of file scope
// into his seat, a subsystem group a phase; what all players share (models, textures, sound buffers, the server)
// stays where it is, marked shared (qvr_checks seat_state).
//
// The access rule: code reaches a seat's state through the seat it runs for. Where the call depth is shallow the seat
// is passed down (`Seat&`); where it is not (console commands from binds, the engine's C frame, deep VR code) the seat
// loop makes the seat current (seat::Scope) and the code reads seat::current(). SPLITSCREEN.md's per-subsystem table
// says which and why. Until the seat loop (phase 6) there is one seat in play: seat 0, always current.
//
// Phase 1: the command buttons and the impulse (seatinput_t, client.h: cl_input.c's in_forward... globals).
// Phase 2: the keys held, the bindings, the alt modifier and gyro buttons, the mouse's motion (seatkeys_t, keys.h);
// the devices: the keyboard and mouse are seat 0's, game controllers join as seats 1-3 (in_sdl.c's pads); the
// commands a seat's binds queue run as that seat (cmd.c's seat markers); `seat <n> <command>` runs one as seat n.
#pragma once

#include "vr_engine.hpp"

namespace qvr::seat
{

inline constexpr int maxSeats = 4;

struct Seat
{
    int index = 0;
    seatinput_t input{}; // the +command buttons and the impulse (cl_input.c; C: CL_SeatInput)
    seatkeys_t keys{};   // the keys held, the bindings, the mouse's motion (keys.c, in_sdl.c; C: Key_Seat)
};

Seat& current();   // the seat the running code is for (seat 0 outside a seat loop)
Seat& at(int i);   // seat i, 0..maxSeats-1 (clamped)
int inPlay();      // seats in play: 1 until the seat loop (phase 6)
void setInPlay(int n); // 1..maxSeats: the seat loop's (phase 6); today vr_seat_test's, which sets it back

// Makes a seat current for its lifetime, the previous one again after it (the seat loop, the tests).
class Scope
{
public:
    explicit Scope(Seat& s);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Seat* prev_;
};

void registerCommands(); // vr_seat_test

} // namespace qvr::seat
