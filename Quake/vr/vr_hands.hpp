// vr_hands.hpp -- per-frame client-side VR body state: head, hands, body yaw and crouch, in
// world space. Computed once per host frame from the backend's tracking (or the standing pose
// when VR is off) and shared by the VR move and the view entities.

#pragma once

#include <glm/glm.hpp>

namespace qvr::hands
{

struct State
{
    bool valid{false};

    glm::vec3 playerOrigin{0.f}; // the player's box (its middle; the floor below the head less the lean)
    glm::vec3 lean{0.f};         // the head's horizontal offset from it (vr_lean_radius)
    glm::vec3 head{0.f};        // world eye position
    glm::vec3 headAngles{0.f};  // pitch, yaw, roll (view convention: pitch down is positive)
    float headHeight{0.f};      // metres above the play-space floor
    float bodyYaw{0.f};
    float crouchRatio{0.f};     // 0 standing .. 1 crouched (relative to vr_height_calibration)
    // Leaning (vr_lean_detect): how sure the head's offset from the box is a lean (1: the body stays where it stands)
    // rather than a step (0: it follows), and the cues that told (vr_debug_lean): the head's drop below its standing
    // height, its tilt towards the offset, the hands left behind, and that standing height (metres).
    float leanHold{0.f};
    glm::vec4 leanCues{0.f};
    float standingHeight{0.f}; // the head's standing height (metres), learnt: how far down it has gone for a lean

    glm::vec3 eyeOrigin[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] left, [1] right (headset only)
    glm::vec3 eyeAngles[2]{glm::vec3{0.f}, glm::vec3{0.f}};

