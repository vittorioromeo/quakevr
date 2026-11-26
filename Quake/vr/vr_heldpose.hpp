// vr_heldpose.hpp -- the held weapon's drawn pose up per player (MP_PLAN.md, B3e): what the server's QC and Box3D ask
// of the weapon a player's hand holds as his own view draws it (the weapons' hotspots, the weapon carried off its handle,
// its axes and reverse grip, the two-handed hold's kind, the drawn weapon's place in the hand: vr_builtins_held.inc's
// weaponhotspot, weaponanygrip, weapondrawnpose 0..5 and 44..46; vr_box3d_hands.cpp's weapon hull; vr_carry2h.cpp's
// reach offset).
//
// Every player's comes from his slot (MP_PLAN B11a: one path for single player and multiplayer). A remote player's
// client measures it in his hand's frame (relative to the move's handpos along the axes of its handrot:
// held::axesFromAngles(..., true), forward, left, up) and sends it up when it changes ("vr_heldpose", reliable, at most
// every vr_net_avatarcal_interval a hand); the server places it on the hand of the move it runs. This game's own player
// (single player, a listen server's host) puts his into his slot before each server frame (publishOwn), with the places
// his view drew this frame (Hand::placed: the readers use those rather than placing it on the move's hand).

#pragma once

#include "vr_engine.hpp"

