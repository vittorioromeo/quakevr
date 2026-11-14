// vr_move.hpp -- the VR block appended to clc_move (see vr_protocol.hpp).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Vocabulary/Optional.hpp"


namespace qvr
{

struct VrHandMove
{
    glm::vec3 pos{0.f};      // world position
    glm::vec3 rot{0.f};      // angles (pitch, yaw, roll)
    glm::vec3 vel{0.f};      // metres / second (play space, turned with it; not the player's own motion)
    glm::vec3 throwVel{0.f}; // smoothed velocity used for throwing
    float velMag{0.f};
    glm::vec3 angVel{0.f};   // radians / second, of the throw
    glm::vec3 throwFlick{0.f}; // metres / second: the part of throwVel the wrist's flick gives (throwing::Estimate::flick)
    glm::vec3 throwPos{0.f}; // world position the thrown object left the hand at
    float throwAge{0.f};     // seconds since it left the hand
    glm::vec3 tracked{0.f};  // world position of the controller's own point: `pos` before the level held the drawn hand
                             // out of it (vr_handpose.cpp; the climbing grab takes a hold at either)
    glm::vec3 ownVel{0.f};   // as `vel`, the controller's own (hands::State::ownVel: in slow motion the slowed hand lags it)
};

// A hand as its client drew it (MP_PLAN.md, B2b): the jointed hand's entity (its place and turn after the palm's fit and the
// firing motion), its fingers' curls as posed (handrig::Pose: the grasp's wrap of what it holds included) and its thumb's
// turn, and the held weapon's model entity. The server passes it on in the avatar (QVR_SVC_AVATAR): other players draw
// that hand and weapon where this client drew them (vr_mpavatar.cpp), no solve of their own. Written compactly
// (writeDrawnHand): places in eighths of a unit from an origin, angles in 16 bits, curls in sixteenths of a frame.
struct VrDrawnHand
{
    static constexpr za::U8 handBit = 1;      // the jointed hand drawn
    static constexpr za::U8 handMirrorBit = 2;
    static constexpr za::U8 weaponBit = 4;    // a weapon model drawn (weaponModel: its model_precache index)
    static constexpr za::U8 weaponMirrorBit = 8;
    static constexpr int fingers = 5, joints = 3;
    za::U8 flags{0};
    glm::vec3 handOrigin{0.f}; // world
    glm::vec3 handAngles{0.f}; // the entity's (pitch as entities have it)
    float curl[fingers][joints]{}; // curl frames, thumb .. little, knuckle first
    glm::quat thumb{1.f, 0.f, 0.f, 0.f}; // the thumb's metacarpal turn
    za::U16 weaponModel{0};
    za::U8 weaponFrame{0};
    za::U8 weaponSkin{0};
    glm::vec3 weaponOrigin{0.f};
    glm::vec3 weaponAngles{0.f};
};

// Bytes of a hand written: flags, then the hand (24 B: place, turn, curls, thumb) and the weapon (16 B) if drawn.
void writeDrawnHand(sizebuf_t* buf, const VrDrawnHand& h, const glm::vec3& origin);
[[nodiscard]] VrDrawnHand readDrawnHand(const glm::vec3& origin);
// A curl as written (sixteenths of a frame, -8 .. 7.94): what the reader gets for `c`.
[[nodiscard]] float quantizedCurl(float c);

// How this client poses its body (vr_avatar.cpp's inputs beyond the head and hands): the torso's yaw (hands::State::bodyYaw),
// the head's height over the play space's floor and its learnt standing height (metres: the crouch, the legs' fit) and
// how sure a lean is. Multiplayer only (valid): the other players pose his body by it (MP_PLAN.md, B2d). Written as a
// byte (valid), then a 16-bit angle, two shorts (mm) and a byte (255ths): 8 B.
struct VrBodyPose
{
    bool valid{false};
    float yaw{0.f};
    float headHeight{0.f};
    float standingHeight{0.f};
    float leanHold{0.f};
};
void writeBodyPose(sizebuf_t* buf, const VrBodyPose& p);
[[nodiscard]] VrBodyPose readBodyPose();

// The holsters as this client drew them last (view::netHolsters; MP_PLAN.md, B6b): each holster's point (body::Holster's
// order, world: QC's holsterpos for him on the server) and the weapon drawn in it, as the hands' (the others draw his
// holstered weapons as he does: his holster places, seated layout, Holstered poses). Multiplayer only (valid).
struct VrDrawnHolsters
{
    static constexpr int count = 6;
    bool valid{false};
    glm::vec3 at[count]{};
    za::U8 weapons{0};  // bit h: a weapon drawn in holster h
    za::U8 mirrored{0}; // bit h: drawn mirrored (the left-hand holsters)
    za::U16 model[count]{}; // model_precache index
    za::U8 frame[count]{};
    za::U8 skin[count]{};
    glm::vec3 origin[count]{}; // world
    glm::vec3 angles[count]{};
};
// Written as a byte (valid), then, valid, the points (eighths from `origin`, 36 B) when `points`, the weapons' and the
// mirrored bytes and 16 B a drawn weapon (model, frame, skin, place, turn).
void writeDrawnHolsters(sizebuf_t* buf, const VrDrawnHolsters& h, const glm::vec3& origin, bool points);
[[nodiscard]] VrDrawnHolsters readDrawnHolsters(const glm::vec3& origin, bool points);

struct VrMove
{
    glm::vec3 headAngles{0.f}; // -> .v_viewangle (.v_angle carries the aim angles)
    float vrYaw{0.f};          // accumulated snap/smooth turn
    VrHandMove hands[2];       // [0] off hand, [1] main hand
    glm::vec3 headVel{0.f};
    glm::vec3 muzzlePos[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    za::U16 vrBits0{0};  // QVR_VRBITS0_* (QC/vr_defs.qc)
    glm::vec3 teleportTarget{0.f};
    za::U8 hotspots[2]{0, 0}; // QVR_HS_* for [0] off hand, [1] main hand
    glm::vec3 roomscaleMove{0.f};   // world units / second
    za::U8 buttons{0};        // protocol::QVR_BUTTON_*
    za::U8 sawCord{0};        // the chainsaws' cords (vr_chainsaw.cpp moveBits) -> .sawcord
    za::U8 handDrop{0};       // a heavy weapon wrenched out of a hand (vr_weight.cpp dropBits) -> .handdrop
    za::U8 handSynth{0};      // the hands whose motion is synthesized, not for gestures (vr_handsource.cpp moveBits) -> .handsynth
    glm::vec3 origin{0.f};          // the player origin the client placed the hands from
    glm::vec3 headPos{0.f};         // the head (between the eyes), world
    // Where each hand's shots go (angles): its aim turned by the held weapon's Shot Pitch and Yaw (weapons::shotAngles);
    // -> .offshotrot, .shotrot (QC VRGetWeaponFireRot).
    glm::vec3 shotRot[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    // Each held gun's loading port as drawn (immersive reloading; vr_view.cpp loadPorts; the hand's position when it has
    // none); -> .offloadportpos, .loadportpos (QC vr_reload.qc).
    glm::vec3 loadPort[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    // And the way a round lies going in there (hands::State::loadPortAxis; zero: none); -> .offloadportaxis,
    // .loadportaxis.
    glm::vec3 loadPortAxis[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // And the way its opening faces (hands::State::loadPortFace); -> .offloadportface, .loadportface.
    glm::vec3 loadPortFace[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // Where each hand's projectile leaves from, from its muzzle in the shot's frame (forward, right, up; zero: the
    // muzzle): the AD crossbow's drawn bolt (adcrossbow::launchOffset); -> .offlaunchofs, .launchofs.
    glm::vec3 launchOfs[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // The magazine attached to each hand's gun as drawn (hands::State::magBox: its middle, then its three half-axes; all
    // zero: none); -> .offmagbox*, .magbox* (QC vr_reload.qc: grips and hits on its whole shape).
    glm::vec3 magBox[2][4]{};
    // Where the rounds stand in the ammo pouch, as placed (view::ammoPouchRounds: a box, its middle and half-axes, the
    // last up along the rounds; all zero: no pouch); -> .ammopouchpos, .ammopouchx/y/z (QC vr_reload.qc: pouch reloading).
    glm::vec3 ammoPouch[4]{};
    // The stealth AI's view of this player (QC vr_stealth.qc; vr_stealth.cpp): his flashlight's beam as his client lit
    // it (flashlight::beamNow; sent only while lit) and the light on him as his client measures it (stealth::lightAt:
    // his map's lightmaps and the dynamic lights he sees, his own lamp's aside; -1 unknown).
    bool lampLit{false};
    // The poses' time (hands::State::poseTime) modulo VR_POSE_CLOCK_WRAP s (a float's precision kept; -1 unknown) ->
    // .vr_posetime: the melee's speeds over the poses' own times (vr_melee.qc VR_Melee_Track, vr_legsweep.cpp).
    float poseClock{-1.f};
    glm::vec3 lampLens{0.f};
    glm::vec3 lampDir{1.f, 0.f, 0.f};
    float lampRange{0.f};
    float lampCos{1.f};
    float light{-1.f};
    // The hands as this client drew them last ([0] off, [1] main; view::netHands): for the other players (the avatar).
    VrDrawnHand drawn[2];
    VrBodyPose body; // (multiplayer, with `drawn`)
    VrDrawnHolsters holsters; // (multiplayer, with `drawn`: B6b)
};

void writeVrMove(sizebuf_t* buf, const VrMove& move);

// The block, read whole (the message stays in step); none if any of its floats is not finite (NaN, infinity): the
// move is dropped, the client's previous one stands.
[[nodiscard]] za::Optional<VrMove> readVrMove();

} // namespace qvr