    glm::vec3 pos[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    glm::vec3 rot[2]{glm::vec3{0.f}, glm::vec3{0.f}};

    // Round 21, third pass: the controllers as tracked (their grip pose, before the calibration's angles, the held
    // weapon's Hand and Weapon Together offset and anything after: vr_show_controller), and their calibrated aim
    // (vr_gunangle and the rest: where the controller points, before the weapon's offset: vr_show_controller_laser).
    glm::vec3 controllerPos[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 controllerRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // The hand calibrated on its controller (hands::calibration), before the held weapon's Hand and Weapon Together
    // offset: the empty hand's place (Match Controller Preview).
    glm::vec3 calibratedPos[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 calibratedRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 aimRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // After the posing test: each controller's grip pose (the runtime's: where its handle is; controllerPos/Rot are the
    // pose moved off it by vr_controller_legacy_pose): the Show Controller preview is drawn there.
    glm::vec3 gripPos[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 gripRot[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // The world turn the held weapon's Hand and Weapon Together offset gave the hand (rot = wholeTurn * the rot it had):
    // the view turns the weapon (posed from the rot before it) rigidly by it, so the muzzle turns exactly with it.
    glm::mat3 wholeTurn[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    // The world turn the hand calibration's roll gave the hand (calibratedRot = calTurn * the rot before it): the weapon
    // is posed from the rot before it and turned rigidly by it too (after it, wholeTurn), so that it rolls exactly with
    // the hand (its angle offsets are Euler angles added to the hand's: turned by them, it would turn a little otherwise).
    glm::mat3 calTurn[2]{glm::mat3{1.f}, glm::mat3{1.f}};

    // Where each hand's palm is (its middle, as the jointed hand is drawn free: the empty hand) relative to pos, in the
    // frame of rot (x forward, y right, z up: hands::redirect's), set by the view: a cup hotspot is taken by the palm.
    bool palmValid[2]{false, false};
    glm::vec3 palmLocal[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 visualRot[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // what the weapons are drawn at (flick spin)

    // Velocities in metres (radians) per second, in Quake axes turned with the play space, not
    // including the player's own movement: what the QC's thresholds and multipliers expect.
    glm::vec3 vel[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 angVel[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 headVel{0.f};

    int hotspot[2]{0, 0}; // body::Hotspot of each hand

    // Weapon muzzles, from the weapon models' anchor vertices: set by the view when it renders,
    // and kept until the next render (moves are sent before rendering).
    bool muzzleValid[2]{false, false};
    glm::vec3 muzzle[2]{glm::vec3{0.f}, glm::vec3{0.f}};

    // Where the other hand grips each weapon in the "fixed" two-handed display mode (a
    // foregrip vertex of the weapon model), placed with the muzzles.
    bool grip2HValid[2]{false, false};
    glm::vec3 grip2H[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float grip2HBias[2]{0.f, 0.f}; // its hotspot's bias: units off the distance it is taken by (round 21)
    bool grip2HCup[2]{false, false}; // it is a cup (a two-handed pistol grip): held, but no two-handed aim
    bool grip2HPalm[2]{false, false}; // a cup hotspot: grip2H is where the helping hand's palm goes (taken by the palm)
};

// Hand calibration (Hand/Gun Calibration > Hand Calibration): where each hand sits on its controller, so that the drawn
// hand is where the real one is. `move`: centimetres along the controller's grip axes (x along the handle, y left, z up:
// Show Controller's red, green, blue, before its preview offsets). `turn`: degrees, pitch (down: vr_gunangle,
// vr_offhandpitch), yaw (left: vr_gunyaw, vr_offhandyaw), roll (the right side down) -- yaw then pitch turn the
// controller (about its tracked point, as ever), then the roll turns the hand about where it points, through the grip.
// The off hand's own values, or (vr_handcal_off_mirror) the main hand's mirrored: y, yaw and roll the other way. The
// whole hand moves (what it holds, its muzzle, its melee points, the body's arm); 0 moves nothing.
struct Calibration
{
    glm::vec3 move{0.f};
    glm::vec3 turn{0.f};
};
[[nodiscard]] Calibration calibration(int hand);

// Where a hand's palm is (State::palmLocal), or its point if not known.
[[nodiscard]] glm::vec3 palmPoint(const State& s, int hand);

// The server set the view yaw: turn the play space to match (headset only).
void setServerYaw(float yaw);

// Turns the play space (thumbstick turning), in degrees, positive to the left.
void addTurn(float degrees);
[[nodiscard]] float playSpaceYaw();

// Motion playback (vr_motion_play.cpp): the play space's turn as a take had it (a pending server yaw
// dropped), and the head's lean off the box's middle (world units, horizontal).
void setPlaySpaceYaw(float yaw);

// The calibration's pitch and yaw on a controller's tracked orientation (tracking space): where the hand points (the
// menu's laser).
[[nodiscard]] glm::quat aimedController(const glm::quat& controller, int hand);

// A new map (VR_OnClientClearState): what follows the client's time (cl.time starts over) begins afresh.
void resetClientState();
void setLean(const glm::vec3& worldLean);

// Updated at most once per host frame; valid only while connected to a VR-protocol server.
[[nodiscard]] State& current();

// World-space distance the head walked in the play space since the last call (room-scale
// movement, sent with each move).
[[nodiscard]] glm::vec3 takeRoomscaleMove();

// Body anchor used for holsters, the torso and shoulder stocks: `offsets` are forward, right
// and up (scaled by vr_height_calibration), from the player origin under the head (with the lean).
[[nodiscard]] glm::vec3 bodyAnchor(const State& s, const glm::vec3& offsets);

// Round 21, third pass: a weapon slot's Hand and Weapon Together offset applied to a hand's pose (`h`: mirrored for
// the off hand), as the hands are updated with the held weapon's; `turn` the world turn it gave (true if it turned the
// hand). And the held weapon's taken off a pose of `s` (the posing mode's hand as an empty hand at its controller).
bool wholeOffset(int slot, int h, glm::vec3& pos, glm::vec3& rot, glm::mat3& turn);
void undoWholeOffset(const State& s, int h, glm::vec3& pos, glm::vec3& rot);

// Helpers.
[[nodiscard]] glm::vec3 forward(const glm::vec3& angles);
void angleVectors(
    const glm::vec3& angles, glm::vec3& fwd, glm::vec3& right, glm::vec3& up);
[[nodiscard]] glm::vec3 rotateYaw(const glm::vec3& v, float degrees); // about the vertical axis
[[nodiscard]] glm::vec3 redirect(const glm::vec3& v, const glm::vec3& angles); // f*x + r*y + u*z
[[nodiscard]] glm::vec3 anglesFromVectors(const glm::vec3& fwd, const glm::vec3& up); // Quake angles

} // namespace qvr::hands
