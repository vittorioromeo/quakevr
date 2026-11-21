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
// Phase 3: the view (seatview_t, client.h: view.c's punch, kick, blend, smoothing; the window area the seat is drawn
// in, the whole window until the layouts) and the HUD (seathud_t: the centre print, the scores shown); the VR files'
// view and HUD state (the visor, the flat crosshair, hit marks, speed FOV, the melee HUD, the death view, the lean and
// the slide's view, the net HUD) as the seat's parts (below).
#pragma once

#include "vr_engine.hpp"

#include "Zancle/Vocabulary/UniquePtr.hpp"

namespace qvr::seat
{

inline constexpr int maxSeats = 4;

// A subsystem's per-seat state whose type only its own file knows (its struct stays private there): the seat owns it,
// made value-initialised on first use, destroyed with the seat. The file reaches it through part<T>(seat, Part::X),
// for the current seat through its own one-line accessor (e.g. vr_visor.cpp's `state()`). One enumerator a subsystem.
enum class Part
{
    Visor,     // vr_visor.cpp: the flat visor HUD's sway, effects, centre-print popup, panels
    FlatXhair, // vr_flatxhair.cpp: the shot line and aim marks drawn
    FlatHit,   // vr_flathit.cpp: the marks of where hits came from
    SpeedFov,  // vr_flatspeedfov.cpp: the speed FOV's widening
    MeleeHud,  // vr_meleehud.cpp: the parry window's glow and embers
    DeathView, // vr_deathview.cpp: the view in the ragdoll's head
    FlatView,  // vr_flatmove.cpp: the flat camera's lean, the slide's view, the view last seen
    NetHud,    // vr_nethud.cpp: the connection's numbers shown
    Crosshair, // vr_crosshair.cpp: the flat crosshair's queued frame
    Count
};

struct PartBase
{
    virtual ~PartBase() = default;
};

template <typename T>
struct PartOf final : PartBase
{
    T value{};
};

[[nodiscard]] inline seatview_t freshView()
{
    seatview_t v{};
    v.contents = {{130, 80, 50}, 0}; // view.c's cshift_empty
    return v;
}

struct Seat
{
    int index = 0;
    seatinput_t input{}; // the +command buttons and the impulse (cl_input.c; C: CL_SeatInput)
    seatkeys_t keys{};   // the keys held, the bindings, the mouse's motion (keys.c, in_sdl.c; C: Key_Seat)
    seatview_t view = freshView(); // the view's state and window area (view.c, gl_screen.c; C: V_SeatView)
    seathud_t hud{};     // the centre print, the scores shown (gl_screen.c, sbar.c; C: SCR_SeatHud)
    za::UniquePtr<PartBase> parts[static_cast<int>(Part::Count)]; // the VR files' own (part<T>)
};

template <typename T>
[[nodiscard]] T& part(Seat& s, const Part p)
{
    za::UniquePtr<PartBase>& slot = s.parts[static_cast<int>(p)];
    if(!slot)
    {
        slot = za::makeUnique<PartOf<T>>();
    }
    return static_cast<PartOf<T>&>(*slot).value;
}

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
