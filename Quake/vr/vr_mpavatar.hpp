// vr_mpavatar.hpp -- other players' VR state (QVR_SVC_AVATAR): their heads, and their hands and held weapons as their own
// clients drew them (the jointed hand's place, its fingers' curls and thumb; the weapon's model, frame and place), kept per
// player and drawn on his entity (MP_PLAN.md, B2b). The server writes them (vr_server.cpp writeAvatars) to each client
// that sees that player. His body under them is vr_avatar.cpp's, posed from them by the code that poses your own (B2d:
// avatar::poseOther), sized and dressed by his calibration; it takes his player model's place (vr_mp_body).

#pragma once

#include "vr_engine.hpp"
#include "vr_move.hpp"

namespace qvr::view
{
struct SkinLook;
struct ViewEntity;
}
namespace qvr::ragdoll
{
struct Rig;
}
namespace qvr::avatar
{
struct Skeleton;
}

namespace qvr::mpavatar
{

// The avatar's first byte: a flatscreen player, and how the server has him move (MP_PLAN.md, B5b: his body lying,
// swimming, crouched, in the air as he is): his crouched and lying boxes (vr_hull.cpp), on the ground, his waterlevel.
inline constexpr int avatarFlat = 1;
inline constexpr int avatarCrouched = 2;
inline constexpr int avatarLying = 4;
inline constexpr int avatarOnGround = 8;
inline constexpr int avatarWaterShift = 4; // (two bits: Quake's waterlevel, 0 .. 3)
// B5c: his off (main) hand holds a ledge (the server's hold: climb::serverHold). Each hold follows the weapons: where it
// is (from his entity's origin, eighths) and the way out from its face (a byte of yaw).
inline constexpr int avatarHoldOff = 64;
inline constexpr int avatarHoldMain = 128;
// The server's byte for client entity `ent` (vr_server.cpp writeAvatars, vr_mp_status).
[[nodiscard]] int movesByte(const edict_t* ent, bool flat);
// The server's holds of client entity `ent` after the weapons (as movesByte's bits say), from `origin`.
void writeHolds(sizebuf_t* msg, const edict_t* ent, const glm::vec3& origin);

struct Avatar
{
    bool valid{false};
    double received{0.0}; // realtime it came
    double msgTime{0.0};  // the server's time of its message (cl.mtime[0]; the one before: when it held, B7's lerp)
    bool flat{false};     // a flatscreen player
    int moves{0};         // that byte as the server sent it (avatarCrouched...)
    glm::vec3 head{0.f};  // from his entity's origin
    glm::vec3 headAngles{0.f};
    VrBodyPose body;      // how his client poses his body (its yaw, the head's height, the lean)
    VrDrawnHand hands[2]; // [0] off hand, [1] main hand; places from his entity's origin
    int weapon{0};
    int weapon2{0};
    glm::vec3 holdAt[2]{};  // [0] off hand, [1] main: where it holds (moves' avatarHold*), from his entity's origin
    glm::vec3 holdOut[2]{}; // ... and the way out from the face (horizontal, unit)
    VrDrawnHolsters holsters; // his holstered weapons as he drew them, from his entity's origin (B6b; no points)
    int armour{0};            // the armour he wears (gearByte: his pauldrons' colour)
    bool sliding{false};      // the server has him sliding (gearByte's avatarGearSliding: his body's slide pose)
    int powerups{0};          // his power-ups (gearByte's avatarGearQuad...: as IT_QUAD, IT_INVULNERABILITY, IT_SUIT, IT_INVISIBILITY)
    // His guns as the server has them (MP_PLAN.md, "Held weapons' state"): each hand's look ([0] off, [1] main), each
    // holstered weapon's flags (low byte) and clip; his torch as he drew it (from his entity's origin).
    VrGunLook look[2];
    za::U8 holsterFlags[VrDrawnHolsters::count]{};
    za::U8 holsterClip[VrDrawnHolsters::count]{};
    za::U8 holsterAttach[VrDrawnHolsters::count]{}; // their attachments (vr_attach.hpp Kind)
    VrDrawnLamp lamp;
    VrDrawnGear gear; // his worn gear as he drew it (syncfix), from his entity's origin; a bot's: placed here
};
// The gear byte's slide bit (mpslide, 2026-11-20): the server's slide (slide::onServer, a flat-screen player's or a
// headset's), so the others pose his body slid as he sees his own (the armour is the byte's low two bits).
inline constexpr int avatarGearArmour = 3;
// syncfix (2026-11-21): his power-ups, drawn on his copy as yours on you (view::powerupLookOf: the glow, its hand lights,
// Quad's arcs, the ring's fade).
inline constexpr int avatarGearQuad = 4;
inline constexpr int avatarGearPent = 8;
inline constexpr int avatarGearRing = 16;
inline constexpr int avatarGearSuit = 32;
inline constexpr int avatarGearSliding = 128;
// The server's gear byte for client entity `ent` (after the holsters): the armour he wears, 0 none .. 3 red; his
// power-ups; his slide.
[[nodiscard]] int gearByte(const edict_t* ent);

// vr_client.cpp's dispatch: QVR_SVC_AVATAR's body (after its sub-command byte).
void parse();

// Angles (pitch, yaw, roll) `t` of the way from `a` to `b`, slerped as rotations (the avatars' turns, and the entities' in
// the interpolation buffer: vr_netsnap.cpp).
[[nodiscard]] glm::vec3 slerpAngles(const glm::vec3& a, const glm::vec3& b, float t);

// Player entity `ent`'s avatar (1..MAX_SCOREBOARD), or nullptr: none received.
[[nodiscard]] const Avatar* of(int ent);
// Player `ent`'s copy's weapon in `hand` as drawn this frame or the last (his shots' casings and flash, syncfix2), or
// nullptr (no copy drawn, the hand empty).
[[nodiscard]] const view::ViewEntity* drawnWeapon(int ent, int hand);

// VR_AddMpAvatars (view.c, after the view's entities): each other player's hands and weapons in sight to cl_visedicts
// (vr_mp_avatar_draw; multiplayer only).
void addEntities();

// VR_AliasBonePoses: an avatar's jointed hand's skinning matrices (their count; 0: not one); his armour's plates' and
// their leather's (B6c: his body's bones as drawn).
[[nodiscard]] int bonePoses(const entity_t* e, const float** matrices);
// The body an avatar's plates (or their leather) are drawn on (avatar::modelScale: his body's scale), or nullptr.
[[nodiscard]] const entity_t* plateBody(const entity_t* e);
// The view entity `e` is of another player's copy (his hands, weapons, holsters, body, pauldrons, plates), or nullptr:
// what the alias renderer asks of yours by view::find (mirrored, a plate's morph: VR_AliasMirrored, VR_AliasMorph).
[[nodiscard]] const view::ViewEntity* find(const entity_t* e);

// Another player's hand or body (an avatar's: 1 a hand, 2 the body; else 0): his look, from his calibration (B6: his
// skin tone, gloves and nails as he set them; none received yet: 0, drawn bare).
[[nodiscard]] int lookOf(const entity_t* e, view::SkinLook& look);

// Player entity `ent`'s copy as drawn last (addEntities: his entity's drawn place and his hands' drawn places, B7's
// interpolated ones), and the host frame it was set up in; false: not drawn yet. (vr_mp_trace: mp_loss.)
[[nodiscard]] bool drawnPlaces(int ent, glm::vec3& body, glm::vec3 hands[2], int& frame);

// The power-up glow's shader code on `e` if it is another player's copy's hand or body (view::powerupGlow), else 0.
[[nodiscard]] float powerupGlow(const entity_t* e);

// (bots4) Player entity `ent`'s weapon in hand `side` ([0] off, [1] main) as drawn last frame or this one: its butt and
// muzzle, and the hand holding it. False: none drawn there. (A client's Show Disarm Volumes: vr_disarmviz.cpp.)
[[nodiscard]] bool drawnWeapon(int ent, int side, glm::vec3& butt, glm::vec3& muzzle, glm::vec3& hand);

// (bots4) Player entity `ent`'s copy was drawn last frame or this one (a client's Show Hit Zones: vr_hitmodel.cpp).
[[nodiscard]] bool drawnNow(int ent);
// (hitzones) His body as drawn now (its size: m2w; its joints). False: his body not posed (none drawn, or id's ranger).
[[nodiscard]] bool drawnSkeleton(int ent, avatar::Skeleton& out);

// (syncfix5) The models player entity `ent`'s copy is drawn with that his wounds are painted on (vr_wounds.cpp): `out` his
// body and his two jointed hands ([1] off, [2] main; each null when not drawn), drawn last frame or this one; `doll` whether
// they draw his dead body. False: no copy of him drawn now (`out` still his copy's three entities, for their masks).
bool woundTargets(int ent, entity_t* out[3], bool& doll);

// (syncfix5) The player (1..MAX_SCOREBOARD) whose copy's held weapon `e` is, its hand in `side`, else 0 (his hot barrel's
// glow: vr_hotblade.cpp).
[[nodiscard]] int weaponOwner(const entity_t* e, int& side);

// A new connection: no avatars.
void clear();

// The avatar calibration (MP_PLAN.md, B2c): what others need of a player's body and see of it, from his own settings --
// height, seated height, arm length, world scale, handedness and the gadget's arm, gloves (kind, colour), nails (and colour: B6), skin
// tone, body (mode, build, bracers, pauldrons; their style, size and place, seated play, a flat-screen body height: B6b), his arms (Body Calibration's measurements, tweaks and shoulders: B3c),
// his holsters' places on his standing body (body::standingHolsters: every holster setting of his, holstersync). Each as an integer (calFields in vr_mpavatar.cpp: metres in mm, shares
// in thousandths...), so what is sent, kept and compared is exact. The client sends his up when it changes (a
// "vr_avatarcal" client command, reliable, at most every vr_net_avatarcal_interval); the server keeps one per client
// slot and passes it on to every client (QVR_SVC_AVATARCAL, reliable), and all of them to a joining client.
// Multiplayer only.
inline constexpr int numCalFields = 84;
struct Calibration
{
    int v[numCalFields]{};
    [[nodiscard]] bool operator==(const Calibration& o) const;
};

// Player entity `ent`'s calibration as this client has it, or nullptr: none (yet, or a client that sends none).
[[nodiscard]] const Calibration* calibrationOf(int ent);
// The server's side: client `slot`'s world scale as his calibration says (vr_world_scale) -> false: none came yet.
[[nodiscard]] bool serverWorldScale(int slot, float& scale);
// The server's side (QC's avatarbody: a dead player's body as his avatar's own, avatardoll): client `slot`'s body build
// (vr_body_build 0..3) and its size (progs/vrdoll*.mdl's model units -> world units: his world scale times his eye height
// over the model's) as his calibration says; single player your own settings; no calibration (a bot): the default build
// at the default world scale. False: none (vr_mp_body 0, his body mode none): id's ranger.
[[nodiscard]] bool serverBody(int slot, int& build, float& scale);
// A dead player's ragdoll's rig (`rig`: progs/vrdoll*.mdl's authored rig, at size `scale`) in his body's proportions:
// his arms' lengths and shoulders as his avatar is drawn (ragdoll::proportioned; backlog1122c); `rig` for any other
// model. The server's side: by his entity number (`owner`: the body's colormap), his calibration as serverBody takes it.
[[nodiscard]] const ragdoll::Rig* serverDollRig(const ragdoll::Rig* rig, int owner, float scale);
// ... the client's: the doll entity `e` (its colours his), his calibration as this client has it (calibrationOf).
[[nodiscard]] const ragdoll::Rig* clientDollRig(const ragdoll::Rig* rig, const entity_t* e, float scale);
// VR_WriteMoveExtras: this client's calibration up, when changed (multiplayer only).
void sendCalibration();
// vr_client.cpp's dispatch: QVR_SVC_AVATARCAL's body.
void parseCalibration();
// The server's side: a client slot reset (connect, drop): its calibration gone, and the clients told.
void serverReset(int slot);
// The server's side: every slot's calibration into a spawning client's message (VR_WriteClientSpawnState).
void serverWriteAll(sizebuf_t* msg);

// vr_mp_avatars (Debug > Reports > Network: Player Avatars; the harness's mp_avatar): the avatars this client has and
// draws, its own hands as drawn and, hosting, the server's view of each player and the avatars sent ("vr_mp_avatars
// hands": each player's hands, held and holstered weapons as drawn too, `mp_hands:` and `mp_hands_own:`).
void init();

} // namespace qvr::mpavatar
