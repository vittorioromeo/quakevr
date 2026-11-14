// vr_heldpose.hpp -- the held weapon's drawn pose up per player (MP_PLAN.md, B3e): what the server's QC and Box3D ask
// of the weapon a player's hand holds as his own view draws it (the weapons' hotspots, the weapon carried off its handle,
// its axes and reverse grip, the two-handed hold's kind, the drawn weapon's place in the hand: vr_builtins_held.inc's
// weaponhotspot, weaponanygrip, weapondrawnpose 0..5 and 44..46; vr_box3d_hands.cpp's weapon hull; vr_carry2h.cpp's
// reach offset).
//
// This game's own player (single player, a listen server's host) reads his view directly, as before. Every other
// player's client measures the same in his hand's frame (relative to the move's handpos along the axes of its handrot:
// held::axesFromAngles(..., true), forward, left, up) and sends it up when it changes ("vr_heldpose", reliable, at most
// every vr_net_avatarcal_interval a hand); the server places it on the hand of the move it runs.

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
    int outOfEnt{0};         // the prop the other hand holds alone that this one is drawn held out of (view::heldOutOf; 0 none)
    glm::vec3 outOf{0.f};    // ... how far (world)
};

// This game's own drawn player: player 1 of single player or a listen server (its view draws him).
[[nodiscard]] bool isLocal(const edict_t* player);

// Server: `player`'s `hand` (0 off, 1 main) as his client sent it; nullptr for this game's own player (ask the view),
// single player, or none come yet.
[[nodiscard]] const Hand* remote(const edict_t* player, int hand);

// Server: `player`'s `hand` as the move the server runs has it: its place and axes (forward, left, up).
void handPose(edict_t* player, int hand, glm::vec3& pos, glm::mat3& axes);

// A point in `player`'s hand frame (Hand's) placed on his hand now.
[[nodiscard]] glm::vec3 toWorld(edict_t* player, int hand, const glm::vec3& local);

// Client: each hand's drawn pose up when it changed (from clientprefs::send).
void send();
// Client: a new connection: they go up again.
void clientClear();
// Server: the slot's player left or a new one came.
void serverReset(int slot);
// vr_mp_drawn, the "vr_heldpose" client command.
void init();

} // namespace qvr::heldpose
