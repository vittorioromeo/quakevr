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
// input and tests take any count up to maxChoices, in its order, clockwise from the top (the layout spaces ten).
//
// The holsters' provider also offers three actions for the hand (his words 2026-11-23, -24): DROP (what it holds dropped,
// as 6 / 7 and the holster menu's DROP), FLIP GRIP (its sword's, dagger's or Crystal Lance's grip turned over, as the eject
// button and the holster menu's FLIP GRIP) and UNLOAD (its gun's live rounds back into the reserve, the holster menu's
// UNLOAD), greyed when they would do nothing (flatholster::canDrop / canFlip / canUnload, the holster menu's own tests).
// And (wheelgrenade, his words 2026-11-28) the back pouch's grenade, after UNLOAD (with the pouch on: vr_handgrenade): a
// slot with the grenade's thumbnail and how many are left; split into two halves (GRENADE on top, CLUSTER below) when
// both rockets and multi-rockets are carried; greyed with neither. Eleven places on the ring then (an oval, the image
// 308 x 318: vr_weaponwheel_size is the width of its first 280 pixels, as the ten-slot wheel's was).
//
// Flatscreen play (vr_flat_wheel, his words 2026-11-23): the same wheel, its image and provider; only the input, the
// cursor and the placement are flat's. Holding 1 / 2 (vr_flat_key_cycle_left / _right) vr_flat_wheel_hold seconds opens it
// for the left / right hand (vr_flatplay.cpp pollHandKeys: a tap still cycles, when let go), drawn on the window where that
// hand is on the screen (vr_flat_wheel_size of its height); the mouse is the cursor (flatMouse: the view stays, as under
// the holster menu); letting go of the key picks the lit slot, or nothing off the wheel; Esc closes it.

#pragma once

#include "vr_hands.hpp"

namespace qvr::gadget
{
enum class MenuIcon : int; // (vr_gadget.hpp)
}

namespace qvr::weaponwheel
{

// One slot of the wheel.
struct Choice
{
    int key{-1};       // the provider's own (the holsters': the holster 0 .. 5, -1 the empty hand, keyDrop, keyUnload, keyFlip)
    char title[20]{};  // its slot's title ("L SHOULDER")
    char name[40]{};   // what is there ("shotgun"; "" nothing: "- EMPTY -")
    char model[64]{};  // its model's file (the thumbnail; "" none)
    bool usable{true}; // a pick would do something now (else greyed, and a pick does nothing)
    // Its gun's ammunition and attachment (wheelui: the line under its name, the thumbnail's attachment and the name's
    // glyph; flatholster::HolsterSlot's): magazine and size (0 none), QC ammo type, reserve (-1 none), attach::Kind.
    int clip{0};
    int clipSize{0};
    int ammoType{0};
    int reserve{-1};
    int attach{0};
    // A split slot (wheelgrenade: the grenade entry with both kinds): 1 its top half, 2 its bottom half (the next choice in
    // the list, at the same place on the ring); 0 a whole slot.
    int half{0};
    // An action (the holsters': EMPTY HAND, DROP, FLIP GRIP, UNLOAD; his words 2026-11-23): drawn smaller, as the holster menu's
    // buttons, this icon above its title (name and model unused). None (the default): a slot.
    gadget::MenuIcon icon{};
};
constexpr int maxChoices = 16;
// The holsters' provider's actions (QC VR_Wheel_Pick's VR_WHEEL_DROP, VR_WHEEL_UNLOAD, VR_WHEEL_FLIP).
constexpr int keyDrop = -2;
constexpr int keyUnload = -3;
constexpr int keyFlip = -4;
// Its grenade entry (wheelgrenade, his words 2026-11-28; QC VR_WHEEL_GRENADE, VR_WHEEL_MULTIGRENADE): a grenade, or the
// multi-grenade (CLUSTER), from the back pouch into the hand (VR_Wheel_Grenade: its weapon put away first). Both in the
// pouch: the entry split in two halves, the grenade on top.
constexpr int keyGrenade = -5;
constexpr int keyMultiGrenade = -6;
// One kind of the back pouch's grenades for hand `hand` (0 the left): the pouch on (vr_handgrenade), how many are left,
// and whether a pick does something (some left, or that kind in the hand: picked, it goes back; the other kind in the
// hand: swapped). The wheel's BACK POUCH and the holster menu's (flatmenu3) read it; pickPouch sends the wheel's pick.
struct PouchKind
{
    bool on{false};
    int left{0};
    bool usable{false};
};
[[nodiscard]] PouchKind pouchKind(int hand, bool multi);
void pickPouch(int hand, bool multi);

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
// Flatscreen play: how long 1 / 2 is held first (vr_flat_wheel_hold, seconds).
[[nodiscard]] double flatHoldSeconds();
// Hand `hand` may open it now: alive, no menu; a headset's game: on (vr_weaponwheel), its button free (not the grappling
// hook's reel in that hand, nor vr_menu_button's hold on it, nor the toolgun's hand); flatscreen play: vr_flat_wheel and
// the holsters on, neither the holster menu nor the toolgun's panel up.
[[nodiscard]] bool canOpen(int hand);
// A press of its button on `hand` is kept from the server while the hold may still open it (vr_input.cpp WheelHold):
// it may open there and that hand holds a weapon, whose press edge would act on it first (a magazine's eject, the super
// shotgun broken open, the revolver's cylinder swung out). An empty hand's goes at once (the pouch's multi-grenade).
[[nodiscard]] bool holdsButtonBack(int hand);
// Its button held vr_weaponwheel_hold on `hand` (vr_input.cpp, which then lets the game's key go), or 1 / 2 held in
// flatscreen play (vr_flatplay.cpp): it opens there.
void open(int hand);
// That button (key) let go: the lit slot picked for that hand, or nothing.
void release(int hand);

[[nodiscard]] bool isOpen();
// Open in a headset's game (the hands are its cursor: timescale's slowed hands follow the controllers meanwhile).
[[nodiscard]] bool openInHeadset();
// Flatscreen play: IN_MouseMotion's (VR_FlatToolgunMouse) while open: the cursor's. True when taken (the view stays).
[[nodiscard]] bool flatMouse(int dx, int dy);
// Flatscreen play: a gamepad's right stick (IN_JoyMove, VR_FlatStickCursor; padflat) while open: points the cursor at
// a slot (x right, y down, -1 .. 1). True when taken (the view stays).
[[nodiscard]] bool flatStick(float x, float y);
// Flatscreen play: Key_Event's Esc while open: closed, nothing picked. True when taken.
[[nodiscard]] bool escape();
// Flatscreen play: the crosshair's opacity share (vr_flatxhair.cpp): faded out as the wheel fades in
// (vr_flat_holster_menu_xhair_fade, the holster menu's).
[[nodiscard]] float crosshairAlpha();
// Flatscreen play: the view's blur behind it now (pixels at 1080p; vr_flat_holster_menu_blur, the holster menu's, by its
// fade; vr_visor.cpp menuBlur): 0 closed, or in a headset's game.
[[nodiscard]] float blurRadius();
// Flatscreen play: VR_End2D's window pass: its image on the window where the hand was, its cursor on it.
void drawFlat();
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
