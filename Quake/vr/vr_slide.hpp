// vr_slide.hpp -- the slide, flatscreen and headset play's one (docs/vr-port/FLATPLAY.md, "Sliding"; ROUND25.md,
// "Sliding in VR").
//
// One slide on the server for both ways of playing (VR_SlideMove, from SV_AirMove): a slide input held while running
// faster than vr_flat_slide_min along the floor starts it -- his speed boosted, then no walking and no ground friction but
// its own, a slope pulling him down it, a crouched box -- until he is slower than vr_flat_slide_end, lets go, jumps (a
// jump keeps the speed: Quake's air move), lies down or swims. Its stamina, its cooldown, its step up, its push of a prop
// and its charge into a monster are the same for both. Only the input differs:
//   - flatscreen play (-flatscreenmode): the crouch key (hull::flatCrouchHeld; vr_flat_slide gates it);
//   - a headset (vr_slide, experimental, default off): the turning stick pulled straight down (vr_input.cpp slideStick),
//     sent in the move's bit 14 (protocol::moveSlideBit; vr_server.cpp setHeadsetInput). Its box is the flat slide's
//     crouched one (vr_hull.cpp: his eyes taken as the flat slide's, slideEye); with vr_slide_camera his view eased down
//     to those eyes (viewDrop, on top of his tracked head), the body the flat slide's pose hung from it (vr_slide_body);
//     no FOV change.
// The client side of flatscreen play (the view's drop, its pose, the tests) is vr_flatmove.cpp's.

#pragma once

