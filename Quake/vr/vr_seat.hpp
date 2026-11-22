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
// Phase 4a: flat play's state (the hands' pose and melee, the keys, crouch and prone, holsters, the ironsight, third
// person, reload, carry, grenade, swap) as the seat's parts; the tests' and tools' own marked shared.
// Phase 4b: the hands. Phase 5: cl, cls and the entity tables (clientseat_t, cl_seat), the VR client's net state.
// Phase 6: the seats in play (a bit each; seats 1-3 joined by a pad), each with its own loopback connection and server
// slot; the host frame runs each playing seat's client frame with it current (host.c). Seats 1-3 run headless until
// phase 7 (CL_SeatHeadless: no view, no sound, none of the shared effects).
// Phase 7: in flatscreen play each seat drawn in its tile; its body (avatar::seatSwitched), climbing pins and plates its
// own; the shared effects fed once per event by the drawn seats (CL_SeatEffectFirst).
#pragma once

#include "vr_engine.hpp"

#include "Zancle/Vocabulary/UniquePtr.hpp"

namespace qvr::seat
{

inline constexpr int maxSeats = 4;
static_assert(maxSeats == MAX_SPLITSEATS, "client.h's MAX_SPLITSEATS");

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
    // Phase 4a: flat play's own.
    FlatPlay,    // vr_flatplay.cpp: the hands' pose, punches, shove, guard, two-handed stance, the drawn hands' feel, the
                 // mode's keys and the crouch and prone eases, the CTRL bind taken, the slide's turn, the toolgun's keys
    FlatHolster, // vr_flatholster.cpp: the holster menu
    FlatSight,   // vr_flatsight.cpp: the ironsight, the grips, the weapon at walls; the crosshair's memo
    FlatTp,      // vr_flattp.cpp: the third-person camera
    FlatReload,  // vr_flatreload.cpp: the reload's trips
    FlatGrab,    // vr_flatfgrab.cpp: what the hands carry (E's grab)
    FlatGrenade, // vr_flatgrenade.cpp: the grenade's throw
    FlatSwap,    // vr_flatswap.cpp: the hands' swap
    // Phase 4b: the hands (flat play's and the headset's).
    Hands,        // vr_hands.cpp: the rig (hands::current()'s state), the play space's turn, the body's walk and lean
    Held,         // vr_held.cpp: what each hand holds, the eases, the hold in both hands, the meets, the skewered gibs
    TwoHand,      // vr_twohand.cpp: the two-handed holds, grips, help and carry records, free grips
    ViewHands,    // vr_view_hands.cpp: the drawn hands on an embedded blade, a skewered gib, a long gun's hotspot
    ViewWeapon,   // vr_view_weapon.cpp: the drawn weapons' reverse grips
    Throw,        // vr_throw.cpp: the hands' motion histories, the physgun's point, the grips' clicks
    Grip,         // vr_grip.cpp: the hands' frames as the client measured them
    GripFit,      // vr_gripfit.cpp: the hands' rig frames
    HandSource,   // vr_handsource.cpp: each hand's source this frame (tracked, flat, ...)
    HeldPose,     // vr_heldpose.cpp: what went up to the server last, each hand's
    Toolgun,      // vr_toolgun.cpp: the toolgun's state
    ToolgunGhosts, // vr_toolgun.cpp: the toolgun's ghosts (held, looked at)
    ToolgunBrush, // vr_toolgun_brush.cpp: the Brush tool's state
    ToolgunPanel, // vr_toolgun_panel.cpp: the toolgun's panel
    ClientButtons, // vr_client.cpp: the hands' buttons as the move reads them
    ClientThrown,  // vr_client.cpp: the grabs and the throws' estimates the move sends
    ClientEntities,// vr_client.cpp: the client world's VR data by entity, the late brush models
    ClientUnposed, // vr_client.cpp: the weapon posing mode's held move
    ClientPrefs,   // vr_clientprefs.cpp: the preferences sent to the server
    NetMove,       // vr_netmove.cpp: the client's moves sent and acknowledged
    NetRagdoll,    // vr_netragdoll.cpp: the client's ragdolls from the server
    NetSnapPoses,  // vr_netsnap.cpp: the client's tallies of the entries read (vr_net_bytes)
    NetSnap,       // vr_netsnap.cpp: the client's snapshots read (the base, the acks)
    NetSnapBytes,  // vr_netsnap.cpp: the client's bytes read by command
    NetSnapInterp, // vr_netsnap.cpp: the client's interpolation between snapshots
    Predict,       // vr_predict.cpp: the client's prediction (its moves replayed, the error's decay)
    PredictTotals, // vr_predict.cpp: the prediction's correction totals
    VoiceListen,   // vr_voicechat.cpp: what the client told the server it hears
    // Phase 7: what each seat draws of its own.
    AvatarSelf,    // vr_avatar.cpp: your body while another seat is current (its pose, eases, gait: avatar::seatSwitched)
    AvatarOthers,  // vr_avatar.cpp: the other players' and dolls' bodies as this seat's client poses them
    ViewPlates,    // vr_view_body.cpp: your armour plates' morphs
    ClimbPins,     // vr_climb.cpp: each hand's drawn place eased onto its hold
    MpAvatars,     // vr_mpavatar.cpp: the other players' avatars as this seat's client read them, and what it drew of them
    // Phase 9: menus per seat.
    SeatMenu,      // vr_seatmenu.cpp: a seat 2-4's own menu (open, cursor, its look sensitivity and invert)
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
    clientseat_t* net = nullptr; // cls, cl, the entity tables (client.h; C: cl_seat, the current seat's): netOf() makes it
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
clientseat_t& netOf(Seat& s); // the seat's client (cls, cl, the entity tables): seat 0's the game's, the others made ready
                              // on first use (client.h; phase 5)
int inPlay();      // seats in play (seat 0 always; the others join and leave: vr_seatplay.cpp)
void setInPlay(int n); // the first n seats in play, seat i driven by pad i-1 (vr_seat_test's, which sets it back)
[[nodiscard]] bool plays(int i); // seat i in play
[[nodiscard]] int padOf(int i);  // the pad that joined seat i (1-3; -1: none). Seat 0: the keyboard and mouse, and the
                                 // first pad no seat has (in_sdl.c's IN_PadSeat)
void setPlays(int i, bool on, int pad); // seat i (1-3) in or out of play, joined by that pad (vr_seatplay.cpp)
[[nodiscard]] clientseat_t* netIfMade(int i); // seat i's client if it was ever made (nullptr: never used)
// A shared effect's emitter run once a frame (phase 7 part 3; CL_SeatEffectFirst's SEATFX_VR): the first drawn seat to run
// it this frame does, the others skip it (key: the emitter's, e.g. an entity's number). One seat: always.
enum class SharedFx
{
    BloodStreams,
    TorchSparks,
    Shells, // phase 8: the casings run (vr_shells.cpp; each seat ejects its own)
};
// Phase 8: the seat whose player is entity `ent` while seats are drawn in tiles (-1: none, or one seat).
[[nodiscard]] int seatOfPlayer(int ent);
[[nodiscard]] inline bool sharedFxFirst(const SharedFx fx, const int key = 0)
{
    return CL_SeatEffectFirst(SEATFX_VR, -(static_cast<int>(fx) + 1) * 65536 - key, nullptr) != 0;
}

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

// Phase 6 (vr_seatplay.cpp): the seats joining and leaving the local game (seat_join, seat_leave), the seat loop's
// per-frame step (CL_SeatFrame), vr_seat_play_test.
void registerPlayCommands();
// Phase 10's join screen: pad k's player into the next free seat (2-4), the local game running or not (a seat in play
// joins the next local game by itself): the seat, or -1 (printed: none free, the pad plays already). A seat leaving.
int joinPad(int pad);
void leaveSeat(int s);
[[nodiscard]] int seatOfPad(int pad); // the seat 1-3 pad k joined (-1: none)

// Phase 10 (vr_splitjoin.cpp): the join screen's commands (menu_splitscreen, vr_split_join_test), its test's step.
void registerJoinCommands();
void joinTestFrame();

// Phase 7 (vr_seatrender.cpp): vr_seat_render_test (each seat drawn in its tile), its per-frame step (CL_SeatFrame's).
void registerRenderCommands();
void renderTestFrame();

// Phase 9 (vr_seatmenu.cpp): seats 2-4's own menus, the menus' pause, vr_seat_menu_test, its per-frame step.
void registerMenuCommands();
void menuTestFrame();
void closeMenu(int i); // seat i's menu shut (a seat joining or leaving)

} // namespace qvr::seat