namespace qvr::heldpose
{

constexpr int maxSpots = 4; // (weapons::maxHotspots)

// One hand's drawn pose, in the hand's frame (see above) unless said.
struct Hand
{
    bool drawn{false};        // a weapon drawn in it lately (slot, mirrored, inHand)
    bool mirrored{false};     // drawn as the off hand holds it
    bool mag{false};          // its magazine drawn seated in it (view::DrawnWeapon::mag: the weapon hull's; B6b)
    int slot{-1};             // its Weapon Offsets slot
    glm::mat4 inHand{1.f};    // its entity's place and turn (view::DrawnWeapon::inHand)
    int spotType[maxSpots]{}; // its hotspots (view::weaponHotspot; type 0 none)
    glm::vec3 spotPos[maxSpots]{};
    float spotBias[maxSpots]{};
    float spotShare[maxSpots]{};
    int helpKind{0};              // twohand::helpKind
    bool carrying{false};         // a weapon carried off its handle (twohand::carrying)
    glm::vec3 carriedHandle{0.f}; // ... its handle and tip as view::carriedWeaponInHand gives them (forward, right, up)
    glm::vec3 carriedTip{0.f};
    glm::mat3 carriedAxes{1.f};   // ... its axes (of view angles: the pitch as drawn)
    bool axesKnown{false};        // view::heldWeaponAxes (as they are: the hand's frame already)
    glm::vec3 axesForward{0.f};
    glm::vec3 axesUp{0.f};
    float flip{0.f};
    bool mayReverse{false};  // view::heldMayReverse
    bool flippable{false};   // view::heldGripFlippable
    bool flipBlocked{false}; // view::gripFlipBlocked (ROUND25.md "flipgate")
    int outOfEnt{0};         // the prop the other hand holds alone that this one is drawn held out of (view::heldOutOf; 0 none)
    glm::vec3 outOf{0.f};    // ... how far (world)
    // (B11c) How this hand takes the weapon its other hand carries off its handle (view::carriedGripChoice: -1 it may not
    // be held reversed, 0 upright, 1 reversed; the cosine of its blade's way), and the reverse grip's share it is drawn
    // with as it leaves the hand (view::leavingFlip: a drop or throw keeps the way it was held).
    int carriedGrip{-1};
    float carriedGripCos{1.f};
    float leaveFlip{0.f};
    int leaveSlot{-1}; // (weapons::heldSlot: the weapon's Weapon Offsets slot as it leaves)
    // Placed where the view measuring it drew them this frame (world; this game's own player's: publishOwn). A remote
    // player's are not (they come a round trip late): the readers place them on the hand of the move the server runs.
    bool placed{false};
    glm::vec3 spotWorld[maxSpots]{}; // the hotspots (view::weaponHotspot)
    bool carriedKnown{false};        // the weapon carried off its handle: view::carriedWeaponPose's place, angles, tip
    glm::vec3 carriedPos{0.f};
    glm::vec3 carriedRot{0.f};
    glm::vec3 carriedTipWorld{0.f};
    glm::vec3 gripRot{0.f};          // the hand's own turn and vr_revgrip a loose weapon's reverse grip is judged by
    bool gripMayReverse{false};      // (view::reverseGripChoice's)
    bool handKnown{false};           // the hand as drawn last (view::drawnHandPose): its forward and up (world)
    glm::vec3 handForward{0.f};
    glm::vec3 handUp{0.f};
    bool leaveAnglesKnown{false};    // a held reversed weapon's exact drawn angles (view::leavingAngles; pitch negated for MakeThrown)
    glm::vec3 leaveAngles{0.f};
};

// (B11c) Where a player's body has its holsters (body::Holster's order, world): this game's own player's placed by
// publishOwn from its body now, a remote player's from his move (as his client drew them, on his entity now).
struct Holsters
{
    bool placed{false}; // publishOwn's (else: his move's)
    bool valid{false};
    glm::vec3 at[6]{};
};
// Server, QC holsterpos: `player`'s holster `slot`'s place; false: not known (QC's own estimate then).
[[nodiscard]] bool holsterPoint(edict_t* player, int slot, glm::vec3& out);

// Server: `player`'s `hand` (0 off, 1 main) from his slot (his client's, or this game's own player's: publishOwn);
// nullptr none come yet.
[[nodiscard]] const Hand* slotHand(const edict_t* player, int hand);

// Server: `player`'s `hand` as the move the server runs has it: its place and axes (forward, left, up).
void handPose(edict_t* player, int hand, glm::vec3& pos, glm::mat3& axes);

// A point in `player`'s hand frame (Hand's) placed on his hand now.
[[nodiscard]] glm::vec3 toWorld(edict_t* player, int hand, const glm::vec3& local);

// B3f: what the server asks of a weapon lying about at a player's grab, and his magnetic holsters' presses. His client
// sends them up ("vr_grabspot <hand> <entity> <index>" when the hotspot an empty hand is near changes, twohand::groundSpotNow;
// "vr_magpoll <bits>" when magholster::poll has some); the reverse grip's choice is the server's, by his move's hand and his
// own vr_revgrip (a preference) and flat-screen play.

// Server, QC weapongrabspot: the hotspot (index + 1; 0 none: its handle) by which `player`'s empty `hand` takes `weapon`.
[[nodiscard]] int grabSpot(edict_t* player, int hand, const edict_t* weapon);
// Server, QC weapondrawnpose(weapon, hand, 43, player): how `player`'s `hand` takes the loose `weapon` now (-1 it may not be
// held reversed, 0 upright, 1 reversed; view::reverseGripChoiceAt); `cosine` its blade's way's. `player` nullptr: this
// game's own.
[[nodiscard]] int reverseGrip(edict_t* player, int hand, const edict_t* weapon, float& cosine);
// Server, QC magholsterpoll: what `player` asked since the last call, taken (magholster::poll's bits).
[[nodiscard]] int magPoll(const edict_t* player);

// Client: each hand's drawn pose up when it changed (from clientprefs::send); the ground spots and holster presses (B3f).
// Hosting, nothing: publishOwn puts them into its own slot.
void send();
// Hosting (clientprefs::ownSlot), before each server frame: this game's own hands' drawn poses (placed), ground spots and
// holster presses into its slot, as a remote player's messages put his (none while it is not connected).
void publishOwn();
// Server, before each server frame (bots3): each bot's hands into its slot, its weapons as every client draws them
// (server::botHeld: view::placeHeld's place, hotspots), in the hand's frame (placed on its hand now by the readers): the
// players' disarm grab (QC VR_PDG_Gap's weaponhotspot) takes a bot's weapon where it is drawn.
void placeBots();
// Client: a new connection: they go up again.
void clientClear();
// Server: the slot's player left or a new one came.
void serverReset(int slot);
// vr_mp_drawn, the "vr_heldpose" client command.
void init();

} // namespace qvr::heldpose
