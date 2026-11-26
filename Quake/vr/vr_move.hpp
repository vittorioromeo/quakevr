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
    // A bot's hand (the server's, B11 part 3): not drawn by a client but its controller (handOrigin, handAngles: QC's
    // handpos, handrot) and the weapon in it (weaponModel, weaponFrame), for the receiving client to place as it places
    // its own (view::solveHands). Written: the place, the turn, the model and the frame (11 B); with weaponBit the weapon's
    // place and turn as the server placed it (12 B more: view::placeHeld, drawn as is; bots3).
    static constexpr za::U8 solveBit = 16;
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
    za::U16 model[count]{}; // view::gearModel's index (the view's own models: the server precaches none)
    za::U8 frame[count]{};
    za::U8 skin[count]{};
    glm::vec3 origin[count]{}; // world
    glm::vec3 angles[count]{};
};
// Written as a byte (valid), then, valid, the points (eighths from `origin`, 36 B) when `points`, the weapons' and the
// mirrored bytes and 16 B a drawn weapon (model, frame, skin, place, turn).
void writeDrawnHolsters(sizebuf_t* buf, const VrDrawnHolsters& h, const glm::vec3& origin, bool points);
[[nodiscard]] VrDrawnHolsters readDrawnHolsters(const glm::vec3& origin, bool points);

// A gun's look beyond its model, frame and skin (MP_PLAN.md, "Held weapons' state"): what the server has of it (QC's
// fields), drawn by the same code for this client's own guns (view::ownGunLook, from the stats) and another player's (his
// avatar: the server writes his, writeGunLook). Its revolver's crane and cylinder, the super shotgun broken open, its
// magazine in or out and the rounds it shows, the ammo screen's clip.
struct VrGunLook
{
    static constexpr za::U8 gunBit = 1;        // a gun in that hand (the rest written)
    static constexpr za::U8 magsShownBit = 2;  // the guns' magazines drawn (immersive reloading, or flat-screen play)
    static constexpr za::U8 clipFullBit = 4;   // no clips at all (the reload mode 0): drawn full
    static constexpr za::U8 magOutBit = 8;     // its magazine sliding out (QC .vr_mag_out)
    static constexpr za::U8 ssgBreaksBit = 16; // the super shotgun breaks open (drawn open by its flag)
    static constexpr za::U8 revolverBit = 32;  // a heavy revolver's state follows (QC .vr_rv_hud nonzero)
    za::U8 bits{0};
    za::U16 flags{0};   // QC's weaponflags (QVR_WPNFLAG_*: no magazine, broken open, its chambers...)
    za::U8 clip{0};     // the rounds in it (its record's wi_clip)
    za::U8 clipSize{0}; // its magazine's size (the ammo screen's warning colour)
    za::I32 revolver{0}; // QC .vr_rv_hud (vr_view.cpp setupRevolverParts: chambers, swung out, the rod)
    float cylinder{0.f}; // QC .vr_rv_cyl, degrees (written in 256ths of a turn)
    [[nodiscard]] bool has(za::U8 bit) const { return (bits & bit) != 0; }
};
// Written as the bits, then with a gun its flags, clip and clip size (4 B), with a revolver its state and turn (4 B).
void writeGunLook(sizebuf_t* buf, const VrGunLook& l);
[[nodiscard]] VrGunLook readGunLook();

// This client's torch as drawn (flashlight::netLamp; MP_PLAN.md, "Held weapons' state"): where it is (on the belt, in a
// hand, on a gun, at the head), its model shown or not, lit; the others draw it there and light its beam from its lens
// along its +x. Multiplayer only.
struct VrDrawnLamp
{
    static constexpr za::U8 placedBit = 1; // placed (the rest written)
    static constexpr za::U8 shownBit = 2;  // its model drawn (flat-screen play hides it at the eyes)
    static constexpr za::U8 litBit = 4;    // lit (its reach follows)
    za::U8 flags{0};
    glm::vec3 origin{0.f};                 // world (the move: absolute; the avatar: from his entity's origin)
    glm::quat rot{1.f, 0.f, 0.f, 0.f};     // its model's axes to the world (+x the beam)
    float range{0.f};                      // the beam's reach, units
};
// Written as the flags, then placed its place (eighths from `origin`) and turn (16-bit x, y, z; w >= 0), lit its reach.
void writeDrawnLamp(sizebuf_t* buf, const VrDrawnLamp& l, const glm::vec3& origin);
[[nodiscard]] VrDrawnLamp readDrawnLamp(const glm::vec3& origin);