#include "vr_engine.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace qvr::slide
{

inline constexpr int slots = 17;        // the server's client numbers 1..16 (edicts 1..maxclients)
inline constexpr float jumpedUp = 60.f; // u/s up off the ground, the jump key held: a jump (it ends a slide)

// A client's slide (the server's, one per client number).
struct Slide
{
    bool on{false};          // sliding (his ground move is the slide's)
    bool held{false};        // his slide input (the crouch key, the stick's pull), last move
    bool grounded{false};    // on the ground, last move
    bool airborne{false};    // left the ground while sliding, not by a jump: the slide goes on when he lands
    double cooldownUntil{0.0};
    double endedAt{-1.0};    // the server's time the last slide ended (-1 none): headsetRising
    double lastTime{-1.0};   // the server's time at the last move (a new map: the clock goes back)
    int starts{0};
    int groundSteps{0};      // this slide's moves on the ground and off it (the tests)
    int airSteps{0};
    int jumpEnds{0};         // slides a jump ended (the tests: a ramp or a step is no jump)
    const char* endWhy{""};  // why the last one ended (the tests, vr_flatmove_status)
    float lastZ{0.f};        // his height at the last move (a step up's rise)
    float startSpeed{0.f};
    // The slide's stamina (vr_flat_slide_stamina, in vr_flat_slide_stamina_parts parts vr_flat_slide_stamina_step s
    // apart: the first as it starts; one not yet due when it ends isn't charged).
    double startedAt{0.0};
    int partsPaid{0};
    float staminaPaid{0.f};  // this slide's stamina so far (the tests, vr_flatmove_status)
    int tiredRefusals{0};    // slides not started for want of the first part (the tests)
};

// Client number `num`'s slide (1..16; anything else: a spare one nobody reads).
[[nodiscard]] Slide& state(int num);
// `ent`'s client number when it is a client on this server (1..16), else -1.
[[nodiscard]] int clientSlot(const edict_t* ent);
// vr_flat_slide: flatscreen play's slide on.
[[nodiscard]] bool slideOn();
// The floor's normal under `ent`, no more than 4 units down (straight up when none is found; `found` says).
[[nodiscard]] glm::vec3 floorNormal(edict_t* ent, bool& found);
// The slide's stamina: its parts, a part's cost (0 when the slide is free), the parts now due paid.
[[nodiscard]] int slideStaminaParts();
[[nodiscard]] float slideStaminaPart();
void paySlideStamina(edict_t* ent, Slide& s, double now);
void endSlide(Slide& s, double now);

// vr_server.cpp, every move of a headset client: its slide input held (the move's protocol::moveSlideBit: vr_slide's
// gesture held; a flatscreen client's crouch key is hull::flatCrouchHeld's).
void setHeadsetInput(const edict_t* ent, bool held);
// Player `ent`'s slide is on (off a ledge mid-slide too); false for anyone else (vr_hull.cpp: no lying down mid-slide,
// a headset's crouched box; vr_box3d_standing.cpp: a prop slid into).
[[nodiscard]] bool onServer(const edict_t* ent);
// Player `ent` a headset player (not flatscreen) sliding: his box the flat slide's crouched one (vr_hull.cpp).
[[nodiscard]] bool headsetOnServer(const edict_t* ent);
// The eyes over the feet the flat slide puts a flatscreen player's at (Quake's 46 less the crouch's and the slide's
// drops): the box a headset player's slide is given is the one these eyes get, the flat slide's.
[[nodiscard]] float slideEye();

// The local player (this machine's server, client 1) sliding on the ground with a headset (vr_slide): the comfort
// vignette, the slide's sound; and the slide's pose wanted (off a ledge mid-slide too) with vr_slide_body.
[[nodiscard]] bool headsetSliding();
[[nodiscard]] bool headsetPose();
// Player `ent` a headset player whose slide ended moments ago (vr_hull.cpp: his eyes, let down by the view drop, coming
// back up do not lay him down).
[[nodiscard]] bool headsetRising(const edict_t* ent);

// The headset's view drop (vr_slide_camera; vr_hands.cpp placeTracked, once the tracked head is placed): while this
// machine's player slides, units to let his whole tracked frame down (the head, the eyes, the hands, the body hung from
// them) so his eyes are the flat slide's (slideEye) -- from where his real eyes were as it started, his head's own moves
// still one to one on top -- eased in vr_slide_camera_time (critically damped) and back up after it. `eyeOverOrigin`: the
// tracked eyes' height (between them) over the client's player origin, undropped. 0 with vr_slide_camera 0 (the view at
// the real head, as before 2026-11-17).
[[nodiscard]] float viewDrop(float eyeOverOrigin);
// The drop easing (in or out): the comfort vignette counts it as a move.
[[nodiscard]] bool viewDropMoving();
// The drop as last stepped (the tests).
[[nodiscard]] float viewDropNow();
// How far down the drop is, 0 (none) to 1 (all of the slide's way): the body's floor goes to the box's with it
// (vr_hands.cpp placeTracked).
[[nodiscard]] float viewDropShare();

// One step of the slide on the ground (the self-test drives it directly): the speed along the floor `vel`, the floor's
// normal, the move keys' way (unit, or zero), seconds. Out: the new speed along the floor; `ended` when it is slower
// than vr_flat_slide_end.
struct SlideStep
{
    glm::vec2 vel{0.f};
    bool ended{false};
};
[[nodiscard]] SlideStep slideStep(const glm::vec2& vel, const glm::vec3& normal, const glm::vec2& wish, float dt);

// The turning stick's slide gesture (vr_slide; vr_input.cpp): pulled within vr_slide_stick_angle degrees of straight
// down past vr_slide_stick_threshold, from the middle (a turn swept round to it never counts). `st` the stick, `allowed`
// whether it may start now (vr_slide on, on foot, out of water, in the game). Returns whether it is held (the stick's
// down then does nothing else: no +movedown, no turn).
[[nodiscard]] bool stickGesture(const glm::vec2& st, bool allowed);
[[nodiscard]] bool gestureHeld();

// VR_Init: vr_slide_test.
void init();
// Every frame (vr_input.cpp): the running test.
void frame();
// Every frame of flatscreen play (vr_flatmove.cpp): vr_slide_pose_test.
void poseTestFrame();

} // namespace qvr::slide
