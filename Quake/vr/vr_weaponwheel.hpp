// vr_weaponwheel.hpp -- the headset's weapon wheel (his design, 2026-11-23; docs/vr-port/ROUND25.md, "Weapon wheel").
//
// Hold the wheel's button (vr_weaponwheel_button: the upper face button, B or Y, by default) on either hand for
// vr_weaponwheel_hold seconds: a wheel of choices opens round that hand (vr_weaponwheel_size units wide, its middle
// vr_weaponwheel_distance beyond the hand, facing the eyes), time slows to vr_weaponwheel_timescale (bullet time's slow
// motion, as the flat holster menu's), and that hand is the cursor: moved out from the middle towards a slot, the slot
// lights (a haptic tick on each new one). Let go of the button there: that hand takes the choice. Let go in the middle or
// outside the wheel: nothing. Its look is the flat holster menu's own image (gadget::renderHolsterMenu: the screens'
// palette, font, thumbnails and CRT face: flatholster::drawImage) on a quad in the world.
//
// What the wheel offers is a Provider's (the extension point): a list of Choices for the hand this frame and what a pick
// does. Today's is the holsters' (holsterProvider(): flatholster::holsterSlot, the holster menu's own model; a pick is
// the client command "vr_wheel_pick <hand> <slot>", QC VR_Wheel_Pick, by the holster menu's steps). Another provider (say
// every weapon the player may take) is a list and a pick of its own given to setProvider(): the wheel's layout, look,
// input and tests take any count up to maxChoices, in its order, clockwise from the top.

#pragma once

#include "vr_hands.hpp"

namespace qvr::weaponwheel
{

// One slot of the wheel.
struct Choice
{
    int key{-1};       // the provider's own (the holsters': the holster 0 .. 5, -1 the empty hand)
    char title[20]{};  // its slot's title ("L SHOULDER")
    char name[40]{};   // what is there ("shotgun"; "" nothing: "- EMPTY -")
    char model[64]{};  // its model's file (the thumbnail; "" none)
    bool usable{true}; // a pick would do something now (else greyed, and a pick does nothing)
};
constexpr int maxChoices = 16;

struct Provider
{
    const char* name;
    // This frame's choices for hand `hand` (0 the off hand, the left; 1 the main) into `out` (at most `max`), in the
    // wheel's order (clockwise from the top): how many.
    int (*list)(int hand, Choice* out, int max);
    // Hand `hand` let go on `c` (one the list gave this frame).
    void (*pick)(int hand, const Choice& c);
};
// The holsters (each holster's weapon, and the empty hand): the default.
[[nodiscard]] const Provider& holsterProvider();
// The provider the wheel uses from its next opening (nullptr: holsterProvider()).
void setProvider(const Provider* p);

// The controller button that opens it (vr_weaponwheel_button): 0 the upper face button, 1 the stick's click, 2 the lower
// face button.
[[nodiscard]] int buttonKind();
// How long it is held first (vr_weaponwheel_hold, seconds).
[[nodiscard]] double holdSeconds();
// Hand `hand` may open it now: on, a headset's game (not flatscreen play), alive, no menu, and its button free (not the
// grappling hook's reel in that hand, nor vr_menu_button's hold on it, nor the toolgun's hand).
[[nodiscard]] bool canOpen(int hand);
// Its button held vr_weaponwheel_hold on `hand` (vr_input.cpp, which then lets the game's key go): it opens there.
void open(int hand);
// That button let go (vr_input.cpp): the lit slot picked for that hand, or nothing.
void release(int hand);

[[nodiscard]] bool isOpen();
// The time scale it asks for (timescale::wanted): vr_weaponwheel_timescale while open, else 1.
[[nodiscard]] float timeScale();

// The host frame, with this frame's hands: the cursor, the lit slot, its tick; the fade; vr_weaponwheel_test's steps.
void frame(const hands::State& s);
// The 2D pass's end (VR_End2D): its image. And its quad in the eye being drawn.
void render();
void drawInEye(const hands::State& s);

// vr_wheel_pick (the server's), vr_weaponwheel_dump, vr_weaponwheel_test.
void registerCommands();

} // namespace qvr::weaponwheel