// A player's worn gear as his client drew it (syncfix, MP_PLAN.md "Sync audit": batch 2), drawn on his copy as he drew it
// (B6b's way for his holstered weapons): the holster sheaths (body::Holster's order: the hips' and the uppers' drawn),
// the wrist gadget and its two straps, the grenade pouch, the ammo pouch. A bot's (solveBit): none drawn by a client --
// each client places its holstered weapons and sheaths on its body as drawn (mpavatar.cpp, solveBotGear).
struct VrDrawnGear
{
    static constexpr int sheaths = 6; // (indices 0..5: body::Holster's; the shoulders' have none)
    static constexpr int gadget = 6, strap = 7, pouch = 9, ammoPouch = 10;
    static constexpr int count = 11;
    static constexpr za::U16 solveBit = 0x8000; // in `shown`: a bot's, placed by each client
    za::U16 shown{0};    // bit i: piece i drawn
    za::U16 mirrored{0}; // bit i: drawn mirrored
    za::U16 model[count]{}; // model_precache index
    za::U8 frame[count]{};
    za::U8 skin[count]{};
    za::U8 scale[count]{}; // thirty-seconds (its size: Holster Setup's for a sheath)
    glm::vec3 origin[count]{}; // world (the avatar: from his entity's origin)
    glm::vec3 angles[count]{}; // entity_t's
    // What the ammo pouch holds (syncfix2: the avatar only, the server's word: QC .vr_pouch_kind, .vr_pouch_count, what
    // your STAT_QVR_POUCHKIND and _POUCHCOUNT carry), its rounds drawn in his pouch by your pouch's code.
    za::U8 pouchKind{0};
    za::U8 pouchCount{0};
    // The hand whose wrist wears the gadget (0 off, 1 main: VrMove::drawn's): the move sends the gadget and its straps
    // from that hand, the rest from the body (syncfix3: still while he walks or turns; the wire only).
    za::U8 wrist{0};
};
// Written as the shown and mirrored shorts, then each piece drawn: its model, frame, skin, scale, place (eighths from
// `origin`), turn (16-bit): 17 B a piece; the ammo pouch's kind and count after it when it is drawn (2 B). syncfix3: the
// wrist byte after the shorts, and each piece's place and turn from what wears it: the gadget and its straps from
// `hands[wrist]` (its place, then turned by its yaw; its angles taken off), the rest from `origin` turned by `bodyYaw`
// (writer and reader the same values: the avatar's hands and body as they are written and read).
void writeDrawnGear(sizebuf_t* buf, const VrDrawnGear& g, const glm::vec3& origin, float bodyYaw, const VrDrawnHand* hands);
[[nodiscard]] VrDrawnGear readDrawnGear(const glm::vec3& origin, float bodyYaw, const VrDrawnHand* hands);

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
    za::U16 handSynth{0};     // the hands whose motion is synthesized, not for gestures (vr_handsource.cpp moveBits) -> .handsynth
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
    VrDrawnLamp lamp;         // (multiplayer, with `drawn`: his torch as drawn, wpnsync)
    VrDrawnGear gear;         // (multiplayer, with `drawn`: his worn gear as drawn, syncfix)
};

void writeVrMove(sizebuf_t* buf, const VrMove& move);

// The block, read whole (the message stays in step); none if any of its floats is not finite (NaN, infinity): the
// move is dropped, the client's previous one stands.
[[nodiscard]] za::Optional<VrMove> readVrMove();

} // namespace qvr
